# reeot - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.
#
#

cmake_minimum_required(VERSION 3.25)
project(reeot VERSION 1.0.0 LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()
if(NOT DEFINED PROJECT_VERSION_PATCH OR PROJECT_VERSION_PATCH STREQUAL "")
    set(PROJECT_VERSION_PATCH 0)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(REEOT_D3D12 "Build the D3D12 renderer backend (OFF selects Vulkan)" ${WIN32})
option(REEOT_PROFILING "Compile the Tracy zones into a Release build of reeot" OFF)
set(REEOT_PROFILING_ON "$<OR:$<BOOL:${REEOT_PROFILING}>,$<NOT:$<CONFIG:Release>>>")

set(REEOT_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(REEOT_SHADER_COMMON_H
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/XenosRecomp/XenosRecomp/shader_common.h")
cmake_path(GET REEOT_SHADER_COMMON_H PARENT_PATH REEOT_SHADER_COMMON_DIR)
set(REEOT_SHADER_CACHE_CPP "${CMAKE_CURRENT_SOURCE_DIR}/generated/shader_cache.cpp")
set(REEOT_SHADER_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders" CACHE PATH
    "The game's shader containers, one per file, the shader cache is built from")

include(generated/rexglue.cmake)
# The macOS Vulkan staging in rexglue_setup_target() reads REXGLUE_ROOT.
if(REXSDK_DIR AND NOT REXGLUE_ROOT)
    set(REXGLUE_ROOT "${REXSDK_DIR}")
endif()
include(cmake/build_info.cmake)

reeot_write_build_info("${REEOT_GEN_DIR}/core/build_info.h")

option(REEOT_BUILD_INSTALLER "Build the built-in first-run installer" ON)
include(cmake/embed.cmake)
reeot_embed_directory(
    ROOT        "${CMAKE_CURRENT_SOURCE_DIR}/res/embed"
    HEADER      "${REEOT_GEN_DIR}/embedded.h"
    SOURCES_VAR REEOT_EMBED_SOURCES)

add_subdirectory(thirdparty)

set(_reeot_default_paks "${CMAKE_CURRENT_SOURCE_DIR}/../reeot/reference_pak")
if(NOT EXISTS "${_reeot_default_paks}/Main.pak"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shader_notes/EOT_pak/Main.pak")
    set(_reeot_default_paks "${CMAKE_CURRENT_SOURCE_DIR}/shader_notes/EOT_pak")
endif()
set(REEOT_REFERENCE_PAKS "${_reeot_default_paks}" CACHE PATH
    "Directory holding the retail Main.pak / Common.pak the content package copies textures from")
set(REEOT_PKZ_DATA "${CMAKE_CURRENT_SOURCE_DIR}/pkzlib")
add_executable(pkzprep pkzlib/tools/pkzprep.cpp)
target_include_directories(pkzprep PRIVATE thirdparty/stb)
set(REEOT_PKZ_OUTPUT "${CMAKE_BINARY_DIR}/pkz/ReeotUI.pkz")
add_custom_command(
    OUTPUT "${REEOT_PKZ_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/pkz"
    COMMAND pkztool make "${REEOT_PKZ_DATA}/ReeotUI.manifest" "${REEOT_PKZ_OUTPUT}"
            --paks "${REEOT_REFERENCE_PAKS}"
    DEPENDS pkztool
            "${REEOT_PKZ_DATA}/ReeotUI.manifest"
            "${REEOT_PKZ_DATA}/strings/ui.txt"
            "${REEOT_PKZ_DATA}/strings/TempusGothic_cyrillic.txt"
            "${REEOT_PKZ_DATA}/strings/SansaCon_cyrillic.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/res/fonts/TempusGothic_texture_0.dds"
            "${CMAKE_CURRENT_SOURCE_DIR}/res/fonts/SteelworksBook_texture_0.dds"
    COMMENT "Authoring ReeotUI.pkz from pkzlib/ReeotUI.manifest"
    VERBATIM)
file(GLOB REEOT_MENU_TRANSLATIONS CONFIGURE_DEPENDS "${REEOT_PKZ_DATA}/strings/menu.*.txt")
set(REEOT_MENU_TABLES "")
foreach(translation IN LISTS REEOT_MENU_TRANSLATIONS)
    cmake_path(GET translation FILENAME name)
    set(table "${REEOT_PKZ_DATA}/generated/${name}")
    add_custom_command(
        OUTPUT "${table}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${REEOT_PKZ_DATA}/generated"
        COMMAND pkzprep strings "${REEOT_PKZ_DATA}/strings/menu.txt" "${translation}" "${table}"
        DEPENDS pkzprep "${REEOT_PKZ_DATA}/strings/menu.txt" "${translation}"
        COMMENT "Writing ${name} over the English menu strings"
        VERBATIM)
    list(APPEND REEOT_MENU_TABLES "${table}")
endforeach()
set(REEOT_MENU_PKZ_OUTPUT "${CMAKE_BINARY_DIR}/pkz/ReeotMenu.pkz")
add_custom_command(
    OUTPUT "${REEOT_MENU_PKZ_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/pkz"
    COMMAND pkztool make "${REEOT_PKZ_DATA}/ReeotMenu.manifest" "${REEOT_MENU_PKZ_OUTPUT}"
            --paks "${REEOT_REFERENCE_PAKS}"
    DEPENDS pkztool
            "${REEOT_PKZ_DATA}/ReeotMenu.manifest"
            "${REEOT_PKZ_DATA}/strings/menu.txt"
            ${REEOT_MENU_TABLES}
    COMMENT "Authoring ReeotMenu.pkz from pkzlib/ReeotMenu.manifest"
    VERBATIM)
set(REEOT_XEX "${CMAKE_CURRENT_SOURCE_DIR}/assets/Default.xex")
set(REEOT_ACH_DUMP "${REEOT_PKZ_DATA}/generated/achievements")
add_custom_command(
    OUTPUT "${REEOT_ACH_DUMP}/achievements.toml"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${REEOT_ACH_DUMP}"
    COMMAND $<TARGET_FILE:rex::rexglue> init --project-name reeot --xex-path "${REEOT_XEX}"
            achievements "${REEOT_XEX}" "${REEOT_ACH_DUMP}"
    DEPENDS "${REEOT_XEX}"
    COMMENT "Extracting the achievements and their icons from Default.xex"
    VERBATIM)
set(REEOT_ACH_STRINGS "${REEOT_PKZ_DATA}/generated/reeot_ach_strings.txt")
add_custom_command(
    OUTPUT "${REEOT_ACH_STRINGS}"
    COMMAND pkzprep achievements "${REEOT_ACH_DUMP}/achievements.toml" REEOT_ACH_
            "${REEOT_PKZ_DATA}/strings/achievements.txt" "${REEOT_ACH_STRINGS}"
    DEPENDS pkzprep "${REEOT_ACH_DUMP}/achievements.toml" "${REEOT_PKZ_DATA}/strings/achievements.txt"
    COMMENT "Writing the achievement strings from the rexglue dump"
    VERBATIM)
set(REEOT_ACH_ICONS "${REEOT_PKZ_DATA}/generated/achievement_icons_64.png")
add_custom_command(
    OUTPUT "${REEOT_ACH_ICONS}"
    COMMAND pkzprep achicons "${REEOT_ACH_DUMP}/icons" "${REEOT_PKZ_DATA}/achievements/secret_plate.png"
            "${REEOT_ACH_ICONS}"
    DEPENDS pkzprep "${REEOT_ACH_DUMP}/achievements.toml" "${REEOT_PKZ_DATA}/achievements/secret_plate.png"
    COMMENT "Building the achievement icon sheet from the rexglue dump"
    VERBATIM)
set(REEOT_ACH_PKZ_OUTPUT "${CMAKE_BINARY_DIR}/pkz/ReeotAchievements.pkz")
add_custom_command(
    OUTPUT "${REEOT_ACH_PKZ_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/pkz"
    COMMAND pkztool make "${REEOT_PKZ_DATA}/ReeotAchievements.manifest" "${REEOT_ACH_PKZ_OUTPUT}"
            --paks "${REEOT_REFERENCE_PAKS}"
    DEPENDS pkztool
            "${REEOT_PKZ_DATA}/ReeotAchievements.manifest"
            "${REEOT_ACH_STRINGS}"
            "${REEOT_ACH_ICONS}"
            "${CMAKE_CURRENT_SOURCE_DIR}/res/achievements/Reeot_Ach_Toast.dds"
    COMMENT "Authoring ReeotAchievements.pkz from pkzlib/ReeotAchievements.manifest"
    VERBATIM)
set(REEOT_ICONS_PKZ_OUTPUT "${CMAKE_BINARY_DIR}/pkz/ReeotIcons.pkz")
add_custom_command(
    OUTPUT "${REEOT_ICONS_PKZ_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/pkz"
    COMMAND pkztool make "${REEOT_PKZ_DATA}/ReeotIcons.manifest" "${REEOT_ICONS_PKZ_OUTPUT}"
            --paks "${REEOT_REFERENCE_PAKS}"
    DEPENDS pkztool
            "${REEOT_PKZ_DATA}/ReeotIcons.manifest"
            "${REEOT_PKZ_DATA}/strings/icons.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/res/controls/Reeot_Icons.dds"
            "${CMAKE_CURRENT_SOURCE_DIR}/res/controls/mash_prompt/Reeot_MashPrompt_PlayStation_D.dds"
            "${CMAKE_CURRENT_SOURCE_DIR}/res/controls/mash_prompt/Reeot_MashPromptY_PlayStation_D.dds"
            "${CMAKE_CURRENT_SOURCE_DIR}/res/controls/mash_prompt/Reeot_MashPrompt_Switch_D.dds"
            "${CMAKE_CURRENT_SOURCE_DIR}/res/controls/mash_prompt/Reeot_MashPrompt_Keyboard_D.dds"
    COMMENT "Authoring ReeotIcons.pkz from pkzlib/ReeotIcons.manifest"
    VERBATIM)
file(GLOB REEOT_SUITS_DDS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/res/suits/*/*.dds")
set(REEOT_SUITS_PKZ_OUTPUT "${CMAKE_BINARY_DIR}/pkz/ReeotSuits.pkz")
add_custom_command(
    OUTPUT "${REEOT_SUITS_PKZ_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/pkz"
    COMMAND pkztool make "${REEOT_PKZ_DATA}/ReeotSuits.manifest" "${REEOT_SUITS_PKZ_OUTPUT}"
            --paks "${REEOT_REFERENCE_PAKS}"
    DEPENDS pkztool
            "${REEOT_PKZ_DATA}/ReeotSuits.manifest"
            ${REEOT_SUITS_DDS}
    COMMENT "Authoring ReeotSuits.pkz from pkzlib/ReeotSuits.manifest"
    VERBATIM)
add_custom_target(reeot_pkz ALL DEPENDS "${REEOT_PKZ_OUTPUT}" "${REEOT_MENU_PKZ_OUTPUT}" "${REEOT_ACH_PKZ_OUTPUT}"
                                        "${REEOT_ICONS_PKZ_OUTPUT}" "${REEOT_SUITS_PKZ_OUTPUT}")
reeot_embed_generated(
    INPUT       "${REEOT_PKZ_OUTPUT}"
    NAME        "pkz/ReeotUI.pkz"
    ACCESSOR    EmbeddedPortPackage
    HEADER      "${REEOT_GEN_DIR}/embedded_package.h"
    SOURCES_VAR REEOT_EMBED_PACKAGE_SOURCE)
reeot_embed_generated(
    INPUT       "${REEOT_MENU_PKZ_OUTPUT}"
    NAME        "pkz/ReeotMenu.pkz"
    ACCESSOR    EmbeddedMenuPackage
    HEADER      "${REEOT_GEN_DIR}/embedded_menu_package.h"
    SOURCES_VAR REEOT_EMBED_MENU_PACKAGE_SOURCE)
list(APPEND REEOT_EMBED_PACKAGE_SOURCE ${REEOT_EMBED_MENU_PACKAGE_SOURCE})
reeot_embed_generated(
    INPUT       "${REEOT_ACH_PKZ_OUTPUT}"
    NAME        "pkz/ReeotAchievements.pkz"
    ACCESSOR    EmbeddedAchievementsPackage
    HEADER      "${REEOT_GEN_DIR}/embedded_achievements_package.h"
    SOURCES_VAR REEOT_EMBED_ACH_PACKAGE_SOURCE)
list(APPEND REEOT_EMBED_PACKAGE_SOURCE ${REEOT_EMBED_ACH_PACKAGE_SOURCE})
reeot_embed_generated(
    INPUT       "${REEOT_ICONS_PKZ_OUTPUT}"
    NAME        "pkz/ReeotIcons.pkz"
    ACCESSOR    EmbeddedIconsPackage
    HEADER      "${REEOT_GEN_DIR}/embedded_icons_package.h"
    SOURCES_VAR REEOT_EMBED_ICONS_PACKAGE_SOURCE)
list(APPEND REEOT_EMBED_PACKAGE_SOURCE ${REEOT_EMBED_ICONS_PACKAGE_SOURCE})
reeot_embed_generated(
    INPUT       "${REEOT_SUITS_PKZ_OUTPUT}"
    NAME        "pkz/ReeotSuits.pkz"
    ACCESSOR    EmbeddedSuitsPackage
    HEADER      "${REEOT_GEN_DIR}/embedded_suits_package.h"
    SOURCES_VAR REEOT_EMBED_SUITS_PACKAGE_SOURCE)
list(APPEND REEOT_EMBED_PACKAGE_SOURCE ${REEOT_EMBED_SUITS_PACKAGE_SOURCE})
include(cmake/host_shaders.cmake)

add_custom_target(reeot_shader_cache_regen
    COMMAND $<TARGET_FILE:XenosRecomp>
            "${REEOT_SHADER_INPUT_DIR}" "${REEOT_SHADER_CACHE_CPP}" "${REEOT_SHADER_COMMON_H}"
    DEPENDS XenosRecomp
    COMMENT "Recompiling EOT Xenos shaders into ${REEOT_SHADER_CACHE_CPP}"
    USES_TERMINAL VERBATIM)

if(EXISTS "${REEOT_SHADER_CACHE_CPP}")
    add_library(reeot_shader_cache OBJECT "${REEOT_SHADER_CACHE_CPP}")
elseif(IS_DIRECTORY "${REEOT_SHADER_INPUT_DIR}")
    add_custom_command(
        OUTPUT "${REEOT_SHADER_CACHE_CPP}"
        COMMAND $<TARGET_FILE:XenosRecomp>
                "${REEOT_SHADER_INPUT_DIR}" "${REEOT_SHADER_CACHE_CPP}" "${REEOT_SHADER_COMMON_H}"
        DEPENDS XenosRecomp "${REEOT_SHADER_COMMON_H}"
        COMMENT "Recompiling the game's shaders into ${REEOT_SHADER_CACHE_CPP}"
        USES_TERMINAL VERBATIM)
    add_library(reeot_shader_cache OBJECT "${REEOT_SHADER_CACHE_CPP}")
else()
    message(WARNING
        "${REEOT_SHADER_CACHE_CPP} is missing and ${REEOT_SHADER_INPUT_DIR} holds no shaders: "
        "the game will draw nothing.")
    add_library(reeot_shader_cache OBJECT src/gpu/shaders/shader_cache_empty.cpp)
endif()
target_include_directories(reeot_shader_cache PRIVATE src/gpu/shaders)
target_compile_options(reeot_shader_cache PRIVATE -O0 -w)

# Sources
set(REEOT_SOURCES
    src/main.cpp
    src/reeot_app.cpp
    src/platform/file_dialog.cpp
    src/platform/desktop_shortcut.cpp
    src/platform/process.cpp
    src/platform/user_dirs.cpp
    src/platform/display.cpp
    src/platform/fatal_dialog.cpp
    src/platform/crash_handler.cpp
    src/ui/theme.cpp
    src/ui/watermark.cpp
    src/platform/update_check.cpp
    src/installer/install_registry.cpp
    src/installer/self_install.cpp
    src/installer/dlc_publish.cpp
    src/mods/mod_manager.cpp
    ${REEOT_EMBED_SOURCES}
    ${REEOT_EMBED_PACKAGE_SOURCE}
    src/core/critical_section.cpp
    src/core/quit.cpp
    src/gpu/settings.cpp
    src/gpu/device.cpp
    src/gpu/frame_ring.cpp
    src/gpu/present.cpp
    src/gpu/imgui_overlay.cpp
    src/gpu/format.cpp
    src/gpu/textures.cpp
    src/gpu/surfaces.cpp
    src/gpu/vertex_layout.cpp
    src/gpu/draw.cpp
    src/gpu/render_thread.cpp
    src/gpu/resolve.cpp
    src/gpu/trace.cpp
    src/gpu/pipeline/pipeline_cache.cpp
    src/gpu/gpu_timing.cpp
    src/gpu/memory_report.cpp
    src/gpu/pipeline/pso_records.cpp
    src/gpu/pipeline/pso_precache.cpp
    src/gpu/pipeline/pso_predictor.cpp
    src/gpu/hooks/package_load.cpp
    src/gpu/patches/update_rate.cpp
    src/gpu/patches/aspect_ratio.cpp
    src/gpu/patches/post_effects.cpp
    src/goliath/controller/pad_remap.cpp
    src/goliath/controller/bind_capture.cpp
    src/goliath/ui/achievement_feed.cpp
    src/goliath/controller/button_glyphs.cpp
    src/goliath/controller/mouse_input.cpp
    src/goliath/debug/freecam.cpp
    src/goliath/ui/aspect_policy.cpp
    src/goliath/ui/overlays/fps.cpp
    src/goliath/loading/reeot_package.cpp
    src/goliath/loading/texture_overrides.cpp
    src/goliath/web_color.cpp
    src/goliath/text/glyph_pages.cpp
    src/gpu/taa.cpp
    src/gpu/shaders/guest_shaders.cpp
    src/gpu/hooks/stubs.cpp
    src/gpu/hooks/state.cpp
    src/gpu/hooks/resource.cpp
)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(REEOT_HUD_POLICY_INC "${REEOT_GEN_DIR}/hud_aspect_policy.inc")
add_custom_command(
    OUTPUT "${REEOT_HUD_POLICY_INC}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${REEOT_GEN_DIR}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/hud/gen_hud_policy.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/config/eot_hud_aspect.toml"
            "${REEOT_HUD_POLICY_INC}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/config/eot_hud_aspect.toml"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/hud/gen_hud_policy.py"
    COMMENT "Compiling the HUD aspect policy"
    VERBATIM)
list(APPEND REEOT_SOURCES "${REEOT_HUD_POLICY_INC}")

if(NOT REEOT_D3D12)
    list(APPEND REEOT_SOURCES thirdparty/XenosRecomp/thirdparty/smol-v/source/smolv.cpp)
endif()

if(APPLE AND NOT REEOT_D3D12)
    list(APPEND REEOT_SOURCES src/platform/moltenvk.cpp)
endif()

if(WIN32 OR APPLE)
    list(APPEND REEOT_SOURCES src/core/sleep.cpp)
endif()
if(WIN32)
    enable_language(RC)
    set(REEOT_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/res/app/icon/reeot.ico")
    configure_file(res/app/windows/reeot.rc.in "${REEOT_GEN_DIR}/reeot.rc" @ONLY)
    add_executable(reeot WIN32 ${REEOT_SOURCES} "${REEOT_GEN_DIR}/reeot.rc")
else()
    add_executable(reeot ${REEOT_SOURCES})
endif()
set_target_properties(reeot PROPERTIES OUTPUT_NAME EdgeOfTimeRecomp)

rexglue_setup_target(reeot)

if(TARGET reeot_GameLogic)
    target_sources(reeot_GameLogic PRIVATE
        src/gamelogic/debug/debug_mode.cpp
        src/gamelogic/patches/game_patches.cpp
        src/gamelogic/ui/options_pages.cpp
        src/gamelogic/ui/achievements_page.cpp
        src/gamelogic/input/input.cpp)
    target_include_directories(reeot_GameLogic PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(reeot_GameLogic PRIVATE
        $<$<BOOL:${REEOT_D3D12}>:EOT_D3D12>)
    # Named as the SDK opens it, so the copy preloaded beside the executable answers its dlopen.
    if(NOT WIN32)
        set_target_properties(reeot_GameLogic PROPERTIES PREFIX "" SUFFIX "")
    endif()
endif()

# volk opens the Vulkan loader and MoltenVK by their plain names.
if(APPLE)
    foreach(vulkan_target vulkan MoltenVK)
        if(TARGET ${vulkan_target})
            set_target_properties(${vulkan_target} PROPERTIES DEBUG_POSTFIX "" RELWITHDEBINFO_POSTFIX "")
        endif()
    endforeach()
endif()

target_include_directories(reeot PRIVATE
    src
    "${REEOT_GEN_DIR}"
    thirdparty/stb
    "${REEOT_SHADER_COMMON_DIR}"
    thirdparty/renderdoc
    $<$<NOT:$<BOOL:${REEOT_D3D12}>>:${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/XenosRecomp/thirdparty/smol-v/source>
    "${REXGLUE_ENTRYPOINT_INCLUDE_DIR}")

if(REEOT_BUILD_INSTALLER)
    target_sources(reeot PRIVATE
        src/installer/disc_install.cpp
        src/installer/extracted_disc.cpp
        src/installer/installer_music.cpp
        src/installer/installer_wizard.cpp)
    if(APPLE)
        target_sources(reeot PRIVATE src/installer/installer_music_mac.mm)
        target_link_libraries(reeot PRIVATE "-framework AVFoundation")
    endif()
endif()

set(reeot_program_files
    "$<TARGET_FILE_NAME:reeot>"
    "$<TARGET_FILE_NAME:rex::runtime>"
    "gamecontrollerdb.txt")
if(NOT WIN32)
    list(APPEND reeot_program_files "build_stamp.txt" "reeot_icon.png")
endif()
if(APPLE AND NOT REEOT_D3D12)
    list(APPEND reeot_program_files
        "vulkan/lib/libvulkan.1.dylib"
        "vulkan/lib/libMoltenVK.dylib"
        "vulkan/share/vulkan/icd.d/MoltenVK_icd.json")
endif()
if(TARGET reeot_GameLogic)
    list(APPEND reeot_program_files "$<TARGET_FILE_NAME:reeot_GameLogic>")
endif()
if(REEOT_D3D12)
    list(APPEND reeot_program_files dxcompiler.dll dxil.dll)
endif()
set(reeot_tracy_target "")
if(TARGET rex::TracyClient)
    set(reeot_tracy_target rex::TracyClient)
elseif(TARGET TracyClient)
    set(reeot_tracy_target TracyClient)
endif()
set(reeot_tracy_entry "")
set(reeot_tracy_line "")
set(reeot_tracy_file "")
if(reeot_tracy_target)
    set(reeot_tracy_entry "$<${REEOT_PROFILING_ON}:    \"$<TARGET_FILE_NAME:${reeot_tracy_target}>\",\n>")
    set(reeot_tracy_line "$<${REEOT_PROFILING_ON}:$<TARGET_FILE_NAME:${reeot_tracy_target}>\n>")
    set(reeot_tracy_file "$<${REEOT_PROFILING_ON}:$<TARGET_FILE:${reeot_tracy_target}>>")
endif()
set(reeot_program_files_cxx "")
foreach(f IN LISTS reeot_program_files)
    string(APPEND reeot_program_files_cxx "    \"${f}\",\n")
endforeach()
string(APPEND reeot_program_files_cxx "${reeot_tracy_entry}")
file(GENERATE OUTPUT "${REEOT_GEN_DIR}/installer/program_files.h" CONTENT
"// Generated by CMakeLists.txt - do not edit.
#pragma once

inline constexpr const char *kProgramFiles[] = {
${reeot_program_files_cxx}
};
")
list(JOIN reeot_program_files "\n" reeot_program_files_txt)
file(GENERATE OUTPUT "$<TARGET_FILE_DIR:reeot>/program_files.txt"
     CONTENT "${reeot_program_files_txt}\n${reeot_tracy_line}")

add_custom_command(TARGET reeot POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/gamecontrollerdb/gamecontrollerdb.txt"
        $<TARGET_FILE_DIR:reeot>
    VERBATIM)

if(UNIX)
    set(reeot_unix_runtime_libs "$<TARGET_FILE:rex::runtime>")
    if(reeot_tracy_file)
        list(APPEND reeot_unix_runtime_libs "${reeot_tracy_file}")
    endif()
    add_custom_command(TARGET reeot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${reeot_unix_runtime_libs}
            "${CMAKE_CURRENT_SOURCE_DIR}/res/app/icon/reeot_icon.png"
            $<TARGET_FILE_DIR:reeot>
        COMMAND_EXPAND_LISTS
        VERBATIM)
    file(GENERATE OUTPUT "$<TARGET_FILE_DIR:reeot>/build_stamp.txt"
         CONTENT "${REEOT_BUILD_TIMESTAMP}\n")
    set_target_properties(reeot PROPERTIES ENABLE_EXPORTS ON)
    find_package(Threads REQUIRED)
    target_link_libraries(reeot PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()

target_compile_definitions(reeot PRIVATE
    _ITERATOR_DEBUG_LEVEL=0
    $<$<BOOL:${REEOT_BUILD_INSTALLER}>:REEOT_BUILD_INSTALLER>
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    $<$<BOOL:${REEOT_D3D12}>:EOT_D3D12>
    $<$<AND:$<BOOL:${APPLE}>,$<NOT:$<BOOL:${REEOT_D3D12}>>>:EOT_MVK>
    $<${REEOT_PROFILING_ON}:EOT_PROFILING>)

# A source tree keeps these headers in its thirdparty folder.
if(REXSDK_DIR)
    target_include_directories(reeot PRIVATE
        "${REXSDK_DIR}/thirdparty/imgui"
        "${REXSDK_DIR}/thirdparty/tomlplusplus/include")
endif()
target_link_libraries(reeot PRIVATE
    plume
    reeot_shader_cache
    xxHash::xxhash
    libzstd_static)

if(WIN32)
    target_link_libraries(reeot PRIVATE winmm shell32 ole32 advapi32 version)
endif()

foreach(shader IN ITEMS copy_vs imgui_vs)
    reeot_host_shader(reeot ${shader} vs_6_0)
endforeach()
foreach(shader IN ITEMS blit_ps copy_depth_ps imgui_ps taa_ps
        velocity_resolve_2x_ps velocity_resolve_4x_ps velocity_resolve_8x_ps
        resolve_msaa_color_2x_ps resolve_msaa_color_4x_ps resolve_msaa_color_8x_ps
        resolve_msaa_depth_2x_ps resolve_msaa_depth_4x_ps resolve_msaa_depth_8x_ps
        derive_depth_stencil_2x_ps
        derive_depth_stencil_4x_ps
        derive_depth_stencil_8x_ps)
    reeot_host_shader(reeot ${shader} ps_6_0)
endforeach()

if(REEOT_D3D12)
    target_link_libraries(reeot PRIVATE d3d12 dxgi Microsoft::DirectXShaderCompiler)
    add_custom_command(TARGET reeot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxil.dll"
            "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxcompiler.dll"
            $<TARGET_FILE_DIR:reeot>
        VERBATIM)
elseif(APPLE)
    target_link_libraries(reeot PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework Metal"
        "-framework Foundation"
        "-framework CoreFoundation"
        "-framework IOKit"
        "-framework IOSurface"
        "-framework CoreGraphics")
else()
    if(REXSDK_DIR AND NOT Vulkan_INCLUDE_DIR
       AND EXISTS "${REXSDK_DIR}/thirdparty/vulkan-headers/include/vulkan/vulkan.h")
        set(Vulkan_INCLUDE_DIR "${REXSDK_DIR}/thirdparty/vulkan-headers/include" CACHE PATH
            "Vulkan headers (the SDK tree's vendored copy)" FORCE)
    endif()
    find_package(Vulkan REQUIRED)
    target_link_libraries(reeot PRIVATE Vulkan::Vulkan)
endif()
