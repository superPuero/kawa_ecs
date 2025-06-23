# 🌊 **kawa::ecs**
![language](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![status](https://img.shields.io/badge/stability-stable-brightgreen)

*A tiny, lightning‑fast C++20 Entity‑Component System*

---

> **kawa::ecs** is a header‑only ECS that ocuses on
> f**raw speed, near-zero dynamic allocations**, and a
> *minimal, modern* API. Drop the header into any C++20 project and you
> have an industrial‑strength data‑oriented backbone for games, simulations,
> or large‑scale AI worlds.

---

## ✨ Features

| 🚀                                | What                                                           | Details                                              |
| --------------------------------- | -------------------------------------------------------------- | ---------------------------------------------------- |
| **Ultra‑fast**                    | Aims to be as fast as possible while maintaining simplicity.    | Cache‑friendly *sparse‑poly* storage, freelist reuse |
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
    registry reg(512);               // 512 max entities

    entity_id e = reg.entity();      // create entity
    reg.emplace<Position>(e, 0, 0);  // add components
    reg.emplace<Velocity>(e, 1, 2);

    // iterate over matching archetype(s)
    reg.query
    (
        [](Position& p, const Velocity& v)
        {
          p.x += v.x; p.y += v.y;
        }
    );
}
```
## 📝 Full Example

```cpp
// components
struct Position { float x, y; };
struct Velocity { float x, y; };
struct Label    { std::string name; };

void update(float dt, Position& p, const Velocity& v)
{
    p.x += v.x * dt;
    p.y += v.y * dt;
}

int main() {
    using namespace kawa::ecs;
    registry reg(256);

    auto e1 = reg.entity();
    reg.emplace<Position>(e1, 0, 0);
    reg.emplace<Velocity>(e1, 2, 3);
    reg.emplace<Label   >(e1, "Dude");

    // simple query
    reg.query([](Label& l){ std::cout << l.name << "\n"; });

    // query with external parameter
    float dt = 0.016f;
    reg.query(update, dt);
}
```

---

## 📚 API Cheat‑Sheet

| Call                               | Purpose                                  |
| ---------------------------------- | ---------------------------------------- |
| `registry(size_t max_entities)`    | Construct a registry                     |
| `entity_id entity()`               | Allocate new entity or returns `nullent` |
| `emplace<T>(id, args…)`            | Construct component `T` on entity        |
| `erase<T>(id)`                     | Destroy component `T`                    |
| `has<T>(id)`                       | Check presence                           |
| `get<T>(id)` / `get_if_has<T>(id)` | Access (ref / pointer)                   |
| `query(fn, extraArgs…)`            | Iterate matching entities                |
| `query_with(id, fn, extraArgs…)`   | Call `fn` if entity matches              |
| `destroy(id)`                      | Remove entity & all its components       |

## 🏗️ Building & Using

1. Copy **`registry.h`** into your include path.
2. `#include "registry.h"`.
3. Compile with **C++20**.
4. Profit.

No third‑party dependencies, no linkage order headaches.

---

## 🔄 Roadmap

* [ ] Optional query matching `query(Model* model)` 
* [ ] Parallel `query()` (thread‑safe sharding)
* [ ] Stable handles / versioned entity IDs
* [ ] Optional *archetype* packing mode
* [ ] Serialization helpers (binary & JSON)

---

> Made with love.
> If you use `kawa::ecs` in something cool,
> let me know!
