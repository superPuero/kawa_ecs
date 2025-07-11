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

        #ifdef KAWA_ECS_PASCALCASE_NAMES
        using kawa::meta::SubTuple;
        using kawa::meta::FunctionTraits;
        using kawa::meta::QueryTraits;
        using kawa::meta::QuerySelfTraits;
        #endif
    }

    namespace ecs {
        using kawa::ecs::entity_id;
        using kawa::ecs::storage_id;
        using kawa::ecs::nullent;
        using kawa::ecs::registry;

        #ifdef KAWA_ECS_PASCALCASE_NAMES
        using kawa::ecs::EntityID;
        using kawa::ecs::StorageID;
        using kawa::ecs::NULLENT;
        using kawa::ecs::Registry;
        #endif
    }
}
