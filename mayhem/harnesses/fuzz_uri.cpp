// URI handling applied to every URL found in tilesets / glTF / layer.json:
// CesiumUtility::Uri parsing, resolution, template substitution, (un)escaping,
// path conversion and UriQuery. The input is split at the first NUL byte into a
// base URI and a relative reference.
#include "fuzz_common.h"

#include <CesiumUtility/Uri.h>

#include <string>

using CesiumUtility::Uri;
using CesiumUtility::UriQuery;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fuzz::quietLogging();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);
  size_t nul = input.find('\0');
  std::string base = input.substr(0, nul);
  std::string relative = nul == std::string::npos ? std::string() : input.substr(nul + 1);

  Uri baseUri(base);
  Uri resolved(baseUri, relative, true);
  std::string out(resolved.toString());
  out += Uri::resolve(base, relative, true);
  out += Uri::resolve(base, relative, false, true);
  out += Uri::substituteTemplateParameters(
      relative, [](const std::string& p) { return p + "_v"; });
  out += Uri::escape(relative);
  out += Uri::unescape(relative);
  out += Uri::uriPathToUnixPath(relative);
  out += Uri::uriPathToWindowsPath(relative);
  out += Uri::windowsPathToUriPath(relative);
  out += Uri::unixPathToUriPath(relative);
  out += Uri::getPath(base);
  out += std::string(resolved.getPath());
  out += std::string(resolved.getHost());
  out += std::string(resolved.getScheme());

  UriQuery query(resolved.getQuery());
  for (const auto& [key, value] : query) {
    out += key;
    out += value;
  }
  query.setValue("fuzz", relative);
  out += query.toQueryString();
  // Uri's setters dereference the parsed URL without a check; the class contract is
  // that a Uri that failed to parse reports isValid() == false (Uri.h), and cesium-native's
  // own helpers (addQuery, setPath(uri, path)) test it before mutating, so do the same.
  if (resolved.isValid()) {
    resolved.setQuery(query.toQueryString());
    resolved.setPath(relative);
    out += std::string(resolved.toString());
  }
  fuzz::consume(out.data(), out.empty() ? 0 : 1);
  return 0;
}
