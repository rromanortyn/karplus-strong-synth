cmake_minimum_required(VERSION 3.22)
project(KarplusStrong VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(JUCE_PATH "" CACHE PATH "Path to the JUCE 8 source directory")
if(NOT TARGET juce::juce_audio_utils)
    if(EXISTS "${JUCE_PATH}/CMakeLists.txt")
        add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
    else()
        find_package(JUCE 8 CONFIG REQUIRED)
    endif()
endif()
set(plugin_formats VST3 Standalone)
if(APPLE)
    list(APPEND plugin_formats AU)
endif()
juce_add_plugin(KarplusStrong_PluckedString
    COMPANY_NAME "KarplusStrong"
    BUNDLE_ID "local.karplusstrong.pluckedstring"
    PLUGIN_MANUFACTURER_CODE Kpls PLUGIN_CODE Kstr
    PRODUCT_NAME "KarplusStrong_PluckedString"
    FORMATS ${plugin_formats}
    IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(KarplusStrong_PluckedString)
juce_add_binary_data(SynthResources SOURCES Resources/Inter.ttf)
target_sources(KarplusStrong_PluckedString PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(KarplusStrong_PluckedString PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_STRICT_REFCOUNTEDPOINTER=1)
target_link_libraries(KarplusStrong_PluckedString PRIVATE SynthResources juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)

include(CTest)
if(BUILD_TESTING)
    add_executable(SynthTests Tests/SynthTests.cpp)
    target_include_directories(SynthTests PRIVATE Source
        $<TARGET_PROPERTY:KarplusStrong_PluckedString,INCLUDE_DIRECTORIES>)
    target_compile_definitions(SynthTests PRIVATE
        $<TARGET_PROPERTY:KarplusStrong_PluckedString,COMPILE_DEFINITIONS>)
    target_link_libraries(SynthTests PRIVATE KarplusStrong_PluckedString SynthResources)
    add_test(NAME synth-engine COMMAND SynthTests)
endif()
