cmake_minimum_required(VERSION 3.22)
project(TimbreAdapterDesktopV2 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.3
)

FetchContent_MakeAvailable(juce)

juce_add_gui_app(TimbreAdapterDesktopV2
    PRODUCT_NAME "Timbre Adapter Desktop V2"
)

juce_generate_juce_header(TimbreAdapterDesktopV2)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    Source/*.cpp
    Source/*.h
)

target_sources(TimbreAdapterDesktopV2 PRIVATE ${APP_SOURCES})

target_compile_definitions(TimbreAdapterDesktopV2 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

if (MSVC)
    target_compile_options(TimbreAdapterDesktopV2 PRIVATE /utf-8)
endif()

target_link_libraries(TimbreAdapterDesktopV2 PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)
