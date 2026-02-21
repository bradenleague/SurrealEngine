# Unreal Gold on Linux via SurrealEngine

Notes on building SurrealEngine from source on Arch Linux and running Unreal Gold (1998).

## Overview

SurrealEngine is a clean-room C++20 reimplementation of Unreal Engine 1. It loads original game package files (`.u`, `.utx`, `.unr`, etc.) and runs them natively on Linux through the current SurrealEngine runtime (no Wine layer).

OldUnreal hosts legacy installers/patches and publishes UnrealScript source repositories that are useful for debugging and modding workflows.

Note: download URLs and checksums in this document are external operational data, not derived from SurrealEngine source code, so re-verify them when using this guide.

## Build (Arch Linux)

### Dependencies

```
sudo pacman -S libx11 gcc git cmake sdl2 alsa-lib waylandpp openal libxkbcommon
```

Most of these are likely already installed. `waylandpp` is optional but recommended for Wayland/KDE setups.

### Compile

```bash
git clone https://github.com/dpjudas/SurrealEngine.git
cd SurrealEngine
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

Produces three binaries:
- `SurrealEngine` — game runtime
- `SurrealEditor` — map editor
- `SurrealDebugger` — UnrealScript debugger (can also export scripts, textures, etc.)

## Game Data

### Download

The Unreal Gold disc image is hosted by OldUnreal (~645MB):

```bash
curl -L -o UNREAL_GOLD.iso "https://files2.oldunreal.net/UNREAL_GOLD.ISO"
```

Verify checksum:
```
sha256: 7e360d0cc9e5533f38859819fd3cbfea7c475ecd428f9f433b5f8e1d5742cbca
```

Backup mirror: `https://archive.org/download/totallyunreal/UNREAL_GOLD.ISO` (requires Archive.org auth).

### Extract

The ISO can be extracted without mounting (no root required):

```bash
mkdir UnrealGold && cd UnrealGold
7z x ../UNREAL_GOLD.iso
```

### Fix Directory Case

The ISO extracts with uppercase directory names (SYSTEM, MAPS, etc.) but SurrealEngine expects mixed-case. Rename them:

```bash
cd UnrealGold
mv MAPS Maps
mv MUSIC Music
mv SOUNDS Sounds
mv SYSTEM System
mv TEXTURES Textures
mv HELP Help
mv MANUALS Manuals
mv SYSTEMLOCALIZED SystemLocalized
```

Without this you'll get: `Could not open System/Default.ini`

## Run

```bash
/path/to/SurrealEngine /path/to/UnrealGold
```

Launch a specific map:
```bash
/path/to/SurrealEngine --url=Vortex2 /path/to/UnrealGold
```

## UnrealScript Source

The compiled `.u` packages in `System/` contain bytecode, but the original UnrealScript source is embedded and can be exported via the debugger. OldUnreal also publishes the full source separately:

```bash
git clone https://github.com/OldUnreal/Unreal-PubSrc.git
```

This repository contains a large UnrealScript corpus plus native source files:

| Package | Files | Contents |
|---------|-------|---------|
| `Engine/Classes/` | ~200+ | Base classes: Actor, Pawn, PlayerPawn, GameInfo, LevelInfo, etc. |
| `UnrealShare/Classes/` | 525 | Shared gameplay: weapons, items, AI, pickups, effects |
| `UnrealI/Classes/` | 159 | Single-player campaign: monsters, scripted sequences, triggers |
| `UPak/Classes/` | 100+ | Return to Na Pali expansion content |
| `UWindow/Classes/` | — | UI framework (menus, HUD, widgets) |
| `UMenu/Classes/` | — | Menu system |
| `Fire/Classes/` | — | Procedural fire/water textures |
| `IpDrv/Classes/` | — | Networking (TCP/UDP links) |

The `.uc` files are fully readable UnrealScript — class definitions, properties, `#exec` asset import directives, and all game logic (weapon behavior, AI states, trigger systems, etc.).

### Modding Note

SurrealEngine runs the compiled bytecode from `.u` packages — it does not compile `.uc` source. To make UnrealScript changes take effect:

1. Use OldUnreal's 227 patch which includes a working Linux `ucc make` compiler
2. Compile modified `.uc` files into `.u` packages
3. Run the resulting packages in SurrealEngine

Alternatively, modify SurrealEngine's C++ source directly for engine-level changes (rendering, physics, native functions) without touching UnrealScript.

## SurrealEngine Architecture (Quick Reference)

```
SurrealEngine/
├── Engine.h/cpp          Central coordinator, game loop
├── Package/              UE1 package loader (.u, .utx, .unr, etc.)
├── UObject/              Object model, reflection, serialization
├── VM/                   UnrealScript bytecode interpreter
├── Native/               127 files — C++ native function implementations
├── Render/               Scene graph, BSP, lightmaps, fog, canvas
├── RenderDevice/Vulkan/  Vulkan backend (27 files, bindless textures)
├── RenderDevice/D3D11/   D3D11 backend (Windows only)
├── Collision/            Spatial grid broad-phase, BSP narrow-phase
├── Audio/                OpenAL spatial audio + OpenMPT tracker music
├── UI/                   Launcher + editor (ZWidget toolkit)
├── Commandlet/           Debug/export tools, VM debugger commands
├── GC/                   Mark-and-sweep garbage collector implementation
├── Math/                 vec, mat, quaternion, rotator, bbox, frustum
└── Utils/                File I/O, logging, JSON, SHA1, string utils
```

## Known Issues

- Menu cursor feels slow (engine renders its own cursor rather than using OS cursor)
- No dynamic lighting (Dispersion Pistol won't illuminate surroundings)
- AI is incomplete (enemies may behave oddly)
- Portal/mirror rendering is buggy
- Gameplay networking/replication is not implemented yet
- GC exists, but periodic collection is not currently wired into the main game loop
- Wayland: menu positioning issues possible

See `Docs/Status.md` for the full bug list.

## Related Docs

- [Status.md](Status.md) — full bug list and supported game versions
- [RmlUi-UI-Replacement.md](RmlUi-UI-Replacement.md) — technical analysis of the existing UI systems and native drawing surface
- [RmlUi-Strategy.md](RmlUi-Strategy.md) — integration strategy, data contracts, implementation order

## Links

- SurrealEngine: https://github.com/dpjudas/SurrealEngine
- OldUnreal: https://www.oldunreal.com
- UnrealScript source: https://github.com/OldUnreal/Unreal-PubSrc
- OldUnreal patches (227): https://github.com/OldUnreal/Unreal-testing
- Game installers: https://github.com/OldUnreal/FullGameInstallers
