cmake_minimum_required(VERSION 3.22)
project(TimbreAdapterDesktop VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.3
)

FetchContent_MakeAvailable(juce)

juce_add_gui_app(TimbreAdapterDesktop
    PRODUCT_NAME "Timbre Adapter Desktop"
)

juce_generate_juce_header(TimbreAdapterDesktop)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    Source/*.cpp
    Source/*.h
)

target_sources(TimbreAdapterDesktop PRIVATE ${APP_SOURCES})

target_compile_definitions(TimbreAdapterDesktop PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(TimbreAdapterDesktop PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
)
