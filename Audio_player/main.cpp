#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <atomic>

// tu se ustawiamy podstawowe parametry aplikacji
const int SAMPLE_RATE = 44100;
const int SAMPLES = 1024;
const int CHANNELS = 2;
const int WINDOW_WIDTH = 1024;
const int WINDOW_HEIGHT = 600;
const int WAVEFORM_HEIGHT = 400;
const int SLIDER_Y = 500;
const int SLIDER_WIDTH = 400;
const int SLIDER_HEIGHT = 30;
const int DEVICE_BUTTON_HEIGHT = 50;
const int DEVICE_BUTTON_MARGIN = 10;

// stan aplikacji - albo wybieramy urządzenie albo już słuchamy
enum class AppState {
    DEVICE_SELECTION,
    LISTENING
};

// zmienne globalne - wiem że to nie jest idealne ale działa xd
std::vector<float> waveformBuffer(SAMPLES * CHANNELS, 0.0f);
std::mutex waveformMutex;
std::atomic<float> volume(1.0f);  // 1.0 to 100% głośności
SDL_AudioDeviceID captureDevice = 0;
SDL_AudioDeviceID playbackDevice = 0;
AppState appState = AppState::DEVICE_SELECTION;
std::vector<std::string> inputDeviceNames;
int hoveredDevice = -1;
int selectedDeviceIndex = -1;

// czcionki do wyświetlania tekstu
TTF_Font* fontLarge = nullptr;
TTF_Font* fontMedium = nullptr;
TTF_Font* fontSmall = nullptr;

// bufor kołowy na audio - działa jak taki pierścień gdzie wpisujemy i odczytujemy
std::vector<float> audioBuffer(SAMPLE_RATE * CHANNELS, 0.0f);
std::atomic<size_t> writePos(0);
std::atomic<size_t> readPos(0);
std::mutex audioMutex;

// ta funkcja się odpala jak mikrofon nagra kawałek dźwięku
void audioCaptureCallback(void* userdata, Uint8* stream, int len) {
    float* samples = reinterpret_cast<float*>(stream);
    int numSamples = len / sizeof(float);
    
    // kopiujemy do bufora żeby narysować fale na ekranie
    {
        std::lock_guard<std::mutex> lock(waveformMutex);
        int copyCount = std::min(numSamples, static_cast<int>(waveformBuffer.size()));
        for (int i = 0; i < copyCount; i++) {
            waveformBuffer[i] = samples[i];
        }
    }
    
    // kopiujemy do bufora kołowego żeby odtworzyć przez głośniki
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        for (int i = 0; i < numSamples; i++) {
            size_t pos = writePos.load() % audioBuffer.size();
            audioBuffer[pos] = samples[i] * volume.load();
            writePos.store((writePos.load() + 1) % audioBuffer.size());
        }
    }
}

// ta funkcja się odpala jak głośniki potrzebują danych do odtworzenia
void audioPlaybackCallback(void* userdata, Uint8* stream, int len) {
    float* output = reinterpret_cast<float*>(stream);
    int numSamples = len / sizeof(float);
    
    std::lock_guard<std::mutex> lock(audioMutex);
    for (int i = 0; i < numSamples; i++) {
        size_t rPos = readPos.load();
        size_t wPos = writePos.load();
        
        // sprawdzamy czy mamy dane w buforze
        size_t available = (wPos >= rPos) ? (wPos - rPos) : (audioBuffer.size() - rPos + wPos);
        
        if (available > static_cast<size_t>(numSamples)) {
            output[i] = audioBuffer[rPos % audioBuffer.size()];
            readPos.store((rPos + 1) % audioBuffer.size());
        } else {
            output[i] = 0.0f;  // jak nie ma danych to cisza, żeby nie trzeszczało
        }
    }
}

