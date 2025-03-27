#include <cstdio>

#define _USE_MATH_DEFINES
#include <cmath>

#include "fft.hpp"
#include "tests/sine_3khz.hpp"

int main(void)
{
    
    auto window_size = 1024;
    std::vector<float> window;
    window.resize(window_size);
    hann(window.data(), window_size, false);

    Columns cols;
    cols.fct = 2.0;
    cols.sampleRate = 48000;
    cols.init(&window, window_size);

    for (int j = 0; j < 48000; j++) {
        sine_3khz[j] *= 1; 
    }
    
    auto n_columns = cols.feed(sine_3khz, 48000);
    printf("processed: %d\n", n_columns);

    for (int i = 0; i < n_columns; i++) {
        printf("%d - %d -> %fHz @ %f\n", i, cols.columns[i].peakBin, cols.columns[i].peakFrequency, cols.columns[i].peakMagnitude);
    }
    
}