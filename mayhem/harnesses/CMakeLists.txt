# Wrapper project for the cesium-unreal fuzz layer: adds the pinned cesium-native
# (extern/cesium-native, fetched into /opt/toolchains/deps by mayhem/Dockerfile) as a
# subdirectory, resolving its dependencies from the in-image vcpkg tree, and builds
# either the fuzz harnesses (MAYHEM_FUZZ=ON) or cesium-native's own doctest suite.
cmake_minimum_required(VERSION 3.24)

set(CESIUM_USE_EZVCPKG OFF CACHE BOOL "")
set(CESIUM_DISABLE_CURL ON CACHE BOOL "")
set(CESIUM_ENABLE_CLANG_TIDY OFF CACHE BOOL "")
set(CESIUM_INSTALL_STATIC_LIBS OFF CACHE BOOL "")
set(CESIUM_INSTALL_HEADERS OFF CACHE BOOL "")
set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "")

project(cesium_unreal_fuzz C CXX)

option(MAYHEM_FUZZ "Build the fuzz harnesses (else only cesium-native's tests)" ON)
set(CESIUM_NATIVE_SRC "" CACHE PATH "cesium-native source tree")
set(MAYHEM_OUT "${CMAKE_BINARY_DIR}/out" CACHE PATH "where fuzz binaries are placed")

add_subdirectory("${CESIUM_NATIVE_SRC}" cesium-native)

# cesium-native's tests locate data/ via CMAKE_SOURCE_DIR, which is this wrapper here;
# point it back at cesium-native's own data/ directory.
if(TARGET cesium-native-tests)
  get_target_property(test_defs cesium-native-tests COMPILE_DEFINITIONS)
  string(REPLACE "${CMAKE_SOURCE_DIR}/data" "${CESIUM_NATIVE_SRC}/data" test_defs "${test_defs}")
  set_target_properties(cesium-native-tests PROPERTIES COMPILE_DEFINITIONS "${test_defs}")
endif()

if(MAYHEM_FUZZ)
  # mayhem/lsan_off.cc: __lsan_is_turned_off() linked into every fuzz and -standalone binary.
  set(MAYHEM_LSAN_OFF "${CMAKE_CURRENT_LIST_DIR}/../lsan_off.cc" CACHE FILEPATH "")
  add_library(mayhem_lsan_off OBJECT "${MAYHEM_LSAN_OFF}")
  add_library(mayhem_standalone_main OBJECT "$ENV{STANDALONE_FUZZ_MAIN}")
  set_source_files_properties("$ENV{STANDALONE_FUZZ_MAIN}" PROPERTIES LANGUAGE C)

  function(mayhem_fuzzer name)
    foreach(kind fuzz standalone)
      if(kind STREQUAL "fuzz")
        set(exe ${name})
      else()
        set(exe ${name}-standalone)
      endif()
      add_executable(${exe} "${CMAKE_CURRENT_LIST_DIR}/${name}.cpp" $<TARGET_OBJECTS:mayhem_lsan_off>)
      set_target_properties(${exe} PROPERTIES
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES
        RUNTIME_OUTPUT_DIRECTORY "${MAYHEM_OUT}")
      target_link_libraries(${exe} PRIVATE ${ARGN} CesiumAsync CesiumUtility)
      if(kind STREQUAL "fuzz")
        target_link_options(${exe} PRIVATE -fsanitize=fuzzer)
      else()
        target_sources(${exe} PRIVATE $<TARGET_OBJECTS:mayhem_standalone_main>)
      endif()
    endforeach()
  endfunction()

  mayhem_fuzzer(fuzz_gltf_reader CesiumGltfReader CesiumGltf)
  mayhem_fuzzer(fuzz_gltf_utilities CesiumGltfContent CesiumGltfReader CesiumGltfWriter CesiumGeometry)
  mayhem_fuzzer(fuzz_tile_content Cesium3DTilesContent CesiumGltfReader)
  mayhem_fuzzer(fuzz_geojson CesiumVectorData CesiumGeospatial)
  mayhem_fuzzer(fuzz_quantized_mesh CesiumQuantizedMeshTerrain CesiumGeospatial)
  mayhem_fuzzer(fuzz_image_decoder CesiumImage CesiumGltf)
  mayhem_fuzzer(fuzz_subtree Cesium3DTilesContent)
  mayhem_fuzzer(fuzz_tileset_json Cesium3DTilesReader Cesium3DTilesWriter)
  mayhem_fuzzer(fuzz_uri CesiumUtility)
  mayhem_fuzzer(fuzz_tileset Cesium3DTilesSelection Cesium3DTilesContent)
endif()
