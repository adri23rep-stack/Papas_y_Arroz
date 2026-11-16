cmake_minimum_required(VERSION 3.20)
project(XuperTVDesktop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2 SDL2main)
pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
if(WIN32)
  # Native engine pilot: link FFmpeg directly, rather than depending on mpv's
  # audio/video playback and intermediate OpenGL-to-SDL image copies.
  pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
  pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
  pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
  pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)
  pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)
  pkg_check_modules(QRENCODE REQUIRED IMPORTED_TARGET libqrencode)
endif()
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_library(MBEDCRYPTO_LIBRARY NAMES mbedcrypto REQUIRED)

# The MSYS2 UCRT64 pkg-config files expose POSIX paths (e.g. /ucrt64/include).
# Native MinGW CMake needs their Windows equivalents (D:/.../ucrt64/include).
if(WIN32 AND MINGW)
  foreach(_pkg_target IN ITEMS PkgConfig::SDL2_TTF PkgConfig::MPV PkgConfig::CURL PkgConfig::WEBP PkgConfig::AVFORMAT PkgConfig::AVCODEC PkgConfig::AVUTIL PkgConfig::SWSCALE PkgConfig::SWRESAMPLE PkgConfig::QRENCODE)
    get_target_property(_inc_dirs "${_pkg_target}" INTERFACE_INCLUDE_DIRECTORIES)
    if(_inc_dirs)
      set(_fixed_dirs "")
      foreach(_inc_dir IN LISTS _inc_dirs)
        if(_inc_dir MATCHES "^/")
          execute_process(
            COMMAND cygpath -m "${_inc_dir}"
            OUTPUT_VARIABLE _win_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _convert_status)
          if(NOT _convert_status EQUAL 0)
            message(FATAL_ERROR "Cannot convert pkg-config path: ${_inc_dir}")
          endif()
          list(APPEND _fixed_dirs "${_win_dir}")
        else()
          list(APPEND _fixed_dirs "${_inc_dir}")
        endif()
      endforeach()
      set_property(TARGET "${_pkg_target}" PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${_fixed_dirs}")
    endif()
  endforeach()
endif()

# WIN32 requests the GUI subsystem. Link SDL2main before SDL2, per SDL2 docs.
if(WIN32)
  add_executable(XuperTVDesktop WIN32)
else()
  add_executable(XuperTVDesktop)
endif()

target_sources(XuperTVDesktop PRIVATE
  source/main.cpp
  source/catalog.cpp
  source/player.cpp
  source/view.cpp
  source/trace.cpp
  source/codec.cpp
  source/ranger_codec.cpp
  source/service.cpp
  source/artwork.cpp
)
# Muted secondary decoder is a Windows-only experimental UI feature.
if(WIN32)
  target_sources(XuperTVDesktop PRIVATE source/live_preview.cpp source/remote_server.cpp source/view_cinematic.cpp source/view_nova.cpp source/remote_catalog.cpp source/youtube_remote.cpp source/gpu_readback.cpp source/remote_browse.cpp)
  target_link_libraries(XuperTVDesktop PRIVATE ws2_32 bcrypt shell32 user32 opengl32 crypt32 ole32 PkgConfig::QRENCODE)
  # The native engine currently builds as an independent decoder pilot.
  # It cannot accidentally replace, break, or steal XuperTV's working playback.
  add_executable(XuperNativeProbe source/native_engine.cpp tests/native_engine_probe.cpp)
  target_include_directories(XuperNativeProbe PRIVATE include)
  target_link_libraries(XuperNativeProbe PRIVATE
    PkgConfig::AVFORMAT PkgConfig::AVCODEC PkgConfig::AVUTIL d3d11 dxgi)

  # New native video/audio player; fully separate from XuperTV's working UI.
  # SDL2's Windows D3D11 backend presents the video, FFmpeg decodes it.
  add_executable(XuperNativePlayer source/native_player.cpp)
  target_include_directories(XuperNativePlayer PRIVATE include)
  target_link_libraries(XuperNativePlayer PRIVATE
    SDL2::SDL2 PkgConfig::SDL2_TTF
    PkgConfig::AVFORMAT PkgConfig::AVCODEC PkgConfig::AVUTIL
    PkgConfig::SWSCALE PkgConfig::SWRESAMPLE
    d3d11 dxgi comdlg32)
endif()
target_include_directories(XuperTVDesktop PRIVATE include include/vendor)
target_link_libraries(XuperTVDesktop PRIVATE
  $<$<TARGET_EXISTS:SDL2::SDL2main>:SDL2::SDL2main>
  SDL2::SDL2
  PkgConfig::SDL2_TTF
  PkgConfig::MPV
  PkgConfig::CURL
  PkgConfig::WEBP
  PNG::PNG
  JPEG::JPEG
  ${MBEDCRYPTO_LIBRARY}
)
if(MSVC)
  target_compile_options(XuperTVDesktop PRIVATE /W3)
else()
  target_compile_options(XuperTVDesktop PRIVATE -Wall -Wextra)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(catalog_test source/catalog.cpp tests/catalog_test.cpp)
  target_include_directories(catalog_test PRIVATE include include/vendor)
  add_test(NAME catalog_test COMMAND catalog_test)
  add_executable(playback_state_test tests/playback_state_test.cpp)
  target_include_directories(playback_state_test PRIVATE include include/vendor)
  add_test(NAME playback_state_test COMMAND playback_state_test)
  add_executable(playback_error_test tests/playback_error_test.cpp)
  target_include_directories(playback_error_test PRIVATE include include/vendor)
  add_test(NAME playback_error_test COMMAND playback_error_test)
  add_executable(saved_catalog_test source/catalog.cpp tests/saved_catalog_test.cpp)
  target_include_directories(saved_catalog_test PRIVATE include include/vendor)
  add_test(NAME saved_catalog_test COMMAND saved_catalog_test)
  add_executable(channel_rail_test tests/channel_rail_test.cpp)
  target_include_directories(channel_rail_test PRIVATE include)
  add_test(NAME channel_rail_test COMMAND channel_rail_test)
  add_executable(cinematic_layout_test tests/cinematic_layout_test.cpp)
  target_include_directories(cinematic_layout_test PRIVATE include)
  add_test(NAME cinematic_layout_test COMMAND cinematic_layout_test)
  add_executable(cinematic_home_test tests/cinematic_home_test.cpp)
  target_include_directories(cinematic_home_test PRIVATE include)
  add_test(NAME cinematic_home_test COMMAND cinematic_home_test)
  add_executable(nova_layout_test tests/nova_layout_test.cpp)
  target_include_directories(nova_layout_test PRIVATE include)
  add_test(NAME nova_layout_test COMMAND nova_layout_test)
  if(WIN32)
    add_executable(native_subtitles_test tests/native_subtitles_test.cpp)
    target_include_directories(native_subtitles_test PRIVATE include)
    add_test(NAME native_subtitles_test COMMAND native_subtitles_test)
    if(MSVC)
      target_compile_options(native_subtitles_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(native_subtitles_test PRIVATE -UNDEBUG)
    endif()
    add_executable(native_player_utils_test tests/native_playback_utils_test.cpp)
    target_include_directories(native_player_utils_test PRIVATE include)
    add_test(NAME native_player_utils_test COMMAND native_player_utils_test)
    if(MSVC)
      target_compile_options(native_player_utils_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(native_player_utils_test PRIVATE -UNDEBUG)
    endif()
    add_test(NAME native_player_sdl_smoke
      COMMAND XuperNativePlayer
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/runtime/switch/xuperswitch/test.mp4"
        --cpu --smoke 12)
    set_tests_properties(native_player_sdl_smoke PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software")
    add_test(NAME native_engine_api_test COMMAND XuperNativeProbe --self-test)
    add_test(NAME native_engine_file_decode_test
      COMMAND XuperNativeProbe "${CMAKE_CURRENT_SOURCE_DIR}/packaging/runtime/switch/xuperswitch/test.mp4" 16)
    add_test(NAME native_engine_file_cpu_test
      COMMAND XuperNativeProbe "${CMAKE_CURRENT_SOURCE_DIR}/packaging/runtime/switch/xuperswitch/test.mp4" 16 cpu)
    add_executable(sdl_startup_smoke tests/sdl_startup_smoke.cpp)
    target_link_libraries(sdl_startup_smoke PRIVATE SDL2::SDL2)
    add_test(NAME sdl_startup_smoke COMMAND sdl_startup_smoke)
    add_executable(remote_server_test tests/remote_server_test.cpp source/remote_server.cpp source/remote_catalog.cpp source/remote_browse.cpp source/youtube_remote.cpp source/catalog.cpp)
    add_executable(provider_catalog_test tests/provider_catalog_test.cpp)
    target_include_directories(provider_catalog_test PRIVATE include)
    add_test(NAME provider_catalog_test COMMAND provider_catalog_test)
    if(MSVC)
      target_compile_options(provider_catalog_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(provider_catalog_test PRIVATE -UNDEBUG)
    endif()
    add_executable(live_tv_recovery_test tests/live_tv_recovery_test.cpp)
    target_include_directories(live_tv_recovery_test PRIVATE include)
    add_test(NAME live_tv_recovery_test COMMAND live_tv_recovery_test)
    if(MSVC)
      target_compile_options(live_tv_recovery_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(live_tv_recovery_test PRIVATE -UNDEBUG)
    endif()
    add_executable(remote_browse_test tests/remote_browse_test.cpp source/remote_browse.cpp)
    target_include_directories(remote_browse_test PRIVATE include)
    target_link_libraries(remote_browse_test PRIVATE SDL2::SDL2)
    add_test(NAME remote_browse_test COMMAND remote_browse_test)
    if(MSVC)
      target_compile_options(remote_browse_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(remote_browse_test PRIVATE -UNDEBUG)
    endif()
    add_executable(remote_settings_test tests/remote_settings_test.cpp)
    target_include_directories(remote_settings_test PRIVATE include)
    add_test(NAME remote_settings_test COMMAND remote_settings_test)
    if(MSVC)
      target_compile_options(remote_settings_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(remote_settings_test PRIVATE -UNDEBUG)
    endif()
    add_executable(remote_pairing_qr_test tests/remote_pairing_qr_test.cpp)
    target_include_directories(remote_pairing_qr_test PRIVATE include)
    target_link_libraries(remote_pairing_qr_test PRIVATE SDL2::SDL2 PkgConfig::QRENCODE)
    add_test(NAME remote_pairing_qr_test COMMAND remote_pairing_qr_test)
    add_executable(remote_pairing_store_test tests/remote_pairing_store_test.cpp)
    target_include_directories(remote_pairing_store_test PRIVATE include)
    target_link_libraries(remote_pairing_store_test PRIVATE crypt32 shell32 ole32)
    add_test(NAME remote_pairing_store_test COMMAND remote_pairing_store_test)
    if(MSVC)
      target_compile_options(remote_pairing_qr_test PRIVATE /UNDEBUG)
      target_compile_options(remote_pairing_store_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(remote_pairing_qr_test PRIVATE -UNDEBUG)
      target_compile_options(remote_pairing_store_test PRIVATE -UNDEBUG)
    endif()
    target_include_directories(remote_server_test PRIVATE include include/vendor)
    target_link_libraries(remote_server_test PRIVATE SDL2::SDL2 ws2_32 bcrypt shell32 user32 crypt32 ole32)
    add_test(NAME remote_server_test COMMAND remote_server_test)
    add_executable(video_enhanced_test tests/video_enhanced_test.cpp)
    target_include_directories(video_enhanced_test PRIVATE include)
    add_test(NAME video_enhanced_test COMMAND video_enhanced_test)
    if(MSVC)
      target_compile_options(video_enhanced_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(video_enhanced_test PRIVATE -UNDEBUG)
    endif()
        add_executable(youtube_remote_test tests/youtube_remote_test.cpp source/youtube_remote.cpp)
    target_include_directories(youtube_remote_test PRIVATE include)
    target_link_libraries(youtube_remote_test PRIVATE shell32 user32)
    add_test(NAME youtube_remote_test COMMAND youtube_remote_test)
    if(MSVC)
      target_compile_options(youtube_remote_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(youtube_remote_test PRIVATE -UNDEBUG)
    endif()
    if(MSVC)
      target_compile_options(remote_server_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(remote_server_test PRIVATE -UNDEBUG)
    endif()
  endif()

  # Test cases use assert() for both checks and setup/side effects.
  # Release adds -DNDEBUG, which otherwise skips their setup and crashes
  # when fixtures are renamed before being created.
  foreach(_test_target IN ITEMS catalog_test playback_state_test playback_error_test saved_catalog_test channel_rail_test cinematic_layout_test cinematic_home_test nova_layout_test)
    if(MSVC)
      target_compile_options(${_test_target} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${_test_target} PRIVATE -UNDEBUG)
    endif()
  endforeach()
endif()
