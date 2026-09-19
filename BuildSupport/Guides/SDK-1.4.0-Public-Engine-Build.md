# Build the EngawaRuntime 1.4.0 public engine

These recipes build `WebCore`, `JavaScriptCore`, and `EngawaRuntimeUtils` from
the standalone public source candidate. They require no proprietary runtime,
Unreal checkout, private Git history, or vendor signing key. They are source
recipes, not recorded native build or recipient replacement evidence. Run and
retain the applicable native build, dependency inventory, and replacement
results before distributing a corresponding binary release.

Use a short local parent directory, with separate sibling directories for the
candidate bundle, reconstructed source, and work. Paths below are examples to
replace with your own. The source must be reconstructed by the bundle's
`prepare_source.py`; do not use a generated SDK binary package as source.
Preparation requires Git and network access to the official WebKit repository.
It fetches only the exact pinned commit, verifies its commit and tree IDs, and
applies the public engine patch and overlay. There is no bundled WebKit archive.
Make your library modifications in the resulting shallow checkout after
preparation. Use a new destination when updating to a new source manifest;
preparation refuses to overwrite an existing checkout or local edits. For work
inside the public repository, use its ignored `Build/` directory for the
checkout and separate build/install directories.

The standalone runner builds `EngawaPublicEngine` and installs component
`EngawaRuntimeBootstrap`. Its receipt and three command logs remain in the
chosen CMake build directory. Successful installation contains the three public
libraries under `bin/` and `include/engawa_webcore.h`; it contains no
`EngawaRuntime` implementation binary. The runner's public-only flags must stay
enabled. Only the Windows gperf launcher is included from `BuildScripts`.
The standalone runner and these recipes supply the public compilation and
installation route; private SDK publication and signing scripts are excluded.

## macOS arm64

Install and select full Xcode, complete its first launch, and install Homebrew.
The copied Brewfile supplies the other build tools and ICU 77. Use a selected
SDK compatible with the distributed library being replaced. This pinned Cocoa
port requires its selected SDK's deployment target; a newer selected SDK does
not prove compatibility with an older recipient system.

Run in a native arm64 shell. Set these three paths, then prepare and provision:

```sh
export ER_PUBLIC_BUNDLE=/Users/you/engawa-public/bundle
export ER_PUBLIC_SOURCE=/Users/you/engawa-public/source
export ER_PUBLIC_WORK=/Users/you/engawa-public/work-arm64
xcodebuild -checkFirstLaunchStatus
xcrun --sdk macosx --show-sdk-path
brew bundle --file "$ER_PUBLIC_BUNDLE/BuildSupport/Tools/MacOS/Brewfile"
"$(brew --prefix python@3.11)/bin/python3.11" \
  "$ER_PUBLIC_BUNDLE/prepare_source.py" --destination "$ER_PUBLIC_SOURCE"
mkdir -p "$ER_PUBLIC_WORK"
```

The following reads paths from those variables and supplies the complete ICU
archive selection, including configuration-specific entries. All three ICU
archives must contain the selected architecture.

