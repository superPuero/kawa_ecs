# 🧩 kawa::ecs

![language](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

**kawa::ecs** is a *lightweight, parallel-friendly C++ Entity-Component System* designed for clarity, speed, and simplicity.  
It provides a minimal and expressive interface for managing entities, components, and systems — optimized for both single-threaded and multithreaded workloads.

---

## ⚙️ Building & Integration

### 🔹 1. Single-header
1. Copy `single_header/kwecs.h` into your include path  
2. `#include "registry.h"`  
3. Compile with **C++20**  
4. Done

### 🔹 2. Organized source
1. Copy the `kawa/` directory into your include path  
2. `#include "kawa/ecs/kwecs.h"`  
3. Compile with **C++20**  
4. Done

✅ No third-party dependencies. No linkage headaches.

---

## ⚡ Features

| Feature                    | Description                                                            |
|---------------------------|-------------------------------------------------------------------------|
| **Fast**                  | Cache-friendly design with performance-focused internals                |
| **Functional queries**    | Intuitive entity matching with strong query semantics *read more below* |
| **Parallel queries**      | Natively parallel via `query_par(...)`                                  |
| **Debug-friendly**        | Rich assertions with `KAWA_ASSERT_MSG`                                  |
| **Single-header**         | Drop in version with `single_header/registry.h`                         |

---

## 🛠️ Quick Start

```cpp
#include "registry.h"

#include <string>

struct Position { float x, y; };
struct Velocity { float x, y; };
struct Label    { std::string name; };

void update_position(float dt, Position& pos, Velocity& vel) {
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;
}

int main() {
    using namespace kawa::ecs;

    registry reg({
        .max_entity_count = 255,
        .max_component_types = 64,
        .thread_count = 8,
        .debug_name = "example::registry"
    });

    entity_id e1 = reg.entity();
    reg.emplace<Position>(e1, 12.4f, 34.6f);
    reg.emplace<Velocity>(e1, 2.0f, 3.0f);

    entity_id e2 = reg.entity_with(
        Position{ 10, 20 },
        Velocity{ 1, 1 },
        Label{ "Ichigo" }
    );

    // Query with optional component
    reg.query([](Position& pos, Label* label) {
        std::cout << (label ? label->name : "unnamed") << " is at (" << pos.x << ", " << pos.y << ")\n";
    });

    // Update using fall-through argument
    float dt = 0.16f;
    reg.query(update_position, dt);

    // Parallel update
    reg.query_par([](Position& pos, Velocity& vel) {
        pos.x += vel.x;
        pos.y += vel.y;
    });

    return 0;
}

```
> More examples and documentation in **examples.cpp**
---

## 📚 API Cheat‑Sheet

| Call                                 | Purpose                                              |
| ------------------------------------ | ---------------------------------------------------- |
| `registry(reg_specs)`                | Construct registry                                   |
| `entity()`                           | Create new entity or return `nullent`                |
| `entity_with<Ts...>(Ts{}...)`        | Streamline entity and components creation            |
| `emplace<T>(id, args…)`              | Construct component `T` on entity with args          |
| `erase<T>(id)`                       | Remove component `T`                                 |
| `has<T>(id)`                         | Check if entity has component `T`                    |
| `get<T>(id)` / `get_if_has<T>(id)`   | Access component (reference / pointer)               |
| `query(fn, args…)`                   | Iterate matching entities                            |
| `query_self(fn, args…)`              | Iterate with `entity_id` as first function parameter |
| `query_par(fn, args…)`               | Parallel query on matching entities                  |
| `query_self_par(fn, args…)`          | Parallel self query                                  |
| `query_with(id, fn, args…)`          | Query a single entity                                |
| `copy<Ts...>(from, to)`              | Copy components                                      |
| `move<Ts...>(from, to)`              | Move components                                      |
| `clone(from, to)`                    | Clone all components from one entity into another    |
| `entity_id clone(id)`                | Clone all components from entity, returns new entity |
| `destroy(id)`                        | Destroy entity and all components                    |


---

## 🔍 Querying Semantics

The `registry::query` method is a **variadic parameter matching** system with a strict grouping system:

```
[ fall-through..., required components..., optional components...]
```

Each group:

| Group            | Type signature example        | Notes                                            |
|------------------|-------------------------------|--------------------------------------------------|
| **Fall-through** | `T`, `T&`, `T*`               | `Passed in` from outside (mimics lambda capture) |
| **Required**     | `Component& / Component`      | Entity must have this to match                   |
| **Optional**     | `Component*`                  | nullptr if the entity lacks the component        |

### ⚠️ Grouping Rules

- Groups **must appear in this strict order**:  
  `fall-through` → `required` → `optional`
- **Each group may be empty**
 
---

- ✅ Valid:

```cpp
reg.query([](float dt, Label* label), dt);
reg.query([](Position& pos, Label* label));
reg.query_self([](entity_id id, float dt, Position& pos, Velocity& vel), dt);
reg.query_par([](float dt, Position& pos, Velocity& vel), dt);
```

- ❌ Invalid:

```cpp
reg.query([](Position& pos, float dt), dt); // ❌ fall-through must come first
reg.query([](Position& pos, Label* opt, Velocity& vel)); // ❌ optional must come last
```

---

### 🧵 Parallel Queries
> **query_par** and **query_self_par** execute the query in *parallel*.

> By default, they use half of hardware threads.
                                            
> You can configure thread count by changing thread_count parameter while constructing registry, set it to 0 to turn off paralellism

---

> Made with care.  
> If you use `kawa::ecs` in something cool, let me know!
