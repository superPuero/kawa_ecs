#ifndef KAWA_ECS
#define KAWA_ECS

#ifndef KAWA_CORE
#define KAWA_CORE

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <format>
#include <chrono>

#if defined(_MSC_VER)
#define KAWA_ASSUME(x) __assume(x)
#elif defined(__clang__) || defined(__GNUC__)
#define KAWA_ASSUME(x) do { if (!(x)) __builtin_unreachable(); } while(0)
#else
#define KAWA_ASSUME(x) ((void)0)
#endif


#ifdef _MSC_VER
#include <intrin.h>
#define KAWA_DEBUG_BREAK() __debugbreak()
#elif __GNUC__ || __clang__
#define KAWA_DEBUG_BREAK() __builtin_trap()
#else
#define KAWA_DEBUG_BREAK() std::terminate() 
#endif

#define KAWA_VERIFY(expr) if(!(expr)) KAWA_DEBUG_BREAK();

#define KAWA_VERIFY_MSG(expr, ...) \
        do { \
            if (!(expr)) { \
                std::cout << std::format(__VA_ARGS__) << '\n'; \
                KAWA_DEBUG_BREAK(); \
            } \
        } while(0)

#ifdef _DEBUG	

#define KAWA_ASSERT(expr) if(!(expr)) KAWA_DEBUG_BREAK();

#define KAWA_ASSERT_MSG(expr, ...) \
        do { \
            if (!(expr)) { \
                std::cout << std::format(__VA_ARGS__) << '\n'; \
                KAWA_DEBUG_BREAK(); \
            } \
        } while(0)

#define KAWA_DEBUG_EXPAND(x) x

#else

#define KAWA_ASSERT(expr) ((void)0)
#define KAWA_ASSERT_MSG(expr, ...) ((void)0)
#define KAWA_DEBUG_EXPAND(x) ((void)0)

#endif

#endif
#ifndef KAWA_META
#define KAWA_META

#include <string>
#include <string_view>
#include <cstdint>
#include <utility>
#include <tuple>
#include <type_traits>

#if defined(__clang__)
#	define KAWA_META_PRETTYFUNC __PRETTY_FUNCTION__
#	define KAWA_META_TYPE_NAME_PREFIX "kawa::meta::type_name() [T = "
#	define KAWA_META_TYPE_NAME_POSTFIX "]"
#elif defined(_MSC_VER)
#	define KAWA_META_PRETTYFUNC __FUNCSIG__	
#	define KAWA_META_TYPE_NAME_PREFIX "kawa::meta::type_name<"
#	define KAWA_META_TYPE_NAME_POSTFIX ">(void) noexcept"
#elif defined(__GNUC__) 
#	define KAWA_META_PRETTYFUNC __PRETTY_FUNCTION__
#	define KAWA_META_TYPE_NAME_PREFIX "kawa::meta::type_name() [with T = "
#	define KAWA_META_TYPE_NAME_POSTFIX "; std::string_view = std::basic_string_view<char>]"
#else
#	error "Function signature macro not defined for this compiler."
#endif

namespace kawa
{
	namespace meta
	{
		namespace _internal
		{

			template<typename T>
			extern const T fake_object;

			inline constexpr std::string_view type_name_helper(std::string_view decorated_name)
			{
				size_t start = decorated_name.find(KAWA_META_TYPE_NAME_PREFIX);
				size_t end = decorated_name.find(KAWA_META_TYPE_NAME_POSTFIX);

#ifdef _MSC_VER	

				auto pre_name = decorated_name.substr(start + sizeof(KAWA_META_TYPE_NAME_PREFIX) - 1, end - (start + sizeof(KAWA_META_TYPE_NAME_PREFIX) - 1));
				size_t pre_name_space = pre_name.find_first_of(' ');

				if (pre_name_space != std::string_view::npos)
				{
					return pre_name.substr(pre_name_space + 1, pre_name.size() - pre_name_space);
				}
				return pre_name;
#else
				return decorated_name.substr(start + sizeof(KAWA_META_TYPE_NAME_PREFIX) - 1, end - (start + sizeof(KAWA_META_TYPE_NAME_PREFIX) - 1));
#endif 

			}

			constexpr uint64_t fnv1a_hash(std::string_view str) noexcept
			{
				constexpr uint64_t fnv_offset_basis = 14695981039346656037ull;
				constexpr uint64_t fnv_prime = 1099511628211ull;

				uint64_t hash = fnv_offset_basis;

				for (char c : str)
				{
					hash ^= static_cast<uint64_t>(c);
					hash *= fnv_prime;
				}

				return hash;
			}
		}

		template<typename T>
		inline constexpr std::string_view type_name() noexcept
		{
			return _internal::type_name_helper(KAWA_META_PRETTYFUNC);
		}

		constexpr uint64_t string_hash(std::string_view str) noexcept
		{
			return _internal::fnv1a_hash(str);
		}

		template<typename T>
		inline constexpr uint64_t type_hash() noexcept
		{
			return string_hash(type_name<T>());
		}

		struct empty_type_info_t {};

		struct type_info
		{
			constexpr type_info()
				: name(type_name<empty_type_info_t>())
				, hash(type_hash<empty_type_info_t>()) {}

			constexpr type_info(std::string_view n, uint64_t h) noexcept
				: name(n)
				, hash(h) {}

			template<typename T>
			static constexpr type_info create() noexcept
			{
				return type_info(type_name<T>(), type_hash<T>());
			}

			constexpr void make_empty() noexcept
			{
				name = type_name<empty_type_info_t>();
				hash = type_hash<empty_type_info_t>();
			}

			constexpr bool operator==(const type_info& other) noexcept
			{
				return (name == other.name && hash == other.hash);
			}

			std::string_view name;
			uint64_t hash;
		};

		template<typename...Types>
		constexpr size_t ptr_type_count_impl()
		{
			return (0 + ... + (std::is_pointer_v<Types> ? 1 : 0));
		}

