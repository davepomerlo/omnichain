cmake_minimum_required(VERSION 3.22)

# macOS: one universal binary for Apple Silicon + Intel, runs on macOS 11+
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(OmniChain VERSION 1.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to JUCE 7 (git clone --branch 7.0.12 https://github.com/juce-framework/JUCE.git)
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE")
add_subdirectory(${JUCE_PATH} JUCE)

# Neural Amp Modeler core (MIT) and Eigen (MPL2)
#   git clone --depth 1 --branch v0.6.0 https://github.com/sdatkinson/NeuralAmpModelerCore.git ThirdParty/NeuralAmpModelerCore
#   git clone --depth 1 --branch 3.4.1  https://gitlab.com/libeigen/eigen.git ThirdParty/eigen
set(NAM_PATH   "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/NeuralAmpModelerCore" CACHE PATH "Path to NeuralAmpModelerCore")
set(EIGEN_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/eigen" CACHE PATH "Path to Eigen")
file(GLOB_RECURSE NAM_SOURCES CONFIGURE_DEPENDS "${NAM_PATH}/NAM/*.cpp")
# OBJECT (not STATIC): NAM registers architectures via static initialisers that a static lib would drop
add_library(nam_core OBJECT ${NAM_SOURCES})
target_include_directories(nam_core PUBLIC "${NAM_PATH}" "${EIGEN_PATH}" "${NAM_PATH}/Dependencies/nlohmann")
target_compile_definitions(nam_core PUBLIC NAM_SAMPLE_FLOAT)
set_target_properties(nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(nam_core PUBLIC /bigobj)
else()
    target_compile_options(nam_core PRIVATE -O3)
endif()

# Formats per platform: VST3 + Standalone everywhere, AU on macOS, LV2 on Linux; CLAP added below
set(OMNICHAIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND OMNICHAIN_FORMATS AU)
elseif(UNIX)
    list(APPEND OMNICHAIN_FORMATS LV2)
endif()

juce_add_plugin(OmniChain
    COMPANY_NAME "OmniChain"
    BUNDLE_ID "com.omnichain.multifx"
    PLUGIN_MANUFACTURER_CODE Omch
    PLUGIN_CODE Omc1
    FORMATS ${OMNICHAIN_FORMATS}
    PRODUCT_NAME "OmniChain"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    AU_SANDBOX_SAFE TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "OmniChain needs audio input to process your guitar or bass."
    LV2URI "https://github.com/davepomerlo/omnichain"
    VST3_AUTO_MANIFEST FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(OmniChain)

target_sources(OmniChain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/dsp/EffectInfo.cpp
    Source/dsp/Effects.cpp
    Source/dsp/NamEffect.cpp
    Source/dsp/Engine.cpp)

target_compile_definitions(OmniChain PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(OmniChain
    PRIVATE juce::juce_audio_utils nam_core
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# CLAP (Bitwig, Reaper, FL Studio, Studio One, ...) via clap-juce-extensions (MIT)
#   git clone --recursive https://github.com/free-audio/clap-juce-extensions.git ThirdParty/clap-juce-extensions
#   (pinned in CI to commit 7adee3a, which supports JUCE 7.0.12)
set(CLAP_JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/clap-juce-extensions" CACHE PATH "Path to clap-juce-extensions")
if(EXISTS "${CLAP_JUCE_PATH}/CMakeLists.txt")
    add_subdirectory(${CLAP_JUCE_PATH} clap-juce-extensions EXCLUDE_FROM_ALL)
    clap_juce_extensions_plugin(TARGET OmniChain
        CLAP_ID "com.omnichain.multifx"
        CLAP_FEATURES audio-effect multi-effects distortion delay reverb stereo)
else()
    message(STATUS "clap-juce-extensions not found - skipping the CLAP format")
endif()

# ASIO (Windows standalone app): Steinberg ASIO SDK under its GPLv3 licence option
#   https://download.steinberg.net/sdk_downloads/ASIO-SDK_2.3.4_2025-10-15.zip -> ThirdParty/asiosdk
set(ASIO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/asiosdk" CACHE PATH "Path to the ASIO SDK")
if(WIN32 AND EXISTS "${ASIO_SDK_PATH}/common/iasiodrv.h")
    target_compile_definitions(OmniChain PUBLIC JUCE_ASIO=1)
    target_include_directories(OmniChain PUBLIC "${ASIO_SDK_PATH}/common")
    message(STATUS "ASIO support enabled")
endif()

if(MINGW)
    # Self-contained binaries: no libc++ / pthread DLLs needed on the target machine
    target_link_options(OmniChain_VST3 PRIVATE -static)
    target_link_options(OmniChain_Standalone PRIVATE -static)
    if(TARGET OmniChain_CLAP)
        target_link_options(OmniChain_CLAP PRIVATE -static)
    endif()
endif()

# ---------------------------------------------------------------- optional headless test host
option(OMNICHAIN_TESTS "Build the headless integration test" OFF)
if(OMNICHAIN_TESTS)
    juce_add_console_app(OmniChainHostTest PRODUCT_NAME "OmniChainHostTest")
    juce_generate_juce_header(OmniChainHostTest)
    target_sources(OmniChainHostTest PRIVATE tests/HostTest.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets.cpp
        Source/dsp/EffectInfo.cpp Source/dsp/Effects.cpp Source/dsp/NamEffect.cpp Source/dsp/Engine.cpp)
    target_compile_definitions(OmniChainHostTest PRIVATE JucePlugin_Name="OmniChain"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(OmniChainHostTest PRIVATE juce::juce_audio_utils nam_core juce::juce_recommended_config_flags)
endif()
