// Post-parse glTF processing: a model read by GltfReader is passed directly through the
// public CesiumGltfContent::GltfUtilities utilities (bounding region, ray picking,
// unused-object removal, buffer compaction) and written back as GLB. These are public
// library APIs fuzzed directly; outside tests nothing in cesium-native calls
// compactBuffers/collapseToSingleBuffer.
#include "gltf_walk.h"

#include <CesiumGeometry/Ray.h>
#include <CesiumGltfContent/GltfUtilities.h>
#include <CesiumGltfReader/GltfReader.h>
#include <CesiumGltfWriter/GltfWriter.h>

#include <glm/mat4x4.hpp>

using CesiumGltfContent::GltfUtilities;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fuzz::quietLogging();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  CesiumGltfReader::GltfReader reader;
  CesiumGltfReader::GltfReaderResult result =
      reader.readGltf(fuzz::asBytes(data, size));
  // The tile content pipeline (TilesetJsonLoader, after a GltfConverter) fails the tile when the
  // read reported errors and only processes models that read cleanly; do the same here.
  if (!result.model || !result.errors.empty())
    return 0;
  CesiumGltf::Model& model = *result.model;

  GltfUtilities::computeBoundingRegion(model, glm::dmat4(1.0));
  GltfUtilities::intersectRayGltfModel(
      CesiumGeometry::Ray(glm::dvec3(0.0, 0.0, 10.0), glm::dvec3(0.0, 0.0, -1.0)),
      model,
      false);

  GltfUtilities::removeUnusedTextures(model);
  GltfUtilities::removeUnusedSamplers(model);
  GltfUtilities::removeUnusedImages(model);
  GltfUtilities::removeUnusedAccessors(model);
  GltfUtilities::removeUnusedBufferViews(model);
  GltfUtilities::removeUnusedBuffers(model);
  GltfUtilities::removeUnusedMeshes(model);
  GltfUtilities::removeUnusedMaterials(model);
  GltfUtilities::compactBuffers(model);
  GltfUtilities::collapseToSingleBuffer(model);
  fuzz::walkAccessors(model);

  std::span<const std::byte> bufferData;
  if (!model.buffers.empty())
    bufferData = model.buffers[0].cesium.data;
  CesiumGltfWriter::GltfWriter writer;
  CesiumGltfWriter::GltfWriterResult written = writer.writeGlb(model, bufferData);
  fuzz::consume(written.gltfBytes.data(), written.gltfBytes.size() > 16 ? 16 : written.gltfBytes.size());
  return 0;
}
