cmake_minimum_required(VERSION 3.22)

# Keep Intel builds compatible with macOS 10.13 High Sierra. This must be set before
# project() enables the compiler so local builds get the same deployment target
# as release builds. CMake ignores this setting on non-Apple platforms, and
# callers may still override it with -DCMAKE_OSX_DEPLOYMENT_TARGET.
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING
        "Minimum macOS version supported by Animatek NME")
endif()

project(AnimatekNME VERSION 0.17.0 LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link the MSVC runtime statically so the Windows .exe runs on machines
# without the Visual C++ Redistributable installed
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE 8's ALSA input drops SysEx messages larger than one sequencer event
# (~256 bytes): chunks are fed to the UMP converter as if they were complete
# messages and everything after the first chunk is lost, truncating patch
# downloads from the Nord Modular. Apply our reassembly patch to the vendored
# submodule until it is fixed upstream (idempotent: skipped when present).
#
# This only runs at configure time. `cmake --build` alone will NOT reapply a
# reverted patch, and a plain incremental build recompiles whatever the .cpp
# on disk currently says — so `git checkout` on the JUCE working tree silently
# ships an unpatched binary next rebuild. Never revert this file to "clean up"
# git status; a dirty submodule ("m JUCE") is the expected, correct state.
if(UNIX AND NOT APPLE)
    execute_process(
        COMMAND grep -q sysexAccumulators
                ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/modules/juce_audio_devices/native/juce_Midi_linux.cpp
        RESULT_VARIABLE NME_JUCE_PATCH_MISSING)
    if(NME_JUCE_PATCH_MISSING)
        execute_process(
            COMMAND git apply ${CMAKE_CURRENT_SOURCE_DIR}/packaging/patches/juce-alsa-sysex-reassembly.patch
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/JUCE
            RESULT_VARIABLE NME_JUCE_PATCH_RESULT)
        if(NME_JUCE_PATCH_RESULT EQUAL 0)
            message(STATUS "Applied JUCE ALSA SysEx reassembly patch")
        else()
            message(FATAL_ERROR "Failed to apply packaging/patches/juce-alsa-sysex-reassembly.patch to JUCE")
        endif()
    endif()
endif()

add_subdirectory(JUCE EXCLUDE_FROM_ALL)
option(NME_BUILD_PLUGIN "Build VST3/CLAP plugin targets" OFF)
option(NME_BUILD_TESTS "Build the unit tests (tests/, ctest)" ON)
option(NME_MCP_BRIDGE "Build embedded MCP control-socket bridge (localhost-only, lets an MCP client add/connect modules in the live editor)" ON)

if(NME_BUILD_PLUGIN)
    set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(libs/clap-juce-extensions)
endif()

# --- Shared source files (used by both standalone and plugin) ---
set(NME_SHARED_SOURCES
    source/MainComponent.cpp

    # Data Model
    source/model/ModuleDescriptions.cpp
    source/model/ModulePresets.cpp
    source/model/ModuleTags.cpp
    source/model/LightMeterLayout.cpp
    source/model/Patch.cpp
    source/model/PatchParser.cpp
    source/model/PatchSerializer.cpp
    source/model/PatchVariations.cpp
    source/model/PatchExtras.cpp
    source/model/Mutator.cpp
    source/model/MutationCategories.cpp
    source/model/PchFileIO.cpp
    source/model/SnipFileIO.cpp
    source/model/ThemeData.cpp

    # Value Formatters (ported from nmformat.js)
    source/format/ValueFormatters.cpp

    # UI
    source/ui/MainLayout.cpp
    source/ui/AppTheme.cpp
    source/ui/ModuleBrowserPanel.cpp
    source/ui/ModuleIconBar.cpp
    # PatchCanvas, split by what each part does. One class, six files: it had
    # grown past nine thousand lines in one.
    source/ui/PatchCanvasComponent.cpp
    source/ui/PatchCanvasPainting.cpp
    source/ui/PatchCanvasMouse.cpp
    source/ui/PatchCanvasComments.cpp
    source/ui/PatchCanvasClipboard.cpp
    source/ui/PatchCanvasPresets.cpp
    source/ui/ColorScheme.cpp
    source/ui/ThemeRegistry.cpp
    source/ui/InspectorPanel.cpp
    source/ui/StatusBar.cpp
    source/ui/PatchHeaderBar.cpp
    source/ui/KnobDrag.cpp
    source/ui/ValueSpinner.cpp
    source/ui/PatchLocationDialog.cpp
    source/ui/AboutDialog.cpp
    source/ui/SlotSelectDialog.cpp
    source/ui/PatchSettingsDialog.cpp
    source/ui/MidiSettingsDialog.cpp
    source/ui/SynthSettingsDialog.cpp
    source/ui/EditorOptionsDialog.cpp
    source/ui/QuickAddPopup.cpp
    source/ui/PatchBrowserPanel.cpp
    source/ui/PresetBrowserWindow.cpp
    source/ui/BankTransferDialog.cpp
    source/ui/KnobFloaterWindow.cpp
    source/ui/KeyboardFloaterWindow.cpp
    source/ui/PatchNotesFloaterWindow.cpp
    source/ui/MutatorWindow.cpp
    source/ui/SysexMonitorWindow.cpp
    source/ui/SlotView.cpp
    source/ui/SlotMdiArea.cpp

    # MIDI/SysEx
    source/midi/SysExCodec.cpp
    source/midi/UploadPacketizer.cpp
    source/midi/NmMessages.cpp
    source/midi/NmProtocol.cpp
    source/midi/MidiDeviceManager.cpp
    source/midi/ConnectionManager.cpp
    source/midi/MidiMonitor.cpp

    # Protocol Messages
    source/protocol/SysExMessage.cpp
    source/protocol/NewCableMessage.cpp
    source/protocol/DeleteCableMessage.cpp
    source/protocol/MoveModuleMessage.cpp
    source/protocol/NewModuleMessage.cpp
    source/protocol/DeleteModuleMessage.cpp
    source/protocol/ParameterEncoder.cpp
    source/protocol/StorePatchMessage.cpp
    source/protocol/DeletePatchMessage.cpp
    source/protocol/SetPatchTitleMessage.cpp
    source/protocol/SetModuleTitleMessage.cpp
    source/protocol/SendControllerSnapshotMessage.cpp
    source/protocol/MorphAssignmentMessage.cpp
    source/protocol/MorphKeyboardAssignmentMessage.cpp
    source/protocol/MorphRangeChangeMessage.cpp
    source/protocol/KnobAssignmentMessage.cpp
    source/protocol/MidiCtrlAssignmentMessage.cpp

    # Synchronization
    source/sync/PatchSynchronizer.cpp
    source/sync/BankTransferManager.cpp

    # Help system
    source/help/ModuleHelpData.cpp
    source/help/ModuleHelpPopup.cpp
)

# --- Source hygiene ---
# juce::String(const char*) decodes bytes as Latin-1, so a stray em dash or
# curly quote in a narrow literal reaches the UI as mojibake (issue #43). Catch
# it at build time. Skipped silently where no Python interpreter is available.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(check_ascii_literals ALL
        COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/check-ascii-literals.py"
                "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Checking string literals are ASCII-only"
        VERBATIM
    )
