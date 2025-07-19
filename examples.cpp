// ===== kawa::ecs Rich Example Usage & API Documentation =====

#include "kawa/ecs/kwecs.h"
//#include "single_header/kwecs.h"
#include <iostream>
#include <string>
#include <vector>

// === 1. User-defined components ===
struct Position { float x, y; };
struct Velocity { float x, y; };
struct Label { std::string name; };
struct Health { int hp; };

// === 2. Free Functions for Queries ===
void print_label(Label* label) 
{
    if (label)
    {
        std::cout << "Entity Label: " << label->name << '\n';
    }
    else
    {
        std::cout << "Unnamed entity" << '\n';
    }
}

void update_movement(float dt, Position& pos, const Velocity& vel)
{
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;
}

int main() 
{
// === 3. kawa::ecs ===
    using namespace kawa::ecs;

    // === 3.1. Initialize Registry ===
    registry reg
    ({
        .max_entity_count = 1024,
        .max_component_types = 64,
        .thread_count = 8, // <- set thread_count to 0 to turn off paralellism
        .debug_name = "demo::ecs_registry"
    });

    // === 3.2. Create Entities ===
    entity_id dummy = reg.entity();

   // === 3.3. Emplace Components ===
    reg.emplace<Label>(dummy, "Dummy");
    reg.emplace<Health>(dummy, 20);

    // Streamlined creation, move constructor is strongly advised
    entity_id player = reg.entity_with(Position{ 0, 0 }, Velocity{ 1, 1 }, Label{ "Player" }, Health{ 100 });
    entity_id enemy = reg.entity_with(Position{ 10, 5 }, Velocity{ -0.5f, 0 }, Label{ "Enemy" }, Health{ 50 });


    // === 3.4. Accessing Components ===
    if (Label* label = reg.get_if_has<Label>(dummy)) 
    {
        std::cout << "Dummy has label: " << label->name << '\n';
    }

    Health& health = reg.get<Health>(player);
	std::cout << "Player has " << health.hp << " HP.\n";

    // === 3.5. Component Checks ===
    if (reg.has<Position, Velocity>(player)) 
    {
        std::cout << "Player is movable.\n";
    }

    // === 3.6. Component Copy / Move / Erase ===
    entity_id ghost = reg.entity();
    reg.copy<Position, Label>(player, ghost);
    reg.move<Health>(enemy, ghost);
    reg.erase<Velocity, Label>(ghost);

    // === 3.7. Cloning ===
    entity_id clone = reg.clone(enemy);
    reg.clone(player, dummy); // Overwrites dummy's components

    // === 3.8. Queries ===

    float dt = 0.16f;

    // Simple query with required components
    reg.query
    (
        [&](Position& pos, Velocity& vel)
        {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
        }
    );

    // Query with optional component
    reg.query
    (
        [](Position& pos, Label* label) 
        {
            std::cout << (label ? label->name : "Unnamed") << " is at (" << pos.x << ", " << pos.y << ")\n";
        }
    );

    // Mixed required and optional + fallthrough
    reg.query
    (
        [](float delta, Position& pos, Label* label, Velocity& vel) 
        {
            pos.x += vel.x * delta;
            pos.y += vel.y * delta;
            std::cout << (label ? label->name : "[No Label]") << " moved.\n";
		}, dt // <- passing fallthrough parameter after function, in order that they are in function signature
    );

    // Query with external function
    reg.query(print_label);

    // Query with external function + fallthrough
    reg.query(update_movement, dt);

    // Query with accumulator (fallthrough reference)
    float total_health = 0;
    reg.query
    (
        [](float& acc, Health& h) 
        {
            acc += h.hp;
        }, total_health
    );
    std::cout << "Total health in system: " << total_health << '\n';

    // === 3.9. Self Queries ===
	// Same as query, but populates the entity ID as the first parameter
    reg.query_self
    (
        [](entity_id id, Label& label, Health* hp) 
        {
            std::cout << "Entity " << id << " named " << label.name;
            if (hp) std::cout << " has " << hp->hp << " HP";
            std::cout << '\n';
        }
    );

    // Self query with conditional destroy
    reg.query_self([&](entity_id id, Health& hp) {
        if (hp.hp <= 0) {
            std::cout << "Entity " << id << " is dead. Scheduling destroy.\n";
            reg.fetch_destroy(id);
        }
        });

    // === 3.10. Parallel Queries ===
	// Same as query, but runs in parallel using multiple threads.
	// Specify ammount of threads to use in kawa::ecs::registry constructor, set it to 0 to turn off parallelism.
    reg.query_par
    (
        [](Position& pos, const Velocity& vel) 
        {
            pos.x += vel.x;
            pos.y += vel.y;
        }
    );

    reg.query_self_par
    (
        [](entity_id id, Position& pos) 
        {
            std::cout << "Parallel Entity: " << id << " at (" << pos.x << ", " << pos.y << ")\n";
        }
    );

    // === 3.11. Single-Entity Query ===
    reg.query_with
    (   
		player, // <- First parameter if entity upon which query will be executed
        [](Position& pos, Velocity& vel) 
        {
            pos.x += vel.x * 0.5f;
            pos.y += vel.y * 0.5f;
        }
    );

    // === 3.12. Entity Destruction ===
    reg.destroy(clone);
    reg.destroy(dummy);

    std::cout << "Demo complete.\n";

    std::cin.get();

    return 0;
}
