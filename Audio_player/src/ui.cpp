// ui.cpp - implementacja calego UI
// tu sie dzieją cuda na ekranie

#include "ui.h"
#include "audio.h"
#include <cmath>
#include <algorithm>

UIManager* g_uiManager = nullptr;

UIManager::UIManager() {
    g_uiManager = this;
}

UIManager::~UIManager() {
    shutdown();
    g_uiManager = nullptr;
}

bool UIManager::init() {
    // inicjalizacja SDL
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blad", "Nie udalo sie uruchomic SDL!", nullptr);
        return false;
    }
    
    // inicjalizacja TTF
    if (TTF_Init() < 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blad", "Nie udalo sie uruchomic TTF!", nullptr);
        SDL_Quit();
        return false;
    }
    
    // tworzymy okno - teraz mozna zmieniac rozmiar!
    window = SDL_CreateWindow(
        "Przelot Audio - Wybierz urzadzenie",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blad", "Nie udalo sie utworzyc okna!", nullptr);
        TTF_Quit();
        SDL_Quit();
        return false;
    }
    
    // ustawiamy minimalny rozmiar okna
    SDL_SetWindowMinimumSize(window, MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
    
    // synchronizujemy wymiary okna z rzeczywistym rozmiarem
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    
    // tworzymy renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blad", "Nie udalo sie utworzyc renderera!", nullptr);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }
    
    // szukamy czcionki
    const char* fontPaths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        nullptr
    };
    
    const char* fontPath = nullptr;
    for (int i = 0; fontPaths[i] != nullptr; i++) {
        TTF_Font* test = TTF_OpenFont(fontPaths[i], 28);
        if (test) {
            fontPath = fontPaths[i];
            TTF_CloseFont(test);
            break;
        }
    }
    
    if (!fontPath) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blad", "Nie znaleziono czcionki systemowej!", nullptr);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }
    
    // ladujemy czcionki w roznych rozmiarach
    fontLarge = TTF_OpenFont(fontPath, 28);
    fontMedium = TTF_OpenFont(fontPath, 20);
    fontSmall = TTF_OpenFont(fontPath, 14);
    
    if (!fontLarge || !fontMedium || !fontSmall) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blad", "Nie udalo sie zaladowac czcionek!", nullptr);
        shutdown();
        return false;
    }
    
    return true;
}

void UIManager::shutdown() {
    if (fontLarge) { TTF_CloseFont(fontLarge); fontLarge = nullptr; }
    if (fontMedium) { TTF_CloseFont(fontMedium); fontMedium = nullptr; }
    if (fontSmall) { TTF_CloseFont(fontSmall); fontSmall = nullptr; }
    if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
    if (window) { SDL_DestroyWindow(window); window = nullptr; }
    TTF_Quit();
    SDL_Quit();
}

void UIManager::handleResize(int newWidth, int newHeight) {
    windowWidth = newWidth;
    windowHeight = newHeight;
}

