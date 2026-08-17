# Tests are opt-in so a normal build is unaffected:
#   cmake -B build -DBDG_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
#
# Two groups, because they have very different requirements:
#   unit    — no audio device, no display. These are the ones CI can run.
#   device  — capture from a real input for several seconds. Developer machines
#             only; a CI runner has no microphone.
#
# Deliberately NOT adding ${SRC_DIR} to any include path: on a case-insensitive
# filesystem that makes the project's Strings.h shadow the system <strings.h>.
# The sources use relative includes instead.

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# ---------------------------------------------------------------- unit tests
juce_add_console_app(BDG_UnitTests PRODUCT_NAME "BDG_UnitTests")

target_sources(BDG_UnitTests PRIVATE
    UnitTests.cpp
    ${SRC_DIR}/UpdateChecker.cpp
)

target_compile_definitions(BDG_UnitTests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="BDG_UnitTests"
    JUCE_APPLICATION_VERSION_STRING="1.0.0"
)

target_link_libraries(BDG_UnitTests PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
)

add_test(NAME unit.versions_and_strings COMMAND BDG_UnitTests)
set_tests_properties(unit.versions_and_strings PROPERTIES LABELS "unit")

# ------------------------------------------------------------- device tests
# Each of these records from the default input, so they need a machine with a
# microphone and permission granted. RotationTest and the others shorten the
# chunk length via BDG_CHUNK_SECONDS so a 5-minute boundary can be exercised
# in seconds.
set(BDG_DEVICE_TESTS
    RotationTest        # chunk rotation loses no samples at the boundaries
    SampleRateTest      # a mid-take rate change stops and saves cleanly
    EmptyTakeTest       # an empty take is not reported as a save failure
    RecoveryTest        # a crashed chunk with a zeroed header is recovered
    DiskFullTest        # a full disk preserves the chunks instead of deleting
)

foreach(test_name IN LISTS BDG_DEVICE_TESTS)
    juce_add_gui_app(BDG_${test_name}
        PRODUCT_NAME "BDG_${test_name}"
        BUNDLE_ID "com.bdg.test.${test_name}"
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "BDG rec test suite records a few seconds of audio."
    )

    target_sources(BDG_${test_name} PRIVATE
        ${test_name}.cpp
        ${SRC_DIR}/AudioEngine.cpp
        ${SRC_DIR}/Dsp.cpp
    )

    target_include_directories(BDG_${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../external/rnnoise/include
    )

    target_compile_definitions(BDG_${test_name} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="BDG_${test_name}"
        JUCE_APPLICATION_VERSION_STRING="1.0.0"
        BDG_CHUNK_SECONDS=2.0
    )

    target_link_libraries(BDG_${test_name} PRIVATE
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
    )
endforeach()

# DiskFullTest needs a small volume mounted at /Volumes/BDGTEST and nearly
# filled; see tests/README.md. Left out of the default device run.
foreach(test_name RotationTest SampleRateTest EmptyTakeTest RecoveryTest)
    add_test(NAME device.${test_name} COMMAND BDG_${test_name})
    set_tests_properties(device.${test_name} PROPERTIES
        LABELS "device"
        TIMEOUT 120
    )
endforeach()
