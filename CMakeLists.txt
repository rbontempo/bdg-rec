cmake_minimum_required(VERSION 3.22)
project(BDG_REC VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(BDG_REC
    PRODUCT_NAME "BDG REC"
    COMPANY_NAME "BDG"
    BUNDLE_ID "com.bdg.rec"
)

target_sources(BDG_REC PRIVATE
    src/Main.cpp
    src/MainComponent.cpp
    src/BdgLookAndFeel.cpp
    src/HeaderBar.cpp
    src/InputPanel.cpp
    src/RecordingPanel.cpp
    src/OutputPanel.cpp
    src/AudioEngine.cpp
    src/Dsp.cpp
    src/VuMeter.cpp
    src/WaveformDisplay.cpp
    src/RecordButton.cpp
)

target_compile_definitions(BDG_REC PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:BDG_REC,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:BDG_REC,JUCE_VERSION>"
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(BDG_REC PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