void UIManager::renderText(const std::string& text, int x, int y, SDL_Color color, bool centered, TTF_Font* font) {
    if (!font) font = fontMedium;
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

void UIManager::render(AppState state, int mouseX, int mouseY, bool mouseDown) {
    // czyscimy ekran
    SDL_SetRenderDrawColor(renderer, 25, 25, 35, 255);
    SDL_RenderClear(renderer);
    
    if (state == AppState::DEVICE_SELECTION) {
        drawDeviceSelection(mouseX, mouseY);
    } else {
        drawWaveform();
        drawSpectrogram();
        drawVolumeSlider(mouseX, mouseY, mouseDown);
        drawVolumeText();
        drawNoiseSlider(mouseX, mouseY, mouseDown);
        drawNoiseText();
        drawFreqSlider(mouseX, mouseY, mouseDown);
        drawFreqText();
        drawInfoBar(mouseX, mouseY);
    }
    
    SDL_RenderPresent(renderer);
}

void UIManager::drawDeviceSelection(int mouseX, int mouseY) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color cyan = {0, 200, 255, 255};
    SDL_Color gray = {150, 150, 170, 255};
    
    // tlo tytulu
    SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
    SDL_Rect titleRect = {0, 0, windowWidth, 80};
    SDL_RenderFillRect(renderer, &titleRect);
    
    // kreska pod tytulem
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
    SDL_Rect titleBar = {0, 70, windowWidth, 5};
    SDL_RenderFillRect(renderer, &titleBar);
    
    // tytul
    renderText("Wybierz urzadzenie wejsciowe", windowWidth / 2, 20, white, true, fontLarge);
    
    // obszar z lista
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
    SDL_Rect listArea = {50, 100, windowWidth - 100, windowHeight - 150};
    SDL_RenderFillRect(renderer, &listArea);
    
    SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
    SDL_RenderDrawRect(renderer, &listArea);
    
    // przyciski urzadzen
    hoveredDevice = -1;
    int buttonY = 120;
    int buttonWidth = windowWidth - 140;
    
    for (size_t i = 0; i < deviceNames.size(); i++) {
        SDL_Rect buttonRect = {70, buttonY, buttonWidth, DEVICE_BUTTON_HEIGHT};
        
        bool isHovered = (mouseX >= 70 && mouseX <= 70 + buttonWidth &&
                         mouseY >= buttonY && mouseY <= buttonY + DEVICE_BUTTON_HEIGHT);
        
        if (isHovered) {
            hoveredDevice = static_cast<int>(i);
            SDL_SetRenderDrawColor(renderer, 0, 100, 150, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
        }
        SDL_RenderFillRect(renderer, &buttonRect);
        
        if (isHovered) {
            SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
        }
        SDL_RenderDrawRect(renderer, &buttonRect);
        
        std::string numText = "[" + std::to_string(i) + "]";
        renderText(numText, 85, buttonY + 12, cyan);
        renderText(deviceNames[i], 140, buttonY + 12, white);
        
        buttonY += DEVICE_BUTTON_HEIGHT + DEVICE_BUTTON_MARGIN;
        if (buttonY > windowHeight - 100) break;
    }
    
    // instrukcja na dole
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_Rect instrRect = {0, windowHeight - 50, windowWidth, 50};
    SDL_RenderFillRect(renderer, &instrRect);
    
    renderText("Kliknij na urzadzenie zeby zaczac  |  ESC zeby wyjsc", windowWidth / 2, windowHeight - 35, gray, true, fontSmall);
}

void UIManager::drawWaveform() {
    if (!g_audioManager) return;
    
    std::vector<float> waveformBuffer;
    g_audioManager->getWaveformData(waveformBuffer);
    float vol = g_audioManager->getVolume();
    
    int waveformHeight = getWaveformHeight();
    
    // tlo
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
    SDL_Rect waveformRect = {0, 0, windowWidth, waveformHeight};
    SDL_RenderFillRect(renderer, &waveformRect);
    
    // linia srodkowa
    SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
    SDL_RenderDrawLine(renderer, 0, waveformHeight / 2, windowWidth, waveformHeight / 2);
    
    // linie pomocnicze
    for (int i = 1; i < 4; i++) {
        int y = (waveformHeight / 4) * i;
        SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
        SDL_RenderDrawLine(renderer, 0, y, windowWidth, y);
    }
    
    if (waveformBuffer.empty()) return;
    
    // skalowanie: mapujemy probki na cala szerokosc okna
    int numSamples = static_cast<int>(waveformBuffer.size() / CHANNELS);
    float sampleScale = static_cast<float>(numSamples) / windowWidth;
    
    // lewy kanal - cyan
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
    for (int x = 0; x < windowWidth - 1; x++) {
        int idx1 = std::min(static_cast<int>(x * sampleScale), numSamples - 1) * CHANNELS;
        int idx2 = std::min(static_cast<int>((x + 1) * sampleScale), numSamples - 1) * CHANNELS;
        
        float sample1 = std::clamp(waveformBuffer[idx1] * vol, -1.0f, 1.0f);
        float sample2 = std::clamp(waveformBuffer[idx2] * vol, -1.0f, 1.0f);
        
        int y1 = static_cast<int>((1.0f - sample1) * waveformHeight / 2);
        int y2 = static_cast<int>((1.0f - sample2) * waveformHeight / 2);
        
        SDL_RenderDrawLine(renderer, x, y1, x + 1, y2);
    }
    
    // prawy kanal - magenta
    SDL_SetRenderDrawColor(renderer, 255, 100, 200, 200);
    for (int x = 0; x < windowWidth - 1; x++) {
        int idx1 = std::min(static_cast<int>(x * sampleScale), numSamples - 1) * CHANNELS + 1;
        int idx2 = std::min(static_cast<int>((x + 1) * sampleScale), numSamples - 1) * CHANNELS + 1;
        
        float sample1 = std::clamp(waveformBuffer[idx1] * vol, -1.0f, 1.0f);
        float sample2 = std::clamp(waveformBuffer[idx2] * vol, -1.0f, 1.0f);
        
        int y1 = static_cast<int>((1.0f - sample1) * waveformHeight / 2);
        int y2 = static_cast<int>((1.0f - sample2) * waveformHeight / 2);
        
        SDL_RenderDrawLine(renderer, x, y1, x + 1, y2);
    }
    
    // etykieta
    SDL_Color white = {255, 255, 255, 255};
    renderText("Fala dzwiekowa", 10, 5, white, false, fontSmall);
}

void UIManager::drawSpectrogram() {
    if (!g_audioManager) return;
    
    std::vector<float> magnitudes;
    g_audioManager->getFFTData(magnitudes);
    
    // prog szumu w dB -> liniowy do wizualizacji
    float noiseGateDB = g_audioManager->getNoiseGateDB();
    float noiseThresholdLinear = AudioManager::dbToLinear(noiseGateDB);
    float maxDisplayFreq = g_audioManager->getMaxDisplayFreq();
    
    int spectY = getSpectrogramY();
    int spectHeight = getSpectrogramHeight();
    
    // tlo spektrogramu
    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect spectRect = {0, spectY, windowWidth, spectHeight};
    SDL_RenderFillRect(renderer, &spectRect);
    
    // ramka
    SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
    SDL_RenderDrawRect(renderer, &spectRect);
    
    // linie pomocnicze poziome
    for (int i = 1; i < 4; i++) {
        int y = spectY + (spectHeight / 4) * i;
        SDL_SetRenderDrawColor(renderer, 30, 30, 45, 255);
        SDL_RenderDrawLine(renderer, 0, y, windowWidth, y);
    }
    
    if (magnitudes.empty()) return;
    
    // ile binow FFT miesci sie w naszym zakresie czestotliwosci
    float freqPerBin = static_cast<float>(SAMPLE_RATE) / FFT_SIZE;
    int maxBin = static_cast<int>(maxDisplayFreq / freqPerBin);
    maxBin = std::min(maxBin, static_cast<int>(magnitudes.size()));
    
    // szukamy max zeby normalizowac (tylko w widocznym zakresie)
    float maxMag = 0.001f;
    for (int i = 0; i < maxBin; i++) {
        maxMag = std::max(maxMag, magnitudes[i]);
    }
    
    // szerokosc slupka na podstawie zakresu
    float barWidth = static_cast<float>(windowWidth) / maxBin;
    
    // rysujemy kazdy slupek
    for (int i = 0; i < maxBin; i++) {
        float mag = magnitudes[i] / maxMag;
        
        // aplikujemy prog szumu (juz jest aplikowany do audio, ale tez do wizualizacji)
        if (mag < noiseThresholdLinear) {
            mag = 0.0f;
        } else if (noiseThresholdLinear < 1.0f) {
            // skalujemy pozostaly zakres do 0-1
            mag = (mag - noiseThresholdLinear) / (1.0f - noiseThresholdLinear);
        }
        
        // skala logarytmiczna zeby lepiej wygladalo
        mag = std::log10(1.0f + mag * 9.0f);
        mag = std::clamp(mag, 0.0f, 1.0f);
        
        int barHeight = static_cast<int>(mag * (spectHeight - 25));
        int barX = static_cast<int>(i * barWidth);
        int barY = spectY + spectHeight - 15 - barHeight;
        
        // kolor zalezy od wysokosci
        int r, g, b;
        if (mag < 0.5f) {
            float t = mag * 2.0f;
            r = 0;
            g = static_cast<int>(t * 255);
            b = static_cast<int>((1.0f - t) * 255);
        } else {
            float t = (mag - 0.5f) * 2.0f;
            r = static_cast<int>(t * 255);
            g = static_cast<int>((1.0f - t) * 255);
            b = 0;
        }
        
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        int actualBarWidth = std::max(1, static_cast<int>(barWidth) - 1);
        SDL_Rect barRect = {barX, barY, actualBarWidth, barHeight};
        SDL_RenderFillRect(renderer, &barRect);
    }
    
    // etykiety
    SDL_Color gray = {150, 150, 170, 255};
    SDL_Color white = {255, 255, 255, 255};
    renderText("Spektrogram (FFT)", 10, spectY + 5, white, false, fontSmall);
    
    // etykiety osi X - dynamiczne na podstawie maxDisplayFreq
    renderText("0", 5, spectY + spectHeight - 18, gray, false, fontSmall);
    
    int midFreq = static_cast<int>(maxDisplayFreq / 2);
    std::string midLabel = midFreq >= 1000 ? std::to_string(midFreq / 1000) + "k" : std::to_string(midFreq);
    renderText(midLabel, windowWidth / 2 - 15, spectY + spectHeight - 18, gray, false, fontSmall);
    
    int maxFreqInt = static_cast<int>(maxDisplayFreq);
    std::string maxLabel = maxFreqInt >= 1000 ? std::to_string(maxFreqInt / 1000) + "k" : std::to_string(maxFreqInt);
    renderText(maxLabel, windowWidth - 30, spectY + spectHeight - 18, gray, false, fontSmall);
}

void UIManager::drawVolumeSlider(int mouseX, int mouseY, bool mouseDown) {
    if (!g_audioManager) return;
    
    int sliderWidth = getSliderWidth();
    int sliderY = getVolumeSliderY();
    int sliderX = (windowWidth - sliderWidth) / 2;
    
    // tlo suwaka
    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    SDL_Rect trackRect = {sliderX, sliderY, sliderWidth, SLIDER_HEIGHT};
    SDL_RenderFillRect(renderer, &trackRect);
    
    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
    SDL_RenderDrawRect(renderer, &trackRect);
    
    float vol = g_audioManager->getVolume();
    float normalizedVolume = vol / 2.0f;
    int sliderPos = static_cast<int>(normalizedVolume * sliderWidth);
    
    // wypelniona czesc
    SDL_SetRenderDrawColor(renderer, 0, 150, 200, 255);
    SDL_Rect filledRect = {sliderX, sliderY, sliderPos, SLIDER_HEIGHT};
    SDL_RenderFillRect(renderer, &filledRect);
    
    // znacznik 100%
    int marker100 = sliderX + sliderWidth / 2;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
    SDL_RenderDrawLine(renderer, marker100, sliderY, marker100, sliderY + SLIDER_HEIGHT);
    
    // uchwyt
    int handleX = sliderX + sliderPos - 5;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect handleRect = {handleX, sliderY - 3, 10, SLIDER_HEIGHT + 6};
    SDL_RenderFillRect(renderer, &handleRect);
    
    // obsluga przeciagania
    if (mouseDown) {
        if (mouseX >= sliderX && mouseX <= sliderX + sliderWidth &&
            mouseY >= sliderY - 10 && mouseY <= sliderY + SLIDER_HEIGHT + 10) {
            float newVolume = static_cast<float>(mouseX - sliderX) / sliderWidth * 2.0f;
            newVolume = std::clamp(newVolume, 0.0f, 2.0f);
            g_audioManager->setVolume(newVolume);
        }
    }
}

void UIManager::drawVolumeText() {
    if (!g_audioManager) return;
    
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color cyan = {0, 200, 255, 255};
    SDL_Color yellow = {255, 200, 0, 255};
    SDL_Color red = {255, 100, 100, 255};
    SDL_Color gray = {100, 100, 120, 255};
    
    int sliderWidth = getSliderWidth();
    int sliderY = getVolumeSliderY();
    int percentage = static_cast<int>(g_audioManager->getVolume() * 100);
    int textX = (windowWidth - sliderWidth) / 2;
    int textY = sliderY - 25;
    
    renderText("Glosnosc:", textX, textY, white, false, fontSmall);
    
    std::string percentText = std::to_string(percentage) + "%";
    SDL_Color percentColor = percentage <= 100 ? cyan : (percentage <= 150 ? yellow : red);
    renderText(percentText, textX + 80, textY, percentColor, false, fontSmall);
    
    renderText("0%", textX - 25, sliderY + 5, gray, false, fontSmall);
    renderText("200%", textX + sliderWidth + 5, sliderY + 5, gray, false, fontSmall);
}

void UIManager::drawNoiseSlider(int mouseX, int mouseY, bool mouseDown) {
    if (!g_audioManager) return;
    
    int sliderSmallWidth = getSliderSmallWidth();
    int sliderY = getNoiseSliderY();
    int sliderX = (windowWidth - sliderSmallWidth) / 2;
    
    // tlo suwaka
    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    SDL_Rect trackRect = {sliderX, sliderY, sliderSmallWidth, SLIDER_HEIGHT};
    SDL_RenderFillRect(renderer, &trackRect);
    
    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
    SDL_RenderDrawRect(renderer, &trackRect);
    
    // normalizujemy dB do 0-1 dla pozycji suwaka
    float noiseDB = g_audioManager->getNoiseGateDB();
    float normalized = (noiseDB - MIN_NOISE_GATE_DB) / (MAX_NOISE_GATE_DB - MIN_NOISE_GATE_DB);
    int sliderPos = static_cast<int>(normalized * sliderSmallWidth);
    
    // wypelniona czesc - pomaranczowa
    SDL_SetRenderDrawColor(renderer, 255, 150, 50, 255);
    SDL_Rect filledRect = {sliderX, sliderY, sliderPos, SLIDER_HEIGHT};
    SDL_RenderFillRect(renderer, &filledRect);
    
    // uchwyt
    int handleX = sliderX + sliderPos - 5;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect handleRect = {handleX, sliderY - 3, 10, SLIDER_HEIGHT + 6};
    SDL_RenderFillRect(renderer, &handleRect);
    
    // obsluga przeciagania
    if (mouseDown) {
        if (mouseX >= sliderX && mouseX <= sliderX + sliderSmallWidth &&
            mouseY >= sliderY - 10 && mouseY <= sliderY + SLIDER_HEIGHT + 10) {
            float normalized = static_cast<float>(mouseX - sliderX) / sliderSmallWidth;
            normalized = std::clamp(normalized, 0.0f, 1.0f);
            float newDB = MIN_NOISE_GATE_DB + normalized * (MAX_NOISE_GATE_DB - MIN_NOISE_GATE_DB);
            g_audioManager->setNoiseGateDB(newDB);
        }
    }
}

void UIManager::drawNoiseText() {
    if (!g_audioManager) return;
    
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color orange = {255, 150, 50, 255};
    SDL_Color gray = {100, 100, 120, 255};
    
    int sliderSmallWidth = getSliderSmallWidth();
    int sliderY = getNoiseSliderY();
    float noiseDB = g_audioManager->getNoiseGateDB();
    int textX = (windowWidth - sliderSmallWidth) / 2;
    int textY = sliderY - 25;
    
    renderText("Bramka szumu:", textX, textY, white, false, fontSmall);
    
    // wyswietlamy dB z jednym miejscem po przecinku
    std::string dbText = std::to_string(static_cast<int>(noiseDB)) + " dB";
    renderText(dbText, textX + 115, textY, orange, false, fontSmall);
    
    renderText("-60", textX - 25, sliderY + 5, gray, false, fontSmall);
    renderText("0 dB", textX + sliderSmallWidth + 5, sliderY + 5, gray, false, fontSmall);
}

void UIManager::drawFreqSlider(int mouseX, int mouseY, bool mouseDown) {
    if (!g_audioManager) return;
    
    int sliderSmallWidth = getSliderSmallWidth();
    int sliderY = getFreqSliderY();
    int sliderX = (windowWidth - sliderSmallWidth) / 2;
    
    // tlo suwaka
    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    SDL_Rect trackRect = {sliderX, sliderY, sliderSmallWidth, SLIDER_HEIGHT};
    SDL_RenderFillRect(renderer, &trackRect);
    
    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
    SDL_RenderDrawRect(renderer, &trackRect);
    
    float maxFreq = g_audioManager->getMaxDisplayFreq();
    // normalizujemy do 0-1 na podstawie MIN_DISPLAY_FREQ do MAX_DISPLAY_FREQ
    float normalized = (maxFreq - MIN_DISPLAY_FREQ) / (MAX_DISPLAY_FREQ - MIN_DISPLAY_FREQ);
    int sliderPos = static_cast<int>(normalized * sliderSmallWidth);
    
    // wypelniona czesc - zielona
    SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
    SDL_Rect filledRect = {sliderX, sliderY, sliderPos, SLIDER_HEIGHT};
    SDL_RenderFillRect(renderer, &filledRect);
    
    // uchwyt
    int handleX = sliderX + sliderPos - 5;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect handleRect = {handleX, sliderY - 3, 10, SLIDER_HEIGHT + 6};
    SDL_RenderFillRect(renderer, &handleRect);
    
    // obsluga przeciagania
    if (mouseDown) {
        if (mouseX >= sliderX && mouseX <= sliderX + sliderSmallWidth &&
            mouseY >= sliderY - 10 && mouseY <= sliderY + SLIDER_HEIGHT + 10) {
            float normalized = static_cast<float>(mouseX - sliderX) / sliderSmallWidth;
            normalized = std::clamp(normalized, 0.0f, 1.0f);
            float newFreq = MIN_DISPLAY_FREQ + normalized * (MAX_DISPLAY_FREQ - MIN_DISPLAY_FREQ);
            g_audioManager->setMaxDisplayFreq(newFreq);
        }
    }
}

void UIManager::drawFreqText() {
    if (!g_audioManager) return;
    
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color green = {100, 200, 100, 255};
    SDL_Color gray = {100, 100, 120, 255};
    
    int sliderSmallWidth = getSliderSmallWidth();
    int sliderY = getFreqSliderY();
    int maxFreq = static_cast<int>(g_audioManager->getMaxDisplayFreq());
    int textX = (windowWidth - sliderSmallWidth) / 2;
    int textY = sliderY - 25;
    
    renderText("Max czestotliwosc:", textX, textY, white, false, fontSmall);
    
    std::string freqText;
    if (maxFreq >= 1000) {
        freqText = std::to_string(maxFreq / 1000) + "." + std::to_string((maxFreq % 1000) / 100) + " kHz";
    } else {
        freqText = std::to_string(maxFreq) + " Hz";
    }
    renderText(freqText, textX + 150, textY, green, false, fontSmall);
    
    renderText("100", textX - 25, sliderY + 5, gray, false, fontSmall);
    renderText("22k", textX + sliderSmallWidth + 5, sliderY + 5, gray, false, fontSmall);
}

void UIManager::drawInfoBar(int mouseX, int mouseY) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color cyan = {0, 200, 255, 255};
    SDL_Color magenta = {255, 100, 200, 255};
    SDL_Color green = {0, 255, 100, 255};
    SDL_Color gray = {150, 150, 170, 255};
    
    // tlo
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_Rect infoRect = {0, windowHeight - 60, windowWidth, 60};
    SDL_RenderFillRect(renderer, &infoRect);
    
    // status
    SDL_SetRenderDrawColor(renderer, 0, 255, 100, 255);
    SDL_Rect statusRect = {20, windowHeight - 45, 15, 15};
    SDL_RenderFillRect(renderer, &statusRect);
    renderText("NASLUCHUJE", 45, windowHeight - 47, green, false, fontSmall);
    
    // nazwa urzadzenia
    if (selectedDeviceIndex >= 0 && selectedDeviceIndex < static_cast<int>(deviceNames.size())) {
        std::string deviceInfo = "Urzadzenie: " + deviceNames[selectedDeviceIndex];
        if (deviceInfo.length() > 50) deviceInfo = deviceInfo.substr(0, 47) + "...";
        renderText(deviceInfo, 150, windowHeight - 47, gray, false, fontSmall);
    }
    
    // legenda kanalow
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
    SDL_Rect leftChannelRect = {windowWidth - 250, windowHeight - 50, 20, 12};
    SDL_RenderFillRect(renderer, &leftChannelRect);
    renderText("Lewy kanal", windowWidth - 225, windowHeight - 52, cyan, false, fontSmall);
    
    SDL_SetRenderDrawColor(renderer, 255, 100, 200, 255);
    SDL_Rect rightChannelRect = {windowWidth - 250, windowHeight - 30, 20, 12};
    SDL_RenderFillRect(renderer, &rightChannelRect);
    renderText("Prawy kanal", windowWidth - 225, windowHeight - 32, magenta, false, fontSmall);
    
    // przycisk wstecz
    int backX = 20;
    int backY = windowHeight - 25;
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
    
    renderText("< Wroc", backX + 15, backY + 2, backButtonHovered ? cyan : gray, false, fontSmall);
}