		template<typename...Types>
		struct ptr_type_count
		{
			constexpr static size_t value = ptr_type_count_impl<Types...>();
		};

		template<typename...Types>
		struct ptr_type_count<std::tuple<Types...>>
		{
			constexpr static size_t value = ptr_type_count_impl<Types...>();
		};

		template<typename...Args>
		constexpr size_t lval_ref_type_count_impl()
		{
			return (0 + ... + (std::is_lvalue_reference_v<Args> ? 1 : 0));
		}

		template<typename...Args>
		struct lval_ref_type_count
		{
			constexpr static size_t value = lval_ref_type_count_impl<Args...>();
		};

		template<typename...Args>
		struct lval_ref_type_count<std::tuple<Args...>>
		{
			constexpr static size_t value = lval_ref_type_count_impl<Args...>();
		};

		template<typename RetTy, typename...ArgTy>
		struct function_traits {};

		template<typename RetTy, typename...ArgTy>
		struct function_traits<RetTy(*)(ArgTy...)>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<size_t i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

		template<typename RetTy, typename...ArgTy>
		struct function_traits<RetTy(&)(ArgTy...)>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<size_t i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

		template<typename RetTy, typename...ArgTy>
		struct function_traits<RetTy(ArgTy...)>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<size_t i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

		template<typename RetTy, typename ObjTy, typename...ArgTy>
		struct function_traits<RetTy(ObjTy::*)(ArgTy...) const>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<size_t i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

		template<typename T>
		struct function_traits<T> : function_traits<decltype(&T::operator())> {};

		template<size_t Start, size_t End>
		struct sub_tuple
		{
			static_assert(Start <= End, "Start index must be <= End");

			template<typename Tuple>
			using of = decltype([]<size_t... I>(std::index_sequence<I...>) -> std::tuple<std::tuple_element_t<Start + I, Tuple>...>
			{}(std::make_index_sequence<End - Start>{}));
		};

		template <typename T, template <typename> typename F>
		struct transform_each;

		template <typename...Types, template <typename> typename F>
		struct transform_each<std::tuple<Types...>, F>
		{
			using type = typename std::tuple<F<Types>...>;
		};

		template <typename T, template <typename...> typename F>
		using transform_each_t = typename transform_each<T, F>::type;

		template <typename T>
		struct remove_suffix_cv {
			using type = T;
		};

		template <typename T>
		struct remove_suffix_cv<T const&>
		{
			using type = T&;
		};

		template <typename T>
		struct remove_suffix_cv<T volatile&>
		{
			using type = T&;
		};

		template <typename T>
		struct remove_suffix_cv<T const volatile&>
		{
			using type = T&;
		};

		template <typename T>
		struct remove_suffix_cv<T const*>
		{
			using type = T*;
		};

		template <typename T>
		struct remove_suffix_cv<T volatile*>
		{
			using type = T*;
		};

		template <typename T>
		struct remove_suffix_cv<T const volatile*>
		{
			using type = T*;
		};

		template <typename T>
		using remove_suffix_cv_t = typename remove_suffix_cv<T>::type;
	}
}
#endif 
#ifndef KAWA_META_ECS_EXT
#define	KAWA_META_ECS_EXT

#include <type_traits>

namespace kawa
{
	namespace meta
	{
		template<typename Fn, typename...Params>
		struct query_traits
		{
			static constexpr size_t params_count = sizeof...(Params);

			using dirty_args_tuple = typename function_traits<Fn>::args_tuple;
			using args_tuple = transform_each_t<dirty_args_tuple, remove_suffix_cv_t>;

			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = sub_tuple<params_count, args_count>::template of<args_tuple>;
			static constexpr size_t no_params_args_count = std::tuple_size_v<no_params_args_tuple>;

			constexpr static size_t require_count = meta::lval_ref_type_count<no_params_args_tuple>::value;
			constexpr static size_t optional_count = meta::ptr_type_count<no_params_args_tuple>::value;
		};

		template<typename Fn, typename...Params>
		struct query_self_traits
		{
			static constexpr size_t params_count = sizeof...(Params) + 1;

			using dirty_args_tuple = typename function_traits<Fn>::args_tuple;
			using args_tuple = transform_each_t<dirty_args_tuple, remove_suffix_cv_t>;

			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = sub_tuple<params_count, args_count>::template of<args_tuple>;
			static constexpr size_t no_params_args_count = std::tuple_size_v<no_params_args_tuple>;

			constexpr static size_t require_count = meta::lval_ref_type_count<no_params_args_tuple>::value;
			constexpr static size_t optional_count = meta::ptr_type_count<no_params_args_tuple>::value;
		};
	};
}
#endif
#ifndef KAWA_ECS_ENTITY_MANAGER
#define KAWA_ECS_ENTITY_MANAGER

#include <limits>

namespace kawa
{
	namespace ecs
	{

		typedef size_t entity_id;

		constexpr inline entity_id nullent = std::numeric_limits<entity_id>::max();

		class entity_manager
		{
		public:
			entity_manager(size_t capacity)
			{
				_capacity = capacity;

				_free_list = new size_t[capacity]();
				_entity_mask = new bool[capacity]();
				_entity_entries = new size_t[capacity]();
				_r_entity_entries = new size_t[capacity]();
			};
			~entity_manager()
			{
				delete[] _free_list;
				delete[] _entity_mask;
				delete[] _entity_entries;
				delete[] _r_entity_entries;

				_free_list = nullptr;
				_entity_mask = nullptr;
				_entity_entries = nullptr;
				_r_entity_entries = nullptr;
			};

			inline entity_id get_new() noexcept
			{
				entity_id id;
				if (_free_list_size)
				{
					id = _free_list[--_free_list_size];
				}
				else
				{
					if (_occupied >= _capacity)
					{
						return nullent;
					}
					id = _occupied++;
				}

				_entity_mask[id] = true;

				_entity_entries[_entries_counter] = id;
				_r_entity_entries[id] = _entries_counter;

				_entries_counter++;

				return id;
			}

