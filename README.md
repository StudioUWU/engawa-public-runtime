# EngawaRuntime 1.4.0 public engine

## Public Notice: This build and patch setup is created by AI. Verified by humans.
As required by Apple Webkit we need to have our patch for the engine public. However,
due to the complexity of this project maintaining public vs private would be a pain. So
enter AI to extract the required parts. Thanks for the understanding!

# Engawa Webkit Public Runtime.
Build WebCore, JavaScriptCore, and EngawaRuntimeUtils independently.
This repository contains the public engine patch.

Run `python3 prepare_source.py --destination Build/WebKit` (or `python` on
Windows). Git and network access to GitHub are required. The tool initializes
a shallow checkout, fetches the exact commit, verifies its commit/tree IDs,
then applies the public patch and overlay. Use a new destination for each
updated source manifest; existing checkouts and local edits are never replaced.
The resulting checkout retains only upstream Git history. Make library changes
there after preparation. Build/ is ignored so downloaded source and generated
outputs stay out of this repository. Windows needs permission to create real
source symlinks (for example Developer Mode) and room for the full WebKit tree.
The two standalone Python tools use the accompanying
licenses/BSD-2-Clause-Engawa-Tools.txt. Upstream and overlay files retain their
own notices and licenses; the tools' license does not relicense those files.

Follow BuildSupport/Guides/SDK-1.4.0-Public-Engine-Build.md for the concrete
native Windows, Mac, and locked Linux dependency and configure commands.
Use the standalone runner after installing the locked platform dependencies:

```
python3 build_public_engine.py --source /new/path/WebKit --build /new/path/build --install /new/path/install --cmake-arg=-DCMAKE_C_COMPILER=clang --cmake-arg=-DCMAKE_CXX_COMPILER=clang++
```

Add repeated `--cmake-arg=-DNAME=VALUE` for the actual compiler, selected SDK,
ICU and other dependency paths. The illustrated compiler choice is generic;
Windows requires clang-cl from the VS developer environment and Mac requires
selected full-Xcode tools. The runner forces the public-only shared boundary,
including USE_APPLE_ICU=OFF on Mac,
builds EngawaPublicEngine and installs the three public libraries plus bridge
header using the EngawaRuntimeBootstrap component. Configure, build and install
logs/exit codes and artifact hashes remain under the selected build directory.
Use separate nonnested source, build and install directories. Only the Windows
gperf launcher is retained from BuildScripts; proprietary product build,
publication and signing Python code is excluded. The standalone runner and
public build guide supply the compilation and installation route. Verify that
route for each final binary distribution.

Windows uses the recorded VS/clang-cl developer environment and separate
dynamic-parent/static-Utils vcpkg prefixes. Mac uses full Xcode and the selected
SDK plus architecture-matching ICU 77; universal output builds both slices
independently before lipo. Linux uses the copied locked Docker dependency recipe.
BuildSupport contains the public dependency setup inputs and source/build guides.
Use local output/install directories.

Dependency source archives outside WebKit are NOT downloaded by this exporter.
The locks and custom ports identify those inputs; assemble and verify any
required dependency sources/notices before claiming corresponding-source
completeness. Retain all per-file licenses and modification notices.
