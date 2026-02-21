# RmlUi UI Replacement — Technical Analysis

This document describes the current state of SurrealEngine's UI systems, how game content authors UI, the exact native drawing surface that scripts depend on, and the constraints a replacement with RmlUi must satisfy.

For the integration strategy, implementation order, data contract architecture, and engine/content separation, see [RmlUi-Strategy.md](RmlUi-Strategy.md).

## Prior Art: vkQuake-RmlUi

The [vkQuake-RmlUi](https://github.com/bradenleague/vkQuake-RmlUi) project replaced vkQuake's entire menu system and HUD with RmlUi, a retained-mode HTML/CSS UI library. Key architectural elements from that project relevant here:

- **Custom Vulkan render backend** (`RenderInterface_VK`) implementing `Rml::RenderInterface` with pool-based buffer allocation, batched texture uploads, and double-buffered garbage collection.
- **Two-buffer composite pipeline**: the game renders to one color buffer, RmlUi renders to a second (transparent background), and a fullscreen post-process pass composites them with configurable effects.
- **Post-process effects**: barrel warp, chromatic aberration, helmet echo, additive/alpha blending modes, contrast/gamma grading — all applied per-pixel in a fragment shader.
- **HUD inertia**: critically-damped spring system that shifts the UI layer in response to player movement (jump bounce, yaw sway).
- **C++/C boundary**: RmlUi integration is C++ exposing a `UI_*` C API to the Quake engine. SurrealEngine is all C++, so this boundary is unnecessary here.
- **File interface**: routes RmlUi file I/O through Quake's pak file system, enabling mod directory precedence. An equivalent would route through SurrealEngine's `PackageManager`.
- **Data model**: bidirectional bindings between game state and RmlUi documents, with dirty-tracking to minimize per-frame DOM updates.

---

## SurrealEngine's Two UI Systems

SurrealEngine has two independent UI frameworks serving different roles.

### 1. ZWidget — Launcher and Editor UI

ZWidget is a third-party retained-mode widget library (`Thirdparty/ZWidget/`) used for the pre-game launcher (`UI/Launcher/`) and map editor (`UI/Editor/`). It renders through an abstract `Canvas` interface.

**Connection to the renderer:** `GameWindow` creates a `RenderDeviceCanvas` (`RenderDevice/RenderDevice.h:170-193`) that bridges ZWidget's `Canvas` calls to `RenderDevice`:

```
ZWidget Canvas interface          RenderDeviceCanvas bridge         RenderDevice
  begin()                    →      Device->Lock()
  fillTile()                 →      Device->DrawTile()
  drawTile()                 →      Device->DrawTile()
  drawLineAntialiased()      →      Device->Draw2DLine()
  drawGlyph()                →      Device->DrawTile()
  end()                      →      Device->Unlock(true)
```

ZWidget is loosely coupled — it depends only on the abstract `Canvas` interface, not on Vulkan or D3D11 directly. It has no interaction with UnrealScript.

### 2. UWindow + UCanvas — In-Game UI

This is the UE1-native UI system. All game menus, HUD elements, console, and in-game windows are built with this system. It has two layers:

**UCanvas** — Low-level immediate-mode drawing surface. Game scripts (HUD classes, weapon overlays, console) receive a `Canvas` object in events like `PostRender(Canvas C)` and draw directly with it every frame.

**UWindow** — Retained-mode widget tree. Menu systems create `UWindow` hierarchies in UnrealScript. Windows persist across frames, handle input events, manage focus, and override `Paint(Canvas C, float X, float Y)` to draw their content using Canvas calls.

Both layers ultimately render through `RenderSubsystem` → `RenderDevice`.

---

## How Game Content Authors UI

### Pattern 1: Immediate-Mode HUD (UCanvas)

Game HUD classes subclass engine-provided HUD actors and override rendering events. The engine calls these events every frame with a `Canvas` argument:

```
Engine frame loop (RenderSubsystem):
  ResetCanvas()           — initialize canvas state (size, scale, origin)
  PreRender()             — calls Console.PreRender(Canvas) and Actor.PreRender(Canvas)
  DrawScene()             — 3D world rendering
  RenderOverlays()        — calls Actor.RenderOverlays(Canvas) — weapon models, etc.
  PostRender()            — calls Actor.PostRender(Canvas) and Console.PostRender(Canvas)
```

Source: `Render/RenderCanvas.cpp:56-95`

Inside these callbacks, UnrealScript code uses Canvas like this (pseudocode from typical UT99 HUD scripts):

```unrealscript
function PostRender(Canvas C)
{
    C.SetPos(10, 10);
    C.DrawColor = WhiteColor;
    C.Font = MyFont;
    C.DrawText("Health: " $ Health);

    C.SetPos(100, 500);
    C.Style = ERenderStyle.STY_Translucent;
    C.DrawTile(CrosshairTex, 32, 32, 0, 0, 32, 32);
}
```

This is analogous to Quake's `sbar.c` — every frame the script redraws everything from scratch using a pen-position state machine.

### Pattern 2: Retained-Mode Menus (UWindow)

UT99 and Deus Ex build their entire menu systems using UWindow trees in UnrealScript. A menu class creates child windows, sets their properties, and connects event handlers:

```unrealscript
class UTMenuStartMatch extends UWindowPulldownMenu;

function Created()
{
    Super.Created();
    AddMenuItem("Start Game", None);
    AddMenuItem("Bot Config", None);
    AddMenuItem("-", None);  // separator
    AddMenuItem("Back", None);
}

function ExecuteItem(int item)
{
    switch(item) { ... }
}
```

Windows override `Paint()` to draw custom content:

```unrealscript
function Paint(Canvas C, float MouseX, float MouseY)
{
    local GC gc;
    gc = GetGC();
    gc.SetFont(MyFont);
    gc.DrawText(0, 0, Width, Height, "Hello World");
    ReleaseGC(gc);
}
```

The window tree is managed through `URootWindow` at the top, with `firstChild`/`nextSibling` linked list traversal. The engine renders this tree via `RenderSubsystem::DrawRootWindow()` → recursive `DrawWindow()`.

### Pattern 3: Deus Ex Extensions

Deus Ex adds `UComputerWindow` — a terminal emulator widget with typing effects, cursor blinking, fade-out animations, and character-grid layout. It also adds numerous other specialized windows (`UConversation`, `UDeusExTextParser`, etc.) that extend the base UWindow system with game-specific native functions.

---

## The Complete Native Drawing Surface

The universally active in-game drawing surface today is `Canvas` (`Native/NCanvas.cpp`). A second surface (`GC`/`UWindow`) exists in source, but its native registration and implementation are currently partial and game-dependent.

### UCanvas Native Functions

Registered in `Native/NCanvas.cpp:15-33`. These are the drawing primitives available to all UnrealScript code:

| Function | Native Index | What It Does |
|---|---|---|
| `DrawTile` | 466 | Textured quad at `(OrgX+CurX, OrgY+CurY)`, advances pen position |
| `DrawTileClipped` | 468 | Same as DrawTile but clips to `(OrgX, OrgY, ClipX, ClipY)` rectangle |
| `DrawText` | 465 | Text string using current Font, with word-wrap and optional centering |
| `DrawTextClipped` | 469 | Text with hotkey underline parsing (`&` prefix), clips to canvas bounds |
| `DrawActor` | 467 | Render a 3D actor mesh onto the 2D canvas |
| `DrawClippedActor` | 471 | Render a 3D actor within a specified 2D viewport rectangle |
| `DrawPortal` | 480 | Portal rendering (currently unimplemented — empty function body) |
| `StrLen` | 464 | Measure text width and height without drawing |
| `TextSize` | 470 | Get raw text pixel dimensions from font metrics |

**Canvas state properties** accessed by these functions (from `UObject/UClient.h`):

| Property | Type | Purpose |
|---|---|---|
| `CurX`, `CurY` | float | Current pen position |
| `OrgX`, `OrgY` | float | Origin offset added to pen position |
| `ClipX`, `ClipY` | float | Clipping rectangle size |
| `SizeX`, `SizeY` | int | Canvas logical size in pixels |
| `DrawColor` | Color (RGBA uint8) | Current drawing color |
| `Style` | uint8 | Blend mode: 0=none, 2=masked, 3=translucent, 4=modulated |
| `Font` | UFont* | Current font for text drawing |
| `SmallFont`, `MedFont`, `LargeFont`, `BigFont` | UFont* | Built-in font references |
| `bCenter` | bool | Center text horizontally in clip region |
| `bNoSmooth` | bool | Disable texture filtering |
| `SpaceX`, `SpaceY` | float | Extra spacing between characters/lines |
| `Z` | float | Depth value for rendering |

### UGC (Graphics Context) Native Functions

Registered in `Native/NGC.cpp`. In the current codebase, these natives are only registered for Deus Ex (`Package/PackageManager.cpp` under `if (IsDeusEx())`). UGC is a higher-level drawing abstraction used by UWindow widgets.

**Drawing functions:**

| Function | Native Index | What It Does |
|---|---|---|
| `DrawIcon` | 1283 | Draw icon texture at position |
| `DrawTexture` | 1284 | Draw texture with dest rect and source origin |
| `DrawPattern` | 1285 | Draw tiled/repeating pattern |
| `DrawBox` | 1286 | Draw bordered box outline |
| `DrawStretchedTexture` | 1287 | Draw texture with full source/dest rect control |
| `DrawActor` | 1288 | Draw 3D actor with options (unlit, scale, skin) |
| `DrawBorders` | 1289 | Draw 9-patch border (corners + edges + center fill) |
| `DrawText` | 1282 | Draw text with alignment in a dest rect |
| `ClearZ` | 1295 | Clear Z-buffer |

**State management functions** (get/set pairs):

| Category | Functions |
|---|---|
| Enable/Disable | `EnableDrawing`, `EnableMasking`, `EnableModulation`, `EnableSmoothing`, `EnableSpecialText`, `EnableTranslucency`, `EnableTranslucentText`, `EnableWordWrap` |
| Query | `IsDrawingEnabled`, `IsMaskingEnabled`, `IsModulationEnabled`, `IsSmoothingEnabled`, `IsSpecialTextEnabled`, `IsTranslucencyEnabled`, `IsTranslucentTextEnabled`, `IsWordWrapEnabled` |
| Alignment | `GetAlignments`/`SetAlignments`, `GetHorizontalAlignment`/`SetHorizontalAlignment`, `GetVerticalAlignment`/`SetVerticalAlignment` |
| Colors | `GetTextColor`/`SetTextColor`, `GetTileColor`/`SetTileColor` |
| Fonts | `GetFonts`/`SetFonts`, `SetFont`, `SetNormalFont`, `SetBoldFont`, `GetFontHeight` |
| Text | `GetTextExtent`, `GetTextVSpacing`/`SetTextVSpacing`, `SetBaselineData` |
| Style | `GetStyle`/`SetStyle` |
| Clipping | `Intersect` (set clip rect) |
| State Stack | `PushGC`/`PopGC`, `CopyGC` |

**UGC properties** (from `UObject/UWindow.h:1052-1141`):

| Property | Type | Purpose |
|---|---|---|
| `Canvas` | UCanvas* | Underlying canvas reference |
| `HAlign`, `VAlign` | uint8 | Text/tile alignment |
| `Style` | uint8 | Rendering style |
| `TextColor`, `tileColor` | Color | Drawing colors |
| `PolyFlags`, `textPolyFlags` | int | Rendering flags |
| `bDrawEnabled` | bool | Master draw enable |
| `bSmoothed`, `bTranslucent`, `bModulated`, `bMasked` | bool | Rendering mode flags |
| `bTextTranslucent`, `bWordWrap`, `bParseMetachars` | bool | Text rendering flags |
| `baselineOffset`, `underlineHeight`, `textVSpacing` | float | Text metrics |
| `normalFont`, `boldFont` | UFont* | Font references |
| `gcStack`, `gcOwner`, `gcFree` | UGC* | State stack pointers |

### Current Implementation Status of UGC Drawing Functions

`Native/NGC.cpp` delegates to `UGC` methods, but many of those methods are explicitly stubbed with `LogUnimplemented(...)` in `UObject/UWindow.cpp` (for example `GC.DrawBox`, `GC.DrawPattern`, `GC.DrawStretchedTexture`, `GC.DrawTexture`, `GC.ClearZ`, `GC.PushGC`, `GC.PopGC`, `GC.GetTextExtent`, and others).

In addition, `RenderSubsystem::DrawRootWindow()` in `Render/RenderWindow.cpp` is currently a simplified debug renderer and still has a TODO to call script draw events:

```cpp
// To do: CallEvent(window, "DrawWindow", { gc });
```

So the full legacy UWindow paint cycle is not fully wired in the current engine state.

---

## The RenderSubsystem Canvas Layer

`RenderSubsystem` (`Render/RenderSubsystem.h`) sits between the native functions and `RenderDevice`. It handles:

**UI scaling** (`Render/RenderCanvas.cpp:11-54`):
```cpp
int vertResolution = engine->LaunchInfo.engineVersion < 400 ? 768 : 960;
Canvas.uiscale = std::max((engine->ViewportHeight + vertResolution / 2) / vertResolution, 1);
```
All coordinates passed to `RenderDevice` are multiplied by `Canvas.uiscale`. Scripts work in logical coordinates; the subsystem scales to physical pixels. Unreal/older games use 768p as baseline, UT/newer use 960p.

**Text rendering** (`Render/RenderCanvas.cpp:262-353`):
Text is rendered glyph-by-glyph. Each character is looked up in a `UFont` to get a `FontGlyph` (texture atlas reference with StartU, StartV, USize, VSize). Each glyph is drawn as an individual `Device->DrawTile()` call. Word-wrapping is computed by splitting text into blocks and accumulating widths against `clipX`.

**Tile rendering** (`Render/RenderCanvas.cpp:145-196`):
Tiles go through `UpdateTexture()` (ensure GPU-ready), animated texture resolution (`GetAnimTexture()`), `FTextureInfo` construction (CacheID, format, mipmaps, palette), then `Device->DrawTile()` with UI-scaled coordinates.

**Clipped tile rendering** (`Render/RenderCanvas.cpp:441-482`):
For tiles that extend beyond a clip rectangle, the subsystem computes proportional UV adjustments:
```cpp
float scaleX = (s.right - s.left) / (d.right - d.left);
// If dest extends past clip, adjust both dest and source proportionally
if (d.left < clipBox.left) {
    s.left += scaleX * (clipBox.left - d.left);
    d.left = clipBox.left;
}
```

**Actor rendering** (`Render/RenderCanvas.cpp:97-143`):
`DrawClippedActor` creates a temporary `FSceneNode` with a viewport sized to the requested 2D rectangle, sets up a projection matrix, renders the actor's mesh via `VisibleMesh`, then restores the canvas frame.

### RenderSubsystem Public Drawing Interface

From `Render/RenderSubsystem.h:31-37`:

```cpp
void DrawActor(UActor* actor, bool WireFrame, bool ClearZ);
void DrawClippedActor(UActor* actor, bool WireFrame, int X, int Y, int XB, int YB, bool ClearZ);
void DrawTile(UTexture* Tex, float x, float y, float XL, float YL,
              float U, float V, float UL, float VL, float Z,
              vec4 color, vec4 fog, uint32_t flags);
void DrawTileClipped(UTexture* Tex, float orgX, float orgY, float curX, float curY,
                     float XL, float YL, float U, float V, float UL, float VL,
                     float Z, vec4 color, vec4 fog, uint32_t flags,
                     float clipX, float clipY);
void DrawText(UFont* font, vec4 color, float orgX, float orgY,
              float& curX, float& curY, float& curXL, float& curYL,
              bool newlineAtEnd, const std::string& text, uint32_t polyflags,
              bool center, float spaceX, float spaceY, float clipX, float clipY,
              bool noDraw = false);
void DrawTextClipped(UFont* font, vec4 color, float orgX, float orgY,
                     float curX, float curY, const std::string& text,
                     uint32_t polyflags, bool checkHotKey, float clipX, float clipY,
                     bool center);
vec2 GetTextSize(UFont* font, const std::string& text, float spaceX, float spaceY);
```

---

## The RenderDevice Interface

`RenderDevice` (`RenderDevice/RenderDevice.h:97-161`) is the abstract GPU backend. All UI rendering calls on it are:

```cpp
virtual void DrawTile(FSceneNode* Frame, FTextureInfo& Info,
    float X, float Y, float XL, float YL,
    float U, float V, float UL, float VL,
    float Z, vec4 Color, vec4 Fog, uint32_t PolyFlags) = 0;

virtual void Draw2DLine(FSceneNode* Frame, vec4 Color, uint32_t LineFlags,
    vec3 P1, vec3 P2) = 0;

virtual void Draw2DPoint(FSceneNode* Frame, vec4 Color, uint32_t LineFlags,
    float X1, float Y1, float X2, float Y2, float Z) = 0;

virtual void ClearZ() = 0;
virtual void SetSceneNode(FSceneNode* Frame) = 0;
```

Two implementations exist:
- `VulkanRenderDevice` (`RenderDevice/Vulkan/VulkanRenderDevice.h`)
- `D3D11RenderDevice` (`RenderDevice/D3D11/D3D11RenderDevice.h`) — Windows only

---

## The UWindow Widget Hierarchy

All widget types, from `UObject/UWindow.h`:

```
UWindow                          — base: position, size, visibility, input, hierarchy
├── UViewportWindow              — embedded 3D viewport (camera, FOV, tracked actors)
├── UTileWindow                  — flow/grid layout container
├── UTextWindow                  — text label with alignment and word-wrap
│   ├── UButtonWindow            — 6-state button (normal/pressed x unfocused/focused/insensitive)
│   │   └── UToggleWindow        — on/off state
│   │       └── UCheckboxWindow  — checkbox with label and custom textures
│   ├── UTextLogWindow           — scrolling timestamped text log
│   └── ULargeTextWindow         — extended multiline text
│       └── UEditWindow          — full text editor with undo/redo, selection, cursor
├── UTabGroupWindow              — keyboard tab navigation container
│   ├── URadioBoxWindow          — mutually exclusive toggle group
│   ├── UClipWindow              — scroll viewport with unit-based positioning
│   └── UModalWindow             — modal dialog with accelerator table
│       └── URootWindow          — top-level window, input routing, cursor management
├── UScrollAreaWindow            — scrollbars + clip child + scroll buttons
├── UScaleWindow                 — slider/scrollbar with ticks, thumb, caps
├── UScaleManagerWindow          — slider + value label + inc/dec buttons
├── UComputerWindow              — terminal emulator (Deus Ex) with typing/fade effects
├── UBorderWindow                — 9-patch decorative border with resize handles
└── UListWindow                  — multi-column sortable table with selection
```

### UWindow Base Native Functions

Declared in `Native/NWindow.cpp`. In the current registration flow they are enabled only in the Deus Ex path (`Package/PackageManager.cpp`).

**Hierarchy & Lifecycle:** `NewChild`, `Destroy`, `DestroyAllChildren`

**Visibility:** `Show`, `Hide`, `IsVisible`, `EnableWindow`, `DisableWindow`, `IsSensitive`

**Positioning:** `SetPos`, `SetSize`, `SetConfiguration`, `SetHeight`, `SetWidth`, `ResetSize`, `ResetWidth`, `ResetHeight`

**Layout:** `SetWindowAlignments`, `ConfigureChild`, `ResizeChild`, `AskParentForReconfigure`, `AskParentToShowArea`, `QueryPreferredSize`, `QueryPreferredHeight`, `QueryPreferredWidth`, `QueryGranularity`

**Focus & Navigation:** `SetFocusWindow`, `GetFocusWindow`, `IsFocusWindow`, `MoveFocusUp/Down/Left/Right`, `GetTabGroupWindow`, `MoveTabGroupNext/Prev`

**Child Traversal:** `GetTopChild`, `GetBottomChild`, `GetHigherSibling`, `GetLowerSibling`, `FindWindow`

**Z-Order:** `Raise`, `Lower`

**Input:** `IsPointInWindow`, `IsKeyDown`, `GrabMouse`, `UngrabMouse`, `GetCursorPos`, `SetCursorPos`

**Appearance:** `SetBackground`, `SetBackgroundStyle`, `SetBackgroundSmoothing`, `SetBackgroundStretching`, `SetTextColor`, `SetTileColor`, `SetFont`, `SetFonts`, `SetNormalFont`, `SetBoldFont`, `SetDefaultCursor`

**Sound:** `PlaySound`, `SetSoundVolume`, `SetFocusSounds`, `SetVisibilitySounds`

**Coordinate Conversion:** `ConvertCoordinates`, `ConvertVectorToCoordinates`

**Graphics Context:** `GetGC`, `ReleaseGC`

**Timers:** `AddTimer`, `RemoveTimer`

**Misc:** `SetClientObject`, `GetClientObject`, `GetParent`, `GetRootWindow`, `GetPlayerPawn`, `AddActorRef`, `RemoveActorRef`, `IsActorValid`, `SetSelectability`, `SetSensitivity`, `SetAcceleratorText`, `SetBaselineData`, `SetChildVisibility`, `ChangeStyle`, `CarriageReturn`, `ConvertScriptString`, `EnableSpecialText`, `EnableTranslucentText`, `GetTickOffset`, `GetModalWindow`

---

## Rendering Pipeline — UWindow vs HUD

The two UI patterns take different paths through the rendering pipeline:

### HUD Path (immediate-mode Canvas)

```
Engine::Run() main loop
  └─ RenderSubsystem::DrawGame()
       ├─ ResetCanvas()                        ← set Canvas.SizeX/SizeY, uiscale, origin
       ├─ PreRender()                          ← Console.PreRender(Canvas)
       ├─ DrawScene()                          ← 3D world
       ├─ RenderOverlays()                     ← PlayerPawn.RenderOverlays(Canvas)
       └─ PostRender()                         ← PlayerPawn.PostRender(Canvas), Console.PostRender(Canvas)
                                                  ↓
                                               UnrealScript HUD code calls:
                                                  Canvas.DrawTile() → NCanvas::DrawTile()
                                                  Canvas.DrawText() → NCanvas::DrawText()
                                                  Canvas.StrLen()   → NCanvas::StrLen()
                                                  ↓
                                               RenderSubsystem::DrawTile() / DrawText()
                                                  ↓
                                               Device->DrawTile()  (one call per tile/glyph)
```

### UWindow Path (retained-mode widget tree)

```
Engine::Run() main loop
  └─ RenderSubsystem::DrawRootWindow()
       ├─ ResetCanvas()
       ├─ Device->SetSceneNode()
       ├─ Device->ClearZ()
       └─ DrawWindow(rootWindow, ...)          ← recursive tree walk
            ├─ check bIsVisible()
            ├─ draw Background texture          → DrawTile()
            ├─ draw button curTexture (if any)  → DrawTile()
            ├─ draw border lines               → Device->Draw2DLine() x4
            └─ for each child: DrawWindow(child, ...)
```

Note: `RenderWindow.cpp` is currently a simplified debug renderer (~100 lines). The full UWindow rendering in the original engine calls UnrealScript `Paint()` events on each window, which then use GC/Canvas primitives. The `DrawWindow` comment at line 31 confirms this: `// To do: CallEvent(window, "DrawWindow", { gc });`

---

## Blend Modes and Poly Flags

The `Style` property on UCanvas/UGC maps to poly flags passed to `RenderDevice`:

| Style Value | Name | Poly Flag | Visual Effect |
|---|---|---|---|
| 0 | None | — | No rendering (skip draw call) |
| 2 | Masked | `PF_Masked` | Binary alpha — fully opaque or fully transparent |
| 3 | Translucent | `PF_Translucent` | Alpha blending |
| 4 | Modulated | `PF_Modulated` | Multiplicative blending (texture modulates framebuffer) |

Additional flags combined with style:
- `PF_NoSmooth` — disable texture filtering (set when `Canvas.bNoSmooth` is true, always set for text)
- `PF_TwoSided` — render both sides (set for all tiles)
- `PF_Highlighted` — used by `DrawTextClipped` for non-hotkey characters

---

## Font System

UE1 uses bitmap fonts. Each `UFont` contains a texture atlas with glyph regions. `UFont::GetGlyph(char)` returns:

```cpp
struct FontGlyph {
    UTexture* Texture;    // atlas texture page
    int StartU, StartV;   // top-left UV in atlas
    int USize, VSize;     // glyph dimensions in texels
};
```

Text rendering iterates characters, looks up each glyph, and emits individual `DrawTile()` calls. There is no vector/TrueType font support in the original system — all fonts are pre-rasterized texture atlases stored in `.utx` packages.

Built-in font references on UCanvas: `SmallFont`, `MedFont`, `LargeFont`, `BigFont`.

---

## UI Scaling

From `Render/RenderCanvas.cpp:11-16`:

```cpp
int vertResolution = engine->LaunchInfo.engineVersion < 400 ? 768 : 960;
Canvas.uiscale = std::max((engine->ViewportHeight + vertResolution / 2) / vertResolution, 1);
```

- Unreal and older games (engine version < 400): baseline is 768p (matching original 1024x768 CRT)
- UT99 and newer (engine version >= 400): baseline is 960p (matching 1280x960)

Scripts work in logical coordinates. `RenderSubsystem` multiplies all positions and sizes by `Canvas.uiscale` before passing to `RenderDevice`. This is an integer scale factor (1x, 2x, 3x...) to avoid fractional pixel positioning.

Canvas logical size is set each frame:
```cpp
int sizeX = (int)(engine->ViewportWidth / (float)Canvas.uiscale);
int sizeY = (int)(engine->ViewportHeight / (float)Canvas.uiscale);
engine->canvas->ClipX() = (float)sizeX;
engine->canvas->ClipY() = (float)sizeY;
engine->canvas->SizeX() = sizeX;
engine->canvas->SizeY() = sizeY;
```

---

## Current Rendering Architecture — No Post-Processing

SurrealEngine currently renders everything in a single pass directly to the swapchain. There is no offscreen render target, no post-process pass, and no separation between game and UI rendering. The rendering sequence within a single frame is:

```
Lock()
  SetSceneNode()           ← 3D viewport setup
  DrawComplexSurface()     ← BSP world geometry
  DrawGouraudPolygon()     ← meshes, actors
  SetSceneNode()           ← canvas frame (orthographic)
  DrawTile() x N           ← HUD tiles, text glyphs, menu backgrounds
  Draw2DLine() x N         ← window borders
Unlock(Blit)               ← present to screen
```

All 2D UI draws are interleaved with the same command buffer as 3D scene rendering. There is no separate UI render target.

---

## Game-Specific Native Window Extensions

### Deus Ex

Deus Ex adds extensive window system extensions beyond the base UWindow set:

| Widget | File | Purpose |
|---|---|---|
| `UComputerWindow` | `Native/NComputerWindow.h` | Terminal emulator with character grid, typing effects, cursor blink, text fade |
| `UConversation` | `Native/NConversation.h` | Conversation/dialogue system |
| `UConEvent` | `Native/NConEvent.h` | Conversation event handling |
| `UConEventRandomLabel` | `Native/NConEventRandomLabel.h` | Random dialogue selection |
| `UDeusExTextParser` | `Native/NDeusExTextParser.h` | Rich text parsing |
| `UExtensionObject` | `Native/NExtensionObject.h` | Window system extensions |
| `UExtString` | `Native/NExtString.h` | Extended string operations for UI |
| `UFlagBase` | `Native/NFlagBase.h` | Game flag state management |

### Tactical-Ops

Tactical-Ops adds its own window extensions, though these run under the UT436/UT469 engine.

---

## Summary of Constraints

Any RmlUi replacement must account for:

1. **UnrealScript compatibility**: Game content calls native drawing and UI functions from compiled UnrealScript bytecode in `.u` packages. For broad compatibility, Canvas signatures/indices must stay stable, and any UWindow/GC behavior added should preserve script-facing contracts.

2. **Two rendering patterns**: The content model includes immediate-mode HUD drawing (PostRender + Canvas) and retained-mode UWindow trees. In current SurrealEngine, the HUD path is the mature one; UWindow/GC remains partial and must be treated as ongoing engine work.

3. **Coordinate system**: Scripts expect pixel coordinates in logical space (pre-uiscale). Canvas properties `SizeX`, `SizeY`, `ClipX`, `ClipY` define the coordinate space. `StrLen` and `TextSize` must return accurate measurements in the same space.

4. **Bitmap fonts**: All text in UE1 games uses pre-rasterized font texture atlases from packages. Any font rendering path must support these.

5. **Blend modes**: Four blend modes (none, masked, translucent, modulated) controlled by `Canvas.Style` / `GC.Style`. These map to specific GPU blend states.

6. **3D-in-2D rendering**: `DrawActor` and `DrawClippedActor` render 3D meshes into 2D canvas regions. This requires render-to-texture or viewport switching.

7. **Animated textures**: `UTexture::GetAnimTexture()` returns the current frame of animated textures. The rendering layer must resolve these each frame.

8. **No current post-processing**: Adding a two-buffer composite (game + UI) requires introducing offscreen render targets that don't currently exist.

9. **Two render backends**: Vulkan (Linux/macOS/Windows) and D3D11 (Windows only). The `RenderDevice` abstraction means changes should ideally work through the abstract interface, but post-process additions will likely require backend-specific work.

10. **ZWidget independence**: The launcher/editor UI (ZWidget) is separate from in-game UI. It can be replaced independently, left alone, or handled later.

11. **Per-game extensions**: Deus Ex and other games add custom native window/UI classes. A replacement needs explicit extension points and per-game shims where script/native contracts diverge.

12. **UWindow tree not fully wired**: The current `RenderWindow.cpp` is a simplified debug renderer. The full UWindow paint cycle (`DrawWindow` → UnrealScript `Paint()` → GC/Canvas calls) is not yet implemented — there's a TODO comment at `RenderWindow.cpp:31`. This means the UWindow rendering path is incomplete in the current codebase regardless of any RmlUi work.