			inline bool alive(entity_id e)	noexcept
			{
				return _entity_mask[e];
			}

			inline void remove_unchecked(entity_id e) noexcept
			{
				_free_list[_free_list_size++] = e;
				_entity_mask[e] = false;

				size_t idx = _r_entity_entries[e];

				_entity_entries[idx] = _entity_entries[--_entries_counter];
				_r_entity_entries[_entity_entries[_entries_counter]] = idx;
			}


			inline void remove(entity_id e) noexcept
			{
				bool& entity_cell = _entity_mask[e];

				if (entity_cell)
				{
					_free_list[_free_list_size++] = e;
					entity_cell = false;

					size_t idx = _r_entity_entries[e];

					_entity_entries[idx] = _entity_entries[--_entries_counter];
					_r_entity_entries[_entity_entries[_entries_counter]] = idx;
				}
			}

			inline entity_id get_at_uncheked(size_t i) noexcept
			{
				return _entity_entries[i];
			}

			inline size_t* begin() noexcept
			{
				return _entity_entries;
			}

			inline entity_id* end() noexcept
			{
				return _entity_entries + _occupied;
			}

			inline entity_id occupied() noexcept
			{
				return _occupied;
			}

		private:
			bool* _entity_mask = nullptr;

			entity_id* _entity_entries = nullptr;
			size_t* _r_entity_entries = nullptr;
			size_t	_entries_counter = 0;

			size_t* _free_list = nullptr;
			size_t	_free_list_size = 0;

			size_t _occupied = 0;
			size_t _capacity = 0;

		};
	}
}

#endif 
#ifndef KAWA_POLY_STORAGE
#define	KAWA_POLY_STORAGE

#include <memory>
#include <new>
#include <cstring> 

#include <iostream>

namespace kawa
{

	struct empty_poly_storage_type_t {};

	class poly_storage
	{
		using delete_fn_t = void(*)(void*);
		using erase_fn_t = void(*)(void*, size_t);
		using copy_fn_t = void(*)(void*, size_t, size_t);
		using move_fn_t = void(*)(void*, size_t, size_t);

	public:
		inline poly_storage() noexcept = default;
		inline ~poly_storage() noexcept
		{
			if (_populated)
			{
				for (size_t i = 0; i < _occupied; i++)
				{
					size_t id = _connector[i];
					_erase_fn(_storage, id);
				}

				_delete_fn(_storage);

				delete[] _mask;
				delete[] _connector;
				delete[] _r_connector;

				_populated = false;
			}
		}

	public:
		template<typename T>
		inline poly_storage* populate(size_t capacity) noexcept
		{
			KAWA_ASSERT_MSG(!_populated, "poly_storage<{}>::populate<{}> on already populated storage", _type_info.name, meta::type_name<T>());

			_type_info = meta::type_info::create<T>();

			_capacity = capacity;

			if constexpr (!std::is_empty_v<T>)
			{
				_storage = ::operator new(sizeof(T) * capacity, std::align_val_t{ alignof(T) });
			}

			_delete_fn =
				[](void* data)
				{
					if constexpr (std::is_empty_v<T>)
					{
						return;
					}

					::operator delete(data, std::align_val_t{ alignof(T) });
				};

			_mask = new bool[capacity]();
			_connector = new size_t[capacity]();
			_r_connector = new size_t[capacity]();

			_erase_fn =
				[](void* data, size_t index)
				{
					if constexpr (std::is_empty_v<T>)
					{
						return;
					}

					(static_cast<T*>(data) + index)->~T();
				};

			_copy_fn = [](void* data, size_t from, size_t to)
				{
					if constexpr (std::is_empty_v<T>)
					{
						return;
					}

					if constexpr (std::is_copy_constructible_v<T>)
					{
						new (static_cast<T*>(data) + to) T(*(static_cast<T*>(data) + from));
					}
					else
					{
						KAWA_VERIFY_MSG(false, "trying to copy uncopyable type [ {} ]", meta::type_name<T>());
					}
				};

			_move_fn = [](void* data, size_t from, size_t to)
				{
					if constexpr (std::is_empty_v<T>)
					{
						return;
					}

					if constexpr (std::is_move_constructible_v<T>)
					{
						new (static_cast<T*>(data) + to) T(std::move(*(static_cast<T*>(data) + from)));
					}
					else
					{
						KAWA_VERIFY_MSG(false, "trying to move unmovable type [ {} ]", meta::type_name<T>());
					}
				};


			_populated = true;

			return this;
		}

	public:
		inline const meta::type_info& get_type_info() const noexcept
		{
			return _type_info;
		}

		inline bool has(size_t index) const noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::has on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::has out of bounds index [ {} ] access", _type_info.name, index);

			return _mask[index];
		}

		template<typename T, typename...Args>
		inline T& emplace(size_t index, Args&&...args) noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::emplace on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::emplace out of bounds index [ {} ] access", _type_info.name, index);
			KAWA_ASSERT_MSG(_validate_type<T>(), "poly_storage<{}>::emplace invalid type [ {} ] access", _type_info.name, meta::type_name<T>());

