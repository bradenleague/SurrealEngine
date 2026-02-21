# Surreal Engine - Current Status

This page is now split into two parts:

1. Code-verified status (grounded in the current repository).
2. Runtime observations (what users report while playing).

## Code-verified status (as of 2026-02-21)

### Game detection currently implemented

`GameFolderSelection::ExamineFolder()` recognizes these game/version signatures:

* Unreal: `200`, `220`, `224v`, `225f`, `226f`, `226b`, `227i`, `227j`, `227k_11`
* Unreal Tournament: `436`, `451`, `469a`, `469b`, `469c`, `469d`
* Deus Ex: `1002`, `1112fm`
* Klingon Honor Guard: `219`
* Nerf Arena Blast: `300`
* TNN Outdoors Pro Hunter: `200`
* Rune Classic: `1.10`
* Clive Barker's Undying: `420`
* Tactical Ops: Assault on Terror: UT `436` / `469d` signatures
* Wheel of Time: `333`

### UnrealScript VM status

The VM is close to complete, but there are still explicit unimplemented expression paths in `VM/ExpressionEvaluator.cpp`:

* `LabelTableExpression`
* `DynArrayElementExpression` (dynamic array element op)
* `NativeParmExpression`

### UI status (important)

* Canvas HUD path is active (`Render/RenderCanvas.cpp`, `Native/NCanvas.cpp`).
* `Canvas.DrawPortal` is registered but currently empty (`Native/NCanvas.cpp`).
* UWindow rendering is still a debug/skeleton path (`Render/RenderWindow.cpp` has a TODO for calling script draw/paint events).
* Many `UWindow` / `UGC` methods log unimplemented in `UObject/UWindow.cpp`.
* `NGC` + `NWindow` native registration is currently scoped to Deus Ex in `Package/PackageManager.cpp`.

### Networking status

* There is partial socket-layer work (`UObject/UInternetLink.cpp`, `UTcpLink`, `UUdpLink`).
* Gameplay networking/replication support is still not in place.

### Garbage collection status

* Mark-and-sweep GC exists (`GC/GC.cpp`).
* A periodic `GC::Collect()` call is not currently wired into the game loop.

## Runtime observations (needs ongoing revalidation)

These are gameplay-level issues and compatibility notes collected during testing. They are useful, but they are not all directly provable from static code inspection:

* Dynamic lighting support is incomplete.
* Portal/mirror behavior is still incomplete.
* Collision and mover edge cases remain.
* AI/native coverage varies significantly by game/version.
* 227/469 families still have substantial native coverage gaps.

If you add or update a runtime issue here, include:

* game + version,
* a concrete reproduction map/command,
* and whether the issue is confirmed on latest `master`.
