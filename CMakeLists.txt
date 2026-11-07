cmake_minimum_required(VERSION 3.22)

project(HeavenLimits VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HeavenLimits
    COMPANY_NAME "CM3CH"
    BUNDLE_ID "com.cm3ch.heavenlimits"
    PLUGIN_MANUFACTURER_CODE Cmch
    PLUGIN_CODE HvLm
    FORMATS VST3 Standalone
    PRODUCT_NAME "Heaven Limits"
    DESCRIPTION "Stereo brickwall limiter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(HeavenLimits)

target_sources(HeavenLimits PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(HeavenLimits PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(HeavenLimits PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