			bool& cell = _mask[index];
			if (!cell)
			{
				size_t l_idx = _occupied++;

				_connector[l_idx] = index;
				_r_connector[index] = l_idx;

				cell = true;

				return *(new (static_cast<T*>(_storage) + index) T(std::forward<Args>(args)...));
			}
			else
			{
				_erase_fn(_storage, index);

				return *(new (static_cast<T*>(_storage) + index) T(std::forward<Args>(args)...));
			}

		}

		template<typename T>
		inline T& get(size_t index) const  noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::get on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::get out of bounds index [ {} ] access", _type_info.name, index);
			KAWA_ASSERT_MSG(_validate_type<T>(), "poly_storage<{}>::get invalid type [ {} ] access", _type_info.name, meta::type_name<T>());
			KAWA_ASSERT_MSG(_mask[index], "poly_storage<{}>::get of uninitialized [ {} ]", _type_info.name, index);

			return *(static_cast<T*>(_storage) + index);
		}

		template<typename T>
		inline T* get_if_has(size_t index) const noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::get_if_has on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::get_if_has out of bounds index [ {} ] access", _type_info.name, index);
			KAWA_ASSERT_MSG(_validate_type<T>(), "poly_storage<{}>::get_if_has invalid type [ {} ] access", _type_info.name, meta::type_name<T>());

			if (!_mask[index])
			{
				return nullptr;
			}

			return static_cast<T*>(_storage) + index;
		}

		template<typename T>
		inline std::enable_if_t<std::is_pointer_v<T>, T> get_defer(size_t index) const noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::get_if_has on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::get_if_has out of bounds index [ {} ] access", _type_info.name, index);
			KAWA_ASSERT_MSG(_validate_type<std::remove_pointer_t<T>>(), "poly_storage<{}>::get_if_has invalid type [ {} ] access", _type_info.name, meta::type_name< std::remove_pointer_t<T>>());

			if (!_mask[index])
			{
				return nullptr;
			}

			return static_cast<T>(_storage) + index;
		}

		template<typename T>
		inline std::enable_if_t<!std::is_pointer_v<T>, T&> get_defer(size_t index) const noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::get on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::get out of bounds index [ {} ] access", _type_info.name, index);
			KAWA_ASSERT_MSG(_validate_type<std::remove_reference_t<T>>(), "poly_storage<{}>::get invalid type [ {} ] access", _type_info.name, meta::type_name<std::remove_reference_t<T>>());
			KAWA_ASSERT_MSG(_mask[index], "poly_storage<{}>::get of uninitialized [ {} ]", _type_info.name, index);

			return *(static_cast<std::remove_reference_t<T>*>(_storage) + index);
		}

		inline void erase(size_t index) noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::erase on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::erase out of bounds index [ {} ] access", _type_info.name, index);

			bool& cell = _mask[index];
			if (cell)
			{
				size_t l_idx = _r_connector[index];

				_erase_fn(_storage, index);

				_connector[l_idx] = _connector[--_occupied];
				_r_connector[_connector[_occupied]] = l_idx;

				cell = false;
			}
		}

		inline void copy(size_t from, size_t to) noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::copy on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(from), "poly_storage<{}>::copy out of bounds index [ {} ] access", _type_info.name, from);
			KAWA_ASSERT_MSG(_validate_index(to), "poly_storage<{}>::copy out of bounds index [ {} ] access", _type_info.name, to);

			if (has(from))
			{
				bool& cell = _mask[to];
				if (!cell)
				{
					size_t l_idx = _occupied++;

					_connector[l_idx] = to;
					_r_connector[to] = l_idx;

					cell = true;

					_copy_fn(_storage, from, to);
				}
				else
				{
					_erase_fn(_storage, to);
					_copy_fn(_storage, from, to);
				}
			}
		}

		inline void move(size_t from, size_t to) noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::move on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(from), "poly_storage<{}>::move out of bounds index [ {} ] access", _type_info.name, from);
			KAWA_ASSERT_MSG(_validate_index(to), "poly_storage<{}>::move out of bounds index [ {} ] access", _type_info.name, to);

			if (has(from))
			{
				bool& cell = _mask[to];
				if (!cell)
				{
					size_t l_idx = _occupied++;

					_connector[l_idx] = to;
					_r_connector[to] = l_idx;

					cell = true;

					_move_fn(_storage, from, to);
				}
				else
				{
					_erase_fn(_storage, to);
					_move_fn(_storage, from, to);
				}

				erase(from);
			}
		}

		inline bool is_populated() const noexcept
		{
			return _populated;
		}

		inline size_t* begin() noexcept
		{
			return _connector;
		}

		inline size_t* end() noexcept
		{
			return _connector + _occupied;
		}

		inline size_t get_at(size_t i) noexcept
		{
			return _connector[i];
		}

		inline size_t occupied() noexcept
		{
			return _occupied;
		}

		inline size_t capcatiy() noexcept
		{
			return _capacity;
		}

	private:
		inline bool _validate_index(size_t id) const noexcept
		{
			if (id >= _capacity)
			{
				return false;
			}

			return true;
		}

		template<typename T>
		inline bool _validate_type() const noexcept
		{
			if (_type_info.hash != meta::type_hash<T>())
			{
				return false;
			}
			return true;
		}

		void* _storage = nullptr;
		bool* _mask = nullptr;
		size_t* _connector = nullptr;
		size_t* _r_connector = nullptr;
		size_t			_occupied = 0;

		erase_fn_t		_erase_fn = nullptr;
		delete_fn_t		_delete_fn = nullptr;
		copy_fn_t		_copy_fn = nullptr;
		move_fn_t		_move_fn = nullptr;
		size_t			_capacity = 0;

		bool			_populated = false;

		meta::type_info	_type_info;
	};
}
#endif
#ifndef KAWA_ECS_QUERY_PAR_ENGINE
#define	KAWA_ECS_QUERY_PAR_ENGINE

#include <thread>
#include <barrier>
#include <new>
#include <functional>

