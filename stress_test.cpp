#include "registry.h"
#include <iostream>
#include <chrono>

struct Position {
    float x, y;
};

struct Velocity {
    float dx, dy;
};

int main()
{
    using namespace std::chrono;
    using namespace kawa::ecs;
    constexpr size_t entity_count = 100'000'000;

    registry registry(entity_count);

    std::cout << "test for " << entity_count << " entities" << '\n';

    std::vector<entity_id> entities;
    entities.reserve(entity_count);

    auto start = high_resolution_clock::now();

    for (size_t i = 0; i < entity_count; ++i)
    {
        entities.push_back(registry.entity());
    }

    auto end = high_resolution_clock::now();
    std::cout << "Entity creation: " << duration_cast<milliseconds>(end - start).count() << " ms\n";

    start = high_resolution_clock::now();

    for (auto e : entities)
    {
        registry.emplace<Position>(e, 1.0f, 2.0f);
        registry.emplace<Velocity>(e, 0.5f, 1.5f);
    }

    end = high_resolution_clock::now();
    std::cout << "Component emplacement: " << duration_cast<milliseconds>(end - start).count() << " ms\n";

    start = high_resolution_clock::now();

    for (auto e : entities)
    {
        auto& pos = registry.get<Position>(e);
        auto& vel = registry.get<Velocity>(e);
        pos.x += vel.dx;
        pos.y += vel.dy;
    }

    end = high_resolution_clock::now();
    std::cout << "Component straight access/update: " << duration_cast<milliseconds>(end - start).count() << " ms\n";

    start = high_resolution_clock::now();

    registry.query
    (
        [](Position& pos, Velocity& vel)
        {
            pos.x += vel.dx;
            pos.y += vel.dy;
        }
    );

    end = high_resolution_clock::now();
    std::cout << "Query processing: " << duration_cast<milliseconds>(end - start).count() << " ms\n";

    start = high_resolution_clock::now();

    registry.query_with
    (
        entities[0],
        [](Position& pos, Velocity& vel)
        {
            pos.x += vel.dx;
            pos.y += vel.dy;
        }
    );

    end = high_resolution_clock::now();
    std::cout << "Query with processing: " << duration_cast<milliseconds>(end - start).count() << " ms\n";

    start = high_resolution_clock::now();

    for (auto e : entities)
    {
        registry.destroy(e);
    }

    end = high_resolution_clock::now();
    std::cout << "Entity/component destruction: " << duration_cast<milliseconds>(end - start).count() << " ms\n";

    return 0;
}
