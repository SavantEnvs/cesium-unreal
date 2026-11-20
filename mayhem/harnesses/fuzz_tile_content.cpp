// 3D Tiles content: Cesium3DTilesContent::GltfConverters::convert picks the
// b3dm / i3dm / cmpt / pnts / glb converter from the content's magic, exactly as
// the tile loader does for downloaded tile content. External URIs (i3dm/glTF
// references) answer 404.
#include "gltf_walk.h"

#include <Cesium3DTilesContent/GltfConverters.h>
#include <Cesium3DTilesContent/registerAllTileContentTypes.h>
#include <CesiumGeometry/Axis.h>

#include <glm/mat4x4.hpp>

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fuzz::quietLogging();
  Cesium3DTilesContent::registerAllTileContentTypes();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto pAccessor = std::make_shared<fuzz::MapAssetAccessor>();
  CesiumAsync::AsyncSystem& asyncSystem = fuzz::asyncSystem();
  Cesium3DTilesContent::AssetFetcher fetcher(
      asyncSystem,
      pAccessor,
      "https://example.com/tiles/content.bin",
      glm::dmat4(1.0),
      {},
      CesiumGeometry::Axis::Y);
  CesiumGltfReader::GltfReaderOptions options;
  auto future = Cesium3DTilesContent::GltfConverters::convert(
      fuzz::asBytes(data, size), options, fetcher);
  Cesium3DTilesContent::GltfConverterResult result = future.waitInMainThread();
  if (result.model)
    fuzz::walkAccessors(*result.model);
  return 0;
}
