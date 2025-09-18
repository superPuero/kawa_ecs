#ifndef KAWA_ECS_BASE
#define KAWA_ECS_BASE

#include "../../../core/core_types.h"
#include "../../../core/meta.h"

namespace kawa
{
namespace ecs
{
	using entity_id = u64;
	using storage_id = u64;
	using component_info = meta::type_info;
}
}

#endif 
