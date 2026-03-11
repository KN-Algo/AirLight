# Audio Passthrough z wizualizacją fali

Apka w C++ do przechwytywania dźwięku z mikrofonu i odtwarzania go w czasie rzeczywistym. Jest wykres fali dźwiękowej, spektrogram FFT i suwaki do kontroli.

## Co to robi?

- **Lista urządzeń audio** - pokazuje wszystkie mikrofony/wejścia audio jakie masz
- **Passthrough w czasie rzeczywistym** - to co słyszy mikrofon leci od razu na głośniki
- **Wizualizacja fali** - na żywo rysuje falę dźwiękową (niebieski = lewy kanał, różowy = prawy)
- **Spektrogram FFT** - pokazuje rozkład częstotliwości w czasie rzeczywistym (kolorowe słupki)
- **Kontrola głośności** - suwak od 0% do 200% (tak, można pogłośnić ponad normę)
- **Próg szumu** - suwak do odcinania szumu (przydatne dla głośnych mikrofonów/fotorezystorów)
- **Zoom częstotliwości** - możesz ograniczyć widok spektrogramu do niższych częstotliwości (100Hz - 22kHz)

## Czego potrzebujesz

- Kompilator C++17 (Visual Studio 2019+ albo jakiś GCC)
- SDL2 i SDL2_ttf (do grafiki i tekstu)

## Jak to zbudować

### Windows (z vcpkg) - polecam

1. Instalujesz SDL2 i SDL2_ttf przez vcpkg:

   ```powershell
   vcpkg install sdl2:x64-windows sdl2-ttf:x64-windows
   ```

2. Budujesz CMake'em:

   ```powershell
   cd Audio_player
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=[sciezka do vcpkg]/scripts/buildsystems/vcpkg.cmake
   cmake --build . --config Release
   ```

### Windows (ręczna instalacja SDL2)

1. Ściągasz SDL2 i SDL2_ttf z ich GitHubów (releases)
2. Ustawiasz zmienne środowiskowe `SDL2_DIR` i `SDL2_TTF_DIR`
3. Budujesz:

   ```powershell
   cd Audio_player
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
   ```

### Linux

1. Instalujesz SDL2:

   ```bash
   # Ubuntu/Debian
   sudo apt install libsdl2-dev libsdl2-ttf-dev
   
   # Fedora
   sudo dnf install SDL2-devel SDL2_ttf-devel
   ```

2. Budujesz:

   ```bash
   cd Audio_player
   mkdir build && cd build
   cmake ..
   make
   ```

## Jak używać

1. Odpalasz exeka
2. Klikasz na urządzenie audio które chcesz użyć (np. twój mikrofon)
3. Suwakiem ustawiasz głośność (przeciągasz myszką)
4. Jak chcesz wrócić do wyboru urządzenia - klikasz "< Wroc" w lewym górnym rogu
5. ESC albo X żeby wyjść

## Sterowanie

- **Przeciąganie suwaka** - zmiana głośności
- **Kliknięcie przycisku urządzenia** - wybór mikrofonu
- **Przycisk Wróć** - powrót do wyboru urządzenia
- **ESC** - wyjście z apki

## Uwagi

- Używaj słuchawek jak masz głośniki blisko mikrofonu, bo będzie sprzężenie i piski
- Głośność powyżej 100% może powodować przesterowania (clipping) - uważaj na uszy!
- Biała kreska na suwaku to oznaczenie 100%
