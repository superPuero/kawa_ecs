#include <iostream>
#include <chrono>
#include <atomic>

 #include "kawa/ecs/kwecs.h"        
//#include "single_header/kwecs.h"

using namespace kawa::ecs;

struct Vec3 { Vec3(float x, float y, float z) {}; Vec3(const Vec3& other) = default; float x, y, z; };
struct Velocity { float x, y, z; };
struct Position { float x, y, z; };

struct Health { int hp; };
struct Score { float value; };
struct Flags { uint32_t mask; };
struct Tag { std::string label; };
struct Transform { float matrix[32] = { 0 }; };
struct AI { int state; };
struct Enemy {};

auto update_movement_1(double dt) 
{
    return [=](Position& pos, const Velocity& vel) 
           {
                pos.x += vel.x * dt;
                pos.y += vel.y * dt;
                pos.z += vel.z * dt;
           };
}

void update_movement_2(double dt, Position& pos, const Velocity& vel)
{
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;
    pos.z += vel.z * dt;
}

constexpr size_t ENTITY_COUNT = 1'000'000;
constexpr size_t BENCH_COUNT = 1;

template <typename Fn>
double benchmark(const std::string& name, Fn&& fn, size_t count = BENCH_COUNT)
{
    double total = 0.0;
    for(size_t i = 0; i < count; i++)
    { 
        using clock = std::chrono::high_resolution_clock;
        auto start = clock::now();
        fn();
        auto end = clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        total += ms;
    }
	
    double avg = total / count;
    std::cout << "[ " << name << " ]: " << count  << " runs avg. time: " << avg << " ms,  " << 1000.0f/avg<< " fps\n";

    return avg;
}

