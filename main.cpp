// ===== kawa::ecs Example Usage & Documentation =====

#include "registry.h"

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

void update_position(float dt, Position& pos, Velocity& vel) 
{
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;
}

int main(int argc, char** argv) {
    using namespace kawa::ecs;

    // === 1. Constructing the Registry ===
    // The registry is the core of kawa::ecs and manages all entities and components.
    // You must provide the maximum number of entities the registry can hold.
    registry reg(256);

    // === 2. Creating Entities ===
    // Use reg.entity() to allocate a new entity.
    // If entity pool is exhausted, a special `nullent` is returned.
    entity_id e1 = reg.entity();
    entity_id e2 = reg.entity();
    entity_id e3 = reg.entity();
    entity_id e4 = reg.entity();

    // Example: checking entity validity
    if (e1 == nullent || !e1) 
    {
        std::cout << "Invalid entity!" << '\n';
    }

    // === 3. Adding Components ===
    // Use emplace<T>(id, args...) to add a component of type T to entity `id`.
    // Pass constructor arguments directly.
    reg.emplace<Position>(e1, 12.4f, 34.6f);
    reg.emplace<Position>(e2, 21.8f, 43.2f);
    reg.emplace<Position>(e3, 34.1f, 21.5f);

    reg.emplace<Velocity>(e1, 2.0f, 3.0f);
    reg.emplace<Velocity>(e2, 2.0f, 4.0f);
    reg.emplace<Velocity>(e3, 3.0f, 2.0f);

    reg.emplace<Label>(e1, "Dude");
    reg.emplace<Label>(e2, "Foo");
    reg.emplace<Label>(e3, "Ichigo");

    // === 4. Accessing Components ===

    // Use get<T>(id) to get a reference to a required component.
    Position& pos_e1 = reg.get<Position>(e1);

    // Use get_if_has<T>(id) for optional access.
    // Returns a pointer or nullptr if the component doesn't exist.
    Label* label_e2 = reg.get_if_has<Label>(e2);

    // === 5. Component Management ===

    // Remove a component from an entity.
    reg.erase<Label>(e2);

    // Check if an entity has a given component.
    bool has = reg.has<Label>(e3);

    // === 6. Querying Entities ===

    // A query will iterate over all entities that match the component signature.

    // Inline lambda that queries for all Labels.
    reg.query
    (
        [](Label& label) 
        {
            std::cout << label.name << '\n';
        }
    );

    // Passing a pre-defined function.
    reg.query(display_labels);

    float delta_time = 0.16f;

    // Inline lambda with capture and multiple components.
    reg.query
    (
        [&](Position& pos, Velocity& vel) 
        {
            pos.x += vel.x * delta_time;
            pos.y += vel.y * delta_time;
        }
    );

    // Lambda with external fall-thorough argument (dt passed in)
    reg.query
    (
        [](float dt, Position& pos, Velocity& vel) 
        {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
        }, 
        delta_time
    );

    // Passing a function that matches the query signature and with fall-thorough argument (passing dt).
    reg.query(update_position, delta_time);

    // === 7. Using reference fall-through arguments ===


    // Accumulator example  
    float sum_pos_x = 0;
    reg.query
    (
        [](float& accm, Position& pos, Velocity& vel) 
        {
            accm += pos.x;
            std::cout << "Sum x is " << accm << " now" << '\n';
        }, 
        sum_pos_x
    );

    // === 8. Single-entity Query ===
    // Useful for directly operating on a known entity.
    reg.query_with(e3, update_position, delta_time);

    // === 9. Destroying an Entity ===
    reg.destroy(e3);

    return 0;
}
