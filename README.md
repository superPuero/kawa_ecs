#  **kawa::ecs**

![language](https://img.shields.io/badge/C%2B%2B-20-blue.svg)  

---

> This is entity component system that focuses on **raw speed, near-zero dynamic allocations**, and a  
> *minimal, modern* API. Drop the header into any C++20 project and get an industrial-strength  
> data-oriented backbone for games, simulations, or large-scale AI worlds.

---

## 🏗️ Building & Using

**1. Single-header** 
1. Copy **`single_header/registry.h`** into your include path.  
2. `#include "registry.h"`  
3. Compile with **C++20**  
4. Profit!

**2. Organized source** 
1. Copy **`kawa`** directory into your include path.  
2. `#include "kawa/ecs/registry.h"`  
3. Compile with **C++20**  
4. Profit!

No third-party dependencies, no linkage headaches.

---

## ✨ Features

| 🚀                             | What                                                          | Details                                               |
| ------------------------------ | ------------------------------------------------------------- | ------------------------------------------------------|
| **Ultra-fast**                 | Designed for maximum performance while remaining simple       | Cache-friendly storage, mindful optimizations         |
| **Type-safe**                  | `reg.emplace<Position>(e, …)`                                 | Compile-time component IDs                            |
| **Functional queries**         | `reg.query([](Pos&, Vel&){…});`                               | Intuitive, flexible entity matching / system building |
| **Parallel queries**           | `reg.query_par([](Pos&, Vel&){…});`                           | Flexible and safe multithreading                      |
| **Debug asserts**              | `KW_ASSERT_MSG`                                               | Catch misuse early                                    |
| **Single-header-option**       | Single header version `single_header/registry.h`              | Drop & go                                             |

---

## 🛠️ Quick Start

```cpp
#include "registry.h"

#include <string>

using namespace kawa::ecs;

struct Position { float x, y; };
struct Velocity { float x, y; };
struct Name { std::string name; };

int main()
{
    registry reg(512);

    entity_id e1 = reg.entity();
    reg.emplace<Position>(e1, 0.f, 0.f);
    reg.emplace<Velocity>(e1, 1.f, 2.f);

    entity_id e2 = reg.entity_with
    (
        Position{ 10, 20 },
        Velocity{ 1, 1 },
        Name{ "Bar" } // Move constructor is strongly advised 
    );

    // Simple query
    reg.query
    (
        [](Position& p, Name* n)
        {
            std::cout << (n ? n->name : "unnamed") << " is at " << p.x << " " << p.y << '\n';
        }
    );

    float delta_time = 0.16;
    // Parallel query (multi-threaded)
    reg.query_par
    (
        [](float dt, Position& p, Velocity& v)
        {
            p.x += v.x * dt;
            p.y += v.y * dt;
        }
        , delta_time
    );

    return 0;
}

```
> More examples and documentation in **examples.cpp**
---

## 📚 API Cheat‑Sheet

| Call                                 | Purpose                                              |
| ------------------------------------ | ---------------------------------------------------- |
| `registry(size_t max_entities)`      | Construct registry                                   |
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

> You can configure thread count by defining the macro `KAWA_ECS_PARALLELISM` before including registry.h.

---

> Made with love.  
> If you use `kawa::ecs` in something cool, let me know!
