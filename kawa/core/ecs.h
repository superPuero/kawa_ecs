#ifndef KAWA_ECS
#define KAWA_ECS

#include "core_types.h"
#include "macros.h"
#include "fast_map.h"
#include "task_manager.h"

namespace kawa
{
	struct entity_id
	{
		constexpr static u64 invalid = std::numeric_limits<u64>::max();

		entity_id() noexcept : val(invalid) {}
		entity_id(u64 id) noexcept : val(id) {}

		bool is_valid() noexcept
		{
			return val != invalid;
		}

		operator u64& () noexcept
		{
			return val;
		}

		operator const u64& () const noexcept
		{
			return val;
		}

		u64 val;
	};

	struct component_info
	{
		meta::type_info type_info;
		void* value_ptr;
	};

	struct _opaque_callback_wrap
	{
		using invoker_fn_t = void(unsized_any&, usize, void*);

		unsized_any _storage;
		invoker_fn_t* invoker = nullptr;


		_opaque_callback_wrap() noexcept = default;
		_opaque_callback_wrap(const _opaque_callback_wrap& other) noexcept = default;
		_opaque_callback_wrap(_opaque_callback_wrap& other) noexcept = default;

		_opaque_callback_wrap& operator=(const _opaque_callback_wrap& other) noexcept = default;
		_opaque_callback_wrap& operator=(_opaque_callback_wrap& other) noexcept = default;


		template<typename Fn, typename...Args>
		void refresh(Args&&...args)
		{
			static_assert(std::is_same_v<typename meta::function_traits<Fn>::template arg_at<0>, entity_id>, "lifetime hook callbacks require first parameter to be entity id");

			using T = std::remove_cvref_t<typename meta::function_traits<Fn>::template arg_at<1>>;

			_storage.refresh<Fn>(std::forward<Args>(args)...);
			invoker = +[](unsized_any& fn, usize e, void* comp)
				{
					fn.unwrap<Fn>()(e, *reinterpret_cast<T*>(comp));
				};
		}

		void release()
		{
			_storage.release();
			invoker = nullptr;
		}

		void try_invoke(usize index, void* comp) noexcept
		{
			if (invoker)
			{
				invoker(_storage, index, comp);
			}
		}
	};
	
	struct component_storage : indirect_array_base
	{
		using func_storage_t = unsized_any;
		using on_construct_invoke_fn_t = void(func_storage_t&, usize, void*);
		using on_destruct_invoke_fn_t = void(func_storage_t&, usize, void*);

		lifetime_vtable _vtable;

		_opaque_callback_wrap on_construct_callback;
		_opaque_callback_wrap on_destruct_callback;

		template<typename T>
		component_storage(meta::construct_tag<T>, usize capacity)
		{
			refresh<T>(capacity);
		}

		component_storage& operator=(const component_storage& other)
		{
			if (this != &other)
			{
				release();

				_vtable = other._vtable;
				_capacity = other._capacity;
				_occupied = other._occupied;

				_allocate();

				memcpy(_mask, other._mask, _capacity * sizeof(_mask[0]));
				memcpy(_indirect_map, other._indirect_map, _capacity * sizeof(_indirect_map[0]));
				memcpy(_reverse_indirect_map, other._reverse_indirect_map, _capacity * sizeof(_reverse_indirect_map[0]));
				
				on_destruct_callback = other.on_construct_callback;
				on_construct_callback = other.on_construct_callback;

				for (auto e : other)
				{
					_vtable.copy_ctor_offset(other._storage, e, _storage, e);
				}
			}

			return *this;
		}

		component_storage(const component_storage& other)
		{
			*this = other;
		}

		component_storage& operator=(component_storage&& other)
		{
			if (this != &other)
			{
				release();

				_vtable = other._vtable;
				_capacity = other._capacity;
				_occupied = other._occupied;
				_storage = other._storage;
				_mask = other._mask;
				_indirect_map = other._indirect_map;
				_reverse_indirect_map = other._reverse_indirect_map;
				
				other._vtable.release();
				other.release();
			}

			return *this;
		}

		component_storage(component_storage&& other)
		{
			*this = std::move(other);
		}

		~component_storage()
		{
			release();
		}

		template<typename T>
		void refresh(usize capacity)
		{
			release();

			_vtable.refresh<T>();
			_capacity = capacity;

			_allocate();
		}

