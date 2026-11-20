// quantized-mesh-1.0 terrain: CesiumQuantizedMeshTerrain::QuantizedMeshLoader::load
// (vertex/index/edge decoding, normals, water mask, metadata extensions) and
// loadMetadata (the availability JSON extension) on the same tile bytes.
#include "gltf_walk.h"

#include <CesiumGeometry/QuadtreeTileID.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumQuantizedMeshTerrain/QuantizedMeshLoader.h>

using namespace CesiumQuantizedMeshTerrain;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fuzz::quietLogging();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const CesiumGeometry::QuadtreeTileID tileID(10, 1000, 300);
  const CesiumGeospatial::BoundingRegion region(
      CesiumGeospatial::GlobeRectangle::fromDegrees(-1.0, -1.0, 1.0, 1.0),
      -100.0,
      1000.0,
      CesiumGeospatial::Ellipsoid::WGS84);
  auto bytes = fuzz::asBytes(data, size);
  QuantizedMeshLoadResult result = QuantizedMeshLoader::load(
      tileID, region, "https://example.com/terrain/10/1000/300.terrain", bytes,
      true, CesiumGeospatial::Ellipsoid::WGS84);
  if (result.model)
    fuzz::walkAccessors(*result.model);
  QuantizedMeshMetadataResult metadata = QuantizedMeshLoader::loadMetadata(bytes, tileID);
  size_t n = metadata.availability.size();
  fuzz::consume(&n, sizeof(n));
  return 0;
}