// pobieramy liste urządzeń audio z systemu
void listAudioDevices() {
    inputDeviceNames.clear();
    int numInputDevices = SDL_GetNumAudioDevices(SDL_TRUE);
    for (int i = 0; i < numInputDevices; i++) {
        const char* name = SDL_GetAudioDeviceName(i, SDL_TRUE);
        std::string deviceName = name ? name : "Nieznane urz\u0105dzenie";
        inputDeviceNames.push_back(deviceName);
    }
}

// pomocnicza funkcja do rysowania tekstu na ekranie
void renderText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool centered = false) {
    if (!font || text.empty()) return;
    
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }
    
    SDL_Rect destRect = {x, y, surface->w, surface->h};
    if (centered) {
        destRect.x = x - surface->w / 2;
    }
    
    SDL_RenderCopy(renderer, texture, nullptr, &destRect);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

// rysujemy ekran wyboru urządzenia
void drawDeviceSelection(SDL_Renderer* renderer, int mouseX, int mouseY) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color cyan = {0, 200, 255, 255};
    SDL_Color gray = {150, 150, 170, 255};
    SDL_Color darkGray = {100, 100, 120, 255};
    
    // tło tytułu
    SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
    SDL_Rect titleRect = {0, 0, WINDOW_WIDTH, 80};
    SDL_RenderFillRect(renderer, &titleRect);
    
    // taka fajna kreska pod tytułem
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
    SDL_Rect titleBar = {0, 70, WINDOW_WIDTH, 5};
    SDL_RenderFillRect(renderer, &titleBar);
    
    // napis na górze
    renderText(renderer, fontLarge, "Wybierz urzadzenie wejsciowe", WINDOW_WIDTH / 2, 20, white, true);
    
    // obszar z listą urządzeń
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
    SDL_Rect listArea = {50, 100, WINDOW_WIDTH - 100, WINDOW_HEIGHT - 150};
    SDL_RenderFillRect(renderer, &listArea);
    
    // ramka
    SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
    SDL_RenderDrawRect(renderer, &listArea);
    
    // rysujemy przyciski dla każdego urządzenia
    hoveredDevice = -1;
    int buttonY = 120;
    int buttonWidth = WINDOW_WIDTH - 140;
    
    for (size_t i = 0; i < inputDeviceNames.size(); i++) {
        SDL_Rect buttonRect = {70, buttonY, buttonWidth, DEVICE_BUTTON_HEIGHT};
        
        // sprawdzamy czy myszka jest nad przyciskiem
        bool isHovered = (mouseX >= 70 && mouseX <= 70 + buttonWidth &&
                         mouseY >= buttonY && mouseY <= buttonY + DEVICE_BUTTON_HEIGHT);
        
        if (isHovered) {
            hoveredDevice = static_cast<int>(i);
            SDL_SetRenderDrawColor(renderer, 0, 100, 150, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
        }
        SDL_RenderFillRect(renderer, &buttonRect);
        
        // ramka przycisku - podswietlona jak najedziemy myszką
        if (isHovered) {
            SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
        }
        SDL_RenderDrawRect(renderer, &buttonRect);
        
        // numer urządzenia
        std::string numText = "[" + std::to_string(i) + "]";
        renderText(renderer, fontMedium, numText, 85, buttonY + 12, cyan);
        
        // nazwa urządzenia
        renderText(renderer, fontMedium, inputDeviceNames[i], 140, buttonY + 12, white);
        
        buttonY += DEVICE_BUTTON_HEIGHT + DEVICE_BUTTON_MARGIN;
        
        // jak jest za dużo urządzeń to nie rysujemy wszystkich
        if (buttonY > WINDOW_HEIGHT - 100) break;
    }
    
    // instrukcja na dole ekranu
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_Rect instrRect = {0, WINDOW_HEIGHT - 50, WINDOW_WIDTH, 50};
    SDL_RenderFillRect(renderer, &instrRect);
    
    renderText(renderer, fontSmall, "Kliknij na urzadzenie zeby zaczac  |  ESC zeby wyjsc", WINDOW_WIDTH / 2, WINDOW_HEIGHT - 35, gray, true);
}

