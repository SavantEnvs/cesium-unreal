// Image decoding: CesiumImage::ImageDecoder::readImage sniffs the bytes and
// decodes KTX2 (Basis transcode), WebP, JPEG (libjpeg-turbo) or PNG/other formats
// (stb_image), as done for every embedded and external glTF/raster image.
#include "fuzz_common.h"

#include <CesiumImage/ImageAsset.h>
#include <CesiumImage/ImageDecoder.h>
#include <CesiumImage/Ktx2TranscodeTargets.h>

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fuzz::quietLogging();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  CesiumImage::ImageReaderResult result = CesiumImage::ImageDecoder::readImage(
      fuzz::asBytes(data, size), CesiumImage::Ktx2TranscodeTargets{});
  if (result.pImage && !result.pImage->pixelData.empty()) {
    const auto& pixels = result.pImage->pixelData;
    fuzz::consume(pixels.data(), 1);
    fuzz::consume(pixels.data() + pixels.size() - 1, 1);
  }
  return 0;
}