namespace kawa
{
	namespace ecs
	{
		namespace _internal
		{
			class query_par_engine
			{
			public:
				query_par_engine(size_t thread_count)
					: _barrier(thread_count + 1)
					, _thread_count(thread_count)
					, _tasks_count(thread_count + 1)
				{
					_starts = new size_t[_tasks_count]();
					_ends = new size_t[_tasks_count]();

					if (_thread_count)
					{
						_threads = (std::thread*)::operator new(sizeof(std::thread) * _thread_count, std::align_val_t{ alignof(std::thread) });

						for (size_t i = 0; i < _thread_count; i++)
						{
							new (_threads + i) std::thread
							(
								[this, i]()
								{
									while (true)
									{
										_barrier.arrive_and_wait();

										if (_should_join)
										{
											_barrier.arrive_and_wait();
											return;
										}

										_query(_starts[i], _ends[i]);

										_barrier.arrive_and_wait();
									}
								}
							);
						}
					}
				}
				~query_par_engine()
				{
					_should_join = true;

					_barrier.arrive_and_wait();

					_barrier.arrive_and_wait();

					for (size_t i = 0; i < _thread_count; i++)
					{
						_threads[i].join();
					}

					if (_thread_count)
					{
						::operator delete(_threads, std::align_val_t{ alignof(std::thread) });
					}

					delete[] _ends;
					delete[] _starts;
				}

			public:
				template<typename Fn>
				void query(Fn&& query, size_t work)
				{
					_query = std::forward<Fn>(query);

					size_t chunk_work = work / _tasks_count;

					size_t tail_chunk_work = work - chunk_work * _tasks_count;

					for (size_t i = 0; i < _tasks_count; ++i)
					{
						size_t start = i * chunk_work;
						size_t end = (i == (_tasks_count - 1)) ? start + chunk_work + tail_chunk_work : start + chunk_work;

						if (start >= end) continue;

						_starts[i] = start;
						_ends[i] = end;
					}

					_barrier.arrive_and_wait();

					_query(_starts[_tasks_count - 1], _ends[_tasks_count - 1]);

					_barrier.arrive_and_wait();
				}

			private:
				std::thread* _threads = nullptr;
				const size_t							_thread_count = 0;
				const size_t							_tasks_count = 0;
				bool									_should_join = false;

				size_t* _starts = nullptr;
				size_t* _ends = nullptr;

				std::function<void(size_t, size_t)>		_query = nullptr;
				std::barrier<>							_barrier;

			};

		}
	}
}
#endif
#ifndef KAWA_ECS_REGISTRY
#define KAWA_ECS_REGISTRY

#include <array>

namespace kawa
{
	namespace ecs
	{
		typedef size_t entity_id;
		typedef size_t storage_id;

		typedef meta::type_info component_info;

		struct registry_specification
		{
			size_t max_entity_count = 256;
			size_t max_component_types = 256;
			size_t thread_count = (std::thread::hardware_concurrency() / 2);
			std::string debug_name = "unnamed";
		};

		class registry
		{
		public:
			inline registry(const registry_specification& reg_spec) noexcept
				: _query_par_engine(reg_spec.thread_count)
				, _entity_manager(reg_spec.max_entity_count)
			{
				_spec = reg_spec;

				_storage = new poly_storage[_spec.max_component_types];
				_storage_mask = new bool[_spec.max_component_types]();
				_fetch_destroy_list = new entity_id[_spec.max_entity_count];
			}

			inline ~registry() noexcept
			{
				delete[] _storage;
				delete[] _storage_mask;
				delete[] _fetch_destroy_list;
			}

		public:
			inline const registry_specification& get_specs() const noexcept
			{
				return _spec;
			}

			inline entity_id entity() noexcept
			{
				return _entity_manager.get_new();
			}

			template<typename...Args>
			inline entity_id entity_with(Args&&...args) noexcept
			{
				entity_id id = entity();

				(emplace<Args>(id, std::forward<Args>(args)), ...);

				return id;
			}

			template<typename T, typename...Args>
			inline T& emplace(entity_id entity, Args&&...args) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				static_assert(!std::is_const_v<T>, "component can not have const qualifier");

				storage_id s_id = get_id<T>();

				bool& storage_cell = _storage_mask[s_id];

				poly_storage& storage = _storage[s_id];

				if (!storage_cell)
				{
					storage.template populate<T>(_spec.max_entity_count);
					storage_cell = true;
				}

				return storage.emplace<T>(entity, std::forward<Args>(args)...);
			}

			template<typename...Args>
			inline void erase(entity_id entity) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				if (_entity_manager.alive(entity))
				{
					(([this, entity]<typename T>()
					{
						storage_id s_id = get_id<T>();
						poly_storage* storage = (_storage + s_id);

						if (_storage_mask[s_id])
						{
							storage->erase(entity);
						}
					}.template operator() < Args > ()), ...);
				}
			}

			template<typename...Args>
			inline bool has(entity_id entity) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				if (_entity_manager.alive(entity))
				{
					return (([this, entity] <typename T>()
					{
						storage_id s_id = get_id<T>();

						if (_storage_mask[s_id])
						{
							return _storage[s_id].has(entity);
						}

						return false;
					}. template operator() < Args > ()) && ...);
				}