		void _allocate()
		{
			_storage = (u8*)::operator new(_capacity * _vtable.type_info.size, std::align_val_t{ _vtable.type_info.alignment });
			_mask = new bool[_capacity] {};
			_indirect_map = new usize[_capacity]{};
			_reverse_indirect_map = new usize[_capacity]{};
		}

		void release() noexcept
		{
			if (_vtable.type_info)
			{
				for (auto i : *(indirect_array_base*)this)
				{
					_destruct_try_callback(i);
				}

				::operator delete(_storage, _vtable.type_info.size * _capacity, std::align_val_t{ _vtable.type_info.alignment });

				_storage = nullptr;

				delete[] _mask;
				delete[] _indirect_map;
				delete[] _reverse_indirect_map;
				_occupied = 0;

				_vtable.release();

				on_construct_callback.release();
				on_destruct_callback.release();
			}
		}

		template<typename T, typename...Args>
		T& _construct_try_callback(usize index, Args&&...args)
		{
			auto out = new (reinterpret_cast<T*>(_storage) + index) T(std::forward<Args>(args)...);

			on_construct_callback.try_invoke(index, (void*)((u8*)_storage + index * _vtable.type_info.size));

			return *out;
		}

		void _copy_try_callback(usize from, usize to)
		{
			_vtable.copy_ctor_offset(_storage, from, _storage, to);

			on_construct_callback.try_invoke(to, (void*)((u8*)_storage + to * _vtable.type_info.size));
		}

		void _move_try_callback(usize from, usize to)
		{
			_vtable.move_ctor_offset(_storage, from, _storage, to);

			on_construct_callback.try_invoke(to, (void*)((u8*)_storage + to * _vtable.type_info.size));
		}

		void _destruct_try_callback(usize index)
		{
			on_destruct_callback.try_invoke(index, (void*)((u8*)_storage + index * _vtable.type_info.size));

			_vtable.dtor_offset(_storage, index);
		}

		template<typename Fn>
		void set_on_construct(Fn&& func)
		{
			on_construct_callback.refresh<Fn>(std::forward<Fn>(func));
		}

		template<typename Fn>
		void set_on_destruct(Fn&& func)
		{
			on_destruct_callback.refresh<Fn>(std::forward<Fn>(func));
		}

		template<typename T, typename...Args>
		T& emplace(usize index, Args&&...args) noexcept
		{
			kw_assert(_vtable.type_info.is<T>());
			kw_assert(index < _capacity);

			_refresh_init(index);

			return _construct_try_callback<T>(index, std::forward<Args>(args)...);
		}

		void _refresh_init(usize index) noexcept
		{
			bool& cell = _mask[index];

			if (!cell)
			{
				cell = true;

				usize _indirect_index = _occupied++;

				_reverse_indirect_map[index] = _indirect_index;
				_indirect_map[_indirect_index] = index;
			}
			else
			{
				_destruct_try_callback(index);
			}
		}

		void erase(usize index) noexcept
		{
			kw_assert(index < _capacity);

			bool& cell = _mask[index];

			if (cell)
			{
				_destruct_try_callback(index);

				usize _indirect_index = _reverse_indirect_map[index];

				_occupied--;
				_indirect_map[_indirect_index] = _indirect_map[_occupied];
				_reverse_indirect_map[_indirect_map[_occupied]] = _indirect_index;

				cell = false;
			}
		}

		template<typename T>
		T& get(usize index) noexcept
		{
			kw_assert_msg(_vtable.type_info.is<T>(), "got: {} expected: {}", meta::type_name<T>(), _vtable.type_info.name);
			kw_assert(index < _capacity);
			kw_assert(_mask[index]);

			return reinterpret_cast<T*>(_storage)[index];
		}

		template<typename T>
		const T& get(usize index) const noexcept
		{
			kw_assert(_vtable.type_info.is<T>());
			kw_assert(index < _capacity);
			kw_assert(_mask[index]);

			return reinterpret_cast<T*>(_storage)[index];
		}

		template<typename T>
		T* try_get(usize index) noexcept
		{
			kw_assert(index < _capacity);
			kw_assert(_vtable.type_info.is<T>());

			if (_mask[index])
			{
				return (reinterpret_cast<T*>(_storage) + index);
			}

			return nullptr;
		}

