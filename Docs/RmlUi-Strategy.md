# RmlUi Integration Strategy

Strategic approach for replacing SurrealEngine's UI with [RmlUi](https://github.com/mikke89/RmlUi). For the technical analysis of the existing UI systems, native drawing surface, and constraints, see [RmlUi-UI-Replacement.md](RmlUi-UI-Replacement.md).

## The Core Tension

In vkQuake the HUD was C code we owned — we ripped out `sbar.c` and replaced it with `.rml` documents bound to game state. Clean separation, full ownership.

SurrealEngine is fundamentally different: the UI code lives in compiled UnrealScript bytecode inside `.u` packages. UT99's `ChallengeHUD.PostRender(Canvas)` directly calls `Canvas.DrawTile()` and `Canvas.DrawText()` dozens of times per frame. Deus Ex builds entire menu trees in script. That code is the product, and compatibility is the project's reason for existing.

But this constraint creates an interesting opportunity rather than blocking the idea.

## The Approach: Parallel Opt-In

Rather than replacing the legacy UI, we run RmlUi alongside it. The engine controls whether each UI element renders via UnrealScript or RmlUi. Games without RmlUi content work exactly as they do today. Games with a `UI/` directory get the modern path.

This works because the HUD is **stateless per frame** — UnrealScript reads game state and draws it. We can read the same state from C++ and draw it with RmlUi instead. Architecturally, this can be implemented as a branch around the existing `PostRender(Canvas)` event dispatch in `RenderSubsystem`.

| Aspect | vkQuake | SurrealEngine |
|--------|---------|---------------|
| HUD code ownership | C code you control | Compiled UnrealScript bytecode |
| Replacement strategy | Full replacement | Parallel + opt-in |
| C/C++ boundary | `extern "C"` API needed | All C++, direct integration |
| File I/O | Pak VFS | PackageManager (same concept) |
| Data contract complexity | ~50 bindings (Quake is simple) | ~50 core + per-game extensions |
| Menu system | Simple enum-based | Full retained-mode widget tree (UWindow) |
| Render backend | Custom Vulkan only | Abstract RenderDevice (Vulkan + D3D11) |

## Separation: Engine vs Game Content

The engine fork is game-agnostic. Per-game UI content lives alongside the game assets, not compiled into the engine. This mirrors how the original engine works: generic engine binary, game-specific content in packages.

### Directory Layout

UI content lives in a `UI/` directory inside each game's root folder, next to `System/`, `Maps/`, `Textures/`, etc:

```
UnrealGold/                         # gameRootFolder
├── System/                         # .u packages, .ini configs
├── Maps/
├── Textures/
├── Music/
├── Sounds/
└── UI/                             # RmlUi content (new, per-game)
    ├── hud.rml
    ├── hud.rcss
    ├── scoreboard.rml
    ├── menu_main.rml
    ├── shared.rcss
    ├── bindings.json               # optional data-driven property bindings
    ├── fonts/                      # TTF/OTF fonts
    └── textures/                   # UI-specific textures/icons
```

Different games get different content:

```
UnrealGold/UI/          single-player: health, armor, weapon, ammo, messages
UnrealTournament/UI/    + team info, flag status, multi-kills, timer, scoreboard
DeusEx/UI/              + augmentations, skills, nano-keys, conversation, terminals
```

Games without a `UI/` directory work exactly as they do today — zero impact on compatibility. Users can delete the `UI/` folder to revert to legacy at any time.

### What the Engine Fork Provides

Game-agnostic C++:

