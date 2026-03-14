#pragma once

// audio.h - obsluga nagrywania i odtwarzania dzwieku
// cala magia audio jest tutaj

#include <SDL2/SDL.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <string>
#include <cmath>
#include "config.h"

// klasa do zarzadzania audio - wszystko w jednym miejscu
class AudioManager {
public:
    AudioManager();
    ~AudioManager();
    
    // pobiera liste urzadzen wejsciowych
    void listInputDevices();
    const std::vector<std::string>& getInputDeviceNames() const { return inputDeviceNames; }
    
    // wlacza/wylacza audio
    bool startAudio(int deviceIndex);
    void stopAudio();
    bool isRunning() const { return captureDevice != 0; }
    
    // glosnosc (0.0 - 2.0, czyli 0% - 200%)
    void setVolume(float vol) { volume.store(vol); }
    float getVolume() const { return volume.load(); }
    
    // prog szumu w dB - wszystko ponizej tego jest traktowane jako cisza
    void setNoiseGateDB(float db) { noiseGateDB.store(db); }
    float getNoiseGateDB() const { return noiseGateDB.load(); }
    
    // konwersja dB na amplityde liniowa
    static float dbToLinear(float db) { return std::pow(10.0f, db / 20.0f); }
    
    // max czestotliwosc do wyswietlenia na spektrogramie
    void setMaxDisplayFreq(float freq) { maxDisplayFreq.store(freq); }
    float getMaxDisplayFreq() const { return maxDisplayFreq.load(); }
    
    // dostep do danych dla wizualizacji (thread-safe)
    void getWaveformData(std::vector<float>& outBuffer);
    void getFFTData(std::vector<float>& outMagnitudes);
    
private:
    // callbacki SDL - musza byc statyczne bo SDL tak chce
    static void audioCaptureCallback(void* userdata, Uint8* stream, int len);
    static void audioPlaybackCallback(void* userdata, Uint8* stream, int len);
    
    // urzadzenia
    SDL_AudioDeviceID captureDevice = 0;
    SDL_AudioDeviceID playbackDevice = 0;
    std::vector<std::string> inputDeviceNames;
    
    // glosnosc
    std::atomic<float> volume{1.0f};
    
    // prog szumu w dB i zoom czestotliwosci
    std::atomic<float> noiseGateDB{DEFAULT_NOISE_GATE_DB};
    std::atomic<float> maxDisplayFreq{22050.0f};
    
    // envelope follower dla bramki szumow (plynne przejscia)
    float gateEnvelope = 1.0f;  // aktualny poziom bramki 0-1
    
    // bufor na fale - do rysowania
    std::vector<float> waveformBuffer;
    std::mutex waveformMutex;
    
    // bufor kolowy na audio passthrough
    std::vector<float> audioBuffer;
    std::atomic<size_t> writePos{0};
    std::atomic<size_t> readPos{0};
    std::mutex audioMutex;
    
    // bufor na FFT
    std::vector<float> fftInputBuffer;
    std::vector<float> fftMagnitudes;
    std::mutex fftMutex;
};

// globalna instancja - leniwe ale wygodne
extern AudioManager* g_audioManager;
