# EngawaRuntimeUtils ICU overlay origin

This overlay is based exactly on the ICU 77.1.0#3 port tree selected by the
repository's pinned WebKitRequirements registry:

- registry: `git+https://github.com/WebKitForWindows/WebKitRequirements`
- port tree: `35de6e422680cb8f07eef2c73ab06ee0803e8ccb`
- ICU source archive: `icu4c-77_1-src.tgz`
- ICU archive SHA-512: `a47d6d9c327d037a05ea43d1d1a06b2fd757cc02a94f7c1a238f35cfc3dfd4ab78d0612790f3a3cca0292c77412a9c2c15c8f24b718f79a857e007e66f07e7cd`

The copied port differs only where the static EngawaRuntimeUtils boundary requires
it: Windows pkg-config metadata names ICU's `s`-prefixed static archives and
publishes their static dependency closure and `-DU_STATIC_IMPLEMENTATION`,
while exported static ICU CMake targets propagate the same definition.  The
port also updates the exported
locations of ICU's `sbin` tools after vcpkg relocates them into `tools/icu`, so
the installed config package has no dangling imported executable targets. The
overlay also keeps the generated ICU data entry point local when building the
static data archive; dynamic ICU data builds retain their normal export.

SHA-256 checksums of the unmodified registry-tree files:

```
df76a6ac4b241b7812cb4bd538ddb39d8c58fc09db59e5629186ee9d02fc5d05  patches/0001-Add-CMake-platform.patch
0ae8e39d0378b75d4b98bacd656f252c8abbd3932791633dc8b732e5d329fd07  patches/0002-Remove-install-suffix-on-Windows.patch
5cf0f99f5d330e1ad1825468ff44b43321e7da1fe111d7ff484eb01d0883bc74  patches/0003-Support-cross-compilation-in-CMake-build.patch
736ae8d7d9a93c234589df6f28b14f49dbf39a6b2dcd319925315aeafbd9af4e  patches/0004-Cross-compilation-support-for-pkgdata.patch
ab286ceded0bfb9efca498d9ea9a32e2b58a0d6a35332e9a739e4050e273f36b  pkgconfig/debug/icu-i18n.pc
6bf313ea11b2e2a7612ff262eed4f1388a9bec248b76551761ef1840ec7720ec  pkgconfig/debug/icu-io.pc
bc864d44f0ad57ef3a5bf5c0c46c53a92be317dc827f41f4c06ba09a25fdc5de  pkgconfig/debug/icu-uc.pc
053f9c36c5594f268d84b1fad57c2d63c0b64951267cef49e07654822cf50815  pkgconfig/release/icu-i18n.pc
9fa1257f18d7da8a209a8795f42fcc7d3cff65331c14254af7b36a9897ca42ad  pkgconfig/release/icu-io.pc
2fce3d16c495987585c9ea20c2b88b6cf0ce5a3bd3739f3eb56a7801da669684  pkgconfig/release/icu-uc.pc
36a49b44c05aef4baff32d2010309c6c5358dc5c8a940e53342df3f9eff3b51c  portfile.cmake
86cd94bc3ab2b67a9a63240f6335efc6ed73e015fa1a2da96a14af53bedbf112  vcpkg.json
```