```sh
"$(brew --prefix python@3.11)/bin/python3.11" - <<'PY'
import os, subprocess, sys
from pathlib import Path

def output(*args):
    return subprocess.check_output(args, text=True).strip()

bundle = Path(os.environ['ER_PUBLIC_BUNDLE']).resolve()
source = Path(os.environ['ER_PUBLIC_SOURCE']).resolve()
work = Path(os.environ['ER_PUBLIC_WORK']).resolve()
icu = Path(output('brew', '--prefix', 'icu4c@77'))
arch = 'arm64'
settings = {
    'CMAKE_MAKE_PROGRAM': output('brew', '--prefix') + '/bin/ninja',
    'CMAKE_C_COMPILER': output('xcrun', '--sdk', 'macosx', '--find', 'clang'),
    'CMAKE_CXX_COMPILER': output('xcrun', '--sdk', 'macosx', '--find', 'clang++'),
    'CMAKE_Swift_COMPILER': output('xcrun', '--find', 'swiftc'),
    'CMAKE_OSX_SYSROOT': output('xcrun', '--sdk', 'macosx', '--show-sdk-path'),
    'CMAKE_OSX_DEPLOYMENT_TARGET': output('xcrun', '--sdk', 'macosx', '--show-sdk-version'),
    'CMAKE_OSX_ARCHITECTURES': arch,
    'CMAKE_APPLE_SILICON_PROCESSOR': arch,
    'ICU_INCLUDE_DIR': str(icu / 'include'),
    'GPERF_EXECUTABLE': output('brew', '--prefix', 'gperf') + '/bin/gperf',
    'PERL_EXECUTABLE': output('brew', '--prefix', 'perl') + '/bin/perl',
    'Python_EXECUTABLE': sys.executable,
    'Ruby_EXECUTABLE': output('brew', '--prefix', 'ruby@3.3') + '/bin/ruby',
    'PKG_CONFIG_EXECUTABLE': output('brew', '--prefix', 'pkgconf') + '/bin/pkgconf',
    'DEVELOPER_MODE': 'ON',
    'ENABLE_EXPERIMENTAL_FEATURES': 'OFF',
    'CMAKE_EXPORT_COMPILE_COMMANDS': 'ON',
}
for component, archive in [('UC', 'uc'), ('I18N', 'i18n'), ('DATA', 'data')]:
    library = icu / 'lib' / ('libicu' + archive + '.a')
    subprocess.run(['xcrun', 'lipo', '-verify_arch', arch, str(library)], check=True)
    for suffix in ('', '_RELEASE', '_DEBUG'):
        settings['ICU_' + component + '_LIBRARY' + suffix] = str(library)
command = [sys.executable, str(bundle / 'build_public_engine.py'),
           '--source', str(source), '--build', str(work / 'build'),
           '--install', str(work / 'install'), '--jobs', '4',
           '--cmake', output('brew', '--prefix') + '/bin/cmake']
command += ['--cmake-arg=-D' + key + '=' + value for key, value in settings.items()]
# The standalone runner forces USE_APPLE_ICU=OFF on macOS.
subprocess.run(command, check=True)
PY
```

For a universal replacement, independently build a second thin x86_64 tree.
Install Intel Homebrew ICU 77 at its Intel prefix; change `icu` to that prefix,
`arch` to `x86_64`, and use a separate `ER_PUBLIC_WORK`. Add
`'ER_MACOS_UNIVERSAL_SLICE': 'ON'` to `settings` for this second build. Keep the
same public sources, selected SDK, deployment target, and feature settings.
Verify both thin builds before combining each of the three corresponding dylib
pairs with `xcrun lipo -create ... -output ...`; verify every result with
`xcrun lipo -verify_arch arm64 x86_64`. A thin build does not establish Intel
runtime behavior, and `lipo` does not compile a missing architecture.

Record `xcrun otool -L` and `xcrun otool -l` for each installed library. Sibling
dependencies must use the package's relative install-name/rpath contract.
macOS applications may require the recipient to sign their modified libraries
and application with their own identity or an appropriate local signature.
The vendor's private Developer ID key is neither supplied nor needed to build
these sources; final application signing and successful loading must be tested
in the recipient's actual application environment.

## Ubuntu 26.04 x86-64 in Docker

Use the bundle's copied `BuildSupport/Redesign/Linux/Dockerfile` and
`Ubuntu26.lock.json` together. They specify the base image digest, dependency
archive checksums, APT package names, and static ICU/HarfBuzz/libwpe recipe.
APT package versions are resolved when the image is built; retain its package
inventory and image identity rather than claiming those versions are frozen by
the base digest alone. Image construction needs network access; compilation
below runs without network access.

On a Linux Docker host, set paths and prepare the source:

