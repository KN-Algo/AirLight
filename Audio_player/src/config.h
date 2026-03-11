#pragma once

// config.h - wszystkie stale i ustawienia apki w jednym miejscu
// dzieki temu jak chcesz cos zmienic to nie musisz szukac po calym kodzie

// ustawienia audio
constexpr int SAMPLE_RATE = 44100;      // czestotliwosc probkowania w Hz
constexpr int SAMPLES = 1024;            // ile sampli na raz przetwarzamy (to tez wielkosc FFT)
constexpr int CHANNELS = 2;              // stereo bo czemu nie

// domyslne wymiary okna (mozna zmienic rozmiarem)
constexpr int DEFAULT_WINDOW_WIDTH = 1024;
constexpr int DEFAULT_WINDOW_HEIGHT = 900;
constexpr int MIN_WINDOW_WIDTH = 800;
constexpr int MIN_WINDOW_HEIGHT = 700;

// proporcje layoutu (w procentach wysokosci okna)
constexpr float WAVEFORM_HEIGHT_RATIO = 0.20f;      // 20% na fale
constexpr float SPECTROGRAM_HEIGHT_RATIO = 0.35f;   // 35% na spektrogram (wiekszy!)
constexpr float CONTROLS_HEIGHT_RATIO = 0.35f;      // 35% na suwaki
constexpr float INFO_BAR_HEIGHT = 60.0f;            // stala wysokosc paska info

// suwaki
constexpr int SLIDER_HEIGHT = 25;
constexpr float SLIDER_WIDTH_RATIO = 0.4f;          // 40% szerokosci okna
constexpr float SLIDER_SMALL_WIDTH_RATIO = 0.3f;    // 30% szerokosci okna

// UI elementow
constexpr int DEVICE_BUTTON_HEIGHT = 50;
constexpr int DEVICE_BUTTON_MARGIN = 10;

// spektrogram
constexpr int FFT_SIZE = 1024;           // wielkosc FFT (musi byc potega 2)
constexpr int SPECTROGRAM_BINS = 512;    // ile slupkow czestotliwosci pokazujemy

// domyslne ustawienia
constexpr float DEFAULT_NOISE_GATE_DB = -60.0f;   // prog szumu w dB (wylaczony)
constexpr float MIN_NOISE_GATE_DB = -60.0f;       // minimum -60 dB (prawie cisza)
constexpr float MAX_NOISE_GATE_DB = 0.0f;         // maximum 0 dB (pelna glosnosc)
constexpr float DEFAULT_MAX_FREQ = 22050.0f;      // max czestotliwosc do wyswietlenia
constexpr float MIN_DISPLAY_FREQ = 100.0f;        // minimalna wartosc max freq
constexpr float MAX_DISPLAY_FREQ = 22050.0f;      // maksymalna wartosc max freq
