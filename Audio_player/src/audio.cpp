// audio.cpp - implementacja obslugi audio
// najbardziej skomplikowana czesc calej apki tbh

#include "audio.h"
#include <cmath>
#include <algorithm>
#include <complex>

// globalna instancja managera audio
AudioManager* g_audioManager = nullptr;

// PI bo C++ nie ma go w standardzie (lol)
constexpr float PI = 3.14159265358979323846f;

// prosta implementacja FFT (Cooley-Tukey)
// nie jest najszybsza ale dziala i jest zrozumiala
void computeFFT(const std::vector<float>& input, std::vector<float>& magnitudes) {
    const int N = static_cast<int>(input.size());
    
    // tworzymy tablice liczb zespolonych
    std::vector<std::complex<float>> data(N);
    for (int i = 0; i < N; i++) {
        // okno Hanninga zeby nie bylo artefaktow na krawdziach
        float window = 0.5f * (1.0f - std::cos(2.0f * PI * i / (N - 1)));
        data[i] = std::complex<float>(input[i] * window, 0.0f);
    }
    
    // bit-reversal permutation - brzmi strasznie ale to tylko zamiana miejscami
    int bits = static_cast<int>(std::log2(N));
    for (int i = 0; i < N; i++) {
        int j = 0;
        for (int k = 0; k < bits; k++) {
            if (i & (1 << k)) j |= (1 << (bits - 1 - k));
        }
        if (i < j) std::swap(data[i], data[j]);
    }
    
    // wlasciwe FFT - iteracyjna wersja Cooley-Tukey
    for (int len = 2; len <= N; len *= 2) {
        float angle = -2.0f * PI / len;
        std::complex<float> wlen(std::cos(angle), std::sin(angle));
        
        for (int i = 0; i < N; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; j++) {
                std::complex<float> u = data[i + j];
                std::complex<float> v = data[i + j + len/2] * w;
                data[i + j] = u + v;
                data[i + j + len/2] = u - v;
                w *= wlen;
            }
        }
    }
    
    // obliczamy magnitudy (tylko pierwsza polowa bo reszta to lustrzane odbicie)
    magnitudes.resize(N / 2);
    for (int i = 0; i < N / 2; i++) {
        // skala logarytmiczna zeby bylo widac ciche dzwieki
        float mag = std::abs(data[i]) / N;
        magnitudes[i] = mag;
    }
}

AudioManager::AudioManager() 
    : waveformBuffer(SAMPLES * CHANNELS, 0.0f)
    , audioBuffer(SAMPLE_RATE * CHANNELS, 0.0f)
    , fftInputBuffer(FFT_SIZE, 0.0f)
    , fftMagnitudes(FFT_SIZE / 2, 0.0f)
{
    g_audioManager = this;
}

AudioManager::~AudioManager() {
    stopAudio();
    g_audioManager = nullptr;
}

void AudioManager::listInputDevices() {
    inputDeviceNames.clear();
    int numInputDevices = SDL_GetNumAudioDevices(SDL_TRUE);
    for (int i = 0; i < numInputDevices; i++) {
        const char* name = SDL_GetAudioDeviceName(i, SDL_TRUE);
        std::string deviceName = name ? name : "Nieznane urzadzenie";
        inputDeviceNames.push_back(deviceName);
    }
}

void AudioManager::audioCaptureCallback(void* userdata, Uint8* stream, int len) {
    if (!g_audioManager) return;
    
    float* samples = reinterpret_cast<float*>(stream);
    int numSamples = len / sizeof(float);
    float vol = g_audioManager->volume.load();
    
    // prog szumu - konwertujemy z dB na amplitude liniowa
    float noiseGateDB = g_audioManager->noiseGateDB.load();
    float noiseThresholdLinear = AudioManager::dbToLinear(noiseGateDB);
    
    // parametry envelope followera (plynne przejscia)
    // attack = jak szybko bramka sie otwiera, release = jak szybko sie zamyka
    const float attackTime = 0.001f;  // 1ms - szybkie otwarcie
    const float releaseTime = 0.050f; // 50ms - wolne zamkniecie (zapobiega skwierczeniu)
    const float attackCoef = 1.0f - std::exp(-1.0f / (SAMPLE_RATE * attackTime));
    const float releaseCoef = 1.0f - std::exp(-1.0f / (SAMPLE_RATE * releaseTime));
    
    // aplikujemy noise gate z plynnym przejsciem
    for (int i = 0; i < numSamples; i += CHANNELS) {
        // liczymy amplityde dla pary sampli (stereo)
        float leftAbs = std::abs(samples[i]);
        float rightAbs = (i + 1 < numSamples) ? std::abs(samples[i + 1]) : 0.0f;
        float maxAmp = std::max(leftAbs, rightAbs);
        
        // docelowy poziom bramki
        float targetGain = (maxAmp >= noiseThresholdLinear) ? 1.0f : 0.0f;
        
        // plynne przejscie do docelowego poziomu
        if (targetGain > g_audioManager->gateEnvelope) {
            // otwieranie bramki (attack)
            g_audioManager->gateEnvelope += attackCoef * (targetGain - g_audioManager->gateEnvelope);
        } else {
            // zamykanie bramki (release)
            g_audioManager->gateEnvelope += releaseCoef * (targetGain - g_audioManager->gateEnvelope);
        }
        
        // aplikujemy gain
        samples[i] *= g_audioManager->gateEnvelope;
        if (i + 1 < numSamples) samples[i + 1] *= g_audioManager->gateEnvelope;
    }
    
    // kopiujemy do bufora waveform
    {
        std::lock_guard<std::mutex> lock(g_audioManager->waveformMutex);
        int copyCount = std::min(numSamples, static_cast<int>(g_audioManager->waveformBuffer.size()));
        for (int i = 0; i < copyCount; i++) {
            g_audioManager->waveformBuffer[i] = samples[i];
        }
    }
    
    // kopiujemy do bufora FFT (tylko lewy kanal dla uproszczenia)
    {
        std::lock_guard<std::mutex> lock(g_audioManager->fftMutex);
        for (int i = 0; i < numSamples / CHANNELS && i < FFT_SIZE; i++) {
            g_audioManager->fftInputBuffer[i] = samples[i * CHANNELS];
        }
        // od razu liczymy FFT
        computeFFT(g_audioManager->fftInputBuffer, g_audioManager->fftMagnitudes);
    }
    
    // kopiujemy do bufora kolowego dla playback
    {
        std::lock_guard<std::mutex> lock(g_audioManager->audioMutex);
        for (int i = 0; i < numSamples; i++) {
            size_t pos = g_audioManager->writePos.load() % g_audioManager->audioBuffer.size();
            g_audioManager->audioBuffer[pos] = samples[i] * vol;
            g_audioManager->writePos.store((g_audioManager->writePos.load() + 1) % g_audioManager->audioBuffer.size());
        }
    }
}

