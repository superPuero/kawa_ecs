# kawa::ecs

![Language](https://img.shields.io/badge/C%2B%2B-20-blue.svg)  
**kawa::ecs** is a *lightweight, modern and parallel ready* Entity-Component System for modern C++.  
It offers a minimal yet powerful API for building games, simulations, and real-time applications with both single-threaded and multi-threaded execution.

---

## Why kawa::ecs?

- **Fast** — cache-friendly storage and minimal indirection  
- **Expressive** — functional, intuitive query interface  
- **Parallel** — simple thread-pool integration for parallel processing  
- **No dependencies** — zero third-party requirements  
- **Debug-friendly** — rich runtime assertions

---

## Integration
```bash
cp -r kawa/ ./include/
```
```cpp
#include "kawa/ecs/kwecs.h"
```
- Compile with **C++20**  
- Profit!

---

## Quick Start

```cpp
#include "kwecs.h"
#include <iostream>
#include <string>

struct Position { float x, y; };
struct Velocity { float x, y; };
struct Label    { std::string name; };

void update_position(float dt, Position& pos, const Velocity& vel) {
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;
}

int main() {
    using namespace kawa::ecs;

    // Create registry
    registry reg({
        .name = "demo",
        .max_entity_count = 255,
        .max_component_types = 64
    });

    // Create entities
    entity_id e1 = reg.entity(Position{12.4f, 34.6f}, Velocity{2.0f, 3.0f});
    entity_id e2 = reg.entity(Position{10, 20}, Velocity{1, 1}, Label{"Ichigo"});

    // Query with optional component
    reg.query([](Position& pos, Label* label) {
        std::cout << (label ? label->name : "unnamed")
                  << " is at (" << pos.x << ", " << pos.y << ")\n";
    });

    // Fall-through parameter
    float dt = 0.16f;
    reg.query(update_position, dt);

    // Parallel query
    kawa::thread_pool tp(8);
    reg.query_par(tp, [](Position& pos, const Velocity& vel) {
        pos.x += vel.x;
        pos.y += vel.y;
    });
}
```

---

## API Overview

### Entity Management
| Call                                   | Purpose                                               |
|----------------------------------------|-------------------------------------------------------|
| `entity()`                             | Create a new empty entity                             |
| `entity(Ts... components)`             | Create entity with given components                   |
| `destroy(id)`                          | Destroy entity and remove all components              |
| `clone(from)`                          | Clone entity into a new one                           |
| `clone(from, to)`                      | Overwrite `to` with `from`’s components               |

### Component Management
| Call                                   | Purpose                                               |
|----------------------------------------|-------------------------------------------------------|
| `emplace<T>(id, args…)`                | Add component `T` to entity                           |
| `erase<Ts...>(id)`                     | Remove one or more components                         |
| `has<Ts...>(id)`                       | Check if entity has all listed components             |
| `get<T>(id)` / `get_if_has<T>(id)`     | Access component (ref / ptr)                          |
| `copy<Ts...>(from, to)`                 | Copy specific components                              |
| `move<Ts...>(from, to)`                 | Move specific components                              |

### Querying
| Call                                   | Purpose                                               |
|----------------------------------------|-------------------------------------------------------|
| `query(fn, args…)`                     | Iterate over matching entities                        |
| `query_par(tp, fn, args…)`              | Same as `query` but parallelized with thread pool     |
| `query_with(id, fn, args…)`             | Run query on a single entity                          |
| `query_info(fn)`                        | Iterate components with metadata for all entities     |
| `query_info_with(id, fn)`               | Iterate components with metadata for one entity       |

### Hooks
| Call                                   | Purpose                                               |
|----------------------------------------|-------------------------------------------------------|
| `on_construct(fn)`                     | Called when component is added to an entity                 |
| `on_destroy(fn)`                       | Called when component is removed from an entity             |

---

## Query Semantics

`registry::query` inspects your function parameters and matches entities accordingly.

### Parameter Groups
| Group         | Example                 | Meaning                                              |
|---------------|-------------------------|------------------------------------------------------|
| **Fall-through** | `T`, `T&`, `T*`         | Passed in from outside (not from components)         |
| **Required**     | `Component&` or `const Component&` | Entity must have this to match                       |
| **Optional**     | `Component*`           | `nullptr` if the entity lacks this component         |

**Order matters:** Fall-through → Components.  
Example:
```cpp
reg.query([](float dt, Position& pos, Label* name), 0.16f);
```

---

## Parallel Queries

`query_par` runs queries in parallel using a thread pool.  
- Pass your own `thread_pool` instance, or use the registry’s default.  
- Set `thread_count = 0` in `registry` constructor to disable parallelism.

---

> Made with care ❤️ — If you build something cool with **kawa::ecs**, share it!
