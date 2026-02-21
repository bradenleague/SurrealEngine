# RmlUI/ — HTML/CSS UI Subsystem

Modern UI layer using [RmlUi](https://github.com/mikke89/RmlUi) as an alternative to the legacy UWindow system.

## Architecture

```
RmlUIManager           ← coordinator, owns lifecycle
RmlUISystemInterface   ← time (engine->TotalTime) + logging
RmlUIFileInterface     ← sandboxed file I/O via Utils/File.h
RmlUIRenderInterface   ← texture/geometry management, calls DrawUITriangles()
```

## Lifecycle

1. **Init:** Created in `Engine::Run()` after `RenderSubsystem`. Checks if `{gameRootFolder}/UI/` exists; if absent, skips silently (zero overhead).
2. **Update:** Called each frame in the main loop after `UpdateInput()`.
3. **Render:** Called in `RenderSubsystem::DrawGame()` after `DrawRootWindow()`, before `PostRender()`.
4. **Shutdown:** Called in `Engine::~Engine()` before `engine = nullptr`.

## Engine.h Member Ordering

`std::unique_ptr<RmlUIManager> rmlui` is declared **after** `render` in Engine.h. This ensures correct destruction order: rmlui is destroyed before the render subsystem.

## Render Pipeline Position

```
DrawScene() → RenderOverlays() → EndFlash() → DrawRootWindow() → [RmlUI Render] → PostRender()
```

RmlUI renders after the UWindow tree but before PostRender, so it overlays on top of the legacy UI.

## RenderDevice Integration

`DrawUITriangles()` is a virtual method on `RenderDevice` (default no-op). The Vulkan implementation converts screen-space `UIVertex` positions to view-space using the `ScreenToView()` helper (shared with `DrawTile`). Uses `PF_Highlighted` pipeline for alpha blending.

## File Sandbox

`RmlUIFileInterface` resolves all paths relative to the `UI/` root directory. Paths containing `..` are rejected with a log warning.

## How to Add UI to a Game

```
{gameRootFolder}/UI/          ← engine checks for this directory
├── index.rml                 ← entry document, auto-loaded on init
├── fonts/                    ← .ttf/.otf files, auto-loaded on init
│   └── YourFont.ttf
└── styles/                   ← optional, for shared RCSS stylesheets
```

## Skills

When editing `.rml`/`.rcss` files, RmlUI C++ interfaces, or RenderDevice UI drawing code, use the `/surreal-ui` skill. It documents critical RCSS differences from CSS (no user-agent stylesheet, no `border: solid`, no `position: fixed`, etc.) and current SurrealEngine integration status.

## Phase 1 Limitations

- No CSS transforms (`SetTransform` not implemented)
- No `LoadTexture` (image files — fonts work via `GenerateTexture`)
- No input routing (mouse/keyboard not forwarded to RmlUi)
- No RmlUi Debugger integration
- No hot-reload (restart engine to see UI changes)
- D3D11 backend has no `DrawUITriangles` implementation

## Completed (Phase 2A)

- GPU scissor region enforcement (`SetUIScissorRegion` on RenderDevice, wired through `RmlUIRenderInterface`)

## Phase 2 Roadmap

- Input forwarding (mouse clicks, keyboard, scroll)
- `LoadTexture` for image assets
- CSS transform support (`SetTransform`)
- RmlUi Debugger toggle
- Hot-reload (file watcher)
- Data bindings for game state (health, ammo, etc.)
