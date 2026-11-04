# motogp0910 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(motogp0910 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk-version.json" MOTOGP_SDK_METADATA)
string(JSON MOTOGP_SDK_VERSION GET "${MOTOGP_SDK_METADATA}" version)
string(JSON MOTOGP_SDK_CMAKE_VERSION GET "${MOTOGP_SDK_METADATA}" cmake_version)
set(REXSDK_VERSION "${MOTOGP_SDK_CMAKE_VERSION}" CACHE STRING "Required ReXGlue SDK version")
include(generated/rexglue.cmake)
if(NOT REXGLUE_VERSION_STRING STREQUAL MOTOGP_SDK_VERSION)
    message(FATAL_ERROR "SDK must match ${MOTOGP_SDK_VERSION}; found ${REXGLUE_VERSION_STRING}")
endif()
option(MOTOGP_STAGE_RUNTIME "Stage the verified matching Windows runtime" ON)
add_library(motogp_config STATIC src/config_file.cpp src/user_data_paths.cpp)
target_include_directories(motogp_config SYSTEM PRIVATE $<TARGET_PROPERTY:rex::runtime,INTERFACE_INCLUDE_DIRECTORIES>)

# Recovered season/class selection and query identity, separate from native hooks.
add_library(motogp_leaderboard_categories STATIC src/leaderboard_categories.cpp src/leaderboard_scope_capture.cpp)
target_include_directories(motogp_leaderboard_categories PUBLIC src)
target_link_libraries(motogp_leaderboard_categories PUBLIC rex::runtime)
rexglue_apply_target_settings(motogp_leaderboard_categories)

# Native LAN protocol/service, separate from guest memory and GPU presentation.
add_library(motogp_lan STATIC src/lan/protocol.cpp src/lan/node.cpp src/online/relay.cpp src/online/client.cpp)
target_include_directories(motogp_lan PUBLIC src)
if(WIN32)
    target_link_libraries(motogp_lan PUBLIC ws2_32 iphlpapi bcrypt winhttp crypt32)
else()
    find_package(Threads REQUIRED)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(motogp_lan PUBLIC Threads::Threads OpenSSL::Crypto)
endif()

add_library(motogp_social STATIC src/online/social.cpp src/online/discord_invites.cpp)
target_include_directories(motogp_social PUBLIC src)
target_link_libraries(motogp_social PUBLIC motogp_lan rex::runtime)
rexglue_apply_target_settings(motogp_social)
add_library(motogp_lan_guest STATIC src/lan/guest_bridge.cpp src/lan/socket_bridge.cpp src/lan/metadata.cpp src/online/stats.cpp src/online/social_guest.cpp)
target_link_libraries(motogp_lan_guest PUBLIC motogp_lan motogp_social motogp_leaderboard_categories rex::runtime)
rexglue_apply_target_settings(motogp_lan_guest)

add_library(motogp_achievements STATIC src/achievement_ui.cpp src/achievement_sound.cpp)
target_link_libraries(motogp_achievements PUBLIC rex::runtime motogp_config)
rexglue_apply_target_settings(motogp_achievements)
if(WIN32)
    target_link_libraries(motogp_achievements PUBLIC winmm)
endif()

add_library(motogp_adaptive STATIC src/adaptive_triggers.cpp)
target_link_libraries(motogp_adaptive PUBLIC rex::runtime)
rexglue_apply_target_settings(motogp_adaptive)

add_library(motogp_settings STATIC src/in_game_settings.cpp)
target_compile_definitions(motogp_settings PUBLIC REX_HAS_FIDELITYFX_SDK=1)
target_link_libraries(motogp_settings PUBLIC rex::runtime motogp_adaptive motogp_config)

rexglue_apply_target_settings(motogp_settings)

# Text overlays are separate from the original game and its voice selection.
add_library(motogp_localization STATIC src/localization.cpp)
target_link_libraries(motogp_localization PUBLIC rex::runtime)
rexglue_apply_target_settings(motogp_localization)

# Local input routing and a transient second local user; no server protocol.
add_library(motogp_local_splitscreen STATIC src/local_splitscreen.cpp)
target_include_directories(motogp_local_splitscreen PUBLIC src)
target_link_libraries(motogp_local_splitscreen PUBLIC rex::runtime)
rexglue_apply_target_settings(motogp_local_splitscreen)

