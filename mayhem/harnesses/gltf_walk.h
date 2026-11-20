// Reads every element of every accessor of a parsed glTF through the public
// AccessorView API (what renderers such as Cesium for Unreal do with a model).
#pragma once

#include "fuzz_common.h"

#include <CesiumGltf/AccessorView.h>
#include <CesiumGltf/Model.h>

namespace fuzz {
inline void walkAccessors(const CesiumGltf::Model& model) {
  for (const CesiumGltf::Accessor& accessor : model.accessors) {
    CesiumGltf::createAccessorView(model, accessor, [](const auto& view) {
      if (view.status() != CesiumGltf::AccessorViewStatus::Valid)
        return;
      for (int64_t i = 0; i < view.size(); ++i) {
        const auto& v = view[i];
        consume(&v, sizeof(v));
      }
    });
  }
}
} // namespace fuzz
