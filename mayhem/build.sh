#!/usr/bin/env bash
# mayhem/build.sh — builds the cesium-unreal fuzz layer.
#
# cesium-unreal's Source/ is Unreal Engine module code; the data it downloads is parsed by its
# pinned submodule extern/cesium-native, which mayhem/Dockerfile fetched (at the submodule's
# commit) into $CESIUM_NATIVE_SRC together with an offline vcpkg tree ($VCPKG_ROOT). This script
# builds, out of tree under /mayhem/mayhem/build-*:
#   1. cesium-native's own doctest suite with normal flags (run by mayhem/test.sh);
#   2. cesium-native instrumented with $SANITIZER_FLAGS + SanCov, and the harnesses in
#      mayhem/harnesses/, each as a libFuzzer binary and a -standalone reproducer in /mayhem.
# Re-runnable (CMake build dirs are incremental) and offline (nothing is fetched).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
: "${CC:=clang}" "${CXX:=clang++}"
SANITIZER_FLAGS=${SANITIZER_FLAGS-"-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g"}
: "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${COVERAGE_FLAGS:=}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${VCPKG_ROOT:=/opt/toolchains/vcpkg}"
: "${CESIUM_NATIVE_SRC:=/opt/toolchains/deps/cesium-native-5079955/src}"
export CC CXX

[ -f "$CESIUM_NATIVE_SRC/CMakeLists.txt" ] || { echo "missing cesium-native at $CESIUM_NATIVE_SRC" >&2; exit 1; }

common=(
  -G Ninja
  --compile-no-warning-as-error
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
  -DVCPKG_TARGET_TRIPLET=x64-linux-release
  -DVCPKG_MANIFEST_MODE=OFF
  -DCESIUM_NATIVE_SRC="$CESIUM_NATIVE_SRC"
  -DCMAKE_BUILD_TYPE=Release
)

# 1) cesium-native's test suite, normal flags (Release semantics, NDEBUG; compiled at -O0 because
#    the -O3 that Release adds doubles the compile time of a suite that runs in seconds).
cmake -S "$SRC/mayhem/harnesses" -B "$SRC/mayhem/build-tests" "${common[@]}" \
  -DMAYHEM_FUZZ=OFF -DCESIUM_TESTS_ENABLED=ON \
  -DCMAKE_C_FLAGS="-O0 $COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="-O0 $COVERAGE_FLAGS" \
  -DCMAKE_C_FLAGS_RELEASE=-DNDEBUG -DCMAKE_CXX_FLAGS_RELEASE=-DNDEBUG
cmake --build "$SRC/mayhem/build-tests" -j"$MAYHEM_JOBS" --target cesium-native-tests

# 2) instrumented cesium-native + harnesses.
# UBSan alignment and vptr are relaxed (only when sanitizing): QuantizedMeshLoader reads its
# little-endian fields through unaligned pointers on every input, and vptr misfires on calls into
# ada-url, which vcpkg builds without RTTI. ASan and every other UBSan check stay on and halting.
relax=""
[ -z "$SANITIZER_FLAGS" ] || relax="-fno-sanitize=alignment,vptr"
fuzz_flags="$SANITIZER_FLAGS $relax $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link"
cmake -S "$SRC/mayhem/harnesses" -B "$SRC/mayhem/build-fuzz" "${common[@]}" \
  -DMAYHEM_FUZZ=ON -DCESIUM_TESTS_ENABLED=OFF -DMAYHEM_OUT="$SRC" \
  -DMAYHEM_LSAN_OFF="$SRC/mayhem/lsan_off.cc" \
  -DCMAKE_C_FLAGS="$fuzz_flags" -DCMAKE_CXX_FLAGS="$fuzz_flags" \
  -DCMAKE_C_FLAGS_RELEASE= -DCMAKE_CXX_FLAGS_RELEASE=
# Only the harnesses (and the cesium-native libraries they link), not every library.
targets=()
for f in "$SRC"/mayhem/harnesses/fuzz_*.cpp; do
  n=$(basename "$f" .cpp)
  targets+=("$n" "$n-standalone")
done
cmake --build "$SRC/mayhem/build-fuzz" -j"$MAYHEM_JOBS" --target "${targets[@]}"

for f in "$SRC"/mayhem/harnesses/fuzz_*.cpp; do
  n=$(basename "$f" .cpp)
  test -x "$SRC/$n" && test -x "$SRC/$n-standalone"
done
echo "build.sh: done"
