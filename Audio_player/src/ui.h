#pragma once

// ui.h - wszystko co zwiazane z rysowaniem na ekranie
// czcionki, przyciski, wykresy, suwaki - wszystko tu

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include "config.h"

// stan aplikacji
enum class AppState {
    DEVICE_SELECTION,
    LISTENING
};

// klasa do zarzadzania UI
class UIManager {
public:
    UIManager();
    ~UIManager();
    
    // inicjalizacja SDL, okna, renderera, czcionek
    bool init();
    void shutdown();
    
    // obsluga zmiany rozmiaru okna
    void handleResize(int newWidth, int newHeight);
    
    // glowna petla rysowania
    void render(AppState state, int mouseX, int mouseY, bool mouseDown);
    
    // gettery
    SDL_Window* getWindow() { return window; }
    SDL_Renderer* getRenderer() { return renderer; }
    int getHoveredDevice() const { return hoveredDevice; }
    bool isBackButtonHovered() const { return backButtonHovered; }
    int getWindowWidth() const { return windowWidth; }
    int getWindowHeight() const { return windowHeight; }
    
    // settery
    void setSelectedDevice(int idx) { selectedDeviceIndex = idx; }
    void setDeviceNames(const std::vector<std::string>& names) { deviceNames = names; }
    
private:
    // pomocnicza funkcja do rysowania tekstu
    void renderText(const std::string& text, int x, int y, SDL_Color color, bool centered = false, TTF_Font* font = nullptr);
    
    // obliczanie layoutu na podstawie rozmiaru okna
    int getWaveformHeight() const { return static_cast<int>(windowHeight * WAVEFORM_HEIGHT_RATIO); }
    int getSpectrogramY() const { return getWaveformHeight() + 10; }
    int getSpectrogramHeight() const { return static_cast<int>(windowHeight * SPECTROGRAM_HEIGHT_RATIO); }
    int getControlsY() const { return getSpectrogramY() + getSpectrogramHeight() + 20; }
    int getSliderWidth() const { return static_cast<int>(windowWidth * SLIDER_WIDTH_RATIO); }
    int getSliderSmallWidth() const { return static_cast<int>(windowWidth * SLIDER_SMALL_WIDTH_RATIO); }
    int getVolumeSliderY() const { return getControlsY(); }
    int getNoiseSliderY() const { return getControlsY() + 70; }
    int getFreqSliderY() const { return getControlsY() + 140; }
    
    // rozne ekrany/elementy
    void drawDeviceSelection(int mouseX, int mouseY);
    void drawWaveform();
    void drawSpectrogram();
    void drawVolumeSlider(int mouseX, int mouseY, bool mouseDown);
    void drawNoiseSlider(int mouseX, int mouseY, bool mouseDown);
    void drawFreqSlider(int mouseX, int mouseY, bool mouseDown);
    void drawVolumeText();
    void drawNoiseText();
    void drawFreqText();
    void drawInfoBar(int mouseX, int mouseY);
    
    // SDL stuff
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* fontLarge = nullptr;
    TTF_Font* fontMedium = nullptr;
    TTF_Font* fontSmall = nullptr;
    
    // wymiary okna
    int windowWidth = DEFAULT_WINDOW_WIDTH;
    int windowHeight = DEFAULT_WINDOW_HEIGHT;
    
    // stan UI
    int hoveredDevice = -1;
    int selectedDeviceIndex = -1;
    bool backButtonHovered = false;
    std::vector<std::string> deviceNames;
};

// globalna instancja UI
extern UIManager* g_uiManager;
