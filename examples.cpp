// ===== kawa::ecs Usage & API Documentation =====

#include "kawa/ecs/kwecs.h"
// #include "single_header/kwecs.h"

#include <iostream>
#include <string>
#include <vector>

// === User-defined components ===
struct Position { float x, y; };
struct Velocity { float x, y; };
struct Label { std::string name; };
struct Health { int hp; };

// === Example external queries ===
void print_label(Label* label) 
{                                       
    if (label)
        std::cout << "Entity Label: " << label->name << '\n';
    else
        std::cout << "Unnamed entity\n";
}

void update_movement(float dt, Position& pos, const Velocity& vel) 
{
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;
}

auto update_movement_factory(float dt) 
{
    return [=](Position& pos, const Velocity& vel) 
        {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
        };
}

int main() 
{

    using namespace kawa::ecs;

    // === 1. Registry creation ===
    registry reg
        ({
            .name = "demo",           // Debug registry name
            .max_entity_count = 16,   // Max number of entities
            .max_component_types = 8  // Max unique component types
        });

    // === 1.1 Thread pool (required for parallel queries) ===
    kawa::thread_pool tp(16);

    // === 2. Entity creation ===
    entity_id dummy = reg.entity();

    // === 2.1 Component emplacement ===
    reg.emplace<Label>(dummy, "Dummy");
    reg.emplace<Health>(dummy, 1);

    // === 2.2 Lifetime hooks ===
    reg.on_construct
    (
        [](entity_id id, Label& l) 
        {
            std::cout << "New Label: " << l.name << " on " << id << '\n';
        }
    );

    reg.on_destroy
    (
        [](entity_id id, Label& l) 
        {
            std::cout << "Destroyed Label: " << l.name << " on " << id << '\n';
        }
    );

    // Streamlined creation (move construction recommended)
    entity_id player = reg.entity(Position{ 0, 0 }, Velocity{ 1, 1 }, Label{ "Player" }, Health{ 100 });
    entity_id enemy = reg.entity(Position{ 10, 5 }, Label{ "Enemy" }, Health{ 50 });

    // === 3.4 Accessing components ===
    if (Label* label = reg.get_if_has<Label>(dummy))
        std::cout << "Dummy has label: " << label->name << '\n';

    Health& health = reg.get<Health>(player);
    std::cout << "Player has " << health.hp << " HP.\n";

    // === 3.5 Component checks ===
    if (reg.has<Position, Velocity>(player))
        std::cout << "Player is movable.\n";

    // === 3.6 Copy / move / erase ===
    entity_id ghost = reg.entity();
    reg.copy<Position, Label>(player, ghost);
    reg.move<Health>(enemy, ghost);
    reg.erase<Velocity, Label>(ghost);

    // === 3.7 Cloning ===
    entity_id clone = reg.clone(enemy);
    entity_id clone2 = reg.clone(dummy);
    reg.clone(player, dummy); // Overwrites dummy's components

    // === 3.8 Queries ===
    float dt = 0.16f;

    // Required components
    reg.query
    (
        [=](Position& pos, Velocity& vel) 
        {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
        }
    );

    // Optional component
    reg.query
    (
        [](Position& pos, Label* label) 
        {
            std::cout << (label ? label->name : "Unnamed")
                << " is at (" << pos.x << ", " << pos.y << ")\n";
        }
    );

    // Mixed required + optional + fallthrough parameter
    reg.query
    (
        [](float delta, Position& pos, Label* label, Velocity& vel) 
        {
            pos.x += vel.x * delta;
            pos.y += vel.y * delta;
            std::cout << (label ? label->name : "[No Label]") << " moved.\n";
        }, dt
    );

    // External function
    reg.query(print_label);

    // External function + fallthrough
    reg.query(update_movement, dt);

    // Callable factory
    reg.query(update_movement_factory(dt));

    // Accumulator
    float total_health = 0;
    reg.query([](float& acc, Health& h) { acc += h.hp; }, total_health);
    std::cout << "Total health in system: " << total_health << '\n';

    // Getting entity_id inside query
    reg.query
    (
        [](entity_id id, Label& label, Health* hp) 
        {
            std::cout << "Entity " << id << " named " << label.name;
            if (hp) std::cout << " has " << hp->hp << " HP";
            std::cout << '\n';
        }
    );

    // === 3.10 Parallel queries ===
    reg.query_par
    (
        tp, 
        [](Position& pos, const Velocity& vel) 
        {
            pos.x += vel.x;
            pos.y += vel.y;
        }
    );

    reg.query_par
    (
        tp, 
        [](entity_id id, Position& pos) 
        {
            std::cout << "Parallel Entity: " << id
                << " at (" << pos.x << ", " << pos.y << ")\n";
        }
    );

    // === 3.11 Single-entity query ===
    reg.query_with
    (
        player, 
        [](Position& pos, Velocity& vel) 
        {
            pos.x += vel.x * 0.5f;
            pos.y += vel.y * 0.5f;
        }
    );

    // === 3.12 Reflection-info queries ===
    reg.query_info_with
    (
        player, 
        [](component_info info) 
        {
            std::cout << "Player has component: " << info.name << '\n';
        }
    );

    reg.query_info
    (
        [](entity_id e, component_info info) 
        {
            std::cout << "Entity " << e << " has component: " << info.name << '\n';
        }
    );

    // === 3.13 Entity destruction ===
    reg.destroy(clone);

    // === 4. Snapshotting / state transfer ===
    registry snapshot(reg); // on_construct hooks will run during copy

    std::cout << "Demo complete.\n";
}
