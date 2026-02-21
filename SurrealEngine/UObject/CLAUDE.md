# UObject/ — UE1 Object Model

C++ implementations of UE1's object hierarchy with reflection and serialization.

## Key Files

| File | Purpose |
|---|---|
| `UObject.h/cpp` | Base class for all engine objects |
| `UClass.h` | Class metadata, reflection, property layout |
| `UProperty.h` | Property descriptors (int, float, object ref, etc.) |
| `UActor.h` | Actor base — position, rotation, physics, rendering props |
| `UWindow.h` | Full UWindow widget hierarchy (1141 lines) |
| `UClient.h` | UCanvas and UViewport — the drawing surface |
| `UFont.h` | Bitmap font atlas (FontGlyph: texture + UV region) |
| `UTexture.h` | Texture hierarchy, animated textures |
| `UMesh.h` | Mesh types (lodmesh, skeletal) |
| `USound.h/UMusic.h` | Audio assets |

## UWindow Widget Tree (from UWindow.h)

```
UWindow (base: position, size, visibility, input, child tree)
├── UViewportWindow       — embedded 3D viewport
├── UTileWindow           — flow layout
├── UTextWindow           — text label
│   ├── UButtonWindow     — 6-state button
│   │   └── UToggleWindow → UCheckboxWindow
│   ├── UTextLogWindow    — scrolling log
│   └── ULargeTextWindow  → UEditWindow (text editor with undo)
├── UTabGroupWindow       — keyboard navigation
│   ├── URadioBoxWindow
│   ├── UClipWindow       — scroll viewport
│   └── UModalWindow → URootWindow (top-level, input routing)
├── UScrollAreaWindow     — scrollbars + clip
├── UScaleWindow          — slider/scrollbar
├── UScaleManagerWindow   — slider + label + buttons
├── UComputerWindow       — Deus Ex terminal
├── UBorderWindow         — 9-patch border
└── UListWindow           — multi-column sortable table
```

## UCanvas Properties (from UClient.h)

The immediate-mode drawing surface. Scripts receive this in PostRender/PreRender:

- Position: `CurX`, `CurY`, `OrgX`, `OrgY`
- Clipping: `ClipX`, `ClipY`, `SizeX`, `SizeY`
- Rendering: `DrawColor`, `Style` (typically 0/2/3/4; any non-zero style draws), `Font`, `Z`
- Flags: `bCenter`, `bNoSmooth`, `SpaceX`, `SpaceY`
- Fonts: `SmallFont`, `MedFont`, `LargeFont`, `BigFont`

## Property Access Pattern

UObject properties use accessor methods that return references:

```cpp
auto& health = UObject::Cast<UActor>(self)->Health();  // returns int&
auto& pos = actor->Location();                         // returns vec3&
auto& tex = canvas->Font();                            // returns UFont*&
```

## Object Creation

```cpp
auto obj = package->NewObject("name", someClass, ObjectFlags::Transient);
auto typed = UObject::Cast<USpecificType>(obj);
```

## Garbage Collection

Mark-and-sweep in `GC/`. Objects are GC-allocated (many via packages, some via direct `GC::Alloc`) and collected when unreachable. `ObjectFlags::Transient` marks objects not saved to disk.
