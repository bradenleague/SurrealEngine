# VM/ — UnrealScript Bytecode Interpreter

Executes compiled UnrealScript from `.u` packages.

## Key Files

| File | Purpose |
|---|---|
| `Frame.h/cpp` | Call stack management, latent actions, local variables |
| `Bytecode.h/cpp` | Opcode decoding |
| `ExpressionEvaluator.h/cpp` | Expression execution (the main interpreter loop) |
| `ScriptCall.h/cpp` | `CallEvent()` — how C++ invokes UnrealScript events |
| `NativeFunc.h` | Native function dispatch table (index → C++ handler) |
| `Iterator.h` | ForEach iterator support |

## Calling UnrealScript from C++

```cpp
#include "VM/ScriptCall.h"

// By enum (fast, compile-time checked):
CallEvent(object, EventName::PostRender, { ExpressionValue::ObjectValue(canvas) });

// By name string (for dynamic/game-specific events):
CallEvent(object, NameString("CustomEvent"), { args... });
```

`EventName` enum covers ~60 probe events: `PreRender`, `PostRender`, `RenderOverlays`, `Tick`, `Timer`, `Touch`, etc.

## ExpressionValue

The universal value type for script ↔ C++ communication:

```cpp
ExpressionValue::IntValue(42)
ExpressionValue::FloatValue(1.0f)
ExpressionValue::StringValue("hello")
ExpressionValue::ObjectValue(someUObject)
ExpressionValue::BoolValue(true)
```

## Known Gaps

- **Dynamic arrays**: Not fully implemented in the VM
- **Network conditionals**: Missing (replication conditions)
- **Some uncommon opcodes**: Still unimplemented (for example `LabelTable` and `NativeParm`)
- **Debugger**: Supports breakpoints and stepping via `SurrealDebugger`