// rysujemy wykres fali dzwiekowej - wyglada mega
void drawWaveform(SDL_Renderer* renderer) {
    std::lock_guard<std::mutex> lock(waveformMutex);
    
    // tlo pod wykres
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
    SDL_Rect waveformRect = {0, 0, WINDOW_WIDTH, WAVEFORM_HEIGHT};
    SDL_RenderFillRect(renderer, &waveformRect);
    
    // linia srodkowa - zero na wykresie
    SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
    SDL_RenderDrawLine(renderer, 0, WAVEFORM_HEIGHT / 2, WINDOW_WIDTH, WAVEFORM_HEIGHT / 2);
    
    // linie pomocnicze zeby ladniej wygladalo
    for (int i = 1; i < 4; i++) {
        int y = (WAVEFORM_HEIGHT / 4) * i;
        SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    }
    
    // rysujemy lewy kanal - taki niebieski/cyan
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
    int samplesPerPixel = std::max(1, static_cast<int>(waveformBuffer.size() / CHANNELS / WINDOW_WIDTH));
    
    for (int x = 0; x < WINDOW_WIDTH - 1; x++) {
        int idx1 = (x * samplesPerPixel) * CHANNELS;
        int idx2 = ((x + 1) * samplesPerPixel) * CHANNELS;
        
        if (idx1 < static_cast<int>(waveformBuffer.size()) && idx2 < static_cast<int>(waveformBuffer.size())) {
            float sample1 = waveformBuffer[idx1];
            float sample2 = waveformBuffer[idx2];
            
            // uwzgledniamy aktualna glosnosc przy rysowaniu
            sample1 *= volume.load();
            sample2 *= volume.load();
            
            // obcinamy wartosci zeby nie wyszly poza wykres
            sample1 = std::clamp(sample1, -1.0f, 1.0f);
            sample2 = std::clamp(sample2, -1.0f, 1.0f);
            
            int y1 = static_cast<int>((1.0f - sample1) * WAVEFORM_HEIGHT / 2);
            int y2 = static_cast<int>((1.0f - sample2) * WAVEFORM_HEIGHT / 2);
            
            SDL_RenderDrawLine(renderer, x, y1, x + 1, y2);
        }
    }
    
    // rysujemy prawy kanal - rozowy/magenta zeby bylo widac roznice
    SDL_SetRenderDrawColor(renderer, 255, 100, 200, 200);
    for (int x = 0; x < WINDOW_WIDTH - 1; x++) {
        int idx1 = (x * samplesPerPixel) * CHANNELS + 1;
        int idx2 = ((x + 1) * samplesPerPixel) * CHANNELS + 1;
        
        if (idx1 < static_cast<int>(waveformBuffer.size()) && idx2 < static_cast<int>(waveformBuffer.size())) {
            float sample1 = waveformBuffer[idx1] * volume.load();
            float sample2 = waveformBuffer[idx2] * volume.load();
            
            sample1 = std::clamp(sample1, -1.0f, 1.0f);
            sample2 = std::clamp(sample2, -1.0f, 1.0f);
            
            int y1 = static_cast<int>((1.0f - sample1) * WAVEFORM_HEIGHT / 2);
            int y2 = static_cast<int>((1.0f - sample2) * WAVEFORM_HEIGHT / 2);
            
            SDL_RenderDrawLine(renderer, x, y1, x + 1, y2);
        }
    }
}

