#ifndef KAWA_ECS_BASE
#define KAWA_ECS_BASE

#include "../core/meta.h"

namespace kawa
{
namespace ecs
{
	using entity_id = size_t;
	using storage_id = size_t;
	using component_info = meta::type_info;
}
}

#endif 