		template<typename T>
		const T* try_get(usize index) const noexcept
		{
			kw_assert(index < _capacity);
			kw_assert(_vtable.type_info.is<T>());

			if (_mask[index])
			{
				return (reinterpret_cast<T*>(_storage) + index);
			}

			return nullptr;
		}

		void move(usize from, usize to)
		{
			kw_assert(_valid_index(from));
			kw_assert(_valid_index(to));
			kw_assert(contains(from));

			_refresh_init(to);

			_move_try_callback(from, to);

			erase(from);
		}

		void copy(usize from, usize to)
		{
			kw_assert(_valid_index(from));
			kw_assert(_valid_index(to));
			kw_assert(contains(from));

			_refresh_init(to);

			_copy_try_callback(from, to);
		}

		bool _valid_index(usize index) noexcept
		{
			return index < _capacity;
		}

		usize occupied() noexcept
		{
			return _occupied;
		}

		indirect_array_base& as_base() noexcept
		{
			return *(indirect_array_base*)this;
		}

	};

	struct registry
	{
		struct defer_buffer
		{
			struct config
			{
				registry& registry;
				bool flush_on_dtor = true; 
				bool fifo = true;
			};
			defer_buffer(const config& cfg) noexcept : _cfg(cfg) {}
			defer_buffer(defer_buffer&& other) noexcept = default;

			~defer_buffer()
			{
				if(_cfg.flush_on_dtor)
				{
					flush();
				}
			}

			template<typename T>
			defer_buffer& add(entity_id index, T&& v)
			{
				tasks.emplace_back(
					[=, this, v = std::forward<T>(v)]() mutable
					{
						_cfg.registry.add(index, std::forward<T>(v));
					}
				);

				return *this;
			}

			template<typename T, typename...Args>
			defer_buffer& emplace(entity_id index, Args&&...args)
			{
				tasks.emplace_back(
					[=, this, ...args = std::forward<Args>(args)]() mutable
					{
						_cfg.registry.emplace<T>(index, std::forward<Args>(args)...);
					}
				);

				return *this;
			}

			template<typename...Args>
			defer_buffer& erase(entity_id e)
			{
				tasks.emplace_back(
					[=, this]() mutable
					{
						_cfg.registry.erase<Args...>(e);
					}
				);

				return *this;
			}

			template<typename...Args>
			defer_buffer& copy(entity_id from, entity_id to)
			{
				tasks.emplace_back(
					[=, this]() mutable
					{
						_cfg.registry.copy<Args...>(from, to);
					}
				);

				return *this;
			}

			template<typename...Args>
			defer_buffer& move(entity_id from, entity_id to)
			{
				tasks.emplace_back(
					[=, this]() mutable
					{
						_cfg.registry.move<Args...>(from, to);
					}
				);

				return *this;
			}

			defer_buffer& clone(entity_id from, entity_id to)
			{
				tasks.emplace_back(
					[=, this]() mutable
					{
						_cfg.registry.clone(from, to);
					}
				);

				return *this;
			}

			defer_buffer& clone(entity_id from)
			{
				tasks.emplace_back(
					[=, this]() mutable
					{
						_cfg.registry.clone(from);
					}
				);

				return *this;
			}

			defer_buffer& destroy(entity_id id)
			{
				tasks.emplace_back(
					[=, this]() mutable
					{
						_cfg.registry.destroy(id);
					}
				);

				return *this;
			}

			void flush()
			{
				if (_cfg.fifo)
				{
					for (auto& t : tasks)
					{
						t();
					}					
				}
				else
				{
					for (auto& t : std::views::reverse(tasks))
					{
						t();
					}
				}

				tasks.clear();
			}

			dyn_array<task_fn> tasks;
			config _cfg;
		};

		struct config
		{
			string name = "unnamed";
			usize max_entity_count = 128;
			usize max_component_count = 128;
		};

		registry(const config& cfg)
			: _cfg(cfg)
			, _storages({ .capacity = cfg.max_component_count, .collision_depth = 16 })
			, _free_list(cfg.max_entity_count)
			, _entries(cfg.max_entity_count)
		{
		}

		registry(const registry& other) = default;
		registry(registry&& other) = default;

		registry& operator=(const registry& other) = default;
		registry& operator=(registry&& other) = default;


		usize entity_count()
		{
			return _entries.occupied();
		}

