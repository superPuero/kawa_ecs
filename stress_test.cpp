#include <iostream>
#include <chrono>
#include "registry.h"

#include <atomic>
#include <new>
#include <cstdlib>

using namespace kawa::ecs;

struct Vec3 { float x, y, z; };
struct Velocity { float x, y, z; };
struct Health { int hp; };
struct Score { float value; };
struct Flags { uint32_t mask; };
struct Tag { std::string label; };
struct Transform { Transform(const float* src) { std::copy(src, src + 16, matrix); } float matrix[16]; };
struct AI { int state; };

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

    benchmark
    (
        "Create Entities", 
        [&]() 
        {
            for (size_t i = 0; i < ENTITY_COUNT; ++i)
                entities.push_back(reg.entity());
        }
    );



    benchmark
    (
        "Add Base Components", [&]() 
        {
            for (entity_id id : entities) 
            {
                reg.emplace<Vec3>(id, 1.0f, 2.0f, 3.0f);
                reg.emplace<Velocity>(id, 0.1f, 0.2f, 0.3f);
                reg.emplace<Score>(id, 10.0f);
                reg.emplace<Flags>(id, 0xFF);
            }
        }
    );

    benchmark
    (
        "Add Extra Components (50%)", 
        [&]() 
        {
            for (size_t i = 0; i < ENTITY_COUNT; i += 2) 
            {
                reg.emplace<Health>(entities[i], 100);
                reg.emplace<AI>(entities[i], 1);
            }
        }
    );

    benchmark
    (
        "Add Tags (Every 10th)", 
        [&]() 
        {
            for (size_t i = 0; i < ENTITY_COUNT; i += 10) 
            {
                reg.emplace<Tag>(entities[i], "Agent_" + std::to_string(i));
            }
        }
    );

    benchmark
    (
        "Add Transforms (Every 4th)", 
        [&]() 
        {
            for (size_t i = 0; i < ENTITY_COUNT; i += 4) 
            {
                float mat[16] = {};
                reg.emplace<Transform>(entities[i], mat);
            }
        }
    );

    benchmark
    (
        "Copy Components: Vec3 + Health (50%)", 
        [&]() 
        {
            for (size_t i = 0; i < ENTITY_COUNT; i += 2) 
            {
                entity_id src = entities[i];
                entity_id dst = entities[i + 1];
                reg.copy<Vec3>(src, dst);
                reg.copy<Health>(src, dst);
            }
        }
    );

    // Move is erasing, so it will erase most of Score and Velocity in this use case, uncomment for benchmark
    //benchmark
    //(
    //    "Move Components: Score + Velocity (50%)", 
    //    [&]() 
    //    {
    //        for (size_t i = 0; i < ENTITY_COUNT; i += 2) 
    //        {
    //            entity_id src = entities[i + 1];
    //            entity_id dst = entities[i];
    //            reg.move<Score>(src, dst);
    //            reg.move<Velocity>(src, dst);
    //        }
    //    }
    //);

    benchmark
    (
        "Vec3 + Velocity + optional Score", 
        [&]() 
        {
            reg.query
            (
                [](Vec3& pos, Velocity& vel, Score* score) 
                {
                    pos.x += vel.x;
                    if (score) pos.y += score->value;
                }
            );
        });

    benchmark
    (
        "Fallthrough: delta + Vec3 + Velocity", 
        [&]() 
        {
            float dt = 0.016f;
            reg.query
            (
                [](float dt, Vec3& pos, Velocity& vel) 
                {
                    pos.x += vel.x * dt;
                    pos.y += vel.y * dt;
                    pos.z += vel.z * dt;
                }
                , dt
            );
        }
    );

    benchmark
    (
        "Fallthrough + optional AI", 
        [&]() 
        {
            int tick = 42;
            reg.query
            (
                [](int tick, AI* ai) 
                {
                    if (ai) ai->state += tick;
                }
                , tick
            );
        }
    );

    benchmark
    (
        "Optional Health Only", 
        [&]() 
        {
            size_t count = 0;
            reg.query
            (
                [&](Health* hp) 
                {
                    if (hp) count++;
                }
            );
            std::cout << "Entities with Health: " << count << "\n";
        });

    benchmark
    (
        "Fallthrough + Score + optional Tag", 
        [&]() 
        {
            float mult = 1.5f;
            reg.query
            (
                [](float mult, Score& s, Tag* tag) 
                {
                    s.value *= mult;
                }
            , mult
            );
        }
    );

    benchmark
    (
        "Multiple Fallthroughs + Velocity", 
        [&]() 
        {
            float scale = 2.0f;
            float offset = 0.5f;
            reg.query
            (
                [](float scale, float offset, Velocity& vel) 
                {
                    vel.x = vel.x * scale + offset;
                }
                , scale
                , offset
            );
        }
    );

    benchmark
    (
        "Purely Optional Query", 
        [&]() 
        {
            size_t count = 0;
            reg.query
            (
                [&](Vec3* p, Velocity* v, Health* h, Tag* t) 
                {          
                    if(p&&h) ++count;
                }
            );
            std::cout << "Mixed presence count: " << count << '\n';
        }
    );

    benchmark
    (
        "Query Vec3 Only", [&]() 
        {
            reg.query
            (
                [](Vec3& pos) 
                {
                    pos.z += 1.0f;
                }
            );
        }
    );

    benchmark
    (
        "Fallthrough (ref) + Vec3 + Health", [&]() 
        {
            int total = 0;
            reg.query
            (
                [](int& sum, Vec3& pos, Health* hp) 
                {
                    if (hp) sum += hp->hp;
                }
                , total
            );
            std::cout << "Total HP: " << total << '\n';
        }
    );

    benchmark
    (
        "Destroy All", [&]() 
        {
            for (entity_id id : entities)
                reg.destroy(id);
        }
    );


    return 0;
}


