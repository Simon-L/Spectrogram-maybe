#include "simde/x86/avx2.h"
#include <cstddef>

void simd_buffer_volume(float* buffer, size_t size, float volume)
{
    simde__m256 a = simde_mm256_set1_ps(volume);
    int i;
    for (i = 0; i < size - size % 8; i += 8)
    {
        simde__m256 vec = simde_mm256_loadu_ps(&buffer[i]);
        vec = simde_mm256_mul_ps(a, vec);
        simde_mm256_storeu_ps(&buffer[i], vec);
    }
    // non-vectorisable remaining elements
    for (int k = i; k < size; k++)
    {
        buffer[k] *= volume;
    }
}

void simd_buffer_stereo_volume(float** buffer, size_t size, float volume)
{
    simd_buffer_volume(buffer[0], size, volume);
    simd_buffer_volume(buffer[1], size, volume);
}

void simd_buffer_dbgain(float* buffer, size_t size, float gain)
{
    float volume = std::min(std::max(gain, -90.0f), 30.0f);
    volume = volume > -90.0f ? powf(10.0f, volume * 0.05f) : 0.0f;
    simd_buffer_volume(buffer, size, volume);
}

void simd_buffer_stereo_dbgain(float** buffer, size_t size, float gain)
{
    float volume = std::min(std::max(gain, -90.0f), 30.0f);
    volume = volume > -90.0f ? powf(10.0f, volume * 0.05f) : 0.0f;
    simd_buffer_volume(buffer[0], size, volume);
    simd_buffer_volume(buffer[1], size, volume);
}

void simd_buffer_add(float* buffer_a, float* buffer_b, size_t size)
{
    int i;
    for (i = 0; i < size - size % 8; i += 8)
    {
        simde__m256 vec_a = simde_mm256_loadu_ps(&buffer_a[i]);
        simde__m256 vec_b = simde_mm256_loadu_ps(&buffer_b[i]);
        vec_a = simde_mm256_add_ps(vec_a, vec_b);
        simde_mm256_storeu_ps(&buffer_a[i], vec_a);
    }
    // non-vectorisable remaining elements
    for (int k = i; k < size; k++)
    {
        buffer_a[k] += buffer_b[k];
    }
}

void simd_buffer_sub(float* buffer_a, float* buffer_b, size_t size)
{
    int i;
    for (i = 0; i < size - size % 8; i += 8)
    {
        simde__m256 vec_a = simde_mm256_loadu_ps(&buffer_a[i]);
        simde__m256 vec_b = simde_mm256_loadu_ps(&buffer_b[i]);
        vec_a = simde_mm256_sub_ps(vec_a, vec_b);
        simde_mm256_storeu_ps(&buffer_a[i], vec_a);
    }
    // non-vectorisable remaining elements
    for (int k = i; k < size; k++)
    {
        buffer_a[k] -= buffer_b[k];
    }
}
