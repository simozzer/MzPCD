cmake_minimum_required(VERSION 3.22)

project(MzPCD VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# JUCE, and MzPCF - whose patterns and clock the PCD plays - live beside the repository
# rather than inside it, so a fix to the PCF's reaches the PCD too. Override either with
# -DJUCE_PATH=... or -DMZPCF_PATH=... if it is somewhere else.
#
set(JUCE_PATH  "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE"  CACHE PATH "Where JUCE is checked out")
set(MZPCF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../MzPCF" CACHE PATH "Where MzPCF is checked out")

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR
        "No JUCE at ${JUCE_PATH}.\n"
        "  cd ${CMAKE_CURRENT_SOURCE_DIR}/..\n"
        "  git clone --depth 1 https://github.com/juce-framework/JUCE\n"
        "or pass -DJUCE_PATH=<wherever it is>.")
endif()

if(NOT EXISTS "${MZPCF_PATH}/Source/PCF/Pcf.cpp")
    message(FATAL_ERROR
        "No MzPCF at ${MZPCF_PATH}.\n"
        "  cd ${CMAKE_CURRENT_SOURCE_DIR}/..\n"
        "  git clone https://github.com/simozzer/MzPCF\n"
        "or pass -DMZPCF_PATH=<wherever it is>.")
endif()

add_subdirectory("${JUCE_PATH}" JUCE)

#
# The per-user VST3 folder, which needs no elevation - JUCE's default of
# C:\Program Files\Common Files\VST3 cannot be written without running as administrator.
#
if(WIN32)
    set(VST3_DEFAULT "$ENV{LOCALAPPDATA}/Programs/Common/VST3")
elseif(APPLE)
    set(VST3_DEFAULT "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
else()
    set(VST3_DEFAULT "$ENV{HOME}/.vst3")
endif()

set(VST3_DESTINATION "${VST3_DEFAULT}" CACHE PATH "Where to install the built VST3")

set(MZPCD_FORMATS VST3)
if(APPLE)
    list(APPEND MZPCD_FORMATS AU)
endif()

#
# The two codes are the plugin's IDENTITY and must never change once anyone has saved a
# project with it - a VST3's class ID is made from them, and a host finds the plugin in a
# saved set by that ID.
#
juce_add_plugin(MzPCD
    COMPANY_NAME                "simozzer"
    COMPANY_COPYRIGHT           "Copyright (C) 2026 Simon Moscrop"
    PRODUCT_NAME                "MzPCD"
    PLUGIN_MANUFACTURER_CODE    Smoz          # never change - see above
    PLUGIN_CODE                 Pcd1          # never change - see above

    # JUCE does not inherit this from project().
    VERSION                     "${PROJECT_VERSION}"

    VST3_COPY_DIR               "${VST3_DESTINATION}"
    VST3_CATEGORIES             Fx Distortion

    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # Installed by install_vst3.cmake instead, so a host holding the old binary open is a
    # message rather than a failed build.
    COPY_PLUGIN_AFTER_BUILD     FALSE

    FORMATS                     ${MZPCD_FORMATS})

set(ENGINE_SOURCES
    Source/PCD/Pcd.cpp
    "${MZPCF_PATH}/Source/PCF/Pcf.cpp")

target_sources(MzPCD PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/Look.cpp
    ${ENGINE_SOURCES})

target_include_directories(MzPCD PRIVATE Source "${MZPCF_PATH}/Source")

add_custom_command(TARGET MzPCD_VST3 POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
            "-DSRC=${CMAKE_CURRENT_BINARY_DIR}/MzPCD_artefacts/$<CONFIG>/VST3/MzPCD.vst3"
            "-DDST=${VST3_DESTINATION}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/install_vst3.cmake"
    VERBATIM)

target_compile_definitions(MzPCD PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MzPCD
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#
# The engine's own check. It links no JUCE: the distortion and its clock are plain C++, so they
# can be held to the grid, aliasing and stability before a plugin is wrapped round them.
#
add_executable(PcdCheck Tests/PcdCheck.cpp ${ENGINE_SOURCES})
target_include_directories(PcdCheck PRIVATE Source/PCD "${MZPCF_PATH}/Source")

enable_testing()
add_test(NAME pcd COMMAND PcdCheck)
