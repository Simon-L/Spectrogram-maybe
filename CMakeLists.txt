# CMake file for DISTRHO Plugins #
# ------------------------------ #

cmake_minimum_required(VERSION 3.7)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(NAME Spectrogram)
project(${NAME})

add_subdirectory(/path/to/DPF bin)

dpf_add_plugin(Spectrogram
  TARGETS jack vst2 lv2
  MONOLITHIC
  FILES_DSP
      Spectrogram.cpp
  FILES_UI
      SpectrogramUI.cpp
      NanoButton.cpp)

target_include_directories(
  Spectrogram PUBLIC ".")

target_compile_options(
  Spectrogram PUBLIC "-march=x86-64" "-mavx2")

add_executable(test_fft tests/test_fft.cpp)
target_include_directories(test_fft PUBLIC ".")
target_compile_options(
  test_fft PUBLIC "-march=x86-64" "-mavx2")
