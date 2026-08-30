cmake_minimum_required(VERSION 3.22)

project(AmpSimCapstone VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(AmpSim
    PRODUCT_NAME "Amp Sim Capstone"
    COMPANY_NAME "Jacob Russell"
    BUNDLE_ID "com.jacobrussell.ampsimcapstone"
)

target_sources(AmpSim PRIVATE
    src/Main.cpp
    src/MainComponent.cpp
    src/MainComponent.h
    src/dsp/AmpProcessor.cpp
    src/dsp/AmpProcessor.h
)

target_compile_definitions(AmpSim PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:AmpSim,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(AmpSim PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

option(AMP_SIM_BUILD_TESTS "Build DSP tests" ON)

if (AMP_SIM_BUILD_TESTS)
    enable_testing()
    add_executable(AmpSimTests
        tests/AmpProcessorTests.cpp
        src/dsp/AmpProcessor.cpp
        src/dsp/AmpProcessor.h
    )
    target_include_directories(AmpSimTests PRIVATE src)
    target_link_libraries(AmpSimTests PRIVATE
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    add_test(NAME AmpSimTests COMMAND AmpSimTests)
endif()

