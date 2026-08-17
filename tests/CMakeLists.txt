# Unit tests for the pure layers: patch codec, SysEx framing, upload
# packetization and module placement. Everything here runs without a GUI and
# without a synth on the wire; the doctest single header is vendored in
# libs/doctest so the tests build offline.
juce_add_console_app(nme_tests PRODUCT_NAME "nme_tests")

target_sources(nme_tests PRIVATE
    test_main.cpp
    test_sysex_codec.cpp
    test_upload_packetizer.cpp
    test_patch_roundtrip.cpp
    test_module_placement.cpp
    test_light_meter_layout.cpp
    test_module_ref.cpp
    test_theme_data.cpp
    test_cable_nets.cpp
    test_patch_title.cpp
    test_patch_file_name.cpp

    # The units under test, pulled straight from the app tree
    ${CMAKE_SOURCE_DIR}/source/model/LightMeterLayout.cpp
    ${CMAKE_SOURCE_DIR}/source/model/ThemeData.cpp
    ${CMAKE_SOURCE_DIR}/source/model/Patch.cpp
    ${CMAKE_SOURCE_DIR}/source/model/ModuleDescriptions.cpp
    ${CMAKE_SOURCE_DIR}/source/model/ModuleTags.cpp
    ${CMAKE_SOURCE_DIR}/source/model/PatchParser.cpp
    ${CMAKE_SOURCE_DIR}/source/model/PatchSerializer.cpp
    ${CMAKE_SOURCE_DIR}/source/model/PchFileIO.cpp
    ${CMAKE_SOURCE_DIR}/source/midi/SysExCodec.cpp
    ${CMAKE_SOURCE_DIR}/source/midi/UploadPacketizer.cpp
    ${CMAKE_SOURCE_DIR}/source/protocol/SysExMessage.cpp
    ${CMAKE_SOURCE_DIR}/source/protocol/SetPatchTitleMessage.cpp
)

target_include_directories(nme_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/libs/doctest
)

# The descriptions XML is loaded from the source tree rather than baked in as
# BinaryData, so the tests do not need the whole NmeData target.
target_compile_definitions(nme_tests PRIVATE
    NME_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(nme_tests PRIVATE
    juce::juce_core
    juce::juce_graphics    # Descriptors carry juce::Colour
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_test(NAME nme_tests COMMAND nme_tests)
