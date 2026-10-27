cmake_minimum_required(VERSION 3.22)
project(MoosikSynth VERSION 0.1.0)

add_subdirectory(JUCE)

juce_add_plugin(MoosikSynth
    COMPANY_NAME "Moosik"
    PLUGIN_MANUFACTURER_CODE Moos       # four chars, at least one upper-case
    PLUGIN_CODE Msy1                    # four chars, exactly one upper-case
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3 Standalone
    PRODUCT_NAME "Moosik Synth"
    COPY_PLUGIN_AFTER_BUILD FALSE       # copying into Program Files needs admin rights
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(MoosikSynth
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(MoosikSynth
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MoosikSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