```sh
export ER_PUBLIC_BUNDLE=/home/you/engawa-public/bundle
export ER_PUBLIC_SOURCE=/home/you/engawa-public/source
export ER_PUBLIC_WORK=/home/you/engawa-public/work-linux
python3 "$ER_PUBLIC_BUNDLE/prepare_source.py" --destination "$ER_PUBLIC_SOURCE"
mkdir -p "$ER_PUBLIC_WORK"
```

Build the dependency image using all required Docker build arguments derived
from the supplied lock. This does not invoke a private setup script:

```sh
python3 - <<'PY'
import hashlib, json, os, subprocess
from pathlib import Path

bundle = Path(os.environ['ER_PUBLIC_BUNDLE']).resolve()
work = Path(os.environ['ER_PUBLIC_WORK']).resolve()
context = bundle / 'BuildSupport/Redesign/Linux'
lock_path = context / 'Ubuntu26.lock.json'
lock = json.loads(lock_path.read_text())
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
arguments = {
    'ER_APT_PACKAGES': ' '.join(lock['apt_packages']),
    'ER_DEPENDENCY_JOBS': '4',
    'ER_DOCKERFILE_SHA256': sha(context / 'Dockerfile'),
    'ER_LOCK_SHA256': sha(lock_path),
}
for name in ('icu', 'harfbuzz', 'libwpe'):
    for key in ('version', 'url', 'sha256'):
        arguments['ER_' + name.upper() + '_' + key.upper()] = lock['dependencies'][name][key]
arguments['ER_LIBWPE_LICENSE_SHA256'] = lock['dependencies']['libwpe']['license_sha256']
command = ['docker', 'build', '--platform', lock['base_image']['platform'],
           '--tag', 'engawa-public-engine:ubuntu26']
for key, value in arguments.items():
    command += ['--build-arg', key + '=' + value]
command.append(str(context))
with (work / 'dependency-image-build.log').open('w') as log:
    result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
(work / 'dependency-image-build.exit').write_text(str(result.returncode) + '\n')
result.check_returncode()
(work / 'dependency-image.json').write_bytes(subprocess.check_output(
    ['docker', 'image', 'inspect', 'engawa-public-engine:ubuntu26']))
PY
```

Compile with the locked private dependency prefixes inside that image. Here
"private" describes isolated dependency installations, not proprietary source.
ICU, HarfBuzz, HarfBuzz-ICU and libwpe feed `EngawaRuntimeUtils`; WebKit support
code stays with its owning public WebCore/JavaScriptCore library.