		template<typename...Args>
		entity_id entity(Args&&...args)
		{
			entity_id id;

			if (!_free_list.empty())
			{
				id = _free_list[_free_list.occupied()];
				_free_list.erase(_free_list.occupied());
			}
			else if (_id_counter < _cfg.max_entity_count)
			{
				id = _id_counter++;
			}

			kw_assert(id.is_valid());

			_entries.emplace(id);

			((emplace<Args>(id, std::forward<Args>(args))), ...);

			return id;
		}

		template<typename T>
		struct is_optional_arg
		{
			constexpr static bool value = std::is_pointer_v<T>;
		};

		template<typename T>
		struct is_required_arg
		{
			constexpr static  bool value = std::is_reference_v<T>;
		};

		template<typename T>
		struct _not_entity_id : std::true_type {};

		template<>
		struct _not_entity_id<entity_id> : std::false_type {};

		template<typename Fn>
		struct query_traits
		{
			using dirty_args = meta::function_traits<Fn>::args_tuple;
			using clear_args = meta::transform_each_t<std::remove_cvref_t, meta::transform_each_t<std::remove_pointer_t, dirty_args>>;

			using require_args = meta::filter_tuple_t<_not_entity_id, meta::filter_tuple_t<is_required_arg, dirty_args>>;
			using clean_require_args = meta::transform_each_t<std::remove_cvref_t, require_args>;

			constexpr static bool has_required_components = std::tuple_size_v<clean_require_args> > 0;
		};

		template<typename Fn>
		void query_info(Fn&& info_func)
		{		
			for (auto e : _entries)
			{
				for (auto& s : _storages.values)
				{
					if (s.contains(e))
					{
						info_func(e, component_info{ s._vtable.type_info, ((u8*)s._storage) + s._vtable.type_info.size * e });
					}
				}
			}
		}

		template<typename Fn>
		void query_info_with(entity_id e, Fn&& info_func)
		{
			for (auto& s : _storages.values)
			{
				if (s.contains(e))
				{
					info_func(component_info{ s._vtable.type_info, ((u8*)s._storage) + s._vtable.type_info.size * e });
				}
			}
		}


		template<typename Fn>
		void query(Fn&& func)
		{
			using q = query_traits<Fn>;

			if constexpr (q::has_required_components)
			{
				_query_required_impl<Fn,
					typename q::dirty_args,
					typename q::clear_args,
					typename q::clean_require_args
				>(
					std::make_index_sequence<std::tuple_size_v<typename q::dirty_args>>{},
					std::make_index_sequence<std::tuple_size_v<typename q::clean_require_args>>{},
					std::forward<Fn>(func)
				);
			}
			else
			{
				_query_no_required_impl<Fn,
					typename q::dirty_args,
					typename q::clear_args
				>(
					std::make_index_sequence<std::tuple_size_v<typename q::dirty_args>>{},
					std::forward<Fn>(func)
				);
			}
		}

		template<typename Fn>
		void query_with(entity_id id, Fn&& func)
		{
			kw_assert(id.is_valid());
			kw_assert(_entries.contains(id));

			using q = query_traits<Fn>;

			if constexpr (q::has_required_components)
			{
				_query_with_required_impl<Fn,
					typename q::dirty_args,
					typename q::clear_args,
					typename q::clean_require_args
				>(
					std::make_index_sequence<std::tuple_size_v<typename q::dirty_args>>{},
					std::make_index_sequence<std::tuple_size_v<typename q::clean_require_args>>{},
					id,
					std::forward<Fn>(func)
				);
			}
			else
			{
				_query_with_no_required_impl<Fn,
					typename q::dirty_args,
					typename q::clear_args
				>(
					std::make_index_sequence<std::tuple_size_v<typename q::dirty_args>>{},
					id,
					std::forward<Fn>(func)
				);
			}
		}

		template<typename Fn>
		void query_par(task_manager& tm, task_schedule_policy policy, usize work_groups, dyn_array<task_handle>& out_handles, Fn&& func)
		{
			using q = query_traits<Fn>;

			if constexpr (q::has_required_components)
			{
				_query_par_with_required_impl<Fn,
					typename q::dirty_args,
					typename q::clear_args,
					typename q::clean_require_args
				>(
					std::make_index_sequence<std::tuple_size_v<typename q::clear_args>>{},
					std::make_index_sequence<std::tuple_size_v<typename q::clean_require_args>>{},
					tm,
					policy,
					work_groups,
					out_handles,
					std::forward<Fn>(func)
				);
			}
			else
			{
				_query_par_with_no_required_impl<Fn,
					typename q::dirty_args,
					typename q::clear_args
				>(
					std::make_index_sequence<std::tuple_size_v<typename q::clear_args>>{},
					tm,
					policy,
					work_groups,
					out_handles,
					std::forward<Fn>(func)
				);
			}
		}

