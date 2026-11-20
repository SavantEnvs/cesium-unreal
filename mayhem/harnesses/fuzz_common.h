// Shared helpers for the cesium-native fuzz harnesses (cesium-unreal's data layer).
// - an AsyncSystem whose task processor runs work inline (like CesiumNativeTests'
//   SimpleTaskProcessor), so every future completes during the input;
// - an IAssetAccessor that serves bytes from an in-memory map (the fuzz input) and
//   answers 404 for everything else, so no network is ever touched.
#pragma once

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/HttpHeaders.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumAsync/ITaskProcessor.h>

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace fuzz {

class InlineTaskProcessor : public CesiumAsync::ITaskProcessor {
public:
  void startTask(std::function<void()> f) override { f(); }
};

class Response : public CesiumAsync::IAssetResponse {
public:
  Response(uint16_t status, std::vector<std::byte> data)
      : _status(status), _data(std::move(data)) {}
  uint16_t statusCode() const override { return _status; }
  std::string contentType() const override { return {}; }
  const CesiumAsync::HttpHeaders& headers() const override { return _headers; }
  std::span<const std::byte> data() const override { return _data; }

private:
  uint16_t _status;
  std::vector<std::byte> _data;
  CesiumAsync::HttpHeaders _headers;
};

class Request : public CesiumAsync::IAssetRequest {
public:
  Request(std::string url, std::unique_ptr<Response> pResponse)
      : _url(std::move(url)), _pResponse(std::move(pResponse)) {}
  const std::string& method() const override { return _method; }
  const std::string& url() const override { return _url; }
  const CesiumAsync::HttpHeaders& headers() const override { return _headers; }
  const CesiumAsync::IAssetResponse* response() const override {
    return _pResponse.get();
  }

private:
  std::string _method{"GET"};
  std::string _url;
  CesiumAsync::HttpHeaders _headers;
  std::unique_ptr<Response> _pResponse;
};

// Serves `files` by exact URL; when `fallback` is set, every other URL gets it,
// otherwise a 404 with an empty body.
class MapAssetAccessor : public CesiumAsync::IAssetAccessor {
public:
  std::map<std::string, std::vector<std::byte>> files;
  std::optional<std::vector<std::byte>> fallback;

  CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
  get(const CesiumAsync::AsyncSystem& asyncSystem,
      const std::string& url,
      const std::vector<THeader>&) override {
    auto it = files.find(url);
    std::unique_ptr<Response> pResponse;
    if (it != files.end()) {
      pResponse = std::make_unique<Response>(uint16_t(200), it->second);
    } else if (fallback) {
      pResponse = std::make_unique<Response>(uint16_t(200), *fallback);
    } else {
      pResponse = std::make_unique<Response>(uint16_t(404), std::vector<std::byte>{});
    }
    return asyncSystem.createResolvedFuture(
        std::shared_ptr<CesiumAsync::IAssetRequest>(
            std::make_shared<Request>(url, std::move(pResponse))));
  }

  CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> request(
      const CesiumAsync::AsyncSystem& asyncSystem,
      const std::string&,
      const std::string& url,
      const std::vector<THeader>& headers,
      const std::span<const std::byte>&) override {
    return this->get(asyncSystem, url, headers);
  }

  void tick() noexcept override {}
};

inline CesiumAsync::AsyncSystem& asyncSystem() {
  static CesiumAsync::AsyncSystem system(std::make_shared<InlineTaskProcessor>());
  return system;
}

// Logging is process-global; it is switched off once (never per input) so the
// fuzzer is not dominated by formatting warnings about malformed data.
inline void quietLogging() { spdlog::set_level(spdlog::level::off); }

inline std::span<const std::byte> asBytes(const uint8_t* data, size_t size) {
  return std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size);
}

// Keeps the compiler from discarding values read from parsed data.
inline void consume(const void* p, size_t n) {
  static volatile unsigned char sink;
  const auto* b = static_cast<const unsigned char*>(p);
  for (size_t i = 0; i < n; ++i) sink = static_cast<unsigned char>(sink ^ b[i]);
}

} // namespace fuzz
