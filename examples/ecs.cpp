// ===== kawa::ecs Usage & API Documentation =====

//#define KAWA_ECS_STORAGE_MAX_UNIQUE_STORAGE_COUNT 128 // default is 256 

#include "../kawa/core/ecs.h"

#include <iostream>
#include <string>
#include <vector>


// === User-defined components ===
struct Position { float x, y; };
struct Velocity { float x, y; };
struct Label { std::string name; };
struct Health { int hp; };

struct Foo { Foo(kawa::entity_id i) { std::cout << "new foo for " << i << '\n'; } };

// === Example external queries ===
void print_label(Label* label)
{
    if (label)
        std::cout << "Entity Label: " << label->name << '\n';
    else
        std::cout << "Unnamed entity\n";
}

auto update_movement_factory(float dt)
{
    return [dt](Position& pos, const Velocity& vel)
        {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
        };
}

struct foo
{
    std::string name = "unnamed";
    int number = 42;
};

int main()
{
    using namespace kawa;

    registry reg
    ({
        .name = "demo",           
        .max_entity_count = 4096,   
        .max_component_count = 32,  

    });

    kawa::task_manager tm(16); // required for parallel queries

    entity_id dummy = reg.entity();

    reg.emplace<Label>(dummy, "Dummy");
    reg.emplace<Health>(dummy, 1);

    reg.on_construct
    (
        [&](entity_id id, Label& l)
        {
            reg.emplace<Foo>(id, id);
            std::cout << "New Label: " << l.name << " on " << id << '\n';
        }
    );

    reg.on_destruct
    (
        [](entity_id id, Label& l)
        {
            std::cout << "Destroyed Label: " << l.name << " on " << id << '\n';
        }
    );

    entity_id player = reg.entity(Position{ 0, 0 }, Velocity{ 1, 1 }, Label{ "Player" }, Health{ 100 });
    entity_id enemy = reg.entity(Position{ 10, 5 }, Label{ "Enemy" }, Health{ 50 });

    if (Label* label = reg.try_get<Label>(dummy))
        std::cout << "Dummy has label: " << label->name << '\n';

    Health& health = reg.get<Health>(player);
    std::cout << "Player has " << health.hp << " HP.\n";

    if (reg.has<Position, Velocity>(player))
        std::cout << "Player is movable.\n";

    entity_id ghost = reg.entity();
    reg.copy<Position, Label>(player, ghost);
    reg.move<Health>(enemy, ghost);
    reg.erase<Velocity, Label>(ghost);

    entity_id clone = reg.clone(enemy);
    entity_id clone2 = reg.clone(dummy);
    reg.clone(player, dummy); // Overwrites dummy's components

    float dt = 0.16f;

    reg.query
    (
        [=](Position& pos, Velocity& vel)
        {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
        }
    );

    reg.query
    (
        [](Position& pos, Label* label)
        {
            std::cout << (label ? label->name : "Unnamed")
                << " is at (" << pos.x << ", " << pos.y << ")\n";
        }
    );

    reg.query
    (
        [=](Position& pos, Label* label, Velocity& vel)
        {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            std::cout << (label ? label->name : "[No Label]") << " moved.\n";
        }
    );

    // External function
    reg.query(print_label);

    // Callable factory
    reg.query(update_movement_factory(dt));

    float total_health = 0;
    reg.query([&](Health& h) { total_health += h.hp; });
    std::cout << "Total health in system: " << total_health << '\n';

    reg.query
    (
        [](entity_id id, Label& label, Health* hp)
        {
            std::cout << "Entity " << id << " named " << label.name;
            if (hp) std::cout << " has " << hp->hp << " HP";
            std::cout << '\n';
        }
    );

    dyn_array<task_handle> query_handles;

    reg.query_par
    (
        tm,
        task_schedule_policy::ensure,
        8,
        query_handles,
        [](Position& pos, const Velocity& vel)
        {
            pos.x += vel.x;
            pos.y += vel.y;
        }
    );

    reg.query_par
    (
        tm,
        task_schedule_policy::ensure,
        8,
        query_handles,
        [](entity_id id, Position& pos)
        {
            std::cout << "Parallel Entity: " << id
                << " at (" << pos.x << ", " << pos.y << ")\n";
        }
    );

    tm.wait(query_handles);

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
            std::cout << "Player has component: " << info.type_info.name << '\n';
        }
    );

    reg.query_info
    (
        [](entity_id e, component_info info)
        {
            std::cout << "Entity " << e << " has component: " << info.type_info.name << '\n';
        }
    );

    // === 3.13 Entity destruction ===
    reg.destroy(clone);

    // === 4. Snapshotting / state transfer ===
    registry snapshot(reg);
    auto moved =  std::move(snapshot);

}