		struct _entity_id_getter 
		{
			inline entity_id get(usize i) const noexcept
			{
				return i;
			}
		};

		template<typename T>
		struct _required_getter
		{
			T* _data;

			inline T& get(usize i) const noexcept
			{
				return _data[i];
			}
		};

		template<typename T>
		struct _optional_getter
		{
			T* _data;       
			bool* _mask;    

			inline T* get(usize i) const noexcept
			{
				if (_mask[i]) 
				{
					return _data + i;
				}
				return nullptr;
			}
		};
	
		template<typename T>
		struct _make_query_param_getter
		{
			registry& r;

			auto operator()(usize worker_count = 1) const noexcept
			{
				using CVT = std::remove_reference_t<std::remove_pointer_t<T>>;
				using CleanT = std::remove_cv_t<CVT>;

				if constexpr (std::is_same_v<entity_id, T>)
				{
					return _entity_id_getter{};
				}
				else if constexpr (std::is_pointer_v<T>)
				{
					auto& s = r._lazy_get_storage<CleanT>();
					return _optional_getter<CVT>{ (CVT*)s._storage, s._mask};
				}
				else if constexpr (std::is_reference_v<T>)
				{
					auto& s = r._lazy_get_storage<CleanT>();
					return _required_getter<CVT>{ (CVT*)s._storage };
				}
			}
		};


		template<
			typename Fn,
			typename dirty_args_tuple,
			typename args_tuple,
			usize...args_idxs
		>
		void _query_with_no_required_impl(
			std::index_sequence<args_idxs...>,
			entity_id i,
			Fn&& func
		) {
			auto getters = std::make_tuple(
				_make_query_param_getter<std::tuple_element_t<args_idxs, dirty_args_tuple>>(*this)()...
			);

			std::forward<Fn>(func)(
				std::get<args_idxs>(getters).get(i)...
			);
		}

		template<
			typename Fn,
			typename dirty_args_tuple,
			typename args_tuple,
			typename require_tuple,
			usize...args_idxs,
			usize...require_idxs
		>
		void _query_with_required_impl(
			std::index_sequence<args_idxs...>,
			std::index_sequence<require_idxs...>,
			entity_id i,
			Fn&& func
		) {
			auto getters = std::make_tuple(
				_make_query_param_getter<std::tuple_element_t<args_idxs, dirty_args_tuple>>(
					*this
				)()...
			);
			
			array<bool*, sizeof...(require_idxs)> required_storage_masks = {
				_lazy_get_storage<std::tuple_element_t<require_idxs, require_tuple>>()._mask...
			};

			if ([&]<usize...I>(std::index_sequence<I...>) {
				return (... && required_storage_masks[I][i]);
			}(std::make_index_sequence<sizeof...(require_idxs) - 1>{}))
			{
				func(
					std::get<args_idxs>(getters).get(i)...
				);
			}
		}

		template<
			typename Fn,
			typename dirty_args_tuple,
			typename args_tuple,
			usize...args_idxs
		>
		void _query_no_required_impl(
			std::index_sequence<args_idxs...>,
			Fn&& func
		) {
			auto getters = std::make_tuple(
				_make_query_param_getter<std::tuple_element_t<args_idxs, dirty_args_tuple>>(*this)()...
			);

			for (auto i : _entries.as_base())
			{
				std::forward<Fn>(func)(
					std::get<args_idxs>(getters).get(i)...
				);
			}
		}

