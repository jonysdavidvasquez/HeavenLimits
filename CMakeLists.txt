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