endif()

# --- Standalone Application ---
juce_add_gui_app(AnimatekNME
    PRODUCT_NAME "AnimatekNME"
    COMPANY_NAME "Animatek"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/data/images/app-icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/data/images/app-icon.png"
    VERSION "${PROJECT_VERSION}"
    PLIST_TO_MERGE
        "<plist><dict><key>LSMinimumSystemVersion</key><string>${CMAKE_OSX_DEPLOYMENT_TARGET}</string></dict></plist>"
)

target_sources(AnimatekNME PRIVATE
    source/Main.cpp
    ${NME_SHARED_SOURCES}
)

# MCP bridge is standalone-app-only (deliberately NOT in NME_SHARED_SOURCES):
# multiple simultaneous plugin instances would fight over the same fixed
# localhost port, and "the live editor" isn't a meaningful concept for a
# plugin instance. juce::StreamingSocket lives in juce_core in this JUCE
# version (networking was folded into core, there's no separate juce_network
# module here) - already linked below, nothing extra needed.
if(NME_MCP_BRIDGE)
    target_sources(AnimatekNME PRIVATE
        source/mcp/McpBridgeServer.cpp
        source/mcp/McpRequestHandler.cpp
    )
endif()
target_compile_definitions(AnimatekNME PRIVATE NME_MCP_BRIDGE=$<BOOL:${NME_MCP_BRIDGE}>)