```sh
docker run --rm -i --platform linux/amd64 --network none \
  --cap-drop ALL --security-opt no-new-privileges \
  --user "$(id -u):$(id -g)" \
  --mount "type=bind,src=$ER_PUBLIC_BUNDLE,dst=/bundle,readonly" \
  --mount "type=bind,src=$ER_PUBLIC_SOURCE,dst=/source,readonly" \
  --mount "type=bind,src=$ER_PUBLIC_WORK,dst=/work" \
  engawa-public-engine:ubuntu26 python3 - <<'PY'
import subprocess, sys
from pathlib import Path

settings = {
    'CMAKE_C_COMPILER': '/usr/bin/clang',
    'CMAKE_CXX_COMPILER': '/usr/bin/clang++',
    'CMAKE_MAKE_PROGRAM': '/usr/bin/ninja',
    'CMAKE_INSTALL_LIBDIR': 'lib', 'CMAKE_INSTALL_BINDIR': 'bin',
    'CMAKE_EXE_LINKER_FLAGS': '-fuse-ld=lld',
    'CMAKE_SHARED_LINKER_FLAGS': '-fuse-ld=lld',
    'CMAKE_MODULE_LINKER_FLAGS': '-fuse-ld=lld',
    'GPERF_EXECUTABLE': '/usr/bin/gperf', 'PERL_EXECUTABLE': '/usr/bin/perl',
    'Python_EXECUTABLE': '/usr/bin/python3', 'Ruby_EXECUTABLE': '/usr/bin/ruby',
    'PKG_CONFIG_EXECUTABLE': '/usr/bin/pkg-config',
    'ICU_INCLUDE_DIR': '/opt/er/icu-77.1/include',
    'HarfBuzz_INCLUDE_DIR': '/opt/er/harfbuzz-11.2.1/include/harfbuzz',
    'HarfBuzz_ICU_INCLUDE_DIR': '/opt/er/harfbuzz-11.2.1/include/harfbuzz',
    'HarfBuzz_LIBRARY': '/opt/er/harfbuzz-11.2.1/lib/libharfbuzz.a',
    'HarfBuzz_ICU_LIBRARY': '/opt/er/harfbuzz-11.2.1/lib/libharfbuzz-icu.a',
    'WPE_INCLUDE_DIR': '/opt/er/libwpe-1.16.3/include/wpe-1.0',
    'WPE_LIBRARY': '/opt/er/libwpe-1.16.3/lib/libwpe-1.0.a',
    'DEVELOPER_MODE': 'ON', 'DEVELOPER_MODE_FATAL_WARNINGS': 'OFF',
    'ENABLE_EXPERIMENTAL_FEATURES': 'OFF', 'CMAKE_EXPORT_COMPILE_COMMANDS': 'ON',
}
for component, archive in [('UC', 'uc'), ('I18N', 'i18n'), ('DATA', 'data')]:
    for suffix in ('', '_RELEASE', '_DEBUG'):
        settings['ICU_' + component + '_LIBRARY' + suffix] = (
            '/opt/er/icu-77.1/lib/libicu' + archive + '.a')
command = [sys.executable, '/bundle/build_public_engine.py', '--source', '/source',
           '--build', '/work/build', '--install', '/work/install', '--jobs', '4',
           '--cmake', '/usr/bin/cmake']
command += ['--cmake-arg=-D' + key + '=' + value for key, value in settings.items()]
subprocess.run(command, check=True)
PY
```

Inspect `readelf -d` and `ldd` on all three installed libraries in the same
container. Record their SONAMEs, `$ORIGIN` sibling lookup, complete dependency
resolution and the exact system package versions. A Docker compile does not
prove loading in Unreal or a packaged recipient application.

## Windows x64

Use a native x64 Visual Studio developer PowerShell with the C++ desktop
workload and Windows SDK. The original native configuration uses Visual
Studio's CMake 3.31.6-msvc6, Ninja and vcpkg, LLVM/clang-cl 20.1.8, Ruby 3.3.12,
GNU gperf 3.0.1, Python 3, and Perl. Install these public tool distributions
locally and substitute their actual paths below. Keep Git's Unix utilities on
PATH for the WebKit generators. Record actual versions when using other tools;
compatibility with them must be established by a native build.

Enable Windows Developer Mode or use an environment permitted to create the
upstream source symlinks. Preparation must preserve those symlinks; converting
them into ordinary text files does not reconstruct the source. Use short local
paths to stay within the nested vcpkg/CMake object-path limits.

```powershell
$env:ER_PUBLIC_BUNDLE = 'C:\er-public\bundle'
$env:ER_PUBLIC_SOURCE = 'C:\er-public\source'
$env:ER_PUBLIC_WORK = 'C:\er-public\work'
$env:ER_PUBLIC_LLVM = 'C:\er-tools\LLVM-20.1.8\bin'
$env:ER_PUBLIC_RUBY = 'C:\er-tools\Ruby-3.3.12\bin\ruby.exe'
$env:ER_PUBLIC_PERL = 'C:\er-tools\Perl\bin\perl.exe'
$env:ER_GPERF_EXECUTABLE = 'C:\er-tools\Gperf-3.0.1\bin\gperf.exe'
python "$env:ER_PUBLIC_BUNDLE\prepare_source.py" --destination $env:ER_PUBLIC_SOURCE
if ($LASTEXITCODE -ne 0) { throw 'Source preparation failed' }
```

