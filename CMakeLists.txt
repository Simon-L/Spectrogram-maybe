# CMake file for DISTRHO Plugins #
# ------------------------------ #

cmake_minimum_required(VERSION 3.7)

set(CMAKE_CXX_STANDARD 20)

set(NAME Spectrogram)
project(${NAME})

add_subdirectory(/path/to/DPF bin)

dpf_add_plugin(Spectrogram
  TARGETS jack vst2 # dssi lv2 vst2 vst3 clap
  FILES_DSP
      Spectrogram.cpp
  FILES_UI
      SpectrogramUI.cpp
      NanoButton.cpp)

target_include_directories(
  Spectrogram PUBLIC ".")
