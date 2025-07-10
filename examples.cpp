// ===== kawa::ecs Example Usage & Documentation =====

#define KAWA_ECS_PARALLELISM 8 // Set the number of threads for parallel queries (optional, default is half of hardware threads)
//#define KAWA_ECS_PARALLELISM 0 // Srtting this to 0 will turn paralellism off, invocation of every "par" query will be executed exclusively on a main thread

#include "registry.h"
#include <iostream>
#include <string>

// Define user components

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
    registry reg(256);  // Provide max amount of entities upfront

    // === 2. Creating Entities ===
    entity_id e1 = reg.entity();
    entity_id e2 = reg.entity();

    // Using entity_with to streamline entity creation
    // Move constructor implementation is strongly advised for components that will be used for streamline creation 
    entity_id e3 = reg.entity_with
    (
        Position{ 10, 20 },
        Velocity{ 1, 1 },
        Label{ "Ichigo" }
    );

    entity_id e4 = reg.entity();

    // Check entity validity
    if (e1 == nullent || !e1)
    {
        std::cout << "Invalid entity!" << '\n';
    }

    // === 3. Adding Components ===
    reg.emplace<Position>(e1, 12.4f, 34.6f);
    reg.emplace<Position>(e2, 21.8f, 43.2f);

    reg.emplace<Velocity>(e1, 2.0f, 3.0f);
    reg.emplace<Velocity>(e2, 2.0f, 4.0f);

    reg.emplace<Label>(e1, "Dude");
    reg.emplace<Label>(e2, "Foo");

    // === 4. Accessing Components ===
    Position& pos_e1 = reg.get<Position>(e1);  // Required (must exist)
    Label* label_e2 = reg.get_if_has<Label>(e2); // Optional (may be nullptr)

    // === 5. Component Management ===

    // Copy components from e1 to e4 (only Position and Velocity)
    reg.copy<Position, Velocity>(e1, e4);

    // Move components from e2 to e4, leveraging move-semantics (will erase them from e2)
    reg.move<Label>(e2, e4);

    reg.erase<Label>(e4);
    bool has = reg.has<Label>(e3);

    // Clone all components from an entity to a new one
    // No need to specify component types — clone works via internal dynamic dispatch.
    entity_id e5 = reg.clone(e3);

    // Clone all components from e3 into e4 (overwrites e4's components if they exist)
    reg.clone(e3, e4);

    // === 6. Querying semantics ===
    // kawa::ecs query supports function signatures of the form:
    //   [fall-through -> required -> optional]
    // 
    // The argument groups must appear in this strict order:
    //  1. Fall-through arguments (can be any value, may be reference or pointer)
    //  2. Required components (T& or T ...) (references in case of T&, copies in case of T)
    //  3. Optional components (T* ...) (populated with pointer component if exsists, nullptr is does not)
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
    // Do NOT call `reg.destroy(id)` or otherwise destroy the current entity from within a `query_self` loop.
    // Doing so can invalidate the internal iteration and cause crashes or undefined behavior.
    // If you need to destroy entities, collect them first:
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

    // === 6.2. Parallel Querying ===
    //
    // query_par behaves like `query`, but executes the loop body in multi-threaded context.
    // It's ideal for parallel updates, physics steps, AI, or any independent per-entity work.
    //
	// Default ammout of threads allocated for is half of the hardware threads.
	// Currently you can change this by setting `KAWA_ECS_PARALLELISM` macro before including the header.
    // 
    // Signature and semantics identical to `query`

    reg.query_par
    (
        [](Position& pos, Velocity& vel)
        {
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

    std::cout << KAWA_ECS_PARALLELISM << '\n';

    std::cin.get();

    return 0;
}
