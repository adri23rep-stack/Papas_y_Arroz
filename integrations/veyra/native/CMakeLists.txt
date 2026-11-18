# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(NovaVeyraNative LANGUAGES C CXX RC)

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "The real Veyra GPU engine requires MSVC 2022 + Windows D3D12.")
endif()
set(VEYRA_SOURCE_ROOT "" CACHE PATH "Exact Veyra 2.1.9 GPLv3 source directory")
if(NOT EXISTS "${VEYRA_SOURCE_ROOT}/CMakeLists.txt" OR
   NOT EXISTS "${VEYRA_SOURCE_ROOT}/src/engine/EngineController.cpp")
  message(FATAL_ERROR "Specify -DVEYRA_SOURCE_ROOT=<Veyra 2.1.9 source folder>")
endif()
# Do not build or run the external QML/Win32 Veyra UI.
# We compile the original GPU library into Nova's own native DLL.
set(VEYRA_BUILD_QML_UI OFF CACHE BOOL "" FORCE)
set(VEYRA_ENABLE_D3D12_DEBUG OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

# Never claim the full NVIDIA engine was built if GPU providers were disabled.
if(NOT VEYRA_ENABLE_DLSS_SR OR NOT VEYRA_ENABLE_DLSS_FG OR
   NOT VEYRA_ENABLE_NVOF OR NOT VEYRA_ENABLE_EXPERIMENTAL_DLSSNR)
  message(FATAL_ERROR "Full NVIDIA build requires DLSS_SR, DLSS_FG, NVOF and DLSSNR enabled.")
endif()
add_subdirectory("${VEYRA_SOURCE_ROOT}" "${CMAKE_BINARY_DIR}/veyra-original")
if(NOT TARGET veyra_engine)
  message(FATAL_ERROR "Real Veyra engine target not configured. Check FFmpeg, NGX and NVOF prerequisites.")
endif()
add_library(nova_veyra_native SHARED NovaVeyraNative.cpp NovaVeyraApi.h)
target_compile_features(nova_veyra_native PRIVATE cxx_std_20)
target_include_directories(nova_veyra_native PRIVATE "${VEYRA_SOURCE_ROOT}/include")
target_compile_definitions(nova_veyra_native PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(nova_veyra_native PRIVATE /utf-8)
# The original NGX/veyra_engine static libraries use /MT. Using /MD here
# crosses the CRT ownership boundary inside EngineController and can corrupt
# std::string, vectors and allocation lifetimes.
set_property(TARGET nova_veyra_native PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(nova_veyra_native PRIVATE veyra_engine)
set_target_properties(nova_veyra_native PROPERTIES OUTPUT_NAME "nova_veyra_native")
# Original Veyra computes shader file paths against CMAKE_BINARY_DIR.
# But because we add it as a subdirectory, its shader tool emits DXIL in
# CMAKE_CURRENT_BINARY_DIR = ${CMAKE_BINARY_DIR}/veyra-original/shaders.
# Without this copy a correctly linked DLL still cannot find its shaders.
add_dependencies(nova_veyra_native veyra_shader_scale_blit veyra_shader_present_blit)
add_custom_command(TARGET nova_veyra_native POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/shaders"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${CMAKE_BINARY_DIR}/veyra-original/shaders"
      "${CMAKE_BINARY_DIR}/shaders"
  COMMENT "Stage Veyra original GPU shaders to path compiled into EngineController"
  VERBATIM)
# A separate real-hardware check: unlike the previous fake-engine fixture,
# this executable imports our Veyra MSVC DLL and calls its REAL native engine.
# It only reports PASS after 10 actual frames, or verified VFG 2x frames.
add_executable(nova_veyra_native_smoke NovaVeyraNativeSmoke.cpp)
target_compile_features(nova_veyra_native_smoke PRIVATE cxx_std_20)
target_include_directories(nova_veyra_native_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(nova_veyra_native_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(nova_veyra_native_smoke PRIVATE /utf-8)
set_property(TARGET nova_veyra_native_smoke PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(nova_veyra_native_smoke PRIVATE nova_veyra_native)
# The original shaders, FFmpeg and NVIDIA runtime DLLs must also be present.
# A successful link is NOT proof the RTX engine will sustain fps.