if(NME_BUILD_PLUGIN)
    # --- VST3/AU Plugin ---
    juce_add_plugin(AnimatekNMEPlugin
        PLUGIN_MANUFACTURER_CODE Antk
        PLUGIN_CODE Nme1
        FORMATS VST3 AU Standalone
        PRODUCT_NAME "Animatek NME"
        COMPANY_NAME "Animatek"
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/data/images/app-icon.png"
        ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/data/images/app-icon.png"
        IS_SYNTH TRUE
        IS_MIDI_EFFECT FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        VST3_CATEGORIES "Instrument" "Tools"
        VERSION "${PROJECT_VERSION}"
    )

    target_sources(AnimatekNMEPlugin PRIVATE
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginEditor.cpp
        ${NME_SHARED_SOURCES}
    )

    clap_juce_extensions_plugin(
        TARGET AnimatekNMEPlugin
        CLAP_ID "com.animatek.nme"
        CLAP_FEATURES "note-effect" "utility"
        CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 0
    )
endif()

juce_add_binary_data(NmeData SOURCES
    data/images/app-icon.png
    data/images/app-icon-dark.png
    data/modules.xml
    data/transformations.xml
    data/classic-theme.xml
    data/credits.txt
    data/images/single/decoration-1.png
    data/images/single/decoration-2.png
    data/images/single/decoration-3.png
    data/images/single/decoration-4.png
    data/images/single/decoration-5.png
    data/images/single/decoration-6.png
    data/images/single/decoration-7.png
    data/images/single/decoration-8.png
    data/images/single/decoration-9.png
    data/images/single/decoration-10.png
    data/images/single/decoration-11.png
    data/images/single/decoration-12.png
    data/images/single/decoration-13.png
    data/images/single/decoration-14.png
    data/images/single/decoration-15.png
    data/images/single/decoration-17.png
    data/images/single/decoration-18.png
    data/images/refresh-icon.svg
)

# --- Shared settings ---
set(NME_TARGETS AnimatekNME)
if(NME_BUILD_PLUGIN)
    list(APPEND NME_TARGETS AnimatekNMEPlugin)
endif()

foreach(target ${NME_TARGETS})
    target_include_directories(${target} PRIVATE source)

    target_compile_definitions(${target} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="Animatek NME"
        JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
        JUCE_VST3_CAN_REPLACE_VST2=0
    )

    target_link_libraries(${target} PRIVATE
        NmeData
        juce::juce_core
        juce::juce_events
        juce::juce_data_structures
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
endforeach()

if(NME_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(UNIX AND NOT APPLE)
    set(NME_DESKTOP_ID "com.animatek.AnimatekNME")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/${NME_DESKTOP_ID}.desktop.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${NME_DESKTOP_ID}.desktop"
        @ONLY
    )

    install(TARGETS AnimatekNME
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/images/app-icon-512.png"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/512x512/apps"
        RENAME "${NME_DESKTOP_ID}.png"
    )

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${NME_DESKTOP_ID}.desktop"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
    )
endif()