		template<
			typename Fn,
			typename dirty_args_tuple,
			typename args_tuple,
			typename require_tuple,
			usize...args_idxs,
			usize...require_idxs
		>
		void _query_required_impl(
			std::index_sequence<args_idxs...>,
			std::index_sequence<require_idxs...>,
			Fn&& func
		) {
			auto getters = std::make_tuple(
				_make_query_param_getter<std::tuple_element_t<args_idxs, dirty_args_tuple>>(
					*this
				)()...
			);

			array<component_storage*, sizeof...(require_idxs)> required_storages = {
				&_lazy_get_storage<std::tuple_element_t<require_idxs, require_tuple>>()... 
			};

			usize driver_index = 0;

			for (usize i = 0; i < required_storages.size(); i++)
			{
				if (required_storages[i]->_occupied < 
					required_storages[driver_index]->_occupied)
				{
					driver_index = i;
				}
			}

			component_storage& driver = *required_storages[driver_index];

			required_storages[driver_index] = required_storages[sizeof...(require_idxs) - 1];

			array<bool*, sizeof...(require_idxs) - 1> required_storage_masks;

			for (usize i = 0; i < sizeof...(require_idxs) - 1; i++)
			{
				required_storage_masks[i] = required_storages[i]->_mask;
			}

			for (auto i : driver)
			{
				if ([&]<usize...I>(std::index_sequence<I...>) { 
					return (... && required_storage_masks[I][i]);
				}(std::make_index_sequence<sizeof...(require_idxs) - 1>{}))
				{
					func(
						std::get<args_idxs>(getters).get(i)...
					);
				}
			}
		}

		template<
			typename Fn,
			typename dirty_args_tuple,
			typename args_tuple,
			typename require_tuple,
			usize...args_idxs,
			usize...require_idxs
		>
		void _query_par_with_required_impl(
			std::index_sequence<args_idxs...>,
			std::index_sequence<require_idxs...>,
			task_manager& mgr,
			task_schedule_policy policy,
			usize work_gouprs,
			dyn_array<task_handle>& out_handles,
			Fn&& func
		) {
			auto getters = std::make_tuple(
				_make_query_param_getter<std::tuple_element_t<args_idxs, dirty_args_tuple>>(
					*this
				)()...
			);
			array<component_storage*, sizeof...(require_idxs)> required_storages = { &_lazy_get_storage<std::tuple_element_t<require_idxs, require_tuple>>()... };

			usize driver_index = 0;

			for (usize i = 0; i < required_storages.size(); i++)
			{
				if (required_storages[i]->_occupied < required_storages[driver_index]->_occupied)
				{
					driver_index = i;
				}
			}

			component_storage* driver = required_storages[driver_index];

			required_storages[driver_index] = required_storages[sizeof...(require_idxs) - 1];

			array<bool*, sizeof...(require_idxs) - 1> required_storage_masks;

			for (usize i = 0; i < sizeof...(require_idxs) - 1; i++)
			{
				required_storage_masks[i] = required_storages[i]->_mask;
			}

			usize work_reminder = driver->_occupied % work_gouprs;
			usize work_per_group = driver->_occupied / work_gouprs;

			for (usize group_id = 0; group_id < work_gouprs; group_id++)
			{
				usize current_group_work = ((group_id == work_gouprs - 1) ? work_reminder + work_per_group : work_per_group);
				out_handles.emplace_back(
					mgr.schedule(
						[=]()
						{
							auto map = driver->_indirect_map;
							usize i;
							for (usize e = 0; e < current_group_work; e++)
							{
								i = map[(group_id * work_per_group) + e];

								if ([&]<usize...I>(std::index_sequence<I...>) 
								{ return (... && required_storage_masks[I][i]); }
								(std::make_index_sequence<sizeof...(require_idxs) - 1>{}))
								{
									func(
										std::get<args_idxs>(getters).get(i)...
									);
								}
							}
						}
						, policy
					)
				);
			}
		}

		template<
			typename Fn,
			typename dirty_args_tuple,
			typename args_tuple,
			usize...args_idxs
		>
		void _query_par_with_no_required_impl(
			std::index_sequence<args_idxs...>,
			task_manager& mgr,
			task_schedule_policy policy,
			usize work_gouprs,
			dyn_array<task_handle>& out_handles,
			Fn&& func
		){
			auto getters = std::make_tuple(
				_make_query_param_getter<std::tuple_element_t<args_idxs, dirty_args_tuple>>(
					*this
				)()...
			);
			usize work_reminder = _entries.as_base()._occupied % work_gouprs;
			usize work_per_group = _entries.as_base()._occupied / work_gouprs;

			dyn_array<defer_buffer> defer_buffers;
			defer_buffers.reserve(work_gouprs);

			for (usize group_id = 0; group_id < work_gouprs; group_id++)
			{
				out_handles.emplace_back(
					mgr.schedule(
						[=]()
						{
							auto map = _entries._indirect_map;

							for (usize e = 0; e < (group_id == work_gouprs - 1) ? work_reminder + work_per_group : work_per_group; e++)
							{
								auto i = map[(group_id + work_per_group) + e];

								func(
									std::get<args_idxs>(getters).get(i)...
								);
							}
						}
						, policy
					)
				);
			}		
		}
		