- **RmlUi render interface** — `Rml::RenderInterface` implementation against `RenderDevice` or Vulkan directly
- **RmlUi file interface** — loads `.rml`/`.rcss` from `gameRootFolder/UI/`
- **RmlUi system interface** — timers, logging, clipboard
- **Data model registry** — base contract + per-game contract selection + optional data-driven bindings
- **Composite pipeline** — offscreen RT for UI, fullscreen composite pass (see [Rendering Pipeline](RmlUi-UI-Replacement.md#current-rendering-architecture--no-post-processing) for current state)
- **Toggle logic** — per-element switching between legacy UnrealScript rendering and RmlUi documents
- **`UI/` directory discovery** — on game load, check for `gameRootFolder/UI/`, load documents if present, fall back to legacy if not

### What Game Content Provides

Per-game `.rml`/`.rcss` documents authored against the data contract keys. The documents are pure content — no C++ compilation needed to create or modify them.

## Data Contracts

The data contract reads live game state from the UObject reflection system and publishes it to RmlUi's data model. Authored in C++, no UnrealScript involved.

### How Property Access Works

SurrealEngine already provides two ways to read any UObject property from C++:

**Typed accessors** — direct references into property data, generated from `PropertyOffsets` in `UObject/UActor.h`:

```cpp
int& Health()       { return Value<int>(PropOffsets_Pawn.Health); }          // line 1472
float& Score()      { return Value<float>(PropOffsets_PlayerReplicationInfo.Score); } // line 1232
UWeapon*& Weapon()  { return Value<UWeapon*>(PropOffsets_Pawn.Weapon); }     // line 1530
int& Charge()       { return Value<int>(PropOffsets_Inventory.Charge); }     // line 530
```

**String-based reflection** — generic lookup by name on any UObject (`UObject/UObject.h:190-197`):

```cpp
uint32_t GetInt(const NameString& name) const;
bool     GetBool(const NameString& name) const;
float    GetFloat(const NameString& name) const;
UObject* GetUObject(const NameString& name);
```

### Per-Frame Sync

A sync function reads game state and publishes it to the RmlUi data model each frame (equivalent of vkQuake's `GameDataModel_SyncFromQuake()`):

```cpp
void SyncHudDataModel(Rml::DataModelHandle model)
{
    auto* pawn = UObject::TryCast<UPawn>(engine->CameraActor);
    if (!pawn) return;

    model.Set("health", pawn->Health());

    auto* pri = pawn->GetUObject("PlayerReplicationInfo");
    if (pri) model.Set("score", pri->GetFloat("Score"));

    auto* weapon = pawn->Weapon();
    if (weapon) {
        model.Set("weapon_name", weapon->GetString("ItemName"));
        auto* ammo = weapon->GetUObject("AmmoType");
        if (ammo) model.Set("ammo", ammo->GetInt("AmmoAmount"));
    }

    model.DirtyAllVariables();
}
```

### Pipeline

```
UObject property data (live game state, updated by UnrealScript VM)
    → C++ data contract (reads via typed accessors or reflection)
        → RmlUi data model (dirty-tracked bindings)
            → .rml documents ({{ health }}, {{ ammo }}, etc.)
```

### Contract Hierarchy (Hybrid Approach)

The engine provides a base contract for properties common to all UE1 games. Per-game contracts extend it. An optional data-driven manifest adds simple bindings without recompilation.

```
BaseHudContract (engine, compiled)
    health, armor, weapon, ammo, score, messages
    works for any UE1 game with standard Pawn/Weapon/Inventory

UnrealGoldHudContract extends BaseHudContract (engine, compiled)
    translators, nali fruit seeds, etc.

UT99HudContract extends BaseHudContract (engine, compiled)
    team info, flag status, multi-kills, remaining time

DeusExHudContract extends BaseHudContract (engine, compiled)
    augmentations, skills, nano-keys, log entries

UI/bindings.json (per-game, data-driven, optional)
    simple property-path overrides or additions
    no recompilation needed for tweaks
```

The engine selects the right contract at runtime via `GameLaunchInfo` (see `GameFolder.h`):

```cpp
if (engine->LaunchInfo.IsUnreal1())
    contract = std::make_unique<UnrealGoldHudContract>();
else if (engine->LaunchInfo.IsUnrealTournament())
    contract = std::make_unique<UT99HudContract>();
else if (engine->LaunchInfo.IsDeusEx())
    contract = std::make_unique<DeusExHudContract>();
```

If `UI/bindings.json` exists, its entries are loaded on top — walking dot-separated property paths via `GetUObject()` + `GetInt()`/`GetFloat()`/`GetString()`.

### Auditing Contracts from UnrealScript Source

The full UnrealScript source is available for study — [OldUnreal/Unreal-PubSrc](https://github.com/OldUnreal/Unreal-PubSrc) has 1,507 `.uc` files. For games without a public source repo, SurrealEngine's debugger can extract embedded source from `.u` packages (`export scripts` command). This makes it straightforward to audit exactly what properties each HUD class reads, what Canvas calls it makes, and what game state drives each element.

## Composite Pipeline

SurrealEngine currently renders everything in a single pass to the swapchain with no separation between 3D and 2D (see [Current Rendering Architecture](RmlUi-UI-Replacement.md#current-rendering-architecture--no-post-processing) for details). Adding a two-buffer composite unlocks:

- **Post-process effects** — barrel warp, chromatic aberration, contrast/gamma, same as vkQuake
- **HUD inertia** — spring-based UI shift in response to player movement
- **Clean UI/game separation** — benefits both legacy Canvas draws and new RmlUi documents

Both legacy and RmlUi UI draws can target the UI render target. The composite pass blends UI over game regardless of which system produced the draws.

## Input Interception & Routing

While the HUD is largely passive, any interactive elements (menus, Deus Ex terminals, scoreboards) will require capturing input *before* it reaches the game simulation.

SurrealEngine input currently flows through `GameWindow`/ZWidget callbacks into `Engine::InputEvent()` and then into UnrealScript console/game handling. RmlUi integration needs middleware in that path:

1. **Input Modes**: Similar to the `vkQuake` approach, define states such as `UI_INPUT_INACTIVE` (HUD only, pass all input to game), `UI_INPUT_OVERLAY` (HUD with interactive elements, share input), and `UI_INPUT_ACTIVE` (Menus, capture all input except system keys like Escape).
2. **Event Interception**: Intercept keyboard, mouse, and wheel events in the window-callback/input-event path. If RmlUi consumes an event, prevent it from propagating to normal game input handling.
3. **Cursor Management**: When RmlUi captures input, the engine must release mouse capture (un-confine the OS mouse cursor) and either render the RmlUi cursor or revert to the hardware cursor.

## HUD vs Menus

**HUD is the natural starting point.** It's immediate-mode, stateless per frame, and the data contract is well-defined. The `PostRender` toggle is trivial.

**UWindow menus are the hard part.** UT99's menus are deeply scripted widget trees with custom `Paint()` overrides, focus management, and keyboard navigation (see [UWindow Widget Hierarchy](RmlUi-UI-Replacement.md#the-uwindow-widget-hierarchy)). Replacing those with documents means reimplementing each menu's layout and behavior in `.rml` — a lot of work per game.

However, the UWindow system is currently incomplete in SurrealEngine (`RenderWindow.cpp:31` TODO: `CallEvent(window, "DrawWindow", { gc })`), and many `UWindow`/`UGC` methods are still `LogUnimplemented(...)`. Also, `NWindow`/`NGC` natives are currently registered in the Deus Ex path. We have to do engine work here regardless, and can choose whether that work prioritizes full legacy UWindow parity or a parallel RmlUi path first.

## Implementation Order

### Phase 1: Foundation

1. **Fork SurrealEngine** — set up the repo, verify clean build
2. **Add RmlUi as a dependency** — CMake integration, third-party submodule
3. **Implement `Rml::RenderInterface`** — target `RenderDevice` abstraction or Vulkan directly
4. **Implement `Rml::FileInterface`** — load from `gameRootFolder/UI/`
5. **Implement `Rml::SystemInterface`** — timers, logging
6. **`UI/` directory discovery** — detect at game load, set a flag for toggle logic

### Phase 2: HUD

7. **Audit Unreal Gold HUD** — read the UnrealScript source, identify every property the HUD accesses
8. **Build `BaseHudContract`** — common UE1 bindings (health, armor, weapon, ammo, score)
9. **Build `UnrealGoldHudContract`** — game-specific extensions
10. **Author `UnrealGold/UI/hud.rml`** — the first document, bound to the data model
11. **Wire the toggle** — skip `PostRender` when RmlUi HUD is active, add a runtime switch (console command or key)

### Phase 3: Composite Pipeline

12. **Add offscreen render targets** — game RT + UI RT
13. **Fullscreen composite pass** — blend UI over game with configurable effects
14. **Redirect legacy Canvas draws to UI RT** — benefits both legacy and RmlUi modes

### Phase 4: More Games

15. **UT99 HUD contract + documents** — team info, flag status, scoreboard
16. **Deus Ex HUD contract + documents** — augmentations, skills, nano-keys
17. **Data-driven `bindings.json`** — for community extensions without recompilation

### Phase 5: Menus (Later)

18. **Assess UWindow state** — how much of the legacy `Paint()` cycle is needed vs. RmlUi replacement
19. **Author menu documents** — per game, starting with Unreal Gold (simplest menus)
20. **Input routing** — keyboard navigation, focus management in RmlUi documents

## Runtime Flow

```
Engine startup
  → GameFolderSelection::ExamineFolder() identifies game
  → GameLaunchInfo populated (gameName, engineVersion, gameRootFolder)
  → Check gameRootFolder/UI/ exists
     ├── Yes: initialize RmlUi context
     │        load documents (hud.rml, etc.)
     │        instantiate per-game data contract (via GameLaunchInfo)
     │        load UI/bindings.json if present
     │        enable RmlUi HUD, disable legacy PostRender
     └── No:  legacy mode, UnrealScript renders everything as before

Per frame (RmlUi active):
  → contract->Sync(dataModel)        read UObject properties → data model
  → Rml::Context::Update()           process dirty bindings, update DOM
  → RenderSubsystem::DrawGame()      3D scene → game RT
  → Rml::Context::Render()           UI draws → UI RT
  → Composite pass                   blend UI RT over game RT → swapchain

Per frame (legacy mode):
  → RenderSubsystem::DrawGame()      3D scene + Canvas HUD + UWindow menus
                                     (unchanged from current behavior)
```
