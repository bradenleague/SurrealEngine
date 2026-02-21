# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SurrealEngine is a C++20 reimplementation of the Unreal Engine 1 runtime. It targets classic UE1 games (Unreal Gold, UT99, Deus Ex, etc.) on modern systems. Rendering uses Vulkan across platforms (including Windows), with an additional D3D11 backend on Windows. The UnrealScript VM is mostly feature-complete, but dynamic arrays, network conditionals, and a few less-common opcodes are still missing.

## Build Commands

```bash
# Configure (from repo root)
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build (from build/)
make -j$(nproc)

# Linux dependencies (Arch)
pacman -S libx11 gcc git cmake sdl2 alsa-lib waylandpp
```

Produces three executables in `build/`: `SurrealEngine`, `SurrealEditor`, `SurrealDebugger`.

A `SurrealEngine.pk3` resource file is copied alongside the game executable post-build.

There is no test suite. CI builds on Windows (MSVC), Linux (GCC 12, Clang 15) via GitHub Actions.

## Running

```bash
# Launch with game folder
./SurrealEngine [--url=<mapname>] [--engineversion=X] [Path to game folder]

# If placed in a game's System/ folder, auto-detects the game
```

Game identification works by SHA1-hashing the game executable against `UE1GameDatabase.h`.

## Architecture

### Build Targets
- **SurrealCommon** — static library containing all engine code
- **SurrealEngine** — game executable (links SurrealCommon)
- **SurrealEditor** — map editor executable
- **SurrealDebugger** — UnrealScript debugger executable
- **SurrealVideo** — shared library for video codecs (Indeo5/ADPCM from FFmpeg)

### Core Subsystems (under `SurrealEngine/`)

- **Engine** (`Engine.h/cpp`) — Central coordinator. Owns PackageManager, GameWindow, RenderSubsystem. Manages map loading, input, camera, console commands, client travel. Global `engine` pointer.

- **Package** (`Package/`) — Loads UE1 package formats (.u, .utx, .umx, .uax, .unr). `PackageManager` handles search paths and INI files. `PackageStream`/`ObjectStream` handle binary deserialization.

- **UObject** (`UObject/`) — UE1 object model with reflection/serialization. Key classes: `UObject` (base), `UClass`, `UProperty`, `UActor`, `ULevel`, `UTexture`, `UMesh`, `USound`, `UMusic`, `UFont`, `UWindow`, `UCanvas`.

- **VM** (`VM/`) — UnrealScript bytecode interpreter. `Frame` manages call stack and latent actions. `Bytecode` decodes opcodes. `ExpressionEvaluator` handles expression execution. Supports debugger breakpoints and stepping.

- **Native** (`Native/`) — C++ implementations of built-in UnrealScript functions (60+ files). Each file is a static class (e.g., `NActor`, `NPawn`, `NCanvas`) with a `RegisterFunctions()` method that binds native function indices to C++ implementations. Game-specific extensions exist (for example Deus Ex and Unreal/UPak variants).

- **Render** (`Render/`) — High-level rendering: BSP clipping (`BspClipper`), lightmaps, fog, canvas drawing, scene rendering. `Visible*` classes encapsulate renderable elements.

- **RenderDevice** (`RenderDevice/`) — Low-level GPU abstraction. `Vulkan/` subdirectory for Vulkan backend, `D3D11/` for Windows D3D11 backend. Uses ZVulkan wrapper library.

- **Collision** (`Collision/`) — `TopLevel/` has `CollisionSystem`, `TraceTest`, `OverlapTest`. `BottomLevel/` has ray/AABB models for geometry intersection.

- **Audio** (`Audio/`) — OpenAL-based spatial audio. Uses dr_wav, minimp3, stb_vorbis, dr_flac for decoding. OpenMPT for tracker music.

- **GC** (`GC/`) — Mark-and-sweep garbage collection for UObject instances.

- **UI** (`UI/`) — Built on ZWidget. `Launcher/` for game selection, `Editor/` for map editor viewports, `ErrorWindow/` for crash display.

### Third-Party Libraries (under `Thirdparty/`)
- **ZVulkan** — Vulkan rendering abstraction
- **ZWidget** — Custom UI toolkit (Wayland/X11/SDL2 backends)
- **openmpt** — Tracker music playback
- **openal-soft** — OpenAL (bundled for Windows only; system lib on Linux)
- Header-only audio codecs, miniz (compression), MurmurHash3, TinySHA1

### Key Patterns
- **Precompiled headers**: `Precomp.h` includes stdint, string, vector, map, memory, Array, Exception
- **Custom Array**: `Utils/Array.h` provides `Array<T>` used throughout instead of `std::vector`
- **Native function registration**: Each `N*.cpp` file registers functions via index-based dispatch matching UE1's native function table
- **Game version branching**: `GameLaunchInfo` (from `GameFolder.h`) has helpers like `IsUnrealTournament()`, `IsDeusEx()` for version-specific behavior
- **Global engine pointer**: `extern Engine* engine` is accessed throughout the codebase
- **Entry points**: `MainGame.cpp` (game), `MainEditor.cpp` (editor), `MainDebugger.cpp` (debugger)

## Build Hygiene

- **Always verify compilation** after refactoring or multi-file changes: `cd build && make -j$(nproc)`
- The PreToolUse hook runs a full build before any `git commit` — if it fails, the commit is blocked. Fix the build first.
- There is no test suite or formatter. Compilation is the primary gate.
- Match existing code style: tabs for indentation, Allman-ish braces (opening brace on own line for functions).

## Code Conventions

- **Includes**: `Precomp.h` first, then project headers, then system headers
- **Indentation**: Tabs (not spaces)
- **Braces**: Opening brace on new line for function/class definitions
- **Types**: Use `Array<T>` (from `Utils/Array.h`), not `std::vector`
- **Strings**: `std::string` throughout, `NameString` for interned names (UObject property/function names)
- **Casts**: `UObject::Cast<T>(obj)` for safe downcasting
- **Global state**: `extern Engine* engine` — accessible everywhere via `#include "Engine.h"`

## Git Workflow

- Ask before committing — review changes with the user first
- This is a fork of `dpjudas/SurrealEngine`. Do not push without explicit approval.
- Commit messages: concise imperative style matching existing log (e.g., "WarpZoneInfo: Initial implementations of (Un)Warp()")

## Subsystem Documentation

Nested `CLAUDE.md` files provide deeper context for agents working in specific areas:

- `SurrealEngine/Native/CLAUDE.md` — native function registration, index constraints
- `SurrealEngine/Render/CLAUDE.md` — rendering pipeline, Canvas, RenderDevice
- `SurrealEngine/VM/CLAUDE.md` — bytecode interpreter, CallEvent, ExpressionValue
- `SurrealEngine/UObject/CLAUDE.md` — object model, UWindow hierarchy, UCanvas properties

## Supported Games

Primary targets (relatively playable): UT99 v436, Unreal Gold v226. Many other UE1 games are detected but have varying levels of functionality. See `Docs/Status.md` for details.