				return false;
			}

			template<typename T>
			inline T& get(entity_id entity) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				storage_id s_id = get_id<T>();

				return _storage[s_id].get<T>(entity);
			}

			template<typename T>
			inline T* get_if_has(entity_id entity) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				if (_entity_manager.alive(entity))
				{
					storage_id s_id = get_id<T>();

					if (_storage_mask[s_id])
					{
						return _storage[s_id].get_if_has<T>(entity);
					}

					return nullptr;
				}

				return nullptr;
			}

			template<typename...Args>
			inline void copy(entity_id from, entity_id to) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(from));
				KAWA_DEBUG_EXPAND(_validate_entity(to));

				if (from != to)
				{
					(([this, from, to]<typename T>()
					{
						storage_id s_id = get_id<T>();
						_storage[s_id].copy(from, to);

					}. template operator() < Args > ()), ...);
				}
			}

			template<typename...Args>
			inline void move(entity_id from, entity_id to) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(from));
				KAWA_DEBUG_EXPAND(_validate_entity(to));

				if (from != to)
				{
					(([this, from, to]<typename T>()
					{
						storage_id s_id = get_id<T>();

						_storage[s_id].move(from, to);

					}. template operator() < Args > ()), ...);
				}
			}

			inline entity_id clone(entity_id from) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(from));

				entity_id e = entity();

				if (!e) return e;

				for (storage_id s_id = 0; s_id < _spec.max_component_types; s_id++)
				{
					if (_storage_mask[s_id])
					{
						_storage[s_id].copy(from, e);
					}
				}

				return e;
			}

			inline void clone(entity_id from, entity_id to) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(from));
				KAWA_DEBUG_EXPAND(_validate_entity(to));

				for (storage_id s_id = 0; s_id < _spec.max_component_types; s_id++)
				{
					if (_storage_mask[s_id])
					{
						_storage[s_id].copy(from, to);
					}
				}
			}

			inline void destroy(entity_id entity) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				if (_entity_manager.alive(entity))
				{
					for (size_t s_id = 0; s_id < _spec.max_component_types; s_id++)
					{
						if (_storage_mask[s_id])
						{
							_storage[s_id].erase(entity);
						}
					}

					_entity_manager.remove_unchecked(entity);
				}
			}

			inline bool is_valid(entity_id e) noexcept
			{
				if (e == nullent || e >= _spec.max_entity_count)
				{
					return false;
				}

				return true;
			}

			inline void fetch_destroy(entity_id entity) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				if (_entity_manager.alive(entity))
				{
					_fetch_destroy_list[_fetch_destroy_list_size++] = entity;
				}
			}
			template<typename Fn, typename...Params>
			inline void query_with_info(entity_id entity, Fn&& fn, Params&&...params) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				if (_entity_manager.alive(entity))
				{
					for (storage_id s = 0; s < _storage_id_counter; s++)
					{
						if (_storage_mask[s])
						{
							auto& storage = _storage[s];
							if (storage.has(entity))
							{
								fn(std::forward<Params>(params)..., storage.get_type_info());
							}
						}
					}
				}
			}

			template<typename Fn, typename...Params>
			inline void query_self_info(Fn&& fn, Params&&...params) noexcept
			{
				for (entity_id entity : _entity_manager)
				{
					for (storage_id s = 0; s < _storage_id_counter; s++)
					{
						if (_storage_mask[s])
						{
							auto& storage = _storage[s];
							if (storage.has(entity))
							{
								fn(entity, std::forward<Params>(params)..., storage.get_type_info());
							}
						}
					}
				}
			}

			template<typename Fn, typename...Params>
			inline void query(Fn&& fn, Params&&...params) noexcept
			{
				using query_traits = typename meta::query_traits<Fn, Params...>;

				if constexpr (query_traits::args_count == query_traits::params_count)
				{
					for (entity_id i : _entity_manager)
					{
						fn(std::forward<Params>(params)...);
					}
				}
				else
				{
					_query_impl<Fn, typename query_traits::no_params_args_tuple>
						(
							std::forward<Fn>(fn),
							std::make_index_sequence<query_traits::no_params_args_count>{},
							std::make_index_sequence<query_traits::require_count>{},
							std::make_index_sequence<query_traits::optional_count>{},
							std::forward<Params>(params)...
						);
				}
				_fetch_destroy_exec();
			}

			template<typename Fn, typename...Params>
			inline void query_par(Fn&& fn, Params&&...params) noexcept
			{
				KAWA_ASSERT_MSG(!_query_par_running, "[ {} ]: trying to invoke kawa::ecs::query_par inside another parallel query body", _spec.debug_name);

				_query_par_running = true;

				using query_traits = typename meta::query_traits<Fn, Params...>;

				if constexpr (query_traits::args_count == query_traits::params_count)
				{
					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							for (size_t i = start; i < end; i++)
							{
								fn(std::forward<Params>(params)...);
							}
						}
						, _entity_manager.occupied()
					);
				}
				else
				{
					_query_par_impl<Fn, typename query_traits::no_params_args_tuple>
						(
							std::forward<Fn>(fn),
							std::make_index_sequence<query_traits::no_params_args_count>{},
							std::make_index_sequence<query_traits::require_count>{},
							std::make_index_sequence<query_traits::optional_count>{},
							std::forward<Params>(params)...
						);
				}

				_query_par_running = false;

				_fetch_destroy_exec();
			}

			template<typename Fn, typename...Params>
			inline void query_self(Fn&& fn, Params&&...params) noexcept
			{
				using query_self_traits = typename meta::query_self_traits<Fn, Params...>;

				static_assert(std::is_same_v<std::tuple_element_t<0, typename query_self_traits::args_tuple>, kawa::ecs::entity_id>, "query self fucntion must take kawa::ecs::entity_id as a first parameter");

				if constexpr (query_self_traits::args_count == query_self_traits::params_count)
				{
					for (entity_id i : _entity_manager)
					{
						fn(i, std::forward<Params>(params)...);
					}
				}
				else
				{
					_query_self_impl<Fn, typename query_self_traits::no_params_args_tuple>
						(
							std::forward<Fn>(fn),
							std::make_index_sequence<query_self_traits::no_params_args_count>{},
							std::make_index_sequence<query_self_traits::require_count>{},
							std::make_index_sequence<query_self_traits::optional_count>{},
							std::forward<Params>(params)...
						);
				}

				_fetch_destroy_exec();
			}

			template<typename Fn, typename...Params>
			inline void query_self_par(Fn&& fn, Params&&...params) noexcept
			{
				KAWA_ASSERT_MSG(!_query_par_running, "[ {} ]: trying to invoke kawa::ecs::query_self_par inside another parallel query body", _spec.debug_name);
				_query_par_running = true;

				using query_self_traits = typename meta::query_self_traits<Fn, Params...>;
				static_assert(std::is_same_v<std::tuple_element_t<0, typename query_self_traits::args_tuple>, kawa::ecs::entity_id>, "query self fucntion must take kawa::ecs::entity_id as a first parameter");

				if constexpr (query_self_traits::args_count == query_self_traits::params_count)
				{
					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							for (size_t i = start; i < end; i++)
							{
								fn(_entity_manager.get_at_uncheked(i), std::forward<Params>(params)...);
							}
						}
						, _entity_manager.occupied()
					);
				}
				else
				{
					_query_self_par_impl<Fn, typename query_self_traits::no_params_args_tuple>
						(
							std::forward<Fn>(fn),
							std::make_index_sequence<query_self_traits::no_params_args_count>{},
							std::make_index_sequence<query_self_traits::require_count>{},
							std::make_index_sequence<query_self_traits::optional_count>{},
							std::forward<Params>(params)...
						);
				}

				_query_par_running = false;

				_fetch_destroy_exec();
			}

			template<typename Fn, typename...Params>
			inline void query_with(entity_id entity, Fn fn, Params&&...params) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				using query_traits = typename meta::query_traits<Fn, Params...>;

				if (_entity_manager.alive(entity))
				{
					if constexpr (query_traits::args_count == query_traits::params_count)
					{
						fn(std::forward<Params>(params)...);
					}
					else
					{
						_query_with_impl<Fn, typename query_traits::no_params_args_tuple>
							(
								entity,
								std::forward<Fn>(fn),
								std::make_index_sequence<query_traits::no_params_args_count>{},
								std::make_index_sequence<query_traits::require_count>{},
								std::make_index_sequence<query_traits::optional_count>{},
								std::forward<Params>(params)...
							);
					}
				}

				_fetch_destroy_exec();
			}

		private:
			inline void _fetch_destroy_exec() noexcept
			{
				for (size_t i = 0; i < _fetch_destroy_list_size; i++)
				{
					entity_id e = _fetch_destroy_list[i];

					for (size_t s_id = 0; s_id < _storage_id_counter; s_id++)
					{
						if (_storage_mask[s_id])
						{
							_storage[s_id].erase(e);
						}
					}

					_entity_manager.remove_unchecked(e);
				}

				_fetch_destroy_list_size = 0;
			}

			template<typename Fn, typename args_tuple, size_t...args_idxs, size_t...req_idxs, size_t...opt_idxs, typename...Params>
			inline void _query_impl(Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t args_count = sizeof...(args_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);
				constexpr size_t req_count = sizeof...(req_idxs);

				auto args_storages = std::forward_as_tuple(_storage[get_id<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()]...);

				std::array<poly_storage*, opt_count> optional_storages;
				_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

				if constexpr (req_count)
				{
					std::array<poly_storage*, req_count> require_storages = {};
					_populate_required_array<args_tuple, args_idxs...>(require_storages);

					poly_storage* smallest = require_storages[0];

					for (poly_storage* st : require_storages)
					{
						if (st->occupied() < smallest->occupied())
						{
							smallest = st;
						}
					}

					std::apply
					(
						[&](auto&&...storages)
						{
							for (entity_id e : *smallest)
							{
								if ((require_storages[req_idxs]->has(e) && ...))
								{
									fn
									(
										std::forward<Params>(params)...,
										storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
									);
								}
							}
						},
						args_storages
					);
				}
				else
				{
					std::apply
					(
						[&](auto&&...storages)
						{
							for (entity_id e : _entity_manager)
							{
								fn
								(
									std::forward<Params>(params)...,
									storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
								);
							}
						},
						args_storages
					);
				}
			}

			template<typename Fn, typename args_tuple, size_t...args_idxs, size_t...req_idxs, size_t...opt_idxs, typename...Params>
			inline void _query_par_impl(Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t args_count = sizeof...(args_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);
				constexpr size_t req_count = sizeof...(req_idxs);

				auto args_storages = std::forward_as_tuple(_storage[get_id<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()]...);

				std::array<poly_storage*, opt_count> optional_storages;
				_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

				if constexpr (req_count)
				{
					std::array<poly_storage*, req_count> require_storages = {};
					_populate_required_array<args_tuple, args_idxs...>(require_storages);

					poly_storage* smallest = require_storages[0];

					for (poly_storage* st : require_storages)
					{
						if (st->occupied() < smallest->occupied())
						{
							smallest = st;
						}
					}

					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							std::apply
							(
								[&](auto&&...storages)
								{
									for (size_t i = start; i < end; ++i)
									{
										entity_id e = smallest->get_at(i);
										if ((require_storages[req_idxs]->has(e) && ...))
										{
											fn
											(
												std::forward<Params>(params)...,
												storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
											);
										}
									}
								},
								args_storages
							);
						}
						, smallest->occupied()
					);
				}
				else
				{
					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							std::apply
							(
								[&](auto&&...storages)
								{
									for (size_t i = start; i < end; ++i)
									{
										entity_id e = _entity_manager.get_at_uncheked(i);
										fn
										(
											std::forward<Params>(params)...,
											storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
										);
									}
								},
								args_storages
							);
						}
						, _entity_manager.occupied()
					);
				}
			}

			template<typename Fn, typename args_tuple, size_t...args_idxs, size_t...req_idxs, size_t...opt_idxs, typename...Params>
			inline void _query_self_impl(Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t args_count = sizeof...(args_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);
				constexpr size_t req_count = sizeof...(req_idxs);

				auto args_storages = std::forward_as_tuple(_storage[get_id<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()]...);

				std::array<poly_storage*, opt_count> optional_storages;
				_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

				if constexpr (req_count)
				{
					std::array<poly_storage*, req_count> require_storages = {};
					_populate_required_array<args_tuple, args_idxs...>(require_storages);

					poly_storage* smallest = require_storages[0];

					for (poly_storage* st : require_storages)
					{
						if (st->occupied() < smallest->occupied())
						{
							smallest = st;
						}
					}

					std::apply
					(
						[&](auto&&...storages)
						{
							for (entity_id e : *smallest)
							{
								if ((require_storages[req_idxs]->has(e) && ...))
								{
									fn
									(
										e,
										std::forward<Params>(params)...,
										storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
									);
								}
							}
						},
						args_storages
					);
				}
				else
				{
					std::apply
					(
						[&](auto&&...storages)
						{
							for (entity_id e : _entity_manager)
							{
								fn
								(
									e,
									std::forward<Params>(params)...,
									storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
								);
							}
						},
						args_storages
					);
				}
			}

			template<typename Fn, typename args_tuple, size_t...args_idxs, size_t...req_idxs, size_t...opt_idxs, typename...Params>
			inline void _query_self_par_impl(Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t args_count = sizeof...(args_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);
				constexpr size_t req_count = sizeof...(req_idxs);

				auto args_storages = std::forward_as_tuple(_storage[get_id<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()]...);

				std::array<poly_storage*, opt_count> optional_storages;
				_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

				if constexpr (req_count)
				{
					std::array<poly_storage*, req_count> require_storages = {};
					_populate_required_array<args_tuple, args_idxs...>(require_storages);

					poly_storage* smallest = require_storages[0];

					for (poly_storage* st : require_storages)
					{
						if (st->occupied() < smallest->occupied())
						{
							smallest = st;
						}
					}

					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							std::apply
							(
								[&](auto&&...storages)
								{
									for (size_t i = start; i < end; ++i)
									{
										entity_id e = smallest->get_at(i);
										if ((require_storages[req_idxs]->has(e) && ...))
										{
											fn
											(
												e,
												std::forward<Params>(params)...,
												storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
											);
										}
									}
								},
								args_storages
							);
						}
						, smallest->occupied()
					);
				}
				else
				{
					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							std::apply
							(
								[&](auto&&...storages)
								{
									for (size_t i = start; i < end; ++i)
									{
										entity_id e = _entity_manager.get_at_uncheked(i);
										fn
										(
											e,
											std::forward<Params>(params)...,
											storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
										);
									}
								},
								args_storages
							);
						}
						, _entity_manager.occupied()
					);
				}
			}


			template<typename Fn, typename args_tuple, size_t...args_idxs, size_t...req_idxs, size_t...opt_idxs, typename...Params>
			inline void _query_with_impl(entity_id entity, Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t args_count = sizeof...(args_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);
				constexpr size_t req_count = sizeof...(req_idxs);

				auto args_storages = std::forward_as_tuple(_storage[get_id<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()]...);

				std::array<poly_storage*, opt_count> optional_storages;
				_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

				if constexpr (req_count)
				{
					std::array<poly_storage*, req_count> require_storages = {};
					_populate_required_array<args_tuple, args_idxs...>(require_storages);

					std::apply
					(
						[&](auto&&...storages)
						{
							if ((require_storages[req_idxs]->has(entity) && ...))
							{
								fn
								(
									std::forward<Params>(params)...,
									storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(entity)...
								);
							}
						},
						args_storages
					);
				}
				else
				{
					std::apply
					(
						[&](auto&&...storages)
						{
							fn
							(
								std::forward<Params>(params)...,
								storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(entity)...
							);
						},
						args_storages
					);
				}
			}

		private:
			template<typename args_tuple, size_t...I, size_t N>
			constexpr void _populate_optional_array(std::array<poly_storage*, N>& out) noexcept
			{
				size_t id = 0;

				(([&]<size_t i>()
				{
					using T = std::tuple_element_t<I, args_tuple>;

					if constexpr (std::is_pointer_v<T>)
					{
						using CleanT = std::remove_pointer_t<T>;

						auto key = get_id<CleanT>();
						bool& cell = _storage_mask[key];
						if (cell)
						{
							out[id] = _storage + key;

							id++;
						}
						else
						{
							cell = true;
							out[id] = _storage[key].template populate<CleanT>(_spec.max_entity_count);
							id++;
						}
					}
				}.template operator() < I > (), void(), 0), ...);
			}

			template<typename args_tuple, size_t...I, size_t N>
			constexpr void _populate_required_array(std::array<poly_storage*, N>& out) noexcept
			{
				size_t id = 0;

				(([&]<size_t i>()
				{
					using T = std::tuple_element_t<I, args_tuple>;

					if constexpr (std::is_reference_v<T>)
					{
						using CleanT = std::remove_reference_t<T>;

						auto key = get_id<CleanT>();
						bool& cell = _storage_mask[key];
						if (cell)
						{
							out[id] = _storage + key;

							id++;
						}
						else
						{
							cell = true;
							out[id] = _storage[key].template populate<CleanT>(_spec.max_entity_count);
							id++;
						}
					}
				}.template operator() < I > (), void(), 0), ...);
			}

		private:
			template<typename T>
			inline storage_id get_id() noexcept
			{
				static storage_id id = _storage_id_counter++;
				KAWA_DEBUG_EXPAND(_validate_storage(id));
				return id;
			}

			inline void _validate_entity(entity_id id) const noexcept
			{
				KAWA_ASSERT_MSG(id != nullent, "[ {} ]: nullent usage", _spec.debug_name);
				KAWA_ASSERT_MSG(id < _spec.max_entity_count, "[ {} ]: invalid entity_id [ {} ] usage", _spec.debug_name, id);
			}

			inline void _validate_storage(storage_id id) const noexcept
			{
				KAWA_ASSERT_MSG(id < _spec.max_component_types, "[ {} ]: max amoount of unique component types reached [ {} ], increase max_component_types", _spec.debug_name, _spec.max_component_types);
			}

		private:
			registry_specification		_spec;
			poly_storage* _storage = nullptr;
			bool* _storage_mask = nullptr;

			storage_id* _registered_ids = nullptr;
			const char** _registered_names = nullptr;

			entity_manager				_entity_manager;

			entity_id* _fetch_destroy_list = nullptr;
			size_t						_fetch_destroy_list_size = 0;

			_internal::query_par_engine	_query_par_engine;
			bool						_query_par_running = false;

		private:
			static inline storage_id	_storage_id_counter = 0;


		};
	}
}

#endif 

#endif