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
3. **Render:** Called in `RenderSubsystem::DrawGame()` after `PostRender()`, before `Device->Unlock()`. This ensures RmlUI draws on top of all script UI.
4. **Shutdown:** Called in `Engine::~Engine()` before `engine = nullptr`.

## Engine.h Member Ordering

`std::unique_ptr<RmlUIManager> rmlui` is declared **after** `render` in Engine.h. This ensures correct destruction order: rmlui is destroyed before the render subsystem.

## Render Pipeline Position

```
DrawScene() → RenderOverlays() → EndFlash() → DrawRootWindow() → PostRender() → [RmlUI Render]
```

RmlUI renders after PostRender, so it always draws on top of all script-rendered UI and UWindow.

## RenderDevice Integration

`DrawUITriangles()` is a virtual method on `RenderDevice` (default no-op). The Vulkan implementation converts screen-space `UIVertex` positions to view-space using the `ScreenToView()` helper (shared with `DrawTile`). Uses `PF_Highlighted` pipeline for alpha blending.

## File Sandbox

`RmlUIFileInterface` resolves all paths relative to the `UI/` root directory. Paths containing `..` are rejected with a log warning.

## How to Add UI to a Game

```
{gameRootFolder}/UI/          ← engine checks for this directory
├── hud.rml                   ← HUD overlay (auto-shown on init)
├── messages.rml              ← toast messages (hidden by default)
├── scoreboard.rml            ← scoreboard panel (hidden by default)
├── console.rml               ← sliding console (hidden by default)
├── menu.rml                  ← game menu (hidden by default)
├── fonts/                    ← .ttf/.otf files, auto-loaded on init
│   └── YourFont.ttf
├── styles/                   ← shared RCSS stylesheets
│   └── base.rcss             ← recommended base stylesheet (see below)
└── images/                   ← optional, for <img> tags and backgrounds
```

Each document is optional — only present files are loaded. The HUD document is shown immediately; all others start hidden and are shown/hidden via `ShowDocument()`/`HideDocument()`/`ToggleDocument()`.

## UI Suppression

`UISuppressionFlags` in `Engine.h` controls per-surface suppression of script-rendered UI:
- `bRmlHUD` — when true, skips `actor->PostRender()` (HUD/scoreboard drawing)
- `bRmlMessages` — when true, skips `console->PostRender()` toast messages
- `bRmlScoreboard` — reserved for scoreboard document
- `bRmlConsole` — when true, skips `console->PostRender()` console drawing
- `bRmlMenus` — reserved for menu document

Flags are automatically set when documents load. The `togglehud` console command toggles the HUD flag.

## Console Commands

- `togglehud` — show/hide the RmlUI HUD (re-enables script HUD when hidden)
- `togglemenu` — show/hide the RmlUI menu
- `reloadui` — shutdown and reinitialize RmlUI (reloads all documents)

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

**Gating:** `Engine::ShouldRouteInputToUI()` returns true when `rmlui` is initialized AND either `viewport->bShowWindowsMouse()` is true OR `HasActiveInteractiveDocument()` is true (menu/console/scoreboard visible). The cursor is automatically unlocked when an interactive document is active.

**Mouse position:** Always forwarded (no gate) for hover tracking. Click/wheel/key events are gated.

**Return-value inversion:** RmlUi's `Process*` methods return `true` = NOT consumed. Our wrappers invert this so `true` = consumed, which matches the engine's early-return pattern.

**Key modifier state:** Queried from `engine->window->GetKeyState()` for Ctrl/Shift/Alt/CapsLock/NumLock/ScrollLock, OR'd into `Rml::Input::KM_*` flags.

## Data Model Bridge

The `HUDViewModel` struct provides a UObject-free data transfer layer between `Engine.cpp` (which has full UObject access) and `RmlUIManager` (which stays UObject-free).

**Flow:** Engine extracts game state from `UPawn` → populates `HUDViewModel` → calls `RmlUIManager::UpdateHUDData()` → manager diffs against cached values → calls `DirtyVariable()` for changed fields.

