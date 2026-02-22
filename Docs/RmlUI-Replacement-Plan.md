# Full UI Replacement Plan — SurrealEngine x RmlUI

Tracking document for the multi-phase replacement of all UE1 script-rendered UI with RmlUI.

## Status

| Phase | Surface | Status | Notes |
|-------|---------|--------|-------|
| 3 | Infrastructure | **Done** | Multi-document, suppression flags, console commands, render reorder |
| 4 | HUD | Pending | Weapon bar, crosshair, frag count, identify, progress messages |
| 5 | Messages | Pending | Chat/kill toasts, typing indicator |
| 6 | Scoreboard | Pending | Tab-key scoreboard panel |
| 7 | Console | Pending | Tilde-key sliding console |
| 8 | Menus | Pending | Escape-key game menu (largest phase) |
| 9 | UWindow Retirement | Pending | Final cleanup after all surfaces replaced |

## Dependency Graph

```
Phase 3: Infrastructure (DONE)
  |---> Phase 4: HUD
  |---> Phase 5: Messages ---> Phase 7: Console
  |---> Phase 6: Scoreboard
  \---> Phase 8: Menus
                  \---> Phase 9: UWindow Retirement
```

Phases 4, 5, 6 are independent after Phase 3. Phase 7 builds on 5's message data. Phase 8 is the largest. Phase 9 is cleanup after all surfaces are replaced.

---

## Phase 3: Infrastructure (DONE)

### What was built
- `UISuppressionFlags` struct in `Engine.h` — per-surface bools gating script PostRender
- `RmlUIManager` multi-document model: `hud.rml` (auto-shown), `messages/scoreboard/console/menu.rml` (hidden)
- `ShowDocument()`/`HideDocument()`/`ToggleDocument()`/`HasActiveInteractiveDocument()`
- Console commands: `togglehud`, `togglemenu`, `reloadui`
- Render reorder: RmlUI draws after PostRender (always on top)
- PostRender gated: actor PostRender skipped when `bRmlHUD`, console PostRender skipped when `bRmlMessages`/`bRmlConsole`
- `ShouldRouteInputToUI()` checks `HasActiveInteractiveDocument()` — cursor auto-unlocks
- 24 tests passing

---

## Phase 4: HUD Replacement

**Goal:** Fully replace `UnrealHUD.PostRender` HUD drawing with RmlUI.

### C++ changes

**`RmlUI/RmlUIManager.h`** — expand `HUDViewModel`:
- `healthMax`, `ammoMax`, `fragCount`, `hudMode` (0-5), `crosshairIndex` (0-6), `crosshairScale`
- `WeaponSlot[10]`: `{ occupied, selected, name, ammo, ammoMax }`
- Identify tooltip: `showIdentify`, `identifyName`
- Progress messages: `showProgress`, `ProgressMsg[5]` `{ text, r, g, b }`

**`Engine.cpp`** — expand data extraction (currently lines ~156-194):
- Walk `Inventory` chain for weapon slots (by `InventoryGroup`)
- Read `HudMode`, `Crosshair`, `CrosshairScale` from HUD actor
- Read `ProgressMessage[]`, `ProgressColor[]`, `ProgressTimeOut` from PlayerPawn
- Read frag count from `PlayerReplicationInfo.Score`

### UnrealScript changes
- `UnrealHUD.uc`: add `var bool bUseRmlHUD;` (default true), early-return in `PostRender`

### Content
- Expand `hud.rml`: weapon inventory bar, crosshair, identify tooltip, progress messages, frag counter
- Expand `styles/hud.rcss` for all HUD mode layouts

### Verification
- RmlUI HUD visible, no script HUD underneath
- Weapon pickup updates inventory bar
- Crosshair renders centered
- Take damage — health updates
- `togglehud` reverts to script HUD

---

## Phase 5: Messages Replacement

**Goal:** Replace `Console.DrawSingleView` toast messages and typing indicator.

### C++ changes
- `MessagesViewModel`: `messages[]` (text, type, age), `isTyping`, `typedString`
- Engine extracts `MsgText[]`/`MsgTick[]`/`MsgType[]` ring buffer from console