# Optional local Discord IPC. Presence needs no server; invitations use the
# existing authenticated community service, without another SDK/runtime DLL.
add_library(motogp_discord STATIC src/discord_presence.cpp src/discord/client.cpp src/discord/launch.cpp)
target_link_libraries(motogp_discord PUBLIC rex::runtime)
if(WIN32)
    target_link_libraries(motogp_discord PRIVATE advapi32)
endif()
rexglue_apply_target_settings(motogp_discord)

# Sources
set(MOTOGP0910_SOURCES
    src/main.cpp
    src/debug_overlay.cpp src/diagnostic_options.cpp
    src/gamer_card.cpp src/nobk_panel.cpp
    src/gauge_hud.cpp src/gauge_capture.cpp src/gauge_classic.cpp
    src/lan/runtime.cpp
    src/button_prompts.cpp src/control_diagram.cpp
    src/ps3_prompt_textures.cpp
    src/physics_assets.cpp
    src/livery_readback.cpp
    src/game_language.cpp
    src/options_menu.cpp
    src/dlc_content.cpp
    src/rider_selection.cpp
    src/online/leaderboard_display.cpp
    src/leaderboard_categories_hooks.cpp
    src/leaderboard_scope_hooks.cpp
    src/frame_pacing.cpp
    src/display_settings.cpp
    src/engine_settings.cpp
    src/ultrawide.cpp
    src/quit_menu.cpp
    src/in_game_settings_input.cpp
)