// rysujemy suwak glosnosci
void drawVolumeSlider(SDL_Renderer* renderer, int mouseX, int mouseY, bool mouseDown) {
    int sliderX = (WINDOW_WIDTH - SLIDER_WIDTH) / 2;
    
    // tlo suwaka
    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    SDL_Rect trackRect = {sliderX, SLIDER_Y, SLIDER_WIDTH, SLIDER_HEIGHT};
    SDL_RenderFillRect(renderer, &trackRect);
    
    // ramka suwaka
    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
    SDL_RenderDrawRect(renderer, &trackRect);
    
    // obliczamy pozycje suwaka (0% do 200% to cala szerokosc)
    float normalizedVolume = volume.load() / 2.0f;
    int sliderPos = static_cast<int>(normalizedVolume * SLIDER_WIDTH);
    
    // wypelniona czesc - pokazuje aktualna glosnosc
    SDL_SetRenderDrawColor(renderer, 0, 150, 200, 255);
    SDL_Rect filledRect = {sliderX, SLIDER_Y, sliderPos, SLIDER_HEIGHT};
    SDL_RenderFillRect(renderer, &filledRect);
    
    // znacznik 100% - taka biala kreska w polowie
    int marker100 = sliderX + SLIDER_WIDTH / 2;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
    SDL_RenderDrawLine(renderer, marker100, SLIDER_Y, marker100, SLIDER_Y + SLIDER_HEIGHT);
    
    // uchwyt suwaka - tym sie przeciaga
    int handleX = sliderX + sliderPos - 5;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect handleRect = {handleX, SLIDER_Y - 5, 10, SLIDER_HEIGHT + 10};
    SDL_RenderFillRect(renderer, &handleRect);
    
    // obsluga przeciagania myszka
    if (mouseDown) {
        if (mouseX >= sliderX && mouseX <= sliderX + SLIDER_WIDTH &&
            mouseY >= SLIDER_Y - 10 && mouseY <= SLIDER_Y + SLIDER_HEIGHT + 10) {
            float newVolume = static_cast<float>(mouseX - sliderX) / SLIDER_WIDTH * 2.0f;
            newVolume = std::clamp(newVolume, 0.0f, 2.0f);
            volume.store(newVolume);
        }
    }
}

// wyswietlamy procent glosnosci i etykiety
void drawVolumeText(SDL_Renderer* renderer) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color cyan = {0, 200, 255, 255};
    SDL_Color yellow = {255, 200, 0, 255};
    SDL_Color red = {255, 100, 100, 255};
    
    int percentage = static_cast<int>(volume.load() * 100);
    int textX = (WINDOW_WIDTH - SLIDER_WIDTH) / 2;
    int textY = SLIDER_Y - 40;
    
    // napis "Glosnosc:"
    renderText(renderer, fontMedium, "Glosnosc:", textX, textY, white);
    
    // wartosc procentowa - zmienia kolor jak jest glosno
    std::string percentText = std::to_string(percentage) + "%";
    SDL_Color percentColor = percentage <= 100 ? cyan : (percentage <= 150 ? yellow : red);
    renderText(renderer, fontMedium, percentText, textX + 120, textY, percentColor);
    
    // etykiety 0% i 200% na koncach
    renderText(renderer, fontSmall, "0%", textX - 25, SLIDER_Y + 5, {100, 100, 120, 255});
    renderText(renderer, fontSmall, "200%", textX + SLIDER_WIDTH + 5, SLIDER_Y + 5, {100, 100, 120, 255});
    
    // znacznik 100% pod suwakiem
    renderText(renderer, fontSmall, "100%", textX + SLIDER_WIDTH/2 - 15, SLIDER_Y + SLIDER_HEIGHT + 5, {100, 100, 120, 255});
}

bool backButtonHovered = false;

