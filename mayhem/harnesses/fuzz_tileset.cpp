// The whole tileset pipeline the plugin drives every frame: a
// Cesium3DTilesSelection::Tileset is created from "tileset.json" served by an
// in-memory asset accessor (TilesetJsonLoader, implicit tiling loaders, external
// tilesets, content loading/conversion), then view updates select and load tiles.
// Input layout: tileset.json bytes, a NUL byte, then the bytes served for every
// other URL (tile content, subtrees, external tilesets).
#include "fuzz_common.h"

#include <Cesium3DTilesSelection/IPrepareRendererResources.h>
#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <Cesium3DTilesSelection/ViewState.h>
#include <Cesium3DTilesContent/registerAllTileContentTypes.h>
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumUtility/CreditSystem.h>
#include <CesiumUtility/Math.h>

#include <rapidjson/document.h>

#include <algorithm>
#include <cmath>

using namespace Cesium3DTilesSelection;
using CesiumGeospatial::Cartographic;
using CesiumGeospatial::Ellipsoid;
using CesiumUtility::Math;

namespace {
class NullRendererResources : public IPrepareRendererResources {
public:
  CesiumAsync::Future<TileLoadResultAndRenderResources> prepareInLoadThread(
      const CesiumAsync::AsyncSystem& asyncSystem,
      TileLoadResult&& tileLoadResult,
      const glm::dmat4&,
      const std::any&) override {
    return asyncSystem.createResolvedFuture(
        TileLoadResultAndRenderResources{std::move(tileLoadResult), nullptr});
  }
  void* prepareInMainThread(Tile&, void*) override { return nullptr; }
  void free(Tile&, void*, void*) noexcept override {}
  void* prepareRasterInLoadThread(CesiumImage::ImageAsset&, const std::any&) override {
    return nullptr;
  }
  void* prepareRasterInMainThread(CesiumRasterOverlays::RasterOverlayTile&, void*) override {
    return nullptr;
  }
  void freeRaster(const CesiumRasterOverlays::RasterOverlayTile&, void*, void*) noexcept override {}
  void attachRasterInMainThread(
      const Tile&, int32_t, const CesiumRasterOverlays::RasterOverlayTile&, void*,
      const glm::dvec2&, const glm::dvec2&) override {}
  void detachRasterInMainThread(
      const Tile&, int32_t, const CesiumRasterOverlays::RasterOverlayTile&, void*) noexcept override {}
};

ViewState makeViewState(double lonDeg, double latDeg, double height) {
  const Ellipsoid& ellipsoid = Ellipsoid::WGS84;
  Cartographic position{Math::degreesToRadians(lonDeg), Math::degreesToRadians(latDeg), height};
  Cartographic focus{position.longitude + Math::degreesToRadians(0.5),
                     position.latitude + Math::degreesToRadians(0.5), 0.0};
  glm::dvec3 viewPosition = ellipsoid.cartographicToCartesian(position);
  glm::dvec3 viewFocus = ellipsoid.cartographicToCartesian(focus);
  glm::dvec2 viewPortSize{500.0, 500.0};
  double hfov = Math::degreesToRadians(60.0);
  double vfov = std::atan(std::tan(hfov * 0.5)) * 2.0;
  return ViewState(viewPosition, glm::normalize(viewFocus - viewPosition),
                   glm::dvec3(0.0, 0.0, 1.0), viewPortSize, hfov, vfov, ellipsoid);
}
} // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fuzz::quietLogging();
  Cesium3DTilesContent::registerAllTileContentTypes();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const uint8_t* end = data + size;
  const uint8_t* nul = std::find(data, end, uint8_t(0));
  // Precondition from the 3D Tiles specification (tileset.json): "A tileset is a JSON
  // object" -- the top-level value of tileset.json must be a JSON object
  // (https://github.com/CesiumGS/3d-tiles/blob/main/specification/schema/tileset.schema.json,
  // "type": "object"). Reject a tileset.json that parses as JSON but is not an object.
  // Malformed JSON is NOT rejected: createFromUrl reports parse errors itself.
  // Known upstream bug this skips: TilesetContentManager::createFromUrl
  // (TilesetContentManager.cpp:807) calls FindMember("root") with no IsObject() check;
  // reproducer: mayhem/tileset/known-findings/createfromurl-non-object.json (body `1`).
  // Nothing else is filtered: the bytes served for every other URL reach the library as-is.
  {
    rapidjson::Document tilesetJson;
    tilesetJson.Parse(reinterpret_cast<const char*>(data), static_cast<size_t>(nul - data));
    if (!tilesetJson.HasParseError() && !tilesetJson.IsObject())
      return -1;
  }
  auto pAccessor = std::make_shared<fuzz::MapAssetAccessor>();
  pAccessor->files["https://example.com/tileset.json"] =
      std::vector<std::byte>(reinterpret_cast<const std::byte*>(data),
                             reinterpret_cast<const std::byte*>(nul));
  if (nul != end)
    pAccessor->fallback = std::vector<std::byte>(
        reinterpret_cast<const std::byte*>(nul + 1), reinterpret_cast<const std::byte*>(end));

  CesiumAsync::AsyncSystem& asyncSystem = fuzz::asyncSystem();
  TilesetExternals externals{
      pAccessor,
      std::make_shared<NullRendererResources>(),
      asyncSystem,
      std::make_shared<CesiumUtility::CreditSystem>()};
  {
    Tileset tileset(externals, "https://example.com/tileset.json");
    const ViewState views[] = {makeViewState(118.0, 32.0, 200.0),
                               makeViewState(0.0, 0.0, 10000000.0),
                               makeViewState(-75.0, 40.0, 50.0)};
    for (const ViewState& view : views) {
      asyncSystem.dispatchMainThreadTasks();
      tileset.updateViewGroup(tileset.getDefaultViewGroup(), {view});
      asyncSystem.dispatchMainThreadTasks();
      tileset.loadTiles();
    }
    asyncSystem.dispatchMainThreadTasks();
  }
  asyncSystem.dispatchMainThreadTasks();
  return 0;
}