Run the following in that same developer shell. It installs the two distinct
vcpkg dependency graphs from the public manifest and registry pins, compiles
the included gperf launcher, then configures the public engine. Utils uses its
own static-library/dynamic-CRT triplet and ICU overlay. Both dependency installs
write logs and exit-code files under the work directory. The actual local
Utils status hash is supplied to CMake so changing the recipient's dependency
build does not require possessing a vendor-specific build-cache identity.

```powershell
@'
import hashlib, json, os, subprocess, sys
from pathlib import Path

bundle = Path(os.environ['ER_PUBLIC_BUNDLE']).resolve()
source = Path(os.environ['ER_PUBLIC_SOURCE']).resolve()
work = Path(os.environ['ER_PUBLIC_WORK']).resolve()
work.mkdir(parents=True, exist_ok=True)
vs = Path(os.environ['VSINSTALLDIR'])
llvm = Path(os.environ['ER_PUBLIC_LLVM'])
cmake = vs / 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
ninja = vs / 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
vcpkg_root = vs / 'VC/vcpkg'
vcpkg = vcpkg_root / 'vcpkg.exe'
toolchain = vcpkg_root / 'scripts/buildsystems/vcpkg.cmake'
resolutions = {
    'parent': {'triplet': 'x64-windows-webkit',
               'manifest_features': ['web', 'avif', 'jpeg-xl', 'lcms', 'skia', 'woff2']},
    'utils': {'triplet': 'x64-windows-engawaruntimeutils',
              'manifest_features': ['web', 'lcms', 'skia', 'woff2']},
}
triplets = source / 'WebKitLibraries/triplets'
ports = source / 'WebKitLibraries/ports/engawaruntimeutils'
os.environ['VCPKG_ROOT'] = str(vcpkg_root)
os.environ['VCPKG_MAX_CONCURRENCY'] = '4'
os.environ['CMAKE_NINJA_FORCE_RESPONSE_FILE'] = '1'
os.environ['PATH'] = os.pathsep.join([
    str(llvm), str(Path(os.environ['ER_PUBLIC_RUBY']).parent),
    str(Path(os.environ['ER_PUBLIC_PERL']).parent), os.environ['PATH']])

def logged(name, command, cwd=None):
    with (work / (name + '.log')).open('w') as log:
        result = subprocess.run([str(arg) for arg in command], cwd=cwd,
                                stdout=log, stderr=subprocess.STDOUT)
    (work / (name + '.exit')).write_text(str(result.returncode) + '\n')
    result.check_returncode()

installed = {}
for role in ('parent', 'utils'):
    installed[role] = work / (role + '-installed')
    dependency = resolutions[role]
    command = [vcpkg, 'install', '--triplet=' + dependency['triplet'],
               '--x-manifest-root=' + str(source),
               '--x-install-root=' + str(installed[role]),
               '--x-buildtrees-root=' + str(work / (role + '-buildtrees')),
               '--x-packages-root=' + str(work / (role + '-packages')),
               '--overlay-triplets=' + str(triplets),
               '--feature-flags=manifests,registries', '--disable-metrics']
    if role == 'utils':
        command += ['--overlay-ports=' + str(ports)]
    command += ['--x-feature=' + feature for feature in dependency['manifest_features']]
    logged('install-' + role, command)
launcher = work / 'gperf-launcher.exe'
logged('gperf-launcher', [llvm / 'clang-cl.exe', '/nologo', '/W4', '/WX', '/O2',
       '/DUNICODE', '/D_UNICODE', source / 'BuildScripts/Windows/gperf_launcher.c',
       '/Fe' + str(launcher)], cwd=work)
status = installed['utils'] / 'vcpkg/status'
settings = {
    'CMAKE_MAKE_PROGRAM': str(ninja), 'CMAKE_TOOLCHAIN_FILE': str(toolchain),
    'CMAKE_C_COMPILER': str(llvm / 'clang-cl.exe'),
    'CMAKE_CXX_COMPILER': str(llvm / 'clang-cl.exe'),
    'CMAKE_OBJDUMP': str(llvm / 'llvm-objdump.exe'),
    'GPERF_EXECUTABLE': str(launcher), 'Python_EXECUTABLE': sys.executable,
    'PERL_EXECUTABLE': os.environ['ER_PUBLIC_PERL'],
    'Ruby_EXECUTABLE': os.environ['ER_PUBLIC_RUBY'],
    'VCPKG_TARGET_TRIPLET': resolutions['parent']['triplet'],
    'VCPKG_MANIFEST_FEATURES': ';'.join(resolutions['parent']['manifest_features']),
    'VCPKG_INSTALLED_DIR': str(installed['parent']),
    'VCPKG_MANIFEST_DIR': str(source), 'VCPKG_MANIFEST_INSTALL': 'OFF',
    'VCPKG_OVERLAY_TRIPLETS': str(triplets),
    'ER_ENGAWARUNTIMEUTILS_VCPKG_INSTALLED_DIR': str(installed['utils']),
    'ER_ENGAWARUNTIMEUTILS_VCPKG_MANIFEST_DIR': str(source),
    'ER_ENGAWARUNTIMEUTILS_VCPKG_MANIFEST_INSTALL': 'OFF',
    'ER_ENGAWARUNTIMEUTILS_VCPKG_TOOLCHAIN_FILE': str(toolchain),
    'ER_ENGAWARUNTIMEUTILS_VCPKG_TARGET_TRIPLET': resolutions['utils']['triplet'],
    'ER_ENGAWARUNTIMEUTILS_VCPKG_STATUS_SHA256': hashlib.sha256(status.read_bytes()).hexdigest(),
    'ER_ENGAWARUNTIMEUTILS_VCPKG_OVERLAY_TRIPLETS': str(triplets),
    'ER_ENGAWARUNTIMEUTILS_VCPKG_OVERLAY_PORTS': str(ports),
    'ER_ENGAWARUNTIMEUTILS_VCPKG_MANIFEST_FEATURES': ';'.join(resolutions['utils']['manifest_features']),
    'DEVELOPER_MODE': 'ON', 'ENABLE_EXPERIMENTAL_FEATURES': 'OFF',
    'CMAKE_EXPORT_COMPILE_COMMANDS': 'ON',
}
command = [sys.executable, bundle / 'build_public_engine.py', '--source', source,
           '--build', work / 'build', '--install', work / 'install',
           '--jobs', '4', '--cmake', cmake]
command += ['--cmake-arg=-D' + key + '=' + value for key, value in settings.items()]
subprocess.run([str(arg) for arg in command], check=True)
'@ | python -
if ($LASTEXITCODE -ne 0) { throw 'Public dependency or engine build failed; inspect work logs' }
```

Inspect the three installed DLLs with the selected LLVM's
`llvm-readobj --file-headers --coff-imports --coff-exports`. Confirm x64,
resolvable imports and the `er_webcore_get_api` bridge exported by WebCore.
Do not copy the parent vcpkg DLL directory into the recipient application:
the declared support aggregate supplies the bundled support-library boundary.

## Validate a recipient replacement

Keep the original licensed binary distribution and its legal notices. In a
copy of the recipient SDK/plugin/application, replace the three public engine
libraries beside its existing `EngawaRuntime` library with compatible rebuilt
libraries. Keep the bridge ABI, filenames, architecture, deployment target and
required runtime dependencies compatible. Test application startup, runtime
creation, view creation, local content, rendering, input, shutdown and repeated
loads. Record the modified source identity, toolchain, replacement binary
hashes, exact recipient application and results. Resolve any recipient signing
or packaging policy that prevents loading the compatible modified libraries.

This source build, a source checksum, or a successful synthetic CMake fixture
does not establish that replacement test. Preserve the dependency sources,
licensing terms and notices associated with the binary distribution.
