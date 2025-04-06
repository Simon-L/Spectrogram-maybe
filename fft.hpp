#include <cmath>

#include "pocketfft.h"
#include "simde/x86/avx2.h"

// https://github.com/sidneycadot/WindowFunctions/blob/master/c99/window_functions.c
void cosine_window(float * w, unsigned n, const float * coeff, unsigned ncoeff, bool sflag)
{
    if (n == 1)
    {
        // Special case for n == 1.
        w[0] = 1.0;
    }
    else
    {
        const unsigned wlength = sflag ? (n - 1) : n;

        for (unsigned i = 0; i < n; ++i)
        {
            float wi = 0.0;

            for (unsigned j = 0; j < ncoeff; ++j)
            {
                wi += coeff[j] * cos(i * j * 2.0 * M_PI / wlength);
            }

            w[i] = wi;
        }
    }
}

void hann(float * w, unsigned n, bool sflag)
{
    const float coeff[2] = { 0.5, -0.5 };
    cosine_window(w, n, coeff, sizeof(coeff) / sizeof(float), sflag);
}

struct Columns {
    std::vector<float> buffer;
    uint32_t window_size;
    std::vector<float> *window;
    float sampleRate;

    struct Column {
        std::vector<float> bins;
        std::vector<float> bins_phase;
        size_t size;
        bool processed = false;
        float peakFrequency;
        float peakMagnitude;
        int peakBin;

        Column(size_t size) {
            bins.resize(size);
            bins_phase.resize(size);
            this->size = size;
        }
    };
    std::vector<Column> columns;

    float fct = 2.0f;

    void init(std::vector<float> *_window, int _window_size)
    {
        window = _window;
        window_size = _window_size;
        buffer.clear();
    }

    int feed(float* data, size_t length) {
        int fed = 0;
        if (buffer.size() + length < window_size) {
            buffer.insert(buffer.end(), data, data + length);
        } else {
            int available = length;
            int idx = 0;
            while ((buffer.size() + available) >= window_size)
            {
                auto eat = window_size - buffer.size();
                buffer.insert(buffer.end(), data + idx, data + idx + eat);
                idx += eat;
                available -= eat;
                if (buffer.size() == window_size) {
                    int i;
                    for (i = 0; i < window_size - window_size % 8; i += 8)
                    {
                        simde__m256 buf = simde_mm256_loadu_ps(&buffer[i]);
                        simde__m256 win = simde_mm256_loadu_ps(&window->data()[i]);
                        buf = simde_mm256_mul_ps(buf, win);
                        simde_mm256_storeu_ps(&buffer[i], buf);
                    }
                    // non-vectorisable remaining elements
                    for (int k = i; k < window_size; k++)
                    {
                        buffer[k] *= window->data()[k];
                    }
                
                    processFFT();
                    buffer.clear();
                    fed++;
                }
            }
        }
        return fed;
    }

    std::vector<std::complex<float>> fftOutput;
    pocketfft::shape_t shape{window_size};
    pocketfft::stride_t stride_in{sizeof(float)}; 
    pocketfft::stride_t stride_out{sizeof(std::complex<float>)}; 

    void processFFT() {
        shape[0] = buffer.size();
        fftOutput.clear();
        fftOutput.resize(window_size / 2 + 1);
        pocketfft::r2c(
            shape,
            stride_in,
            stride_out,
            0,
            pocketfft::FORWARD,
            buffer.data(),
            reinterpret_cast<std::complex<float>*>(fftOutput.data()),
            fct
        );

        Column col(fftOutput.size());
        int peakIndex = 0;
        float peakMag = 0;
        for (size_t i = 0; i < fftOutput.size(); ++i) {
            float magnitude = std::abs(fftOutput[i]) / (window_size / 2);
            if (magnitude > peakMag) { peakMag = magnitude; peakIndex = i; }
            col.bins[i] = magnitude;
            col.bins_phase[i] = std::arg(fftOutput[i]);
        }
        col.peakFrequency = peakIndex * (sampleRate / (fftOutput.size() - 1) / 2);
        col.peakBin = peakIndex;
        col.peakMagnitude = peakMag;

        // columns_memory_size accounts for some excedent, half of the columns_memory_size oldest columns get removed
        if (columns.size() > columns_memory_size ) {
            columns.erase(columns.begin(), columns.begin() + (columns_memory_size / 2));
        }
        
        columns.push_back(col);
    }
    int columns_memory_size = 8192;
};