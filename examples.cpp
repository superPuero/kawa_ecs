// ===== kawa::ecs Example Usage & Documentation =====

 #include "kawa/ecs/kwecs.h"
//#include "single_header/kwecs.h"

#include <iostream>
#include <string>

// User defined components
struct Position
{
    float x;
    float y;
};

struct Velocity
{
    float x;
    float y;
};

struct Label
{
    std::string name;
};

// Sample functions used for queries
void display_labels(Label& label)
{
    std::cout << label.name << '\n';
}

// Example with fall-through argument (delta_time)
void update_position(float dt, Position& pos, Velocity& vel)
{
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;
}

int main(int argc, char** argv)
{
    using namespace kawa::ecs;

    // === 1. Constructing the Registry ===
    registry reg
    ({
        .max_entity_count = 255,            // Maximum number of entities the registry can work with
        .max_component_types = 64,          // Maximum number of unique component types
        .thread_count = 8,                  // Number of threads for query_par (0 = disable, default = half hardware threads)
        .debug_name = "example::registry"   // Debug name, improves assertion/debug output
    });

    // === 2. Creating Entities ===
    entity_id e1 = reg.entity();
    entity_id e2 = reg.entity();

    // Use entity_with to streamline entity creation
    // Move constructor implementation is strongly advised for components used in entity_with 
    entity_id e3 = reg.entity_with
    (
        Position{ 10, 20 },
        Velocity{ 1, 1 },
        Label{ "Ichigo" }
    );

    entity_id e4 = reg.entity();

    // Check entity validity
    if (e1 == nullent || !reg.is_valid(e1))
    {
        std::cout << "Invalid entity!" << '\n';
    }

    // === 3. Adding Components ===
    reg.emplace<Position>(e1, 12.4f, 34.6f);
    reg.emplace<Position>(e2, 21.8f, 43.2f);

    reg.emplace<Velocity>(e1, 2.0f, 3.0f);
    reg.emplace<Velocity>(e2, 2.0f, 4.0f);

    //reg.emplace<Label>(e1, "Dude");
    reg.emplace<Label>(e2, "Foo");

    // === 4. Accessing Components ===
    Position& pos_e1 = reg.get<Position>(e1);  // Required (must exist)
    Label* label_e2 = reg.get_if_has<Label>(e2); // Optional (may be nullptr)

    // === 5. Component Management ===

    //
    // Template Arity Convention
    // 
    // General rule:
    //  All void methods(e.g.erase, copy, move) are variadic (can operate on more than one component type at the time).
    //
    //  All methods that return values are not variadic, with one exception:
    //  has<Ts...>() is variadic, returns true only if the entity has all listed component types
    //

    // Copy components from e1 to e4
    reg.copy<Position, Velocity>(e1, e4);

    // Move components from e2 to e4, leveraging move-semantics (will erase them from e2)
    reg.move<Label, Velocity>(e2, e4);

    // Erase components from e4 (destructor invocation)
    reg.erase<Label, Velocity>(e4);

    // Check, does e3 has specific components
    bool has = reg.has<Position, Velocity>(e3);

    // Clone all components from an entity to a new one
    // No need to specify component types
    entity_id e5 = reg.clone(e3);

    // Clone all components from e3 into e4 (overwrites e4's components if they exist)
    reg.clone(e3, e4);

    // === 6. Queries ===
    // 
    // kawa::ecs::query is the primary mechanism for iterating over entities that match a specific component signature.
    // It automatically filters entities based on the components required by your function signature and calls your function with direct access to those components
    // This allows for fast, expressive, and safe logic application across only the relevant subset of entities—no manual filtering or casting needed.
    // 
    // However, queries must follow a strict convention known as querying semanticss:
    //
    // === Querying semantics ===
    // kawa::ecs::query supports function signatures of the form:
    //   [fall-through -> required -> optional]
    // 
    // The argument groups of function that is passed for querying must appear in this strict order:
    //  1. Fall-through group (can be any value, may be reference or pointer)
    //  2. Required group (T& or T ...) (references in case of T&, copies in case of T)
    //  3. Optional group (T* ...) (populated with pointer to component if it exists, nullptr if it does not)
    //
    // !!! You may NOT reorder or mix between groups.
    // !!! Each group can be any size (including empty).
    //
    // Valid:
    //     (float dt, Position& pos, Velocity& vel)
    //     (Position& pos, Label* optLabel)
    //
    // Invalid:
    //     (Position& pos, float dt), dt   // !!! fall-through must be first
    //     (Position& pos, Label* opt, Velocity& vel) // !!! optional can't be in the middle

    // Non-capturing lambda query
    reg.query
    (
        [](Label& label)
        {
            std::cout << label.name << '\n';
        }
    );

    // Plain function query
    reg.query(display_labels);

    float delta_time = 0.16f;

    // Capturing lambda query
    reg.query
    (
        [&](Position& pos, Velocity& vel)
        {
            pos.x += vel.x * delta_time;
            pos.y += vel.y * delta_time;
        }
    );

    // Fall-through (float dt)
    // Required (Position& pos, Velocity& vel)
    // Optional (Label* label)
    reg.query
    (                                     
        [](float dt, Position& pos, Velocity& vel, Label* label)
        {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            std::cout << "Updated Position for : "
                << (label ? label->name : "-") << '\n';
        }
        , delta_time
    );

    // Fall-through with plain function
    reg.query(update_position, delta_time);

    // Fall-through reference
    float sum_pos_x = 0;
    reg.query
    (
        [](float& accm, Position& pos, Velocity& vel)
        {
            accm += pos.x;
            std::cout << "Sum x is " << accm << " now" << '\n';
        }
        , sum_pos_x
    );

    // === 6.1. Self Querying ===
    //
    // query_self differs from `query` in that the **first parameter** of the callback
    // must be `entity_id`, allowing users to directly access the entity being iterated.
    // 
    // IMPORTANT:
    // Do NOT call `kawa::ecs::destroy(id)` directly in query function body, there are two possible solutions to this problem
	// 1. (more idiomatic) collect entities during query yourself and schedule destroying.
    // 2. (easier) use special `kawa::ecs::fetch_destroy(id)` to offload collecting and destroying entities inside queries to registry.
    //
    // Signature:
    //     [](entity_id id, fallthrough..., required..., optional...)

    reg.query_self
    (
        [](entity_id id, Position& pos, Velocity& vel)
        {
            pos.x += vel.x;
            pos.y += vel.y;
            std::cout << "Entity " << id << " moved to (" << pos.x << ", " << pos.y << ")\n";
        }
    );

    reg.query_self
    (
        [&](entity_id id, Label& label)
        {
            if (label.name == "Dude")
            {
                std::cout << "Scheduling delete for entity: " << id << '\n';
				reg.fetch_destroy(id); // Collect entities for destruction
            }
        }       
    // Accumulated entities will be destroyed right before query exits.
    );

    // === 6.2. Parallel Querying ===
    //
    // query_par behaves like `query`, but divides the work between multiple threads.
    // It's ideal for parallel updates, physics steps, AI, or any independent per-entity work.
    //
    // Default amount of threads allocated for is half of the hardware threads.
    // Currently you can change this by setting `KAWA_ECS_PARALLELISM` macro before including the header.
    // 
    // IMPORTANT:
    // Do NOT call `kawa::ecs::query_par()` inside another kawa::ecs::query_par() body
    // 
    // Signature and semantics identical to `query`

    reg.query_par
    (
        [&](Position& pos, Velocity& vel)
        {
			//reg.query_par([](Position& p) {std::cout << p.x; }); // Forbidden
            pos.x += vel.x;
            pos.y += vel.y;
        }
    );

    // "Self" par alternative
    reg.query_self_par
    (
        [](entity_id i, Position& pos, Velocity& vel)
        {
            pos.x += vel.x;
            pos.y += vel.y;

            // CAUTION: Cout is not synchronized
            std::cout << i << '\n';
        }
    );

    // === 6.3 Single-entity Query ===
    // First parameter is entity to which query will be applied
    reg.query_with(e3, update_position, delta_time);

    // === 7. Destroying an Entity ===
    reg.destroy(e3);

    std::cin.get();

    return 0;
}