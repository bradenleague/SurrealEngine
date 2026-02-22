# Render/ — High-Level Rendering Subsystem

Sits between the native function layer and the abstract `RenderDevice` GPU backend.

## Frame Rendering Order

```
RenderSubsystem::DrawGame(float levelTimeElapsed)
  ├─ ResetCanvas()           — set Canvas.SizeX/SizeY, uiscale, origin
  ├─ PreRender()             — CallEvent(console/actor, PreRender, {Canvas})
  ├─ DrawScene()             — 3D BSP world + actors
  ├─ RenderOverlays()        — CallEvent(actor, RenderOverlays, {Canvas})
  ├─ DrawRootWindow()        — UWindow tree (if dxRootWindow exists)
  ├─ PostRender()            — CallEvent(actor/console, PostRender, {Canvas}) [gated by UISuppressionFlags]
  └─ RmlUI Render            — RmlUi overlay (if initialized, via DrawUITriangles)
```

PostRender is gated by `UISuppressionFlags`: when `bRmlHUD` is true, actor PostRender is skipped; when `bRmlMessages`/`bRmlConsole` are true, console PostRender is skipped.

Final output is presented to the swapchain, but backends use offscreen intermediate targets (for example post-processing/bloom paths).

## Key Files

| File | Purpose |
|---|---|
| `RenderSubsystem.h/cpp` | Central coordinator, owns the frame loop above |
| `RenderCanvas.cpp` | Canvas drawing: DrawTile, DrawText, DrawClippedActor, UI scaling |
| `RenderWindow.cpp` | UWindow tree rendering (**incomplete** — has TODO for proper script-driven window drawing) |
| `BspClipper.cpp` | BSP visibility/clipping |
| `Lightmap/` | Lightmap generation and caching |

## UI Scaling

```cpp
int vertResolution = engine->LaunchInfo.engineVersion < 400 ? 768 : 960;
Canvas.uiscale = max((ViewportHeight + vertResolution/2) / vertResolution, 1);
```

Scripts work in logical coordinates. RenderSubsystem multiplies by `uiscale` before passing to RenderDevice. Integer scale only (1x, 2x, 3x).

## RenderDevice Interface (all UI draws bottom out here)

```cpp
Device->DrawTile(Frame, Info, X, Y, XL, YL, U, V, UL, VL, Z, Color, Fog, PolyFlags);
Device->DrawUITriangles(Frame, Info, Vertices, NumVertices, Indices, NumIndices);  // RmlUi
Device->Draw2DLine(Frame, Color, LineFlags, P1, P2);
Device->Draw2DPoint(Frame, Color, LineFlags, X1, Y1, X2, Y2, Z);
Device->ClearZ();
```

`DrawUITriangles` is a virtual with default no-op. Vulkan implementation uses `ScreenToView()` helper (shared with `DrawTile`) and `PF_Highlighted` pipeline for alpha blending.

Implementations: `RenderDevice/Vulkan/VulkanRenderDevice` and `RenderDevice/D3D11/D3D11RenderDevice`.

## UWindow Rendering Status

`RenderWindow.cpp` is ~100 lines of simplified debug rendering (draws backgrounds, buttons, borders directly). The real UWindow paint cycle should be script-driven per window. Current TODO (line 31): `// To do: CallEvent(window, "DrawWindow", { gc });`
