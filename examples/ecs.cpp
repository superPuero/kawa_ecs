#include "../kawa/core/ecs.h"

using namespace kawa;

struct vec2
{
	f32 x;
	f32 y;
};

struct player_tag{};
struct enemy_tag {};

struct position
{
	vec2 value;
};

struct velocity
{
	vec2 value;
};

struct health
{
	i32 value;
};

int main()
{
	registry reg({ .max_entity_count = 4096, .max_component_count = 32 });

	reg.entity(
		player_tag{},
		position{ 12.4f, 43.4f },
		velocity{ 0.f, 0.f },
		health{100}
	);

	entity_id enemy = reg.entity(
		enemy_tag{},
		position{ 12.4f, 43.4f },
		velocity{ 0.f, 0.f}
	);

	entity_id enemy_2 = reg.clone(enemy);

	reg.query(
		[&](player_tag&, velocity& v)
		{
			//if(keyboard::key_press("w")) <- pseudo code
			{
				v.value.x = 1;
			}
			//else
			{
				v.value.x = 0;
			}
		}
	);

	f32 dt = 0.16f;

	reg.query(
		[=](position& pos, const velocity& vel)
		{
			pos.value.x += vel.value.x * dt;
			pos.value.y += vel.value.y * dt;
		}
	);

	auto defer_buffer = reg.defer();

	reg.query(
		[&](entity_id id, health& h)
		{
			if (h.value <= 0)
			{
				defer_buffer.destroy(id);
			}
		}
	);

	defer_buffer.flush();

}