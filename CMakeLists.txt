cmake_minimum_required(VERSION 3.22)
project(AnimatekNME VERSION 0.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)
option(NME_BUILD_PLUGIN "Build VST3/CLAP plugin targets" OFF)

if(NME_BUILD_PLUGIN)
    set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(libs/clap-juce-extensions)
endif()

# --- Shared source files (used by both standalone and plugin) ---
set(NME_SHARED_SOURCES
    source/MainComponent.cpp

    # Data Model
    source/model/ModuleDescriptions.cpp
    source/model/Patch.cpp
    source/model/PatchParser.cpp
    source/model/PatchSerializer.cpp
    source/model/PchFileIO.cpp
    source/model/SnipFileIO.cpp
    source/model/ThemeData.cpp

    # Value Formatters (ported from nmformat.js)
    source/format/ValueFormatters.cpp

    # UI
    source/ui/MainLayout.cpp
    source/ui/AppTheme.cpp
    source/ui/ModuleBrowserPanel.cpp
    source/ui/PatchCanvasComponent.cpp
    source/ui/ColorScheme.cpp
    source/ui/InspectorPanel.cpp
    source/ui/StatusBar.cpp
    source/ui/PatchHeaderBar.cpp
    source/ui/PatchLocationDialog.cpp
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

    # MIDI/SysEx
    source/midi/SysExCodec.cpp
    source/midi/NmMessages.cpp
    source/midi/NmProtocol.cpp
    source/midi/MidiDeviceManager.cpp
    source/midi/ConnectionManager.cpp

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

# --- Standalone Application ---
juce_add_gui_app(AnimatekNME
    PRODUCT_NAME "AnimatekNME"
    COMPANY_NAME "Animatek"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/data/images/app-icon-dark.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/data/images/app-icon-dark.png"
    VERSION "0.6.0"
)

target_sources(AnimatekNME PRIVATE
    source/Main.cpp
    ${NME_SHARED_SOURCES}
)

if(NME_BUILD_PLUGIN)
    # --- VST3/AU Plugin ---
    juce_add_plugin(AnimatekNMEPlugin
        PLUGIN_MANUFACTURER_CODE Antk
        PLUGIN_CODE Nme1
        FORMATS VST3 AU Standalone
        PRODUCT_NAME "Animatek NME"
        COMPANY_NAME "Animatek"
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/data/images/app-icon-dark.png"
        ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/data/images/app-icon-dark.png"
        IS_SYNTH TRUE
        IS_MIDI_EFFECT FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        VST3_CATEGORIES "Instrument" "Tools"
        VERSION "0.6.0"
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
    data/images/app-icon-dark.png
    data/modules.xml
    data/transformations.xml
    data/classic-theme.xml
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
        JUCE_APPLICATION_VERSION_STRING="0.6.0"
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
