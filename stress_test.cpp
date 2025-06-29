#include <iostream>
#include <chrono>
#include "registry.h"

using namespace kawa::ecs;

struct Vec3 { float x, y, z; };
struct Velocity { float x, y, z; };
struct Health { int hp; };

constexpr size_t ENTITY_COUNT = 1'000'000;

template <typename Fn>
double benchmark(const std::string& name, Fn&& fn)
{
    using clock = std::chrono::high_resolution_clock;

    auto start = clock::now();
    fn();
    auto end = clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "[ " << name << " ]: " << ms << " ms\n";
    return ms;
}

int main()
{
    registry reg(ENTITY_COUNT);

    std::vector<entity_id> entities;
    entities.reserve(ENTITY_COUNT);

    benchmark("Entity Creation", [&]() {
        for (size_t i = 0; i < ENTITY_COUNT; ++i)
            entities.push_back(reg.entity());
        });

    benchmark
    (
        "Add Position & Velocity",
        [&]()
        {
            for (entity_id id : entities)
            {
                reg.emplace<Vec3>(id, 1.0f, 2.0f, 3.0f);
                reg.emplace<Velocity>(id, 0.1f, 0.2f, 0.3f);
            }
        }
    );

    benchmark("Query Vec3 + Velocity", [&]() {
        reg.query([](Vec3* pos, Velocity* vel) {
            pos->x += vel->x;
            pos->y += vel->y;
            pos->z += vel->z;
            });
        });

    benchmark("Add Health to half", [&]() {
        for (size_t i = 0; i < ENTITY_COUNT; i += 2)
            reg.emplace<Health>(entities[i], 100);
        });

    benchmark("Query Vec3 + Health", [&]() {
        reg.query([](Vec3& pos, Health& hp) {
            pos.x += hp.hp * 0.001f;
            });
        });

    // Optional Health after required Vec3
    benchmark("Query Vec3 + optional Health", [&]() {
        reg.query([](Vec3& pos, Health* hp) {
            if (hp)
                pos.x += hp->hp * 0.001f;
            });
        });

    // Optional Health after required Velocity
    benchmark("Query Velocity + optional Health", [&]() {
        reg.query([](Velocity& vel, Health* hp) {
            if (hp)
                vel.x += hp->hp * 0.002f;
            });
        });

    // Both Vec3 and Velocity required, Health optional
    benchmark("Query Vec3 + Velocity + optional Health", [&]() {
        reg.query([](Vec3& pos, Velocity& vel, Health* hp) {
            pos.x += vel.x;
            if (hp)
                pos.x += hp->hp * 0.001f;
            });
        });

    // Optional only Health (no required components)
    // If your system allows this, stress it separately.
    benchmark("Query optional Health only", [&]() {
        size_t count = 0;
        reg.query([&](Health* hp) {
            if (hp) count++;
            });
        std::cout << "Entities with Health: " << count << '\n';
        });

    benchmark("Destroy All", [&]() {
        for (entity_id id : entities)
            reg.destroy(id);
        });

    benchmark("Re-Add Entities", [&]() {
        for (size_t i = 0; i < ENTITY_COUNT; ++i)
            reg.entity();
        });



    return 0;
}