void AudioManager::audioPlaybackCallback(void* userdata, Uint8* stream, int len) {
    if (!g_audioManager) return;
    
    float* output = reinterpret_cast<float*>(stream);
    int numSamples = len / sizeof(float);
    
    std::lock_guard<std::mutex> lock(g_audioManager->audioMutex);
    for (int i = 0; i < numSamples; i++) {
        size_t rPos = g_audioManager->readPos.load();
        size_t wPos = g_audioManager->writePos.load();
        
        size_t available = (wPos >= rPos) ? (wPos - rPos) : (g_audioManager->audioBuffer.size() - rPos + wPos);
        
        if (available > static_cast<size_t>(numSamples)) {
            output[i] = g_audioManager->audioBuffer[rPos % g_audioManager->audioBuffer.size()];
            g_audioManager->readPos.store((rPos + 1) % g_audioManager->audioBuffer.size());
        } else {
            output[i] = 0.0f;
        }
    }
}

bool AudioManager::startAudio(int deviceIndex) {
    const char* inputDeviceName = SDL_GetAudioDeviceName(deviceIndex, SDL_TRUE);
    
    // reset buforow
    writePos.store(0);
    readPos.store(0);
    std::fill(audioBuffer.begin(), audioBuffer.end(), 0.0f);
    std::fill(waveformBuffer.begin(), waveformBuffer.end(), 0.0f);
    std::fill(fftInputBuffer.begin(), fftInputBuffer.end(), 0.0f);
    std::fill(fftMagnitudes.begin(), fftMagnitudes.end(), 0.0f);
    
    // konfiguracja capture
    SDL_AudioSpec captureSpec, obtainedCaptureSpec;
    SDL_zero(captureSpec);
    captureSpec.freq = SAMPLE_RATE;
    captureSpec.format = AUDIO_F32;
    captureSpec.channels = CHANNELS;
    captureSpec.samples = SAMPLES;
    captureSpec.callback = audioCaptureCallback;
    
    captureDevice = SDL_OpenAudioDevice(inputDeviceName, SDL_TRUE, &captureSpec, &obtainedCaptureSpec, 0);
    if (captureDevice == 0) {
        return false;
    }
    
    // konfiguracja playback
    SDL_AudioSpec playbackSpec, obtainedPlaybackSpec;
    SDL_zero(playbackSpec);
    playbackSpec.freq = SAMPLE_RATE;
    playbackSpec.format = AUDIO_F32;
    playbackSpec.channels = CHANNELS;
    playbackSpec.samples = SAMPLES;
    playbackSpec.callback = audioPlaybackCallback;
    
    playbackDevice = SDL_OpenAudioDevice(nullptr, SDL_FALSE, &playbackSpec, &obtainedPlaybackSpec, 0);
    if (playbackDevice == 0) {
        SDL_CloseAudioDevice(captureDevice);
        captureDevice = 0;
        return false;
    }
    
    SDL_PauseAudioDevice(captureDevice, 0);
    SDL_PauseAudioDevice(playbackDevice, 0);
    
    return true;
}

void AudioManager::stopAudio() {
    if (captureDevice != 0) {
        SDL_PauseAudioDevice(captureDevice, 1);
        SDL_CloseAudioDevice(captureDevice);
        captureDevice = 0;
    }
    if (playbackDevice != 0) {
        SDL_PauseAudioDevice(playbackDevice, 1);
        SDL_CloseAudioDevice(playbackDevice);
        playbackDevice = 0;
    }
}

void AudioManager::getWaveformData(std::vector<float>& outBuffer) {
    std::lock_guard<std::mutex> lock(waveformMutex);
    outBuffer = waveformBuffer;
}

void AudioManager::getFFTData(std::vector<float>& outMagnitudes) {
    std::lock_guard<std::mutex> lock(fftMutex);
    outMagnitudes = fftMagnitudes;
}