// dolny pasek z informacjami i przyciskiem wstecz
void drawInfo(SDL_Renderer* renderer, int mouseX, int mouseY) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color cyan = {0, 200, 255, 255};
    SDL_Color magenta = {255, 100, 200, 255};
    SDL_Color green = {0, 255, 100, 255};
    SDL_Color gray = {150, 150, 170, 255};
    
    // tlo paska informacyjnego
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_Rect infoRect = {0, WINDOW_HEIGHT - 60, WINDOW_WIDTH, 60};
    SDL_RenderFillRect(renderer, &infoRect);
    
    // zielona kropka i napis ze nasluchujemy
    SDL_SetRenderDrawColor(renderer, 0, 255, 100, 255);
    SDL_Rect statusRect = {20, WINDOW_HEIGHT - 45, 15, 15};
    SDL_RenderFillRect(renderer, &statusRect);
    renderText(renderer, fontSmall, "NASLUCHUJE", 45, WINDOW_HEIGHT - 47, green);
    
    // pokazujemy nazwe aktualnego urzadzenia
    if (selectedDeviceIndex >= 0 && selectedDeviceIndex < static_cast<int>(inputDeviceNames.size())) {
        std::string deviceInfo = "Urzadzenie: " + inputDeviceNames[selectedDeviceIndex];
        if (deviceInfo.length() > 50) deviceInfo = deviceInfo.substr(0, 47) + "...";
        renderText(renderer, fontSmall, deviceInfo, 150, WINDOW_HEIGHT - 47, gray);
    }
    
    // legenda kanalow - zeby bylo wiadomo co jest co
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
    SDL_Rect leftChannelRect = {WINDOW_WIDTH - 250, WINDOW_HEIGHT - 50, 20, 12};
    SDL_RenderFillRect(renderer, &leftChannelRect);
    renderText(renderer, fontSmall, "Lewy kanal", WINDOW_WIDTH - 225, WINDOW_HEIGHT - 52, cyan);
    
    SDL_SetRenderDrawColor(renderer, 255, 100, 200, 255);
    SDL_Rect rightChannelRect = {WINDOW_WIDTH - 250, WINDOW_HEIGHT - 30, 20, 12};
    SDL_RenderFillRect(renderer, &rightChannelRect);
    renderText(renderer, fontSmall, "Prawy kanal", WINDOW_WIDTH - 225, WINDOW_HEIGHT - 32, magenta);
    
    // przycisk wstecz
    int backX = 20;
    int backY = WINDOW_HEIGHT - 25;
    int backW = 80;
    int backH = 20;
    
    backButtonHovered = (mouseX >= backX && mouseX <= backX + backW &&
                         mouseY >= backY && mouseY <= backY + backH);
    
    if (backButtonHovered) {
        SDL_SetRenderDrawColor(renderer, 0, 100, 150, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 50, 50, 70, 255);
    }
    SDL_Rect backRect = {backX, backY, backW, backH};
    SDL_RenderFillRect(renderer, &backRect);
    
    SDL_SetRenderDrawColor(renderer, backButtonHovered ? 0 : 100, backButtonHovered ? 200 : 100, backButtonHovered ? 255 : 120, 255);
    SDL_RenderDrawRect(renderer, &backRect);
    
    renderText(renderer, fontSmall, "< Wroc", backX + 15, backY + 2, backButtonHovered ? cyan : gray);
}

// wlaczamy nagrywanie i odtwarzanie audio
bool startAudio(int deviceIndex, SDL_Renderer* renderer, SDL_Window* window) {
    const char* inputDeviceName = SDL_GetAudioDeviceName(deviceIndex, SDL_TRUE);
    
    // resetujemy bufory zeby nie bylo starych danych
    writePos.store(0);
    readPos.store(0);
    std::fill(audioBuffer.begin(), audioBuffer.end(), 0.0f);
    std::fill(waveformBuffer.begin(), waveformBuffer.end(), 0.0f);
    
    // konfigurujemy nagrywanie z mikrofonu
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
    
    // konfigurujemy odtwarzanie przez glosniki
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
    
    // odpalamy nagrywanie i odtwarzanie
    SDL_PauseAudioDevice(captureDevice, 0);
    SDL_PauseAudioDevice(playbackDevice, 0);
    
    return true;
}