int main()
{   
    kawa::thread_pool x(16);

    registry reg
    ({
        .name = "registry::stress_test",
        .max_entity_count = ENTITY_COUNT,
        .max_component_types = 16
    });

    std::vector<entity_id> entities;
    entities.reserve(ENTITY_COUNT);

    std::cout << '\n';

    benchmark
    (
        "Create Entities",
        [&]()
        {
            for (size_t i = 0; i < ENTITY_COUNT; ++i)
                entities.push_back(reg.entity());
        }, 1

    );

    benchmark
    (
        "Add Base Components", [&]()
        {
            for (entity_id id : entities)
            {
                reg.emplace<Vec3>(id, 1.0f, 2.0f, 3.0f);
                reg.emplace<Flags>(id, 0xFF);
                reg.emplace<Velocity>(id, 0.1f, 0.2f, 0.3f); 
                reg.emplace<Position>(id, 0.1f, 0.2f, 0.3f);
            }
        }, 1
    );


    benchmark
    (
        "Add Extra Components (50%)",
        [&]()
        {
            for (size_t i = 0; i < ENTITY_COUNT; i += 2)
            {
                reg.emplace<Health>(entities[i], 100);
                reg.emplace<Score>(entities[i], 10.0f);
                reg.emplace<AI>(entities[i], 1);
            }
        }, 1
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
        }, 1
    );

    benchmark
    (
        "Add Transforms (Every 4th)",
        [&]()
        {
            for (size_t i = 0; i < ENTITY_COUNT; i += 4)
            {
                reg.emplace<Transform>(entities[i]);
            }
        }, 1
    );

    benchmark
    (
        "Add Flag Component (Every 4th)",
        [&]()
        {
            for (size_t i = 0; i < ENTITY_COUNT; i += 4)
            {
                reg.emplace<Enemy>(entities[i]);
            }
        }, 1
    );

    benchmark
    (
        "Fallthrough + optional AI",
        [&]()
        {
            volatile size_t counter = 0;
            int tick = 42;
            reg.query
            (
                [&](int tick, AI* ai)
                {
                    if (ai) counter++;
                }
                , tick
            );
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
    //            reg.move<Score, Velocity>(src, dst);
    //        }
    //    }
    //);

    volatile double dt = 0.16;
    volatile double f = 123;
    volatile double g = 321;


    benchmark
    (
        "empty",
        [&]()
        {
            reg.query
            (
                [](entity_id e, float f, Position& pos)
                {
                    pos.x += 12 * f;
                }, 12.f

            );
        }
    );

    benchmark
    (
        "update_movement_1",
        [&]()
        {
            reg.query
            (
                update_movement_1(dt)
            );
        }
    );



    benchmark
    (
        "update_movement_1",
        [&]()
        {
            reg.query
            (
                [=](Position& pos, const Velocity& vel)
                    {
                        pos.x += vel.x * dt;
                        pos.y += vel.y * dt;
                        pos.z += vel.z * dt;
                    }
            );
        }
    );

    benchmark
    (
        "update_movement_2",
        [&]()
        {
            reg.query(update_movement_2, dt);
        }
    );


    benchmark
    (
        "Vec3 + Velocity + optional Score",
        [&]()
        {
            reg.query
            (
                [](Vec3& pos, Score* score, const Velocity& vel)
                {
                    pos.x += vel.x;
                    if (score) pos.y += score->value;
                }
            );
        }
    );

#if 1
    
    benchmark
    (
        "Parallel Vec3 + Velocity + optional Score",
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
        }
    );

    benchmark
    (
        "Fallthrough: delta + Vec3 + Velocity",
        [&]()
        {
            float dt = 0.016f;
        }
    );

    benchmark
    (
        "Parallel Fallthrough: delta + Vec3 + Velocity",
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
        "Parallel Fallthrough + optional AI",
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
            volatile size_t count = 0;
            reg.query
            (
                [&](Health* hp)
                {
                    if (hp) count++;
                }
            );
        }
    );

    benchmark
    (
        "Parallel Optional Health Only (Atomic Counter)",
        [&]()
        {
            std::atomic<size_t> count = 0;
            reg.query
            (
                [&](Health* hp)
                {
                    if (hp) count++;
                }
            );
        }
    );

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
        "Parallel Fallthrough + Score + optional Tag",
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
        "Parallel Multiple Fallthroughs + Velocity",
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
        "Query Entities with Flag",
        [&]()
        {
            volatile size_t count = 0;
            reg.query
            (
                [&](Enemy&, Vec3& pos)
                {
                    pos.y += 1.0f;
                    ++count;
                }
            );
        }
    );

    benchmark
    (
        "Parallel Query with Flag (Atomic counter)",
        [&]()
        {
            std::atomic<size_t> count = 0;
            reg.query
            (
                [&](Enemy&, Vec3& pos)
                {
                    pos.y += 1.0f;
                    ++count;
                }
            );
        }
    );


    benchmark
    (
        "Purely Optional Query",
        [&]()
        {
            volatile size_t count = 0;
            reg.query
            (
                [&](Vec3* p, Velocity* v, Health* h, Tag* t)
                {
                    if (p && h) ++count;
                }
            );
        }
    );

    benchmark
    (
        "Parallel Purely Optional Query (Atomic Counter)",
        [&]()
        {
            std::atomic<size_t> count = 0;
            reg.query
            (
                [&](Vec3* p, Velocity* v, Health* h, Tag* t)
                {
                    if (p && h) count.fetch_add(1);
                }
            );
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
        "Query: Vec3 Only",
        [&]()
        {
            reg.query(
                [](Vec3& pos)
                {
                    pos.x += 1.0f;
                }
            );
        }
    );

    benchmark
    (
        "Query Self: Vec3 Only",
        [&]()
        {
            reg.query_self(
                [](entity_id id, Vec3& pos)
                {
                    pos.x += 1.0f;
                }
            );
        }
    );

    benchmark
    (
        "Query Self: Fallthrough + Velocity",
        [&]()
        {
            float multiplier = 1.1f;
            reg.query_self
            (
                [](entity_id id, float m, Velocity& v)
                {
                    v.x *= m;
                    v.y *= m;
                    v.z *= m;
                }
            );
        }
    );

    benchmark
    (
        "Query Self: Optional Health",
        [&]()
        {
            volatile size_t count = 0;
            reg.query_self
            (
                [&](entity_id id, Health* hp)
                {
                    if (hp) count++;
                }
            );
        }
    );

    benchmark
    (
        "Fallthrough (ref) + Vec3 + Health", [&]()
        {
            volatile size_t total = 0;
            reg.query
            (
                [](volatile size_t& sum, Vec3& pos, Health* hp)
                {
                    if (hp) sum += hp->hp;
                }
                , total
            );
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
    
#endif

    std::cin.get();    
    
}
