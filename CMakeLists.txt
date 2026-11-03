cmake_minimum_required(VERSION 3.22)
project(FreeAnticip8 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC: interpret source as UTF-8 so French accents / avoid mojibake in UI strings
if(MSVC)
  add_compile_options(/utf-8)
endif()
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
# JUCE can also be supplied locally: -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.6
  GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
  set(ANTICIP_FORMATS VST3 AU Standalone)
else()
  set(ANTICIP_FORMATS VST3 Standalone)
endif()

juce_add_plugin(FreeAnticip8
  COMPANY_NAME "FREE AUDIO"
  PLUGIN_MANUFACTURER_CODE FrAu
  PLUGIN_CODE Fa8x
  PRODUCT_NAME "FREE ANTICIP8"
  FORMATS ${ANTICIP_FORMATS}
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Reverb
  AU_MAIN_TYPE kAudioUnitType_Effect
  BUNDLE_ID com.freeaudio.freeanticip8)

target_sources(FreeAnticip8 PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(FreeAnticip8 PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_USE_MP3AUDIOFORMAT=1
  JUCE_DISPLAY_SPLASH_SCREEN=0
  ANTICIP8_VERSION="${PROJECT_VERSION}")

target_include_directories(FreeAnticip8 PRIVATE Source)

target_link_libraries(FreeAnticip8
  PRIVATE juce::juce_audio_utils
  PUBLIC  juce::juce_recommended_config_flags
          juce::juce_recommended_lto_flags
          juce::juce_recommended_warning_flags)