**Available variables in RML** (data-model="hud"):
- `{{ health }}` — int, pawn health
- `{{ health_max }}` — int, max health (default 100)
- `{{ armor }}` — int, sum of all armor inventory charges
- `{{ ammo }}` — int, current weapon charge
- `{{ weapon_name }}` — string, current weapon ItemName
- `{{ player_name }}` — string, from PlayerReplicationInfo
- `{{ score }}` — float, from PlayerReplicationInfo
- `{{ deaths }}` — float, from PlayerReplicationInfo
- `{{ has_weapon }}` — bool, whether pawn has a weapon equipped
- `{{ frag_count }}` — int, score cast to int
- `{{ crosshair }}` — int, crosshair index (0-6) from HUD actor
- `{{ hud_mode }}` — int, HUD mode (0-5) from HUD actor
- `{{ weapon_slots }}` — array of WeaponSlot structs (10 elements)

**WeaponSlot struct members** (for `data-for` loops):
- `slot.occupied` — bool, weapon present in this slot
- `slot.selected` — bool, currently equipped weapon
- `slot.name` — string, weapon ItemName
- `slot.ammo` — int, weapon charge

**Example RML:**
```html
<div data-model="hud">
    <span>{{ health }}</span>
    <span data-if="has_weapon">{{ weapon_name }} ({{ ammo }})</span>
    <div data-for="slot : weapon_slots">
        <span data-if="slot.occupied">{{ slot.name }} ({{ slot.ammo }})</span>
    </div>
</div>
```

### Messages Data Model (data-model="messages")

**Flow:** Engine reads console ring buffer (MsgText/MsgTick/MsgType arrays) → populates `MessagesViewModel` → calls `RmlUIManager::UpdateMessagesData()` → manager diffs and dirties.

**Available variables:**
- `{{ messages }}` — array of MessageEntry structs (up to 4 visible)
- `{{ is_typing }}` — bool, whether player is typing
- `{{ typed_string }}` — string, current typed text

**MessageEntry struct members** (for `data-for` loops):
- `msg.text` — string, message text
- `msg.type` — string, message type name ("Say", "TeamSay", "CriticalEvent", "Console")
- `msg.color` — string, CSS color mapped from type ("#44dd66"=Say, "#4488ff"=Critical, "#dddddd"=default)
- `msg.time_remaining` — float, seconds until message expires

**Example RML:**
```html
<div data-model="messages">
    <div data-for="msg : messages">
        <span data-style-color="msg.color">{{ msg.text }}</span>
    </div>
    <div data-if="is_typing">
        <span>&gt; {{ typed_string }}</span>
    </div>
</div>
```

### Console PostRender Suppression

Console PostRender handles both `DrawSingleView` (toast messages) and `DrawConsoleView` (console panel). The suppression gate uses AND: both `bRmlMessages` AND `bRmlConsole` must be true to suppress. This allows partial replacement (Phase 5 messages without Phase 7 console) without breaking the console view.

## Skills

When editing `.rml`/`.rcss` files, RmlUI C++ interfaces, or RenderDevice UI drawing code, use the `/surreal-ui` skill. It documents critical RCSS differences from CSS (no user-agent stylesheet, no `border: solid`, no `position: fixed`, etc.) and current SurrealEngine integration status.

## Completed

- **Phase 1:** Static rendering (geometry, textures, fonts via GenerateTexture)
- **Phase 2A:** GPU scissor region enforcement
- **Phase 2B:** Input routing (mouse, keyboard, scroll, text input)
- **Phase 2B.2:** `LoadTexture` via stb_image (PNG, JPG, BMP, GIF, TGA, PSD)
- **Phase 2C:** Base stylesheet template
- **Phase 2D:** HUD data model bridge (health, armor, ammo, weapon, player info)
- **Phase 3:** Infrastructure — multi-document, suppression flags, console commands, render reorder
- **Phase 4:** HUD expansion — weapon inventory bar, crosshair, frag count, struct array data model
- **Phase 5:** Messages replacement — toast messages, typing indicator, data-style-color, AND suppression logic
- **Phase 6:** Scoreboard replacement — Tab-key panel, PlayerReplicationInfo iteration, score sorting
- **Phase 7:** Console replacement — tilde-key panel, log mirror, UScript state passthrough

## Remaining Roadmap
- **Phase 6:** Scoreboard replacement
- **Phase 7:** Console replacement
- **Phase 8:** Menu replacement
- **Phase 9:** UWindow retirement
- CSS transform support (`SetTransform`)
- RmlUi Debugger toggle
- Hot-reload (file watcher)
- D3D11 backend `DrawUITriangles` implementation
