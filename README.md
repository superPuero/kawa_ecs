# 🌊 **kawa::ecs**
![language](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![status](https://img.shields.io/badge/stability-stable-brightgreen)

*A tiny, lightning‑fast C++20 Entity‑Component System*

---

> **kawa::ecs** is a header‑only ECS that focuses on  
> **raw speed, near-zero dynamic allocations**, and a  
> *minimal, modern* API. Drop the header into any C++20 project and you  
> have an industrial‑strength data‑oriented backbone for games, simulations,  
> or large‑scale AI worlds.

---

## 🏗️ Building & Using

1. Copy **`registry.h`** into your include path.
2. `#include "registry.h"`.
3. Compile with **C++20**.
4. Profit.

No third‑party dependencies, no linkage order headaches.

---

## ✨ Features 

| 🚀                                | What                                                           | Details                                             |
| --------------------------------- | -------------------------------------------------------------- | ---------------------------------------------------- |
| **Ultra‑fast**                    | Aims to be as fast as possible while maintaining simplicity.   | Cache‑friendly *sparse‑poly* storage, freelist reuse |
| **Header‑only**                   | `registry.h`                                                   | No library to build, no deps                         |
| **Type‑safe**                     | `reg.emplace<Position>(e, …)`                                  | Compile‑time component IDs                           |
| **Variadic queries**              | `reg.query([](Pos&, Vel&){…});`                                | Accepts lambdas or functions, auto‑deduces args      |
| **Debug asserts**                 | `KW_ECS_ASSERT_MSG`                                            | Catches OOB & misuse in debug                        |

---

## 🛠️ Quick Start

```cpp
#include "registry.h"

using namespace kawa::ecs;

int main()
{
    registry reg(512);               

    entity_id e = reg.entity();      // create entity
    reg.emplace<Position>(e, 0, 0);  // add components
    reg.emplace<Velocity>(e, 1, 2);

    // iterate over matching archetype
    reg.query
    (
        [](Position& p, Velocity& v)
        {
          p.x += v.x; p.y += v.y;
        }
    );
    // More examples in examples.cpp
}
```

---

## 📚 API Cheat‑Sheet

| Call                                 | Purpose                                  |
| -------------------------------------| ---------------------------------------- |
| `registry(size_t max_entities)`      | Construct a registry                     |
| `entity_id entity()`                 | Allocate new entity or returns `nullent` |
| `emplace<T>(id, args…)`              | Construct component `T` on entity        |
| `erase<T>(id)`                       | Destroy component `T`                    |
| `has<T>(id)`                         | Check presence                           |
| `get<T>(id)` / `get_if_has<T>(id)`   | Access (ref / pointer)                   |
| `query(fn, args…)`                   | Iterate matching entities                |
| `query_with(id, fn, args…)`          | Call `fn` if entity matches              |
| `destroy(id)`                        | Remove entity & all its components       |
| `entity_with<Ts...>(Ts{args...}...)` | Create an entity and emplace components  |

---

## 🔍 Querying Semantics

The `registry::query` method supports **variadic parameter matching** with a strict grouping system:

```
[ fall-through..., required components..., optional components...]
```

Each group:

| Group            | Type signature example        | Notes                                            |
|------------------|-------------------------------|--------------------------------------------------|
| **Fall-through** | `T`, `T&`, `T*`               | `Passed in` from outside (mimics lambda capture) |
| **Required**     | `Component& / Component`      | Entity must have this to match                   |
| **Optional**     | `Component*`                  | Null if the entity lacks the component           |

### ⚠️ Grouping Rules

- Groups **must appear in this strict order**:  
  `fall-through` → `required` → `optional`
- **Each group may be empty**
 
---

- ✅ Valid:

```cpp
reg.query([](float dt, Position& pos, Velocity& vel), dt);
reg.query([](Position& pos, Label* label));
reg.query([](float dt, Label* label), dt);
```

- ❌ Invalid:

```cpp
reg.query([](Position& pos, float dt), dt); // ❌ fall-through must come first
reg.query([](Position& pos, Label* opt, Velocity& vel)); // ❌ optional must come last
```

---


## 🔄 Roadmap

* [ ] Parallel `query()` (thread‑safe sharding)
* [ ] Stable handles / versioned entity IDs
* [ ] Optional *archetype* packing mode
* [ ] Serialization helpers (binary & JSON)

---

> Made with love.  
> If you use `kawa::ecs` in something cool, let me know!
