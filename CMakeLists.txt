cmake_minimum_required(VERSION 3.22)
project(MicaField VERSION 0.1.0 LANGUAGES C CXX)
option(MICA_DSP_ONLY "Build the headless engine checks without JUCE" OFF)
option(MICA_ENABLE_SANITIZERS "Enable address and undefined behavior checks" OFF)
set(MICA_JUCE_DIR "" CACHE PATH "Optional existing JUCE 8.0.12 source directory")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(mica_dsp STATIC Source/Patch.cpp Source/Dsp.cpp)
target_include_directories(mica_dsp PUBLIC Source)
if(MICA_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(mica_dsp PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(mica_dsp PUBLIC -fsanitize=address,undefined)
endif()
include(CTest)
if(BUILD_TESTING)
    add_executable(mica_dsp_checks Tests/DspChecks.cpp)
    target_link_libraries(mica_dsp_checks PRIVATE mica_dsp)
    add_test(NAME mica_dsp_checks COMMAND mica_dsp_checks)
endif()
if(MICA_DSP_ONLY)
    return()
endif()
if(MICA_JUCE_DIR)
    add_subdirectory("${MICA_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        URL https://codeload.github.com/juce-framework/JUCE/tar.gz/refs/tags/8.0.12
        URL_HASH SHA256=dec1a8baee5aaec4502b717a421b850e257ab7e9c2efd03766185ec1957cf102)
    FetchContent_MakeAvailable(JUCE)
endif()
juce_add_plugin(MicaField
    COMPANY_NAME "Mica Field"
    BUNDLE_ID "audio.micafield.instrument"
    PRODUCT_NAME "Mica Field"
    PLUGIN_MANUFACTURER_CODE McFd
    PLUGIN_CODE Mc01
    FORMATS Standalone VST3
    VST3_CATEGORIES Instrument Synth
    IS_SYNTH TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(MicaField PRIVATE Source/Instrument.cpp Source/Editor.cpp Source/EditorPages.cpp)
target_compile_definitions(MicaField PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_JACK=0 JUCE_ALSA=1
    JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(MicaField PRIVATE
    mica_dsp juce::juce_audio_utils juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags)

if(BUILD_TESTING)
    add_executable(mica_native_checks Tests/NativeChecks.cpp)
    target_link_libraries(mica_native_checks PRIVATE MicaField)
    target_include_directories(mica_native_checks PRIVATE $<TARGET_PROPERTY:MicaField,INCLUDE_DIRECTORIES>)
    target_compile_definitions(mica_native_checks PRIVATE $<TARGET_PROPERTY:MicaField,COMPILE_DEFINITIONS>)
    add_test(NAME mica_native_checks COMMAND mica_native_checks)
endif()
