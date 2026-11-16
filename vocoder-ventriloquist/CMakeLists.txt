cmake_minimum_required(VERSION 3.22)

# Mac : binaire universel (Apple Silicon + Intel), compatible macOS 10.15 et plus.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Version minimale de macOS")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures macOS")

project(VocoderVentriloquist VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# JUCE est téléchargé automatiquement (ou pris dans VQ_JUCE_PATH s'il est déjà sur le disque).
set(VQ_JUCE_PATH "" CACHE PATH "Dossier JUCE local (optionnel)")
set(VQ_JUCE_TAG "8.0.15" CACHE STRING "Version de JUCE à télécharger")
if(VQ_JUCE_PATH)
    add_subdirectory("${VQ_JUCE_PATH}" juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${VQ_JUCE_TAG}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Formats : VST3 partout, plus Audio Unit sur Mac (pour Logic Pro).
set(VQ_FORMATS VST3)
if(APPLE)
    list(APPEND VQ_FORMATS AU)
endif()

# Copie le plugin dans le dossier système des plugins après la compilation.
if(APPLE)
    option(VQ_INSTALL_AFTER_BUILD "Installer le plugin après la compilation" ON)
else()
    option(VQ_INSTALL_AFTER_BUILD "Installer le plugin après la compilation" OFF)
endif()

juce_add_plugin(VocoderVentriloquist
    PRODUCT_NAME "vocoder ventriloquist"
    COMPANY_NAME "the insider"
    BUNDLE_ID "com.theinsider.vocoderventriloquist"
    PLUGIN_MANUFACTURER_CODE Insd
    PLUGIN_CODE Vntq
    FORMATS ${VQ_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD ${VQ_INSTALL_AFTER_BUILD})

target_sources(VocoderVentriloquist PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/gui/TheatreView.cpp)

target_include_directories(VocoderVentriloquist PRIVATE Source)

target_compile_definitions(VocoderVentriloquist PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(MSVC)
    # Sources lues en UTF-8.
    target_compile_options(VocoderVentriloquist PRIVATE /utf-8)
endif()

target_link_libraries(VocoderVentriloquist
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Polices de l'interface : tous les .ttf / .otf de Assets/Fonts sont embarqués dans le plugin.
file(GLOB VQ_FONTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Fonts/*.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Fonts/*.otf")
if(VQ_FONTS)
    juce_add_binary_data(VQAssets SOURCES ${VQ_FONTS})
    target_link_libraries(VocoderVentriloquist PRIVATE VQAssets)
    target_compile_definitions(VocoderVentriloquist PRIVATE VQ_HAS_FONTS=1)
endif()

# Test du moteur audio sans DAW (optionnel, sans JUCE).
option(VQ_BUILD_TESTS "Compiler le test du moteur" OFF)
if(VQ_BUILD_TESTS)
    add_executable(vq_render_test Tests/render_test.cpp)
    target_include_directories(vq_render_test PRIVATE Source)
endif()