		template<typename Fn>
		void on_construct(Fn&& func)
		{
			static_assert(std::is_same_v<typename meta::function_traits<Fn>::template arg_at<0>, entity_id>, "lifetime hook callbacks require first parameter to be entity id");

			using T = std::remove_cvref_t<typename meta::function_traits<Fn>::template arg_at<1>>;

			_lazy_get_storage<T>().set_on_construct<Fn>(std::forward<Fn>(func));
		}

		template<typename Fn>
		void on_destruct(Fn&& func)
		{
			static_assert(std::is_same_v<typename meta::function_traits<Fn>::template arg_at<0>, entity_id>, "lifetime hook callbacks require first parameter to be entity id");

			using T = std::remove_cvref_t<typename meta::function_traits<Fn>::template arg_at<1>>;

			_lazy_get_storage<T>().set_on_destruct<Fn>(std::forward<Fn>(func));
		}

		template<typename T>
		T& add(entity_id index, T&& v)
		{
			return _lazy_get_storage<std::remove_cvref_t<T>>().emplace<std::remove_cvref_t<T>>(index, std::forward<T>(v));
		}

		template<typename T, typename...Args>
		T& emplace(entity_id index, Args&&...args)
		{
			return _lazy_get_storage<T>().emplace<T>(index, std::forward<Args>(args)...);
		}

		template<typename...Args>
		void erase(entity_id e)
		{
			((_lazy_get_storage<Args>().erase(e)), ...);
		}

		template<typename...Args>
		void copy(entity_id from, entity_id to)
		{
			((_lazy_get_storage<Args>().copy(from, to)), ...);
		}

		template<typename...Args>
		void move(entity_id from, entity_id to)
		{
			((_lazy_get_storage<Args>().move(from, to)), ...);
		}

		void clone(entity_id from, entity_id to)
		{
			for (auto& s : _storages.values	)
			{
				if (s.contains(from))
				{
					s.copy(from, to);
				}
			}
		}

		entity_id clone(entity_id from)
		{
			entity_id to = entity();

			clone(from, to);

			return to;
		}

		template<typename T>
		T& get(entity_id e)
		{
			return _lazy_get_storage<T>().get<T>(e);
		}

		//template<typename...Args>
		//tuple<Args&...> get(entity_id e)
		//{
		//	return std::forward_as_tuple(_lazy_get_storage<Args>().get<Args>(e)...);
		//}

		template<typename T>
		T* try_get(entity_id e)
		{
			return _lazy_get_storage<T>().try_get<T>(e);
		}

		//template<typename...Args>
		//tuple<Args*...> try_get(entity_id e)
		//{
		//	return std::forward_as_tuple(_lazy_get_storage<Args>().try_get<Args>(e)...);
		//}

		void destroy(entity_id id)
		{
			if (!_entries.contains(id)) return;

			for (auto& s : _storages.values)
			{
				s.erase(id);
			}
		}

		template<typename...Args>
		bool has(entity_id e) noexcept
		{
			return ((_lazy_get_storage<Args>().contains(e)) && ...);
		}

		defer_buffer defer(bool flush_on_dtor = true, bool fifo = true)
		{
			return { {*this, flush_on_dtor, fifo} };
		}

		template<typename T>
		component_storage&_lazy_get_storage() noexcept
		{
			constexpr auto hash = meta::type_hash<T>();

			if (auto v = _storages.try_get(hash))
			{
				return *v;
			}
			else
			{
				return _storages.insert(hash, meta::construct_tag<T>{}, _cfg.max_entity_count);
			}
		}		

		hash_map<component_storage> _storages;
		indirect_array<entity_id> _free_list;
		indirect_array<entity_id> _entries;
		entity_id _id_counter = 0;
		config _cfg;
	};
}

#endif // !KAWA_ECS
