// main.cpp - glowny plik programu
// teraz jest duzo krocej bo wywalilismy wszystko do osobnych modulow

#include "src/config.h"
#include "src/audio.h"
#include "src/ui.h"

// glowna funkcja - tu sie wszystko zaczyna
int main(int argc, char* argv[]) {
    // tworzymy managery
    UIManager ui;
    AudioManager audio;
    
    // inicjalizacja UI (SDL, okno, czcionki)
    if (!ui.init()) {
        return 1;
    }
    
    // pobieramy liste urzadzen audio
    audio.listInputDevices();
    ui.setDeviceNames(audio.getInputDeviceNames());
    
    // stan aplikacji
    AppState appState = AppState::DEVICE_SELECTION;
    int selectedDeviceIndex = -1;
    
    // glowna petla
    bool running = true;
    SDL_Event event;
    int mouseX = 0, mouseY = 0;
    bool mouseDown = false;
    bool mouseClicked = false;
    
    while (running) {
        mouseClicked = false;
        
        // obsluga eventow
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                    
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE) {
                        if (appState == AppState::LISTENING) {
                            audio.stopAudio();
                            appState = AppState::DEVICE_SELECTION;
                            selectedDeviceIndex = -1;
                            ui.setSelectedDevice(-1);
                            SDL_SetWindowTitle(ui.getWindow(), "Przelot Audio - Wybierz urzadzenie");
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
                    
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                        event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        ui.handleResize(event.window.data1, event.window.data2);
                    }
                    break;
            }
        }
        
        // rysujemy UI
        ui.render(appState, mouseX, mouseY, mouseDown);
        
        // obsluga klikniec
        if (appState == AppState::DEVICE_SELECTION) {
            if (mouseClicked && ui.getHoveredDevice() >= 0) {
                if (audio.startAudio(ui.getHoveredDevice())) {
                    selectedDeviceIndex = ui.getHoveredDevice();
                    ui.setSelectedDevice(selectedDeviceIndex);
                    appState = AppState::LISTENING;
                    SDL_SetWindowTitle(ui.getWindow(), "Przelot Audio - Nasluchuje");
                }
            }
        } else {
            if (mouseClicked && ui.isBackButtonHovered()) {
                audio.stopAudio();
                appState = AppState::DEVICE_SELECTION;
                selectedDeviceIndex = -1;
                ui.setSelectedDevice(-1);
                SDL_SetWindowTitle(ui.getWindow(), "Przelot Audio - Wybierz urzadzenie");
            }
        }
    }
    
    // sprzatamy
    audio.stopAudio();
    ui.shutdown();
    
    return 0;
}