// zatrzymujemy audio i zamykamy urzadzenia
void stopAudio() {
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

// glowna funkcja programu - tu sie wszystko zaczyna
int main(int argc, char* argv[]) {
    // inicjalizacja SDL - bez tego nic nie zadziala
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blad", "Nie udalo sie uruchomic SDL!", nullptr);
        return 1;
    }
    
    // inicjalizacja TTF do wyswietlania tekstu
    if (TTF_Init() < 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blad", "Nie udalo sie uruchomic TTF!", nullptr);
        SDL_Quit();
        return 1;
    }
    
    // pobieramy liste dostepnych urzadzen audio
    listAudioDevices();
    
    // tworzymy okno aplikacji
    SDL_Window* window = SDL_CreateWindow(
        "Przelot Audio - Wybierz urzadzenie",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blad", "Nie udalo sie utworzyc okna!", nullptr);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    // tworzymy renderer do rysowania
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blad", "Nie udalo sie utworzyc renderera!", nullptr);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    // szukamy czcionki systemowej - probujemy kilka popularnych
    const char* fontPaths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        nullptr
    };
    
    const char* fontPath = nullptr;
    for (int i = 0; fontPaths[i] != nullptr; i++) {
        fontLarge = TTF_OpenFont(fontPaths[i], 28);
        if (fontLarge) {
            fontPath = fontPaths[i];
            TTF_CloseFont(fontLarge);
            break;
        }
    }
    
    if (!fontPath) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blad", "Nie znaleziono czcionki systemowej!", nullptr);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    // ladujemy czcionki w roznych rozmiarach
    fontLarge = TTF_OpenFont(fontPath, 28);
    fontMedium = TTF_OpenFont(fontPath, 20);
    fontSmall = TTF_OpenFont(fontPath, 14);
    
    if (!fontLarge || !fontMedium || !fontSmall) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blad", "Nie udalo sie zaladowac czcionek!", nullptr);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    // glowna petla aplikacji
    bool running = true;
    SDL_Event event;
    int mouseX = 0, mouseY = 0;
    bool mouseDown = false;
    bool mouseClicked = false;
    
    while (running) {
        mouseClicked = false;
        
        // obsluga eventow - klikniecia, klawisze itp.
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE) {
                        if (appState == AppState::LISTENING) {
                            stopAudio();
                            appState = AppState::DEVICE_SELECTION;
                            selectedDeviceIndex = -1;
                            SDL_SetWindowTitle(window, "Przelot Audio - Wybierz urzadzenie");
                        } else {
                            running = false;
                        }
                    }
                    break;
                case SDL_MOUSEMOTION:
                    mouseX = event.motion.x;
                    mouseY = event.motion.y;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouseDown = true;
                        mouseX = event.button.x;
                        mouseY = event.button.y;
                        mouseClicked = true;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouseDown = false;
                    }
                    break;
            }
        }
        
        // czyscimy ekran przed rysowaniem
        SDL_SetRenderDrawColor(renderer, 25, 25, 35, 255);
        SDL_RenderClear(renderer);
        
        if (appState == AppState::DEVICE_SELECTION) {
            // rysujemy ekran wyboru urzadzenia
            drawDeviceSelection(renderer, mouseX, mouseY);
            
            // obsluga klikniecia na urzadzenie
            if (mouseClicked && hoveredDevice >= 0) {
                if (startAudio(hoveredDevice, renderer, window)) {
                    selectedDeviceIndex = hoveredDevice;
                    appState = AppState::LISTENING;
                    SDL_SetWindowTitle(window, "Przelot Audio - Nasluchuje");
                }
            }
        } else {
            // rysujemy wykres i kontrolki
            drawWaveform(renderer);
            drawVolumeSlider(renderer, mouseX, mouseY, mouseDown);
            drawVolumeText(renderer);
            drawInfo(renderer, mouseX, mouseY);
            
            // obsluga przycisku wstecz
            if (mouseClicked && backButtonHovered) {
                stopAudio();
                appState = AppState::DEVICE_SELECTION;
                selectedDeviceIndex = -1;
                SDL_SetWindowTitle(window, "Przelot Audio - Wybierz urzadzenie");
            }
        }
        
        // wyswietlamy to co narysowalismy
        SDL_RenderPresent(renderer);
    }
    
    // sprzatamy po sobie - zamykamy wszystko ladnie
    stopAudio();
    if (fontLarge) TTF_CloseFont(fontLarge);
    if (fontMedium) TTF_CloseFont(fontMedium);
    if (fontSmall) TTF_CloseFont(fontSmall);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    
    return 0;
}
