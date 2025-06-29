// ===== kawa::ecs Example Usage & Documentation =====

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

int main(int argc, char** argv) {
    using namespace kawa::ecs;

    // === 1. Constructing the Registry ===
    registry reg(256);  // Provide max amount of entities upfront

    // === 2. Creating Entities ===
    entity_id e1 = reg.entity();
    entity_id e2 = reg.entity();

    // Using entity_with to streamline entity creation
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

    // Move components from e2 to e4, levaraging move-semantics (will erase them from e2)
    reg.move<Label>(e2, e4);  

    reg.erase<Label>(e2);
    bool has = reg.has<Label>(e3);

    // === 6. Querying semantics ===
    // kawa::ecs query supports function signatures of the form:
    //   [fall-through -> required -> optional]

    reg.query
    (
        [](Label& label)
        {
            std::cout << label.name << '\n';
        }
    );

    reg.query(display_labels);

    float delta_time = 0.16f;

    reg.query
    (
        [&](Position& pos, Velocity& vel)
        {
            pos.x += vel.x * delta_time;
            pos.y += vel.y * delta_time;
        }
    );

    reg.query
    (
        [](float dt, Position& pos, Velocity& vel)
        {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
        }
        , delta_time
    );

    reg.query(update_position, delta_time);

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

    // === Optional components ===
    reg.query
    (
        [](Position& pos, Label* label_opt)
        {
            std::cout << "Position: (" << pos.x << ", " << pos.y << ")";
            std::cout << " Label: " << (label_opt ? label_opt->name : "-");
            std::cout << '\n';
        }
    );

    // === 7. Single-entity Query ===
    reg.query_with(e3, update_position, delta_time);

    // === 8. Destroying an Entity ===
    reg.destroy(e3);

    return 0;
}
