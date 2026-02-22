# RmlUI/ — HTML/CSS UI Subsystem

Modern UI layer using [RmlUi](https://github.com/mikke89/RmlUi) as an alternative to the legacy UWindow system.

## Architecture

```
RmlUIManager           ← coordinator, owns lifecycle + input routing + data model
RmlUISystemInterface   ← time (engine->TotalTime) + logging
RmlUIFileInterface     ← sandboxed file I/O via Utils/File.h
RmlUIRenderInterface   ← texture/geometry management, calls DrawUITriangles()
```

## Lifecycle

1. **Init:** Created in `Engine::Run()` after `RenderSubsystem`. Checks if `{gameRootFolder}/UI/` exists; if absent, skips silently (zero overhead). Sets up HUD data model before loading documents.
2. **Update:** Called each frame in the main loop after `UpdateInput()` and HUD data extraction.
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
├── styles/                   ← optional, for shared RCSS stylesheets
│   └── base.rcss             ← recommended base stylesheet (see below)
└── images/                   ← optional, for <img> tags and backgrounds
```

## Base Stylesheet Template

RmlUi has **no user-agent stylesheet**. All elements default to `display: inline`. Without a base stylesheet, `<div>` elements collapse and block layout breaks.

Games should include `styles/base.rcss` via `<link type="text/rcss" href="styles/base.rcss"/>` in their `index.rml`:

```css
body { display: block; width: 100%; height: 100%; overflow: hidden; }
div, p, h1, h2, h3, h4, section, header, footer, nav { display: block; }
h1 { font-size: 2em; } h2 { font-size: 1.5em; } h3 { font-size: 1.2em; }
p { margin: 0.5em 0; }
img { display: inline-block; }
```

## Input Routing

Input flows from `Engine::OnWindow*` handlers → `RmlUIManager::Process*` → `Rml::Context::Process*`.

**Gating:** `Engine::ShouldRouteInputToUI()` returns true when `rmlui` is initialized AND `viewport->bShowWindowsMouse()` is true (cursor visible = menu mode). During gameplay, the cursor is locked via SDL relative mode, so `OnWindowMouseMove` doesn't fire — only `OnWindowRawMouseMove` (deltas). Combined with the `bShowWindowsMouse` gate, RmlUI is fully silent during gameplay.

**Mouse position:** Always forwarded (no gate) for hover tracking. Click/wheel/key events are gated.

**Return-value inversion:** RmlUi's `Process*` methods return `true` = NOT consumed. Our wrappers invert this so `true` = consumed, which matches the engine's early-return pattern.

**Key modifier state:** Queried from `engine->window->GetKeyState()` for Ctrl/Shift/Alt/CapsLock/NumLock/ScrollLock, OR'd into `Rml::Input::KM_*` flags.

## Data Model Bridge

The `HUDViewModel` struct provides a UObject-free data transfer layer between `Engine.cpp` (which has full UObject access) and `RmlUIManager` (which stays UObject-free).

**Flow:** Engine extracts game state from `UPawn` → populates `HUDViewModel` → calls `RmlUIManager::UpdateHUDData()` → manager diffs against cached values → calls `DirtyVariable()` for changed fields.

**Available variables in RML** (data-model="hud"):
- `{{ health }}` — int, pawn health
- `{{ armor }}` — int, sum of all armor inventory charges
- `{{ ammo }}` — int, current weapon charge
- `{{ weapon_name }}` — string, current weapon ItemName
- `{{ player_name }}` — string, from PlayerReplicationInfo
- `{{ score }}` — float, from PlayerReplicationInfo
- `{{ deaths }}` — float, from PlayerReplicationInfo
- `{{ has_weapon }}` — bool, whether pawn has a weapon equipped

**Example RML:**
```html
<div data-model="hud">
    <span>{{ health }}</span>
    <span data-if="has_weapon">{{ weapon_name }} ({{ ammo }})</span>
</div>
```

## Skills

When editing `.rml`/`.rcss` files, RmlUI C++ interfaces, or RenderDevice UI drawing code, use the `/surreal-ui` skill. It documents critical RCSS differences from CSS (no user-agent stylesheet, no `border: solid`, no `position: fixed`, etc.) and current SurrealEngine integration status.

## Completed

- **Phase 1:** Static rendering (geometry, textures, fonts via GenerateTexture)
- **Phase 2A:** GPU scissor region enforcement
- **Phase 2B:** Input routing (mouse, keyboard, scroll, text input)
- **Phase 2B.2:** `LoadTexture` via stb_image (PNG, JPG, BMP, GIF, TGA, PSD)
- **Phase 2C:** Base stylesheet template
- **Phase 2D:** HUD data model bridge (health, armor, ammo, weapon, player info)

## Remaining Roadmap

- CSS transform support (`SetTransform`)
- RmlUi Debugger toggle
- Hot-reload (file watcher)
- D3D11 backend `DrawUITriangles` implementation
- Extended data models (scoreboard, inventory list, etc.)
