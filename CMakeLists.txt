# CMake file for DISTRHO Plugins #
# ------------------------------ #

cmake_minimum_required(VERSION 3.7)

set(CMAKE_CXX_STANDARD 20)

set(NAME Meters)
project(${NAME})

add_subdirectory(/home/xox/Programmation/DPF bin)

dpf_add_plugin(Meters
  TARGETS jack vst2 # dssi lv2 vst2 vst3 clap
  FILES_DSP
      ExamplePluginMeters.cpp
  FILES_UI
      ExampleUIMeters.cpp
      NanoButton.cpp)

target_include_directories(
  Meters PUBLIC ".")
