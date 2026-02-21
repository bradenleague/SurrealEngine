# Modern UI Architectural Goals for SurrealEngine

This document outlines high-level conceptual goals for the RmlUi integration, drawing inspiration from modern iterations of Unreal Engine (UE3, UE4, and UE5). These are not immediate implementation requirements, but rather "North Star" principles to guide the long-term architecture.

---

## 1. Persistent UI Lifecycle (The "GameInstance" Concept)
In UE1, the UI (HUD and UWindow) is deeply tied to the life of a `Level`. When a map changes, everything is torn down.

*   **Goal:** Decouple the RmlUi Context from the `Level` lifecycle.
*   **Modern Inspiration:** UE4's `GameInstance`.
*   **Benefit:** Allows for seamless loading screens, persistent global chat overlays, and multi-map transition animations. The UI remains "alive" and responsive even while the engine is destroying and recreating the 3D world.

## 2. Semantic Input & Focus Routing (The "CommonUI" Concept)
UE1 hardcodes raw inputs (mouse clicks, specific keycodes) directly into window logic. This makes supporting Gamepads or complex nested menus difficult.

*   **Goal:** Implement a semantic input middleware. Instead of passing `SDL_SCANCODE_SPACE`, the engine maps it to a UI action like `UI_Accept`. 
*   **Modern Inspiration:** UE5's `CommonUI`.
*   **Benefit:** Enables seamless switching between Mouse/Keyboard and Gamepad. It also simplifies "Focus Path" management—ensuring that if a modal dialog is open, it takes priority over the HUD or menus underneath without those layers needing to know about the modal.

## 3. Thematic Variable-Based Styling (The "Style Asset" Concept)
Maintaining visual consistency across different games (Unreal vs. UT99 vs. Deus Ex) can lead to duplicated CSS.

*   **Goal:** Lean heavily into RCSS (CSS) variables for all thematic elements (colors, borders, font-sizes).
*   **Modern Inspiration:** UE4/UE5's Slate Style Assets.
*   **Benefit:** The `.rml` documents (the HTML structure) can remain identical across different games, while the specific aesthetic is swapped simply by loading a different "Theme" stylesheet.
    *   *Unreal Gold:* Sets `--theme-primary` to translucent blue.
    *   *UT99:* Sets `--theme-primary` to gritty metallic red.
    *   *Deus Ex:* Sets `--theme-primary` to neon amber.

## 4. Decoupled ViewModels (The MVVM Pattern)
The original engine often draws UI by directly reaching into the Player actor's private variables during the render loop.

*   **Goal:** Enforce a strict View-ViewModel-Model (MVVM) boundary via the Data Model.
*   **Modern Inspiration:** UE4/UE5's implementation of Model-View-ViewModel.
*   **Benefit:** The UI never "knows" about the `APawn` or `AWeapon` classes directly. It only knows about a `HUDViewModel` struct. This makes the UI thread-safe, easier to debug, and allows the C++ data contract to handle the "translation" of messy legacy engine state into clean, UI-ready properties.

## 5. Player-Instanced UI
Modern Unreal assumes every UI element is owned by a `LocalPlayer`.

*   **Goal:** Design the Data Models to be instanced per-viewport rather than being global engine singletons.
*   **Modern Inspiration:** UE4 `UUserWidget` ownership.
*   **Benefit:** While UE1 games were primarily single-viewport, this architecture future-proofs the engine for split-screen support or advanced "Picture-in-Picture" security camera displays (common in Deus Ex) rendered via RmlUi.
