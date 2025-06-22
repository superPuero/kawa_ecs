#include <iostream>
#include <chrono>
#include "registry.h"

using namespace kawa::ecs;

struct Vec3 { float x, y, z; };
struct Velocity { float x, y, z; };
struct Health { int hp; };

constexpr size_t ENTITY_COUNT = 100'000'000;

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

    // Benchmark: Entity creation
    benchmark
    (
        "Entity Creation", 
        [&]() 
        {
            for (size_t i = 0; i < ENTITY_COUNT; ++i)
            {
                entities.push_back(reg.entity());
            }
        }
    );

    // Benchmark: Emplacing Vec3 and Velocity
    benchmark
    (
        "Add Position & Velocity", 
        [&]() 
        {
            for (entity_id id : entities)
            {
                reg.emplace<Vec3>(id, 1.0f, 2.0f, 3.0f );
                reg.emplace<Velocity>(id, 0.1f, 0.2f, 0.3f );
            }
        }
    );

    // Benchmark: Query with multiple components
    benchmark
    (
        "Query Vec3 + Velocity", 
        [&]() 
        {
            reg.query
            (
                [](Vec3& pos, Velocity& vel) 
                {
                    pos.x += vel.x;
                    pos.y += vel.y;
                    pos.z += vel.z;
                }
            );
        }
    );

    // Add Health to only half
    benchmark
    (
        "Add Health to half", 
        [&]() 
        {
            for (size_t i = 0; i < ENTITY_COUNT; i += 2)
            {
                reg.emplace<Health>(entities[i],  100 );
            }
        }
    );

    // Benchmark: Selective query (Vec3 + Health only)
    benchmark
    (
        "Query Vec3 + Health", 
        [&]() 
        {
            reg.query
            (
                [](Vec3& pos, Health& hp) 
                {
                    pos.x += hp.hp * 0.001f;
                }
            );
        }
    );

    // Benchmark: Destruction
    benchmark
    (
        "Destroy All", 
        [&]() 
        {
        for (entity_id id : entities)
            {
                reg.destroy(id);
            }
        }
    );

    // Benchmark: Re-adding entities (using the free entity queue)
    benchmark
    (
        "Re-Add Entities", 
        [&]() 
        {
            for (size_t i = 0; i < ENTITY_COUNT; ++i)
            {
                reg.entity();  // Re-add entities
            }
        }
    );

    return 0;
}
