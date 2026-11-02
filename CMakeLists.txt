cmake_minimum_required(VERSION 3.22)

# Binaire universel (Apple Silicon + Intel) ; ces réglages doivent précéder project()
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures macOS" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Version minimale de macOS")

project(Strain VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6   # si ce tag n'existe pas, prends le dernier tag 8.0.x sur GitHub
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Strain
    COMPANY_NAME                "Perso"
    BUNDLE_ID                   com.perso.meghirstress
    PLUGIN_MANUFACTURER_CODE    Pers
    PLUGIN_CODE                 Mstr
    FORMATS                     AU VST3 Standalone
    PRODUCT_NAME                "Meghir Stress"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Dynamics
)

target_sources(Strain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(Strain PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Strain
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
