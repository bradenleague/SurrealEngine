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

CI builds on Windows (MSVC), Linux (GCC 12, Clang 15) via GitHub Actions. A lightweight test executable `TestRmlUI` validates the RmlUI subsystem:

```bash
./build/TestRmlUI
```

## Running

```bash
# Launch with game folder (from repo root)
./build/SurrealEngine [--url=<mapname>] [--engineversion=X] [Path to game folder]

# Unreal Gold (installed locally)
./build/SurrealEngine ~/dev/games/UnrealGold

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
- **TestRmlUI** — lightweight test executable for RmlUI subsystem (`Tests/TestRmlUI.cpp`)

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

- **RmlUI** (`RmlUI/`) — HTML/CSS UI layer using RmlUi library. Conditionally initializes when a game provides a `UI/` folder. Manager + three interfaces (System, File, Render). Renders via `DrawUITriangles()` on `RenderDevice`. See `SurrealEngine/RmlUI/CLAUDE.md` for details.

### Third-Party Libraries (under `Thirdparty/`)
- **ZVulkan** — Vulkan rendering abstraction
- **ZWidget** — Custom UI toolkit (Wayland/X11/SDL2 backends)
- **openmpt** — Tracker music playback
- **openal-soft** — OpenAL (bundled for Windows only; system lib on Linux)
- **RmlUi** — HTML/CSS UI library (static, core only; freetype2 required)
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
- No general test suite or formatter. Compilation is the primary gate. `TestRmlUI` covers the RmlUI subsystem.
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

## Skills

- **`/surreal-ui`** — Use when editing `.rml`/`.rcss` files, RmlUI C++ interfaces, or RenderDevice UI drawing code. Covers critical RCSS differences from CSS and SurrealEngine integration patterns.

## Subsystem Documentation

Nested `CLAUDE.md` files provide deeper context for agents working in specific areas:

- `SurrealEngine/Native/CLAUDE.md` — native function registration, index constraints
- `SurrealEngine/Render/CLAUDE.md` — rendering pipeline, Canvas, RenderDevice
- `SurrealEngine/VM/CLAUDE.md` — bytecode interpreter, CallEvent, ExpressionValue
- `SurrealEngine/UObject/CLAUDE.md` — object model, UWindow hierarchy, UCanvas properties
- `SurrealEngine/RmlUI/CLAUDE.md` — RmlUi integration, lifecycle, interfaces, phase roadmap

## UnrealScript Source (Unreal-PubSrc)

A hard fork of [OldUnreal/Unreal-PubSrc](https://github.com/OldUnreal/Unreal-PubSrc) lives at `~/dev/Unreal-PubSrc/` (branch `surreal`). It contains the full v227 UnrealScript source for Unreal Gold plus our custom `SurrealUI` package.

### Compile workflow

```bash
cd ~/dev/Unreal-PubSrc
./build.sh          # copies source + wine UCC.exe make → produces .u in game System/
./build.sh clean    # delete .u and recompile
```

Requires Wine (`sudo pacman -S wine`). Compiles against `~/dev/games/UnrealGold` (v226).

### Key directories

- `~/dev/Unreal-PubSrc/SurrealUI/Classes/` — our custom UnrealScript package
- `~/dev/Unreal-PubSrc/Engine/Classes/` — base Engine classes (HUD, Canvas, Console, etc.)
- `~/dev/Unreal-PubSrc/UnrealShare/Classes/` — Unreal Gold gameplay (UnrealHUD, menus, scoreboard)

See `~/dev/Unreal-PubSrc/CLAUDE.md` for full details.

## Supported Games

Primary targets (relatively playable): UT99 v436, Unreal Gold v226. Many other UE1 games are detected but have varying levels of functionality. See `Docs/Status.md` for details.

## UI Content

The RmlUI system is developed in the game's `UI/` folder. Our active content directory:

```
~/dev/games/UnrealGold/UI/
├── hud.rml                ← HUD overlay (auto-shown on init)
├── messages.rml           ← toast messages (stub, hidden by default)
├── scoreboard.rml         ← scoreboard panel (stub, hidden by default)
├── console.rml            ← sliding console (stub, hidden by default)
├── menu.rml               ← game menu (stub, hidden by default)
├── fonts/                 ← LatoLatin, SpaceGrotesk, SpaceMono, DejaVuSans, OpenSans
└── styles/
    ├── base.rcss          ← reset/block-display defaults
    ├── hud.rcss           ← HUD layout and styling
    ├── messages.rcss      ← message toast styling (stub)
    ├── scoreboard.rcss    ← scoreboard panel styling (stub)
    ├── console.rcss       ← console panel styling (stub)
    └── menu.rcss          ← menu overlay styling (stub)
```

Each document is optional — only present files are loaded. Changes take effect on next launch (or via `reloadui` console command). Use `/surreal-ui` when editing these files.

## UI Replacement Plan

We are replacing all UE1 script-rendered UI (HUD, messages, scoreboard, console, menus) with RmlUI. Phase 3 (infrastructure) is complete. See `Docs/RmlUI-Replacement-Plan.md` for the full plan, phase status, and dependency graph.

Console commands for development:
- `togglehud` — toggle RmlUI HUD (script HUD reappears when disabled)
- `togglemenu` — toggle RmlUI menu
- `reloadui` — reinitialize all RmlUI documents from disk
