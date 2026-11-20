// Implicit-tiling subtrees: Cesium3DTilesContent::SubtreeAvailability::loadSubtree
// downloads a .subtree (binary header + JSON + buffers, or plain JSON) through the
// asset accessor, builds the availability bitstreams, then availability is queried.
#include "fuzz_common.h"

#include <Cesium3DTilesContent/SubtreeAvailability.h>
#include <CesiumGeometry/QuadtreeTileID.h>

using namespace Cesium3DTilesContent;

static const char* kUrl = "https://example.com/subtrees/0.0.0.subtree";

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fuzz::quietLogging();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto pAccessor = std::make_shared<fuzz::MapAssetAccessor>();
  auto bytes = fuzz::asBytes(data, size);
  pAccessor->files[kUrl] = std::vector<std::byte>(bytes.begin(), bytes.end());
  auto future = SubtreeAvailability::loadSubtree(
      ImplicitTileSubdivisionScheme::Quadtree,
      5,
      fuzz::asyncSystem(),
      pAccessor,
      spdlog::default_logger(),
      kUrl,
      {});
  std::optional<SubtreeAvailability> availability = future.waitInMainThread();
  if (!availability)
    return 0;
  const CesiumGeometry::QuadtreeTileID root(0, 0, 0);
  int count = 0;
  for (uint32_t level = 0; level < 6; ++level) {
    uint32_t dim = 1u << level;
    for (uint32_t x = 0; x < dim && x < 4; ++x) {
      for (uint32_t y = 0; y < dim && y < 4; ++y) {
        CesiumGeometry::QuadtreeTileID id(level, x, y);
        count += availability->isTileAvailable(root, id);
        count += availability->isContentAvailable(root, id, 0);
        if (level == 5)
          count += availability->isSubtreeAvailable(root, id);
      }
    }
  }
  fuzz::consume(&count, sizeof(count));
  return 0;
}