### UnrealScript changes
- Console: `var bool bUseRmlMessages;`, early-return in `DrawSingleView`

### Content
- `messages.rml`: top-left toast container, typing indicator
- `styles/messages.rcss`: type-based colors (Say=green, Death=red, Critical=blue)

---

## Phase 6: Scoreboard Replacement

**Goal:** Replace `UnrealScoreBoard.ShowScores` with RmlUI on Tab.

### C++ changes
- `ScoreboardViewModel`: `players[]` (name, score, ping, team), server info, elapsed time
- Engine iterates `Level->Actors` for `UPlayerReplicationInfo`, shows/hides on `bShowScores`

### UnrealScript changes
- `UnrealHUD.uc`: `var bool bUseRmlScoreboard;`, guard the `bShowScores` block

### Content
- `scoreboard.rml`: centered panel, sorted player table
- `styles/scoreboard.rcss`: semi-transparent dark panel

---

## Phase 7: Console Replacement

**Goal:** Replace `Console.DrawConsoleView` sliding console panel.

### C++ changes
- `ConsoleViewModel`: `logLines[]`, `typedStr`, `visible`
- Event listener for form submission → `engine->ConsoleCommand()`
- Command history (up/down arrow)
- Tilde key intercept when `bRmlConsole`

### Content
- `console.rml`: sliding panel with scrollable log + text input
- `styles/console.rcss`: dark translucent, monospace font

---

## Phase 8: Menu Replacement

**Goal:** Replace `UnrealMainMenu` + UWindow menus with RmlUI.

### C++ changes
- `MenuViewModel`: `activeScreen`, settings (volumes, resolution, fullscreen), save slots
- Menu event handler: button clicks → `Engine::ConsoleCommand()` or direct engine calls
- Escape key intercept when `bRmlMenus`

### UnrealScript changes
- `UnrealHUD.uc`: `var bool bUseRmlMenus;`, guard `bShowMenu` → `DisplayMenu` block

### Content
- `menu.rml`: multi-screen menu (main, game, options, load/save, new game)
- `styles/menu.rcss`: full-screen overlay, Unreal Gold themed

---

## Phase 9: UWindow Retirement

**Goal:** Disable UWindow rendering entirely when all RmlUI surfaces are active.

### Changes
- Skip `DrawRootWindow()` in `RenderSubsystem.cpp` when all flags active
- `ShouldRouteInputToUI()` no longer considers `bShowWindowsMouse`
- Guard UWindow creation in `WindowConsole.LaunchUWindow()` path

---

## File Summary

| File | Phases |
|------|--------|
| `SurrealEngine/Engine.h` | 3, 4, 5, 6, 7, 8 |
| `SurrealEngine/Engine.cpp` | 3, 4, 5, 6, 7, 8 |
| `SurrealEngine/RmlUI/RmlUIManager.h` | 3, 4, 5, 6, 7, 8 |
| `SurrealEngine/RmlUI/RmlUIManager.cpp` | 3, 4, 5, 6, 7, 8 |
| `SurrealEngine/Render/RenderSubsystem.cpp` | 3, 9 |
| `SurrealEngine/Render/RenderCanvas.cpp` | 3 |
| `Tests/TestRmlUI.cpp` | 3, 4+ |
| `Unreal-PubSrc/UnrealShare/Classes/UnrealHUD.uc` | 4, 6, 8 |
| `~/dev/games/UnrealGold/UI/*.rml` | 3-8 |
| `~/dev/games/UnrealGold/UI/styles/*.rcss` | 3-8 |

## Verification (each phase)

1. Build: `cd build && make -j$(nproc)`
2. Build UnrealScript: `cd ~/dev/Unreal-PubSrc && ./build.sh` (if script changes)
3. Run tests: `./build/TestRmlUI`
4. Launch: `./build/SurrealEngine ~/dev/games/UnrealGold`
5. Visually verify replaced surface works, remaining surfaces still function via script fallback
