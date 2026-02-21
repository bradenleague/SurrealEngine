# Native/ — UnrealScript Native Function Implementations

C++ implementations of built-in UnrealScript functions. 60+ files, each a static class (e.g., `NActor`, `NCanvas`, `NWindow`).

## Critical Constraint

Native function indices are **hardcoded in compiled .u bytecode**. Changing an index, parameter signature, or return type breaks all game content. Indices must match UE1's original native function table exactly.

## Registration Pattern

```cpp
// In RegisterFunctions():
RegisterVMNativeFunc_3("Window", "NewChild", &NWindow::NewChild, 1410);
//                      ^class    ^name       ^handler             ^index (MUST match UE1)

// Handler signature — Self is always the first implicit arg:
void NWindow::NewChild(UObject* Self, UObject* ChildClass, UObject** ReturnValue)
```

Variants: `RegisterVMNativeFunc_0` (no args) through `RegisterVMNativeFunc_N` (N args).

## Key Files for UI Work

| File | Native Indices | Purpose |
|---|---|---|
| `NCanvas.cpp` | 464-480 | Low-level drawing: DrawTile, DrawText, StrLen, DrawActor |
| `NGC.cpp` | 1200-1295 | UGC graphics context: higher-level drawing, state stack, fonts, alignment |
| `NWindow.cpp` | 1409-1499 | UWindow base: hierarchy, positioning, visibility, input, focus |
| `NButtonWindow.cpp` | — | Button states |
| `NCheckboxWindow.cpp` | — | Checkbox accessors |
| `NComputerWindow.h` | — | Deus Ex terminal emulator |

## Adding New Native Functions

New functions that don't exist in original UE1 can use indices in unused ranges. Check `VM/NativeFunc.h` for the dispatch table. Any index conflict = crash or wrong function called.

## Game-Specific Extensions

Files like `NDeusExPlayer.cpp` register additional natives for specific games. These are conditionally loaded based on `GameLaunchInfo`.
