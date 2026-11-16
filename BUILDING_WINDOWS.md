# Windows build guide (MSVC)

This guide follows the repository's Windows-MSVC CI configuration and builds the
existing CMake project. The Windows CI matrix currently uses Windows Server
2022, x64 MSVC, Qt 6.11.2 and the `ci` CMake preset. The project requires C++23.

## Prerequisites

- Windows 10 or 11, x64.
- Visual Studio 2022 with **Desktop development with C++**, the MSVC x64
  toolset, and a Windows 10/11 SDK. The repository does not pin an MSVC or SDK
  patch version; use a current Visual Studio 2022 installation. Run commands
  from **Developer PowerShell for VS 2022** so `cl.exe` and the SDK are on
  `PATH`.
- CMake 3.28 or newer and Ninja. The top-level `CMakeLists.txt` asks for CMake
  3.25, but `CMakePresets.json` requires 3.28 and selects Ninja Multi-Config.
- Qt 6.8 or newer, built for MSVC 2022 x64. CI currently builds with Qt 6.11.2
  and installs the `qtimageformats` and `qtnetworkauth` modules. Required Qt
  components are Concurrent, Core, CoreTools, Network, NetworkAuth, OpenGL,
  Test, Widgets and Xml. A Qt installation under `C:\Qt` is found by the
  project's CMake hints.
- Git with submodule support. The `cmake/vcpkg` submodule provides vcpkg; CMake
  manifest mode installs the native dependencies from `vcpkg.json` (ECM, cmark,
  libarchive with bzip2/lz4/lzma/lzo/zstd, libqrencode, tomlplusplus 3.2.0 or
  newer, and zlib).
- JDK 17 is provisioned in CI. The CMake project does not use `find_package(Java)`;
  this is the CI reference version, not a separately checked CMake dependency.
- To make the installer: NSIS with `makensis` on `PATH`. The repository does
  not pin an NSIS version. The packaging workflow additionally downloads the
  NScurl plugin v24.9.26.122 and verifies its SHA-256 before use.

## Configure, build and test

Open Developer PowerShell for VS 2022, change to the repository root, and run:

```powershell
git submodule update --init --recursive
.\cmake\vcpkg\bootstrap-vcpkg.bat -disableMetrics

$env:ARTIFACT_NAME = "Windows-MSVC-Qt6"
$env:BUILD_PLATFORM = "local"

cmake --preset ci `
  -D VCPKG_HOST_TRIPLET=x64-windows `
  -D VCPKG_TARGET_TRIPLET=x64-windows `
  -D Launcher_APP_BINARY_NAME=ZyronClient
cmake --build --preset ci --config Release
ctest --preset ci --build-config Release
cmake --install build --config Release
cmake --install build --config Release --component Runtime --prefix (Join-Path $PWD "dist")
cmake --install build --config Release --component bundle --prefix (Join-Path $PWD "dist")
```

The preset uses vcpkg's manifest mode and the `x64-windows` triplet. If Qt is
not found automatically, point CMake at its package config (adjust the
installation path if needed):

```powershell
cmake --preset ci `
  -D VCPKG_HOST_TRIPLET=x64-windows `
  -D VCPKG_TARGET_TRIPLET=x64-windows `
  -D Qt6_DIR="C:\Qt\6.11.2\msvc2022_64\lib\cmake\Qt6"
```

The CI `ci` preset treats warnings as errors. Build and CTest commands above
use the same preset and Release configuration as CI.
On Windows, the default executable target/output is `ZyronClient.exe`; other
platforms retain the upstream `prismlauncher` binary name. The two component
install commands stage the application and its runtime dependencies (including
required Qt plugins and DLLs) under `dist`, without optional PDB install rules.
They do not create an installer or prove the staged application starts.
The existing `Build` GitHub Actions workflow performs a Windows x64 startup
smoke test and uploads `ZyronClient-Windows-x64-Release-<commit>.zip` when run
with the `Release` build type. That archive also contains Microsoft's signed
x64 Visual C++ Redistributable installer for systems that do not already have
the runtime. In GitHub Actions, download it from the successful
`Build (Windows-MSVC)` job's artifacts.

## Build the NSIS installer

After a successful configure, build, and install, run the following from the
repository root. These commands mirror the Windows packaging workflow, including
its pinned NScurl download and checksum:

```powershell
$nscurlVersion = "v24.9.26.122"
$nscurlSha256 = "AEE6C4BE3CB6455858E9C1EE4B3AFE0DB9960FA03FE99CCDEDC28390D57CCBB0"
New-Item -ItemType Directory -Force -Path "NSISPlugins" | Out-Null
Invoke-WebRequest `
  -Uri "https://github.com/negrutiu/nsis-nscurl/releases/download/$nscurlVersion/NScurl.zip" `
  -OutFile "NSISPlugins\NScurl.zip"
if ((Get-FileHash "NSISPlugins\NScurl.zip" -Algorithm SHA256).Hash -ne $nscurlSha256) {
    throw "NScurl.zip SHA-256 mismatch"
}
Expand-Archive -Path "NSISPlugins\NScurl.zip" -DestinationPath "NSISPlugins\NScurl" -Force

Push-Location "install"
try {
    makensis -NOCD "..\build\program_info\win_install.nsi"
} finally {
    Pop-Location
}
```

The current NSIS template names the installer `PrismLauncher-Setup.exe` to match
the existing packaging workflow; its displayed product and Windows install
identity are Zyron Client, and it installs `ZyronClient.exe`. Run the installer
interactively to install and launch the application. A successful build or
installer creation alone does not verify Minecraft installation, Microsoft
authentication, or game launch.
