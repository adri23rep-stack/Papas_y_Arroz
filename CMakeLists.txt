cmake_minimum_required(VERSION 3.20)
project(XuperTVDesktop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2 SDL2main)
pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_library(MBEDCRYPTO_LIBRARY NAMES mbedcrypto REQUIRED)

# The MSYS2 UCRT64 pkg-config files expose POSIX paths (e.g. /ucrt64/include).
# Native MinGW CMake needs their Windows equivalents (D:/.../ucrt64/include).
if(WIN32 AND MINGW)
  foreach(_pkg_target IN ITEMS PkgConfig::SDL2_TTF PkgConfig::MPV PkgConfig::CURL PkgConfig::WEBP)
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

  # Test cases use assert() for both checks and setup/side effects.
  # Release adds -DNDEBUG, which otherwise skips their setup and crashes
  # when fixtures are renamed before being created.
  foreach(_test_target IN ITEMS catalog_test playback_state_test playback_error_test saved_catalog_test channel_rail_test)
    if(MSVC)
      target_compile_options(${_test_target} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${_test_target} PRIVATE -UNDEBUG)
    endif()
  endforeach()
endif()