if(WIN32)
    enable_language(RC)
    function(motogp_embed_icon target artwork)
        set(MOTOGP_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/${artwork}.ico")
        configure_file(resources/icons/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/${target}-icon.rc" @ONLY)
        target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${target}-icon.rc")
        set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${target}-icon.rc" APPEND PROPERTY
            OBJECT_DEPENDS "${MOTOGP_ICON_FILE}")
    endfunction()
    add_library(motogp_crash_report STATIC src/crash_report.cpp)
    target_link_libraries(motogp_crash_report PUBLIC dbghelp bcrypt)

    add_executable(motogp0910-crash-monitor WIN32 src/crash_monitor.cpp)
    target_include_directories(motogp0910-crash-monitor SYSTEM PRIVATE $<TARGET_PROPERTY:rex::runtime,INTERFACE_INCLUDE_DIRECTORIES>)
    target_link_libraries(motogp0910-crash-monitor PRIVATE motogp_crash_report shell32)
    add_executable(motogp0910 WIN32 ${MOTOGP0910_SOURCES})
    target_link_libraries(motogp0910 PRIVATE motogp_crash_report)
    target_link_options(motogp0910 PRIVATE "LINKER:/MAP:${CMAKE_CURRENT_BINARY_DIR}/motogp0910.map")
    motogp_embed_icon(motogp0910 game)
    configure_file(resources/gamer-card/assets.rc.in "${CMAKE_CURRENT_BINARY_DIR}/gamer-card-assets.rc" @ONLY)
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/gamer-card-assets.rc" APPEND PROPERTY
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/gamer-card/default-helmet.png;${CMAKE_CURRENT_SOURCE_DIR}/resources/gamer-card/emoji.png;${CMAKE_CURRENT_SOURCE_DIR}/resources/gamer-card/emoji.json")
    target_sources(motogp0910 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/gamer-card-assets.rc")
    target_link_libraries(motogp0910 PRIVATE xinput winhttp shell32 windowscodecs ole32)

    add_library(motogp_launcher_achievements STATIC src/launcher/achievements.cpp)
    target_link_libraries(motogp_launcher_achievements PUBLIC rex::runtime motogp_config shell32 bcrypt ole32 uuid)
    rexglue_apply_target_settings(motogp_launcher_achievements)
    add_library(motogp_launcher_updates STATIC src/launcher/updates.cpp src/launcher/update_install.cpp)
    target_link_libraries(motogp_launcher_updates PUBLIC winhttp bcrypt shell32)
    add_executable(motogp0910-update-helper WIN32 src/launcher/update_helper.cpp)
    target_compile_definitions(motogp0910-update-helper PRIVATE UNICODE _UNICODE)
    # The helper runs from TEMP while the installation's VC DLLs are replaced.
    set_property(TARGET motogp0910-update-helper PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_libraries(motogp0910-update-helper PRIVATE bcrypt shell32 user32)
    add_library(motogp0910-launcher-ui SHARED src/launcher/main.cpp)
    set_target_properties(motogp0910-launcher-ui PROPERTIES OUTPUT_NAME "motogp0910-ui" PREFIX "")
    target_compile_definitions(motogp0910-launcher-ui PRIVATE MOTOGP_LAUNCHER_DLL=1)
    add_executable(motogp0910-launcher WIN32 src/launcher/bootstrap.cpp)
    set_target_properties(motogp0910-launcher PROPERTIES OUTPUT_NAME "MotoGP09-10Launcher")
    motogp_embed_icon(motogp0910-launcher launcher)
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(motogp0910-launcher PRIVATE /GS- /Zl /Oi-)
    else()
        target_compile_options(motogp0910-launcher PRIVATE -fno-stack-protector -fno-builtin -fno-exceptions -fno-rtti)
    endif()
    target_link_options(motogp0910-launcher PRIVATE "LINKER:/NODEFAULTLIB" "LINKER:/ENTRY:LauncherEntry")
    target_link_libraries(motogp0910-launcher PRIVATE kernel32 user32)
    add_dependencies(motogp0910-launcher motogp0910-launcher-ui motogp0910-update-helper)

    target_compile_definitions(motogp0910-launcher-ui PRIVATE UNICODE _UNICODE)
    target_include_directories(motogp0910-launcher-ui SYSTEM PRIVATE
        $<TARGET_PROPERTY:rex::runtime,INTERFACE_INCLUDE_DIRECTORIES>)
    target_link_libraries(motogp0910-launcher-ui PRIVATE motogp_launcher_updates motogp_launcher_achievements motogp_lan user32 shell32 ole32 comctl32 bcrypt uuid dxgi gdiplus gdi32 dwmapi uxtheme shlwapi advapi32)
    add_custom_command(TARGET motogp0910-launcher-ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:motogp0910-launcher-ui>/resources/archive"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/7zip/7z.exe"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/7zip/7z.dll"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/7zip/7zip-source.tar.xz"
            "$<TARGET_FILE_DIR:motogp0910-launcher-ui>/resources/archive"
        VERBATIM)
    foreach(steam_artwork icon.png logo.png banner.png cover.png wide.png)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/steam/${steam_artwork}")
            add_custom_command(TARGET motogp0910-launcher-ui POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:motogp0910-launcher-ui>/resources/steam"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/resources/steam/${steam_artwork}"
                    "$<TARGET_FILE_DIR:motogp0910-launcher-ui>/resources/steam/${steam_artwork}"
                VERBATIM)
        endif()
    endforeach()

    configure_file(resources/launcher/assets.rc.in "${CMAKE_CURRENT_BINARY_DIR}/launcher-assets.rc" @ONLY)
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/launcher-assets.rc" APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/launcher/background.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/verification/game-files.sha256"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/launcher/third-party-notices.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/launcher-fonts/OpenSans-Regular.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/launcher-fonts/OpenSans-SemiBold.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/launcher-fonts/OpenSans-Bold.ttf")
foreach(launcher_asset_target motogp0910-launcher)
        target_sources(${launcher_asset_target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/launcher-assets.rc")
    endforeach()

else()
    add_executable(motogp0910 ${MOTOGP0910_SOURCES})
endif()

rexglue_setup_target(motogp0910 GPU_PLUGINS xenos)
if(TARGET motogp0910_recomp)
    target_sources(motogp0910_recomp PRIVATE src/bike_capacity.cpp)
endif()
add_custom_command(TARGET motogp0910 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:motogp0910>/resources/ui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/open-sans/OpenSans-Semibold.ttf"
        "$<TARGET_FILE_DIR:motogp0910>/resources/ui/OpenSans-Semibold.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/launcher-fonts/OpenSans-Regular.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/launcher-fonts/OpenSans-Bold.ttf"
        "$<TARGET_FILE_DIR:motogp0910>/resources/ui"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:motogp0910>/resources/hud/motogp17"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/hud/motogp17/gauge.json"
        "$<TARGET_FILE_DIR:motogp0910>/resources/hud/motogp17/gauge.json")
set_property(TARGET motogp0910 APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/hud/motogp17/gauge.json")
foreach(classic_style motogp06 motogpurt2 motogpurt3)
    add_custom_command(TARGET motogp0910 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:motogp0910>/resources/hud/${classic_style}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/hud/${classic_style}/gauge.json"
            "$<TARGET_FILE_DIR:motogp0910>/resources/hud/${classic_style}/gauge.json")
    set_property(TARGET motogp0910 APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/hud/${classic_style}/gauge.json")
endforeach()
target_link_libraries(motogp0910 PRIVATE motogp_discord motogp_lan_guest motogp_achievements motogp_settings motogp_local_splitscreen)

foreach(artwork_target motogp0910)
    if(TARGET ${artwork_target})
        set_property(TARGET ${artwork_target} APPEND PROPERTY LINK_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/controller/ps3-prompts.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/controller/ps3-controls.bin")
        add_custom_command(TARGET ${artwork_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${artwork_target}>/resources/controller"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/controller/ps3-prompts.bin"
                "$<TARGET_FILE_DIR:${artwork_target}>/resources/controller/ps3-prompts.bin"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/controller/ps3-controls.bin"
                "$<TARGET_FILE_DIR:${artwork_target}>/resources/controller/ps3-controls.bin"
            VERBATIM)
    endif()
endforeach()

# Keep the runtime pair consistent even when building directly from an IDE.
# The exact SDK code generator/import library remain pinned by sdk-version.json; the
# separately verified runtime enables bundled spatial shaders and experimental AMD providers.
if(WIN32 AND MOTOGP_STAGE_RUNTIME)
    if(CMAKE_CROSSCOMPILING)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
foreach(stage_target motogp0910)
            if(TARGET ${stage_target})
                add_custom_command(TARGET ${stage_target} POST_BUILD
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/stage-spatial-runtime.py
                        $<TARGET_FILE_DIR:${stage_target}>
                    VERBATIM)
            endif()
        endforeach()
    else()
        find_program(MOTOGP_POWERSHELL NAMES powershell pwsh REQUIRED)
foreach(stage_target motogp0910)
            if(TARGET ${stage_target})
                add_custom_command(TARGET ${stage_target} POST_BUILD
                    COMMAND ${MOTOGP_POWERSHELL} -NoProfile -ExecutionPolicy Bypass -File
                        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/stage-spatial-runtime.ps1
                        -Destination $<TARGET_FILE_DIR:${stage_target}>
                    VERBATIM)
            endif()
        endforeach()
    endif()
endif()

set(MOTOGP_LIVERY_GPU_PLUGIN "" CACHE FILEPATH "Matching Linux GPU plugin built with the career readback patch")
if(NOT WIN32 AND MOTOGP_LIVERY_GPU_PLUGIN)
    if(NOT EXISTS "${MOTOGP_LIVERY_GPU_PLUGIN}")
        message(FATAL_ERROR "MOTOGP_LIVERY_GPU_PLUGIN does not exist")
    endif()
foreach(stage_target motogp0910)
        if(TARGET ${stage_target})
            add_custom_command(TARGET ${stage_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MOTOGP_LIVERY_GPU_PLUGIN}" "$<TARGET_FILE_DIR:${stage_target}>/librexgpu-xenos.so"
                VERBATIM)
        endif()
    endforeach()
endif()

set(MOTOGP_RUNTIME_LIBRARY "" CACHE FILEPATH "Matching custom Linux runtime library")
if(NOT WIN32 AND MOTOGP_RUNTIME_LIBRARY)
foreach(stage_target motogp0910)
        if(TARGET ${stage_target})
            add_custom_command(TARGET ${stage_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MOTOGP_RUNTIME_LIBRARY}"
                    "$<TARGET_FILE_DIR:${stage_target}>/librexruntime.so" VERBATIM)
        endif()
    endforeach()
endif()

foreach(locale_target motogp0910)
    if(TARGET ${locale_target})
        target_link_libraries(${locale_target} PRIVATE motogp_localization)
    endif()
endforeach()
foreach(locale_target motogp0910 motogp0910-launcher)
    if(TARGET ${locale_target})
        file(GLOB_RECURSE locale_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/localization/tr/overlay/*")
        set_property(TARGET ${locale_target} APPEND PROPERTY LINK_DEPENDS ${locale_files})
        add_custom_command(TARGET ${locale_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/localization/tr/overlay"
                "$<TARGET_FILE_DIR:${locale_target}>/resources/localization/tr/overlay"
            VERBATIM)
    endif()
endforeach()
