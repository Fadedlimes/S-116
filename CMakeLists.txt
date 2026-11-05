cmake_minimum_required(VERSION 3.20)
project(Simple116 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# 1. Platform-Specific Build Configurations
# ==============================================================================

# macOS Universal Binary (Apple Silicon arm64 + Intel x86_64)
if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
endif()

# 2. Fetch JUCE 8 Framework
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
)
FetchContent_MakeAvailable(JUCE)

# Determine Plugin Formats per Operating System
set(PLUGIN_FORMATS VST3 Standalone)
if (APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

# ==============================================================================
# 3. Define Plugin Target
# ==============================================================================
juce_add_plugin(Simple116
    COMPANY_NAME "Fadedlimes"
    BUNDLE_ID "com.fadedlimes.simple116"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Fade
    PLUGIN_CODE S116
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Simple116"
    HARDENED_RUNTIME TRUE
)

# Generate JuceHeader.h
juce_generate_juce_header(Simple116)

# ==============================================================================
# 4. Source Files (Complete Engine, UI, DSP & Expansion Card System)
# ==============================================================================
set(S116_FONT_DIR ${CMAKE_SOURCE_DIR}/Source/Resources/Fonts)
if(EXISTS ${S116_FONT_DIR}/BarlowCondensed-Bold.ttf AND EXISTS ${S116_FONT_DIR}/ShareTechMono-Regular.ttf)
    juce_add_binary_data(S116Fonts SOURCES ${S116_FONT_DIR}/BarlowCondensed-Bold.ttf ${S116_FONT_DIR}/ShareTechMono-Regular.ttf)
    target_link_libraries(Simple116 PRIVATE S116Fonts)
    target_compile_definitions(Simple116 PRIVATE S116_HAS_FONTS=1)
endif()
option (S116_SNAPSHOTS "Generate debug snapshots" OFF)
if (S116_SNAPSHOTS)
    target_compile_definitions (Simple116 PRIVATE S116_SNAPSHOTS=1)
endif()



target_sources(Simple116 PRIVATE
    # Processor & Editor Core
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h

    # DSP Architecture & Synthesis Engine
    Source/DSP/PolyBLEPOscillator.h
    Source/DSP/SynthVoice.h
    Source/DSP/VoiceManager.h
    Source/DSP/SequencerEngine.h
    Source/DSP/ModulationMatrix.h
    Source/DSP/ExpansionCardManager.h
    Source/DSP/PresetBank.h
    Source/DSP/LFO.h
    Source/DSP/StereoChorus.h
    Source/DSP/StereoDelay.h
    Source/DSP/StereoReverb.h
    Source/DSP/JunoFilter.h
    Source/DSP/ADSREnvelope.h

    # Workstation UI, LCD Displays & Theming
    Source/UI/SkinManager.h
    Source/UI/OscilloscopeComponent.h
    Source/UI/SilverLookAndFeel.h
    Source/UI/VintageButtonKeyboard.h
)

# ==============================================================================
# 5. Include Directories
# ==============================================================================
target_compile_definitions (Simple116 PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

target_include_directories(Simple116 PRIVATE
    Source
    Source/DSP
    Source/UI
)

# ==============================================================================
# 6. Compiler Flags (Optimized across GCC, Clang, and MSVC)
# ==============================================================================
if (MSVC)
    target_compile_options(Simple116 PRIVATE
        /utf-8          # Enforce UTF-8 source encoding (prevents LCD glyph corruption)
        /MP             # Multi-processor compilation speed-up
        /W3             # Warning level 3
        /wd4244         # Disable precision loss warnings
        /wd4305
    )
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(Simple116 PRIVATE
        -Wno-calloc-transposed-args
        -Wno-error
        -Wall
        -Wextra
        -Wno-unused-parameter
    )
endif()

# ==============================================================================
# 7. Audio Driver Configurations (Windows ASIO / WASAPI)
# ==============================================================================
if (WIN32)
    if (DEFINED ASIO_SDK_DIR)
        target_compile_definitions(Simple116 PRIVATE JUCE_ASIO=1)
        target_include_directories(Simple116 PRIVATE ${ASIO_SDK_DIR})
        message(STATUS "ASIO SDK found: ${ASIO_SDK_DIR} - ASIO driver enabled.")
    else()
        target_compile_definitions(Simple116 PRIVATE JUCE_WASAPI=1)
        message(STATUS "ASIO SDK not specified - Standalone using Windows WASAPI driver.")
    endif()
endif()

# Global Public Definitions
target_compile_definitions(Simple116 PUBLIC
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
)

# ==============================================================================
# 8. Link Framework Modules & Optimized Runtime Flags
# ==============================================================================
target_link_libraries(Simple116 PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_devices
    juce::juce_audio_basics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_core
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
)
