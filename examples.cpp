// ===== kawa::ecs Usage & API Documentation =====

//#include "kawa/ecs/kwecs.h"
#include "single_header/kwecs.h"


#include <iostream>          
#include <string>
#include <vector>

// === User-defined components ===
struct Position { float x, y; };
struct Velocity { float x, y; };
struct Label { std::string name; };
struct Health { int hp; };

// === User-defined queries
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
    // You access ecs throgh kawa::ecs namespace
    using namespace kawa::ecs;
{

    std::cin.ignore();          
    // === 1. Registry creation ===

    registry reg({
        .name = "demo", // <- Name for better debug assertions output
        .max_entity_count = 16, // <- Max ammount of entities that registry can hold
        .max_component_types = 8 // <- Max ammount of unique component types that registry can hold
        });

    // === 1.1 (Optional) Thread Pool creation (required for parallel queries) ===
    kawa::thread_pool tp(8); // <- Ammount of threads that pool will own

    // === 2. Entitiy creation ===
    entity_id dummy = reg.entity();

    // === 2.1 Component emplacement ===
    reg.emplace<Label>(dummy, "Dummy");
    reg.emplace<Health>(dummy, 1);

    // === 2.2 Lifetime Hooks == 
    // 
    // Component is deducted from arguments of a passed-in function
    reg.on_construct([](entity_id i, Label& l) { std::cout << "New Label: " << l.name << " on " << i << '\n'; });

    auto s = "Destroyed Label: ";  
                                           
    reg.on_destroy([=](entity_id i, Label& l) { std::cout << s << l.name << " on " << i << '\n'; });

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
    entity_id clone2 = reg.clone(dummy);

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

    // === 3.10. Parallel Queries ===
    // Same as query, but runs in parallel using multiple threads.
    // Pass kawa::parallel_executor object as first parameter, 
    reg.query_par
    (
        tp,
        [](Position& pos, const Velocity& vel)
        {
            pos.x += vel.x;
            pos.y += vel.y;
        }
    );

    reg.query_self_par
    (
        tp,
        [](entity_id id, Position& pos)
        {
            std::cout << "Parallel Entity: " << id << " at (" << pos.x << ", " << pos.y << ")\n";
        }
    );

    // === 3.11. Single-Entity Query ===
    reg.query_with
    (
        player, // <- First parameter is entity upon which query will be executed
        [](Position& pos, Velocity& vel)
        {
            pos.x += vel.x * 0.5f;
            pos.y += vel.y * 0.5f;
        }
    );

    // === 3.12. Reflection-Info Queries ===
    // These queries use ECS metadata reflection to inspect component types dynamically.

    // kawa::ecs::query_with_info
    // Calls the callback once for every component the entity has.
    // The callback receives a kawa::ecs::component_info (alias of kawa::meta::type_info) object
    // containing the component's name, type hash, will contain other metadata in future.

    reg.query_with_info
    (
        player, // <- First parameter is entity upon which query will be executed
        [](component_info info)
        {
            std::cout << "Player has component: " << info.name << '\n';
        }
    );

    //// kawa::ecs::query_self_info
    //// Calls the callback once for every component on every entity.
    //// The entity ID and component metadata are passed in.

    reg.query_self_info
    (
        [](entity_id e, component_info info)
        {
            std::cout << "Entity " << e << " has component: " << info.name << '\n';
        }
    );  

    // === 3.13. Entity Destruction ===
    reg.destroy(clone);

    // === 4. Snapshoting, state transfering
    // "Snapshoting" or deep coping all of registry state is as trivial as invoking copy 
    registry spanshot(reg); // <- be aware that appropriate on_construct callabacks will be invoked while copying all of source registry component state

    // Loading back can be performed either by copy or move assignment operator
    reg = spanshot;

    std::cout << "Demo complete.\n";
    
    std::cin.get();
}

std::cin.get();

}
