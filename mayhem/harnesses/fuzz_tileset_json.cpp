// tileset.json: Cesium3DTilesReader::TilesetReader::readFromJson (the generated
// 3D Tiles JSON handlers incl. all registered extensions), round-tripped through
// Cesium3DTilesWriter::TilesetWriter.
#include "fuzz_common.h"

#include <Cesium3DTilesReader/TilesetReader.h>
#include <Cesium3DTilesWriter/TilesetWriter.h>

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fuzz::quietLogging();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  Cesium3DTilesReader::TilesetReader reader;
  auto result = reader.readFromJson(fuzz::asBytes(data, size));
  if (!result.value)
    return 0;
  Cesium3DTilesWriter::TilesetWriter writer;
  auto written = writer.writeTileset(*result.value);
  fuzz::consume(written.tilesetBytes.data(), written.tilesetBytes.empty() ? 0 : 1);
  return 0;
}
