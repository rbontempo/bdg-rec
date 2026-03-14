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

# ---- RNNoise (vendored) ----
add_library(rnnoise STATIC
    external/rnnoise/src/denoise.c
    external/rnnoise/src/rnn.c
    external/rnnoise/src/rnnoise_data.c
    external/rnnoise/src/rnnoise_tables.c
    external/rnnoise/src/pitch.c
    external/rnnoise/src/kiss_fft.c
    external/rnnoise/src/celt_lpc.c
    external/rnnoise/src/nnet.c
    external/rnnoise/src/nnet_default.c
    external/rnnoise/src/parse_lpcnet_weights.c
)
target_include_directories(rnnoise PUBLIC
    external/rnnoise/include
    external/rnnoise/src
)
target_compile_definitions(rnnoise PRIVATE COMPILE_OPUS)
# Suppress warnings from third-party C code
target_compile_options(rnnoise PRIVATE -w)

juce_add_gui_app(BDG_REC
    PRODUCT_NAME "BDG REC"
    COMPANY_NAME "BDG"
    BUNDLE_ID "com.bdg.rec"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "BDG REC precisa acessar o microfone para gravar audio."
)

juce_add_binary_data(BDG_REC_BinaryData SOURCES
    resources/logo-bdg-rec.png
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
    src/DspOverlay.cpp
)

target_compile_definitions(BDG_REC PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:BDG_REC,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:BDG_REC,JUCE_VERSION>"
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_compile_options(BDG_REC PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
target_include_directories(BDG_REC PRIVATE external/rnnoise/include)

target_link_libraries(BDG_REC PRIVATE
    BDG_REC_BinaryData
    rnnoise
    juce::juce_core
    juce::juce_data_structures
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

# Copy app icon into bundle after build
add_custom_command(TARGET BDG_REC POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns"
        "$<TARGET_BUNDLE_DIR:BDG_REC>/Contents/Resources/Icon.icns"
    COMMENT "Copying app icon"
)
