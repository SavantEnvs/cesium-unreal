// GeoJSON: CesiumVectorData::GeoJsonDocument::fromGeoJson, a walk over every
// point / line / polygon through the public iterators, then GeoJSON -> glTF
// conversion (CesiumVectorData::GltfConverter), as used for GeoJSON tile content.
#include "gltf_walk.h"

#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumVectorData/GeoJsonDocument.h>
#include <CesiumVectorData/GeoJsonObject.h>
#include <CesiumVectorData/GltfConverter.h>

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fuzz::quietLogging();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto parsed = CesiumVectorData::GeoJsonDocument::fromGeoJson(fuzz::asBytes(data, size));
  if (!parsed.value)
    return 0;
  const CesiumVectorData::GeoJsonObject& root = parsed.value->rootObject;
  for (const auto& point : root.points())
    fuzz::consume(&point, sizeof(point));
  for (const auto& line : root.lines())
    for (const auto& p : line)
      fuzz::consume(&p, sizeof(p));
  for (const auto& polygon : root.polygons())
    for (const auto& ring : polygon)
      for (const auto& p : ring)
        fuzz::consume(&p, sizeof(p));
  for (const auto& object : root) {
    auto type = object.getType();
    fuzz::consume(&type, sizeof(type));
  }

  auto converted = CesiumVectorData::GltfConverter::convert(
      *parsed.value, CesiumGeospatial::Ellipsoid::WGS84);
  if (converted.value)
    fuzz::walkAccessors(*converted.value);
  return 0;
}
