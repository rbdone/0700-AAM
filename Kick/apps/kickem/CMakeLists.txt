cmake_minimum_required(VERSION 3.15)

## KT_KickEM: the Kick software simulator (enable with -DKICK_SIMULATOR=ON).
## Same application as apps/kick, with the serial control surface replaced by an on-screen one.

set(TargetName    KT_KickEM)
set(TargetVersion 0.3.0)
set(TargetCompany MUSICTribe)

project(${TargetName} VERSION ${TargetVersion})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

## Runtime data next to the build (the app reads ./assets, ./templates, ./demos on desktop)...
file(COPY "${CMAKE_SOURCE_DIR}/assets"    DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY "${CMAKE_SOURCE_DIR}/templates" DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY "${CMAKE_SOURCE_DIR}/demos"     DESTINATION "${CMAKE_BINARY_DIR}")

## Generate config.h (included by modules/common/common.h) the same way apps/kick does...
set(TargetSrc_DIR ".")
configure_file(${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

juce_add_gui_app(${TargetName}
        PRODUCT_NAME ${TargetName}
        COMPANY_NAME ${TargetCompany}
        MICROPHONE_PERMISSION_ENABLED TRUE   # macOS: audio input needs a usage description or the app is killed
)

set_target_properties(${TargetName} PROPERTIES
        CXX_STANDARD                    17
        VS_DEBUGGER_WORKING_DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(MSVC OR APPLE)
    set_target_properties(${TargetName} PROPERTIES
                        RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/$<0:>
                        LIBRARY_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/$<0:>
                        ARCHIVE_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/$<0:>
    )
endif()

target_include_directories(${TargetName}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/apps/kick       # reuse the monolith's adapters
)

target_sources(${TargetName}
    PRIVATE
        Main.cpp
        simulator/kick_Simulator.h
        simulator/kick_Simulator.cpp
        simulator/kick_SimulatorInterfaceImpl.h
        simulator/kick_SimulatorInterfaceImpl.cpp
)

target_compile_definitions(${TargetName}
    PRIVATE
        SIM_BUILD=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${TargetName},JUCE_PROJECT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${TargetName},JUCE_VERSION>"
        JUCE_ALSA=1
        JUCE_JACK=1

        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCER_ENABLE_GPL_MODE=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_ASIO=0
        JUCE_ENABLE_REPAINT_DEBUGGING=0

        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1

        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_VST=0
        JUCE_PLUGINHOST_VST3=${USE_VST}
        JUCE_PLUGINHOST_AU=${USE_AU}

        THPAR_SQLITE=1
        USE_OPTICK=${USE_OPTICK_VAL}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(${TargetName} PRIVATE "-latomic")
endif()

target_link_libraries(${TargetName}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags

    PRIVATE
        audio_engine
        kick_ui
        program_edit_ui
        juce::juce_gui_extra
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        contracts
        ipc
)

if(TIMESTAMP)
    target_link_libraries(${TargetName} PRIVATE timestamp)
endif()
