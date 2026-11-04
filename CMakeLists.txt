cmake_minimum_required(VERSION 3.22)
project(OmniChain VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to JUCE 7 (git clone --branch 7.0.12 https://github.com/juce-framework/JUCE.git)
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE")
add_subdirectory(${JUCE_PATH} JUCE)

juce_add_plugin(OmniChain
    COMPANY_NAME "OmniChain"
    BUNDLE_ID "com.omnichain.multifx"
    PLUGIN_MANUFACTURER_CODE Omch
    PLUGIN_CODE Omc1
    FORMATS VST3 Standalone
    PRODUCT_NAME "OmniChain"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx
    VST3_AUTO_MANIFEST FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(OmniChain)

target_sources(OmniChain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/dsp/EffectInfo.cpp
    Source/dsp/Effects.cpp
    Source/dsp/Engine.cpp)

target_compile_definitions(OmniChain PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(OmniChain
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(MINGW)
    # Self-contained binaries: no libc++ / pthread DLLs needed on the target machine
    target_link_options(OmniChain_VST3 PRIVATE -static)
    target_link_options(OmniChain_Standalone PRIVATE -static)
endif()

# ---------------------------------------------------------------- optional headless test host
option(OMNICHAIN_TESTS "Build the headless integration test" OFF)
if(OMNICHAIN_TESTS)
    juce_add_console_app(OmniChainHostTest PRODUCT_NAME "OmniChainHostTest")
    juce_generate_juce_header(OmniChainHostTest)
    target_sources(OmniChainHostTest PRIVATE tests/HostTest.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets.cpp
        Source/dsp/EffectInfo.cpp Source/dsp/Effects.cpp Source/dsp/Engine.cpp)
    target_compile_definitions(OmniChainHostTest PRIVATE JucePlugin_Name="OmniChain"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(OmniChainHostTest PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
