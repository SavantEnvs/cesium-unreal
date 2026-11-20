# Known upstream findings for `tileset` (not seeds: never listed in a `testsuite:`)

## `createfromurl-non-object.json`

Body: `1` (a valid JSON document whose top-level value is not an object).

Served as `tileset.json`, it reaches `TilesetContentManager::createFromUrl`
(cesium-native `Cesium3DTilesSelection/src/TilesetContentManager.cpp:807`, pinned commit
`50799553`), which calls `tilesetJson.FindMember("root")` with no `IsObject()` check: a
rapidjson assertion in debug builds, undefined behavior under `NDEBUG`.
`TilesetJsonLoader.cpp:753` has the same gap for external tilesets.

The `tileset` harness rejects a non-object tileset.json (the 3D Tiles spec requires the
top-level value to be a JSON object), so this input returns -1 there. Reproduce the bug
by serving this body to `Tileset(externals, url)` without that check.
