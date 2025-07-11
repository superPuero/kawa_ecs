module;

#include "registry.h"

export module kawa.ecs;

export namespace kawa {
    namespace meta {
        using kawa::meta::get_ptr_type_count;
        using kawa::meta::get_ptr_type_count_tuple;
        using kawa::meta::sub_tuple;
        using kawa::meta::function_traits;
        using kawa::meta::query_traits;
        using kawa::meta::query_self_traits;
    }

    namespace ecs {
        using kawa::ecs::entity_id;
        using kawa::ecs::storage_id;
        using kawa::ecs::nullent;
        using kawa::ecs::registry;
    }
}
