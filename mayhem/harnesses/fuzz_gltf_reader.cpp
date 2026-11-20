// glTF / GLB reader: CesiumGltfReader::GltfReader::readGltf with the default
// options (data URIs, embedded images, Draco, meshopt, SPZ decoding), then every
// accessor is read through AccessorView.
#include "gltf_walk.h"

#include <CesiumGltfReader/GltfReader.h>

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fuzz::quietLogging();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  CesiumGltfReader::GltfReader reader;
  CesiumGltfReader::GltfReaderResult result =
      reader.readGltf(fuzz::asBytes(data, size));
  if (result.model)
    fuzz::walkAccessors(*result.model);
  return 0;
}
