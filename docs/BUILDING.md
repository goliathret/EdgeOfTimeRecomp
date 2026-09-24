# Building

> [!CAUTION]
> The public repository doesn't contain the native renderer yet. A build from it recompiles the game code but **won't display a picture**. These instructions describe the full project's build, and they'll apply to this repository once the renderer is published.

> [!IMPORTANT]
> No game files are included. You need `Default.xex` and `Data/GameLogic.dll` from your own copy of the game, with the **title update applied**. See [Game Files](#1-game-files).

## Table of Contents

- [Requirements](#requirements)
- [1. Game Files](#1-game-files)
- [2. Clone the Repository](#2-clone-the-repository)
- [3. Build](#3-build)
    - [Windows](#windows)
    - [Linux (including Steam Deck)](#linux-including-steam-deck)
    - [macOS](#macos)
    - [Rebuilding](#rebuilding)
- [Build Output](#build-output)
- [Packaging for Linux & macOS](#packaging-for-linux--macos)

## Requirements

All platforms|Version
-|-
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)|0.10.0 (pinned in `reeot_manifest.toml`)
CMake|3.25 or newer
Ninja|Any recent version
Python|3.x
Clang|20 or newer
Git|Needed for the submodules

**Windows:**
- Visual Studio 2022 with the **C++ Clang tools for Windows** (LLVM) component.
- The D3D12 renderer uses the DirectX Shader Compiler. Its `dxcompiler.dll` and `dxil.dll` are copied next to the executable during the build.

**Linux:**
- `clang` and `lld` from LLVM 19 or newer. LLVM 20 is recommended to match the SDK.
- The Vulkan loader: `libvulkan1` on Debian/Ubuntu, or `vulkan-icd-loader` on Arch.
- GTK 3 headers: `libgtk-3-dev` on Debian/Ubuntu.
- If your system has no Vulkan headers, the SDK's bundled copy is used.

**macOS:**
- Xcode Command Line Tools.
- Latest Vulkan SDK that includes MoltenVK. The Vulkan renderer runs on Metal through MoltenVK.

## 1. Game Files

The recompiler reads all files from your copy of the game. The two files that are most important for the code recompiler via ReX's Codegen are the `Default.xex` and the `GameLogic.dll`.

**The title update is required.** The port is generated from the *patched* executable, so both files need the title update applied. A clean disc copy of `Default.xex`/`GameLogic.dll` won't match the function addresses in the config files. Beenox placed all the separate Shader Containers into each `.pkz` file, meaning you will also have to decompress via the bms script, and then run the shader recompiler on the entire folder. This will take a severely large amount of time to compile due to the stress of scanning every single decompressed pack (upwards of 7 GB of data)

The `assets/` folder is ignored by git, so these files can never be committed by accident.

## 2. Clone the Repository

Clone recursively so the submodules come with it:

```bash
git clone --recursive https://github.com/goliathret/EdgeOfTimeRecomp.git
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

The build expects the ReXGlue SDK source tree **next to** this repository:

```
Github/
  EdgeOfTimeRecomp/
  rexglue-sdk/     <- ReXGlue SDK source
```

To keep the SDK somewhere else, pass `-DREXSDK_DIR=<path>` when configuring.

## 3. Build

There are no build scripts; every platform builds by hand in the same three steps:

1. Generate the recompiled code with `rexglue codegen`.
2. Configure with a CMake preset.
3. Build with the matching build preset.

Run all commands from the repository root. Swap `release` for `debug` or `relwithdebinfo` to get a different configuration (see [CMake Presets](#cmake-presets)).

### Windows

From a **Developer Command Prompt for VS 2022**:

```bat
rexglue codegen reeot_manifest.toml
cmake --preset win-amd64-release -DREXSDK_DIR=C:/path/to/rexglue-sdk
cmake --build --preset win-amd64-release --parallel
```

The renderer is D3D12 (`REEOT_D3D12=ON` is the default on Windows).

### Linux (including Steam Deck)

```bash
rexglue codegen reeot_manifest.toml
cmake --preset linux-amd64-release -DREXSDK_DIR=/path/to/rexglue-sdk
cmake --build --preset linux-amd64-release --parallel
```

Use the `linux-arm64-*` presets on ARM machines. The renderer is Vulkan.

### macOS

```bash
rexglue codegen reeot_manifest.toml
cmake --preset mac-arm64-release -DREXSDK_DIR=/path/to/rexglue-sdk
cmake --build --preset mac-arm64-release --parallel
```

The renderer is Vulkan, through MoltenVK.

### Rebuilding

You only need to run `rexglue codegen` again when the game files or the `config/*.toml` files change. After changing hand-written sources, just run the `cmake --build` step.

## CMake Presets

Every preset uses the Ninja generator and builds into `out/build/<preset>`.

Platform|Presets
-|-
Windows x64|`win-amd64-debug`, `win-amd64-release`, `win-amd64-relwithdebinfo`
Linux x64|`linux-amd64-debug`, `linux-amd64-release`, `linux-amd64-relwithdebinfo`
Linux ARM64|`linux-arm64-debug`, `linux-arm64-release`, `linux-arm64-relwithdebinfo`
macOS Apple Silicon|`mac-arm64-debug`, `mac-arm64-release`, `mac-arm64-relwithdebinfo`

- x64 builds target `x86-64-v2`, and ARM builds target `armv8-a`.
- Linux presets link with LLD.
- `relwithdebinfo` is the configuration used for testing. It's optimized but keeps enough debug information for useful crash reports.


## How Code Generation Works

`reeot_manifest.toml` lists the two modules to recompile.

Module|Input|Output|Config files
-|-|-|-
Executable|`assets/Default.xex`|`generated/default`|`config/reeot_default_xex*.toml`, `config/hooks/default_xex_crt.toml`
Game logic|`assets/Data/GameLogic.dll`|`generated/gamelogic`|`config/reeot_gamelogic*.toml`, `config/hooks/gamelogic_crt.toml`

For each module, `rexglue codegen`:
1. Reads the PowerPC machine code from the input.
2. Splits it into functions using the boundaries in the config files.
3. Writes one C++ function per guest function into the output folder.

The config files name the functions and mark which ones are replaced by hand-written code (hooks).

The generated code is compiled like any other source. `GameLogic.dll` becomes its own shared library, `reeot_GameLogic`, which the executable loads the way the Xbox 360 loaded the DLL.

A stamp file, `generated/default/codegen.build.stamp`, records when code generation last ran, so CMake only runs it again when the inputs change.

### Shader cache

The game's Xbox 360 shaders are translated ahead of time into `generated/shader_cache.cpp`. That file is regenerated manually, because it takes about ten minutes:

```bash
cmake --build --preset <preset> --target reeot_shader_cache
```

If the file is missing, CMake prints a warning and builds a stub instead, and the game can't draw.

## Build Output

Everything lands in `out/build/<preset>/`:

File|Platform
-|-
`reeot.exe` / `reeot`|All
The ReXGlue runtime|All
`reeot_GameLogic` (`.dll` / `.so` / `.dylib`)|All
`gamecontrollerdb.txt`|All (controller mappings)
`dxcompiler.dll`, `dxil.dll`|Windows
`reeot_icon.png`, `build_stamp.txt`|Linux, macOS
`program_files.txt`|All (the list of files a release needs)

Run the executable from that folder. On first launch, the [installer](/README.md#how-to-install) asks for your game files. Developers can skip it with `--game_data_root <path to extracted game>`.


## Packaging for Linux & macOS

Windows builds are shared as a zip of the build folder. On Linux and macOS, the build folder can be wrapped into a single file that's easier to hand out:
- **Linux:** an AppImage.
- **macOS:** an app bundle inside a disk image.

Both are put together by hand from the files listed in `out/build/<preset>/program_files.txt`. These dedicated binaries make it easy for any Unix based device to easily manage their application with little-to-no hassle.