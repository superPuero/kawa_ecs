#ifndef KAWA_ECS
#define KAWA_ECS

#ifndef KAWA_ECS_REGISTRY
#define KAWA_ECS_REGISTRY

#include <array>
#include <concepts>

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
		namespace _
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
			return _::type_name_helper(KAWA_META_PRETTYFUNC);
		}

		constexpr uint64_t string_hash(std::string_view str) noexcept
		{
			return _::fnv1a_hash(str);
		}

		template<typename T>
		inline constexpr uint64_t type_hash() noexcept
		{
			return string_hash(type_name<T>());
		}

		struct empty_type_info_t {};

		struct type_info
		{
			constexpr inline type_info() noexcept
				: name(type_name<empty_type_info_t>())
				, hash(type_hash<empty_type_info_t>())
				, size(0)
				, alignment(std::align_val_t{ 0 }) {}

			constexpr inline type_info(const type_info& other) noexcept
				: name(other.name)
				, hash(other.hash)
				, size(other.size)
				, alignment(other.alignment) {}

			constexpr inline type_info(std::string_view n, uint64_t h, size_t s, std::align_val_t align) noexcept
				: name(n)
				, hash(h)
				, size(s)
				, alignment(align) {}

			template<typename T>
			[[nodiscard]] static constexpr inline type_info create() noexcept
			{
				return type_info(type_name<T>(), type_hash<T>(), sizeof(T), std::align_val_t{ alignof(T) });
			}

			constexpr inline void make_empty() noexcept
			{
				name = type_name<empty_type_info_t>();
				hash = type_hash<empty_type_info_t>();
				size = 0;
				alignment = std::align_val_t{ 0 };
			}

			constexpr bool operator==(const type_info& other) noexcept
			{
				return (name == other.name && hash == other.hash);
			}

			std::string_view name;
			uint64_t hash;
			size_t size;
			std::align_val_t alignment;
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

		template<typename Tuple, size_t start, size_t end, typename = void>
		struct sub_tuple
		{
			using tuple = decltype([]<size_t... I>(std::index_sequence<I...>) -> std::tuple<std::tuple_element_t<start + I, Tuple>...>
			{}(std::make_index_sequence<end - start>{}));
		};

		template<typename Tuple, size_t start, size_t end>
		struct sub_tuple<Tuple, start, end, std::enable_if_t<((end - start) > std::tuple_size_v<Tuple>)>>
		{
			using tuple = typename std::tuple<>;
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

		template<template <typename> typename P, typename... Args>
		struct collect
		{
			static constexpr bool val = (P<Args>::value && ...);
		};

		template<template <typename, typename> typename P, typename tpl1, typename tpl2>
		struct collect_pairs
		{
			static constexpr bool val = []<size_t...I>(std::index_sequence<I...>)
			{
				return (P<std::tuple_element_t<I, tpl1>, std::tuple_element_t<I, tpl2>>::value && ...);
			}(std::make_index_sequence<std::tuple_size_v<tpl1>>{});
		};

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
#ifndef KAWA_ECS_BASE
#define KAWA_ECS_BASE

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

#ifndef KAWA_META_ECS_EXT
#define	KAWA_META_ECS_EXT

#include <type_traits>

namespace kawa
{
	namespace meta
	{
		template<typename Fn, size_t offset, typename...Params>
		struct query_traits
		{
			static constexpr size_t params_count = sizeof...(Params) + offset;


			using dirty_args_tuple = typename function_traits<Fn>::args_tuple;

			using params_tuple = typename sub_tuple<dirty_args_tuple, offset, params_count>::tuple;

			using args_tuple = transform_each_t<dirty_args_tuple, remove_suffix_cv_t>;

			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = sub_tuple<args_tuple, params_count, args_count>::tuple;
			static constexpr size_t no_params_args_count = std::tuple_size_v<no_params_args_tuple>;

			constexpr static size_t require_count = meta::lval_ref_type_count<no_params_args_tuple>::value;
			constexpr static size_t optional_count = meta::ptr_type_count<no_params_args_tuple>::value;
		};

		template<typename Fn, typename...Params>
		struct query_self_traits
		{
			static constexpr size_t params_count = sizeof...(Params) + 1;

			using dirty_args_tuple = typename function_traits<Fn>::args_tuple;

			using params_tuple = typename sub_tuple<dirty_args_tuple, 1, params_count>::tuple;

			using args_tuple = transform_each_t<dirty_args_tuple, remove_suffix_cv_t>;

			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = sub_tuple<args_tuple, params_count, args_count>::tuple;
			static constexpr size_t no_params_args_count = std::tuple_size_v<no_params_args_tuple>;

			constexpr static size_t require_count = meta::lval_ref_type_count<no_params_args_tuple>::value;
			constexpr static size_t optional_count = meta::ptr_type_count<no_params_args_tuple>::value;
		};


		template<typename T>
		consteval inline bool valid_component_fn() noexcept
		{
			const bool result = !std::is_const_v<T> &&
				!std::is_volatile_v<T>;

			static_assert(!std::is_const_v<T>, "Component allowed to be const qualified only in query context.");

			static_assert(!std::is_volatile_v<T>, "Component allowed to be volatile qualified only in query context.");

			return result;
		}

		template<typename T>
		concept valid_component = valid_component_fn<T>();


		template<typename Fn, size_t offset, typename...Params>
		consteval inline bool ensure_fallthrough_parameters_fn() noexcept
		{
			constexpr bool result = collect_pairs<std::is_convertible, std::tuple<Params...>, typename meta::query_traits<Fn, offset, Params...>::params_tuple>::val;

			static_assert(result, "All fall-through parameters must be convertable to corresponding query function parameters.");

			return result;
		}

		template<typename Fn, size_t offset, typename...Params>
		concept ensure_fallthrough_parameters = ensure_fallthrough_parameters_fn<Fn, offset, Params...>();

		template<typename Fn, size_t index, typename T, typename...Params>
		consteval inline bool ensure_parameter_fn() noexcept
		{
			constexpr bool result = std::is_same_v<typename meta::function_traits<Fn>::template arg_at<sizeof...(Params) + index>, T>;

			static_assert(result, "Unexpected query parameter.");

			return result;
		}

		template<typename Fn, size_t index, typename T, typename...Params>
		concept ensure_parameter = ensure_parameter_fn<Fn, index, T, Params...>();

		template<typename Fn, size_t index, typename...Params>
		consteval inline bool ensure_entity_id_fn() noexcept
		{
			constexpr bool result = std::is_same_v<typename meta::function_traits<Fn>::template arg_at<sizeof...(Params) + index>, kawa::ecs::entity_id>;

			static_assert(result, "Unexpected query function parameter, expected kawa::ecs::entity_id.");

			return result;
		}

		template<typename Fn, size_t index, typename...Params>
		concept ensure_entity_id = ensure_entity_id_fn<Fn, index, Params...>();

		template<typename Fn, size_t index, typename...Params>
		consteval inline bool ensure_component_info_fn() noexcept
		{
			constexpr bool result = std::is_same_v<typename meta::function_traits<Fn>::template arg_at<sizeof...(Params) + index>, kawa::ecs::component_info>;

			static_assert(result, "Unexpected query function parameter, expected kawa::ecs::component_info.");

			return result;
		}

		template<typename Fn, size_t index, typename...Params>
		concept ensure_component_info = ensure_component_info_fn<Fn, index, Params...>();

	};
}
#endif
#ifndef KAWA_ECS_POLY_STORAGE
#define	KAWA_ECS_POLY_STORAGE

#include <memory>
#include <new>
#include <cstring> 

#include <iostream>

namespace kawa
{
	namespace ecs
	{
		namespace _
		{
			class poly_storage
			{
				using delete_fn_t = void(*)(void*);
				using erase_fn_t = void(*)(void*, size_t);
				using copy_fn_t = void(*)(void*, void*, size_t, size_t);
				using move_fn_t = void(*)(void*, void*, size_t, size_t);

				using handler_invoke_fn_t = void(*)(void*, void*, entity_id);
				using copy_handler_fn_t = void(*)(void*, void*&);

			public:
				inline poly_storage() noexcept = default;

				inline poly_storage(const poly_storage& other) noexcept
					: _type_info(other._type_info)
					, _capacity(other._capacity)
					, _occupied(other._occupied)
				{
					if (other._populated)
					{
						if (other._storage)
						{
							_storage = ::operator new(_type_info.size * _capacity, _type_info.alignment);
						}

						_delete_fn = other._delete_fn;
						_erase_fn = other._erase_fn;
						_copy_fn = other._copy_fn;
						_move_fn = other._move_fn;

						if (other._on_construct_fn)
						{
							_on_construct_fn_copy_fn = other._on_construct_fn_copy_fn;
							_on_construct_fn_copy_fn(other._on_construct_fn, _on_construct_fn);
							_on_construct_invoke_fn = other._on_construct_invoke_fn;
							_on_construct_delete_fn = other._on_construct_delete_fn;
						}

						if (other._on_destroy_fn)
						{
							_on_destroy_fn_copy_fn = other._on_destroy_fn_copy_fn;
							_on_destroy_fn_copy_fn(other._on_destroy_fn, _on_destroy_fn);
							_on_destroy_invoke_fn = other._on_destroy_invoke_fn;
							_on_destroy_delete_fn = other._on_destroy_delete_fn;
						}

						_mask = new bool[_capacity];
						std::copy(other._mask, other._mask + _capacity, _mask);

						_connector = new size_t[_capacity];
						std::copy(other._connector, other._connector + _capacity, _connector);

						_r_connector = new size_t[_capacity];
						std::copy(other._r_connector, other._r_connector + _capacity, _r_connector);

						for (size_t i = 0; i < _occupied; i++)
						{
							size_t id = _connector[i];
							if (_mask[id])
							{
								_copy_fn(other._storage, _storage, id, id);
								_on_construct(id);
							}
						}

						_populated = true;
					}
				}

				inline poly_storage(poly_storage&& other) noexcept
					: _type_info(other._type_info)
					, _capacity(other._capacity)
					, _occupied(other._occupied)
					, _storage(other._storage)
					, _mask(other._mask)
					, _connector(other._connector)
					, _r_connector(other._r_connector)
					, _delete_fn(other._delete_fn)
					, _erase_fn(other._erase_fn)
					, _copy_fn(other._copy_fn)
					, _move_fn(other._move_fn)
					, _on_construct_fn(other._on_construct_fn)
					, _on_destroy_fn(other._on_destroy_fn)
					, _on_construct_invoke_fn(other._on_construct_invoke_fn)
					, _on_destroy_invoke_fn(other._on_destroy_invoke_fn)
					, _on_construct_delete_fn(other._on_construct_delete_fn)
					, _on_destroy_delete_fn(other._on_destroy_delete_fn)
					, _populated(other._populated)
				{
					KAWA_ASSERT_MSG(other._populated, "poly_storage::poly_storage(poly_storage&& other) on unpopulated storage");

					other._populated = false;
				}

				inline poly_storage& operator=(const poly_storage& other) noexcept
				{
					if (this != &other)
					{
						this->clear();

						if (other._populated)
						{
							_type_info = other._type_info;
							_capacity = other._capacity;
							_occupied = other._occupied;

							if (other._storage)
							{
								_storage = ::operator new(_type_info.size * _capacity, _type_info.alignment);
							}

							_delete_fn = other._delete_fn;
							_erase_fn = other._erase_fn;
							_copy_fn = other._copy_fn;
							_move_fn = other._move_fn;

							if (other._on_construct_fn)
							{
								_on_construct_fn_copy_fn = other._on_construct_fn_copy_fn;
								_on_construct_fn_copy_fn(other._on_construct_fn, _on_construct_fn);
								_on_construct_invoke_fn = other._on_construct_invoke_fn;
								_on_construct_delete_fn = other._on_construct_delete_fn;
							}

							if (other._on_destroy_fn)
							{
								_on_destroy_fn_copy_fn = other._on_destroy_fn_copy_fn;
								_on_destroy_fn_copy_fn(other._on_destroy_fn, _on_destroy_fn);
								_on_destroy_invoke_fn = other._on_destroy_invoke_fn;
								_on_destroy_delete_fn = other._on_destroy_delete_fn;
							}

							_mask = new bool[_capacity];
							std::copy(other._mask, other._mask + _capacity, _mask);

							_connector = new size_t[_capacity];
							std::copy(other._connector, other._connector + _capacity, _connector);

							_r_connector = new size_t[_capacity];
							std::copy(other._r_connector, other._r_connector + _capacity, _r_connector);

							for (size_t i = 0; i < _occupied; i++)
							{
								size_t id = _connector[i];
								if (_mask[id])
								{
									_copy_fn(other._storage, _storage, id, id);
									_on_construct(id);
								}
							}

							this->_populated = true;
						}
					}

					return *this;
				}

				inline poly_storage& operator=(poly_storage&& other) noexcept
				{
					if (this != &other)
					{
						this->clear();

						if (other._populated)
						{
							_type_info = other._type_info;
							_capacity = other._capacity;
							_occupied = other._occupied;
							_storage = other._storage;
							_mask = other._mask;
							_connector = other._connector;
							_r_connector = other._r_connector;
							_delete_fn = other._delete_fn;
							_erase_fn = other._erase_fn;
							_copy_fn = other._copy_fn;
							_move_fn = other._move_fn;
							_on_construct_fn = other._on_construct_fn;
							_on_destroy_fn = other._on_destroy_fn;
							_on_construct_invoke_fn = other._on_construct_invoke_fn;
							_on_destroy_invoke_fn = other._on_destroy_invoke_fn;
							_on_construct_delete_fn = other._on_construct_delete_fn;
							_on_destroy_delete_fn = other._on_destroy_delete_fn;

							other._populated = false;

							this->_populated = true;
						}
					}

					return *this;
				}

				inline ~poly_storage() noexcept
				{
					clear();
				}


			public:
				template<typename T>
				inline poly_storage* populate(size_t capacity) noexcept
				{
					KAWA_ASSERT_MSG(!_populated, "poly_storage<{}>::populate<{}> on already populated storage", _type_info.name, meta::type_name<T>());

					_type_info = meta::type_info::create<T>();

					_capacity = capacity;

					if  constexpr (!std::is_empty_v<T>)
					{
						_storage = ::operator new(sizeof(T) * capacity, _type_info.alignment);
					}
					else
					{
						if constexpr (!std::is_trivially_destructible_v<T>)
						{
							_storage = ::operator new(sizeof(T) * 1, _type_info.alignment);
						}
					}

					_delete_fn =
						[](void* data)
						{
							::operator delete(data, std::align_val_t{ alignof(T) });
						};

					_mask = new bool[capacity]();
					_connector = new size_t[capacity]();
					_r_connector = new size_t[capacity]();

					_erase_fn =
						[](void* data, size_t index)
						{
							if constexpr (!std::is_empty_v<T>)
							{
								(static_cast<T*>(data) + index)->~T();
							}
							else
							{
								if constexpr (!std::is_trivially_destructible_v<T>)
								{
									(static_cast<T*>(data))->~T();
								}
							}
						};

					_copy_fn = [](void* from_ptr, void* to_ptr, size_t from, size_t to)
						{
							if constexpr (std::is_copy_constructible_v<T>)
							{
								new (static_cast<T*>(to_ptr) + to) T(*(static_cast<T*>(from_ptr) + from));
							}
							else
							{
								KAWA_VERIFY_MSG(false, "trying to copy uncopyable type [ {} ]", meta::type_name<T>());
							}
						};

					_move_fn = [](void* from_ptr, void* to_ptr, size_t from, size_t to)
						{
							if constexpr (std::is_move_constructible_v<T>)
							{
								new (static_cast<T*>(to_ptr) + to) T(std::move(*(static_cast<T*>(from_ptr) + from)));
							}
							else
							{
								KAWA_VERIFY_MSG(false, "trying to move unmovable type [ {} ]", meta::type_name<T>());
							}
						};


					_populated = true;

					return this;
				}

				inline void clear() noexcept
				{
					if (_populated)
					{
						for (size_t i = 0; i < _occupied; i++)
						{
							size_t id = _connector[i];

							_on_destory(id);
							_erase_fn(_storage, id);
						}

						_delete_fn(_storage);

						if (_on_construct_fn)
						{
							_on_construct_delete_fn(_on_construct_fn);
							_on_construct_fn = nullptr;
						}

						if (_on_destroy_fn)
						{
							_on_destroy_delete_fn(_on_destroy_fn);
							_on_destroy_fn = nullptr;
						}

						delete[] _mask;
						delete[] _connector;
						delete[] _r_connector;

						_populated = false;
					}
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

						T& val = *(new (static_cast<T*>(_storage) + index) T(std::forward<Args>(args)...));

						_on_construct(index);

						return val;
					}
					else
					{
						_on_destory(index);

						_erase_fn(_storage, index);

						return *(new (static_cast<T*>(_storage) + index) T(std::forward<Args>(args)...));
					}
				}

				template<typename T>
				inline T& get(size_t index) const noexcept
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
				//requires (!std::is_pointer_v<T>)
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

						_on_destory(index);
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

					if (_mask[from])
					{
						bool& cell = _mask[to];
						if (!cell)
						{
							size_t l_idx = _occupied++;

							_connector[l_idx] = to;
							_r_connector[to] = l_idx;

							cell = true;

							_copy_fn(_storage, _storage, from, to);
							_on_construct(to);
						}
						else
						{
							_on_destory(to);
							_erase_fn(_storage, to);
							_copy_fn(_storage, _storage, from, to);
							_on_construct(to);

						}
					}
				}

				inline void move(size_t from, size_t to) noexcept
				{
					KAWA_ASSERT_MSG(_populated, "poly_storage<->::move on non populated storage");
					KAWA_ASSERT_MSG(_validate_index(from), "poly_storage<{}>::move out of bounds index [ {} ] access", _type_info.name, from);
					KAWA_ASSERT_MSG(_validate_index(to), "poly_storage<{}>::move out of bounds index [ {} ] access", _type_info.name, to);

					if (_mask[from])
					{
						bool& cell = _mask[to];
						if (!cell)
						{
							size_t l_idx = _occupied++;

							_connector[l_idx] = to;
							_r_connector[to] = l_idx;

							cell = true;

							_move_fn(_storage, _storage, from, to);
							_on_construct(to);
						}
						else
						{
							_on_destory(to);
							_erase_fn(_storage, to);
							_move_fn(_storage, _storage, from, to);
							_on_construct(to);
						}

						_on_destory(from);
						erase(from);
					}
				}

				template<typename Fn>
				inline void set_on_construct(Fn&& fn) noexcept
				{
					if (_on_construct_fn)
					{
						_on_construct_delete_fn(_on_construct_fn);
					}

					_on_construct_fn = new Fn(std::forward<Fn>(fn));

					_on_construct_delete_fn =
						[](void* fn)
						{
							delete static_cast<Fn*>(fn);
						};

					_on_construct_invoke_fn =
						[](void* fn, void* data, entity_id entity)
						{
							using T = typename meta::template function_traits<Fn>::template arg_at<1>;
							static_cast<Fn*>(fn)->operator()(entity, *(static_cast<std::remove_reference_t<T>*>(data) + entity));
						};

					_on_construct_fn_copy_fn =
						[](void* from, void*& to)
						{
							to = new Fn(*static_cast<Fn*>(from));
						};
				}

				template<typename Fn>
				inline void set_on_destroy(Fn&& fn) noexcept
				{
					if (_on_destroy_fn)
					{
						_on_destroy_delete_fn(_on_construct_fn);
					}

					_on_destroy_fn = new Fn(std::forward<Fn>(fn));

					_on_destroy_delete_fn =
						[](void* fn)
						{
							delete static_cast<Fn*>(fn);
						};

					_on_destroy_invoke_fn =
						[](void* fn, void* data, entity_id entity)
						{
							using T = typename meta::template function_traits<Fn>::template arg_at<1>;
							static_cast<Fn*>(fn)->operator()(entity, *(static_cast<std::remove_reference_t<T>*>(data) + entity));
						};

					_on_destroy_fn_copy_fn =
						[](void* from, void*& to)
						{
							to = new Fn(*static_cast<Fn*>(from));
						};
				}

				inline void remove_on_construct() noexcept
				{
					if (_on_construct_fn)
					{
						_on_construct_delete_fn(_on_construct_fn);
						_on_construct_fn = nullptr;
					}
				}

				inline void remove_on_destroy() noexcept
				{
					if (_on_destroy_fn)
					{
						_on_destroy_delete_fn(_on_construct_fn);
						_on_destroy_fn = nullptr;
					}
				}

				inline bool has_on_construct() const noexcept
				{
					return _on_construct_fn;
				}

				inline bool has_on_destroy() const noexcept
				{
					return _on_destroy_fn;
				}

				inline bool is_populated() const noexcept
				{
					return _populated;
				}

				inline entity_id* begin() noexcept
				{
					return _connector;
				}

				inline entity_id* end() noexcept
				{
					return _connector + _occupied;
				}

				inline entity_id get(size_t i) const noexcept
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

				inline entity_id operator[](size_t i) const noexcept
				{
					return get(i);
				}

			private:
				inline void _on_destory(entity_id id) noexcept
				{
					if (_on_destroy_fn)
					{
						_on_destroy_invoke_fn(_on_destroy_fn, _storage, id);
					}
				}

				inline void _on_construct(entity_id id) noexcept
				{
					if (_on_construct_fn)
					{
						_on_construct_invoke_fn(_on_construct_fn, _storage, id);
					}
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

			private:
				size_t			_capacity = 0;
				void* _storage = nullptr;
				bool* _mask = nullptr;
				entity_id* _connector = nullptr;
				size_t* _r_connector = nullptr;
				size_t			_occupied = 0;

				erase_fn_t		_erase_fn = nullptr;
				delete_fn_t		_delete_fn = nullptr;
				copy_fn_t		_copy_fn = nullptr;
				move_fn_t		_move_fn = nullptr;

				void* _on_construct_fn = nullptr;
				void* _on_destroy_fn = nullptr;

				handler_invoke_fn_t	_on_construct_invoke_fn = nullptr;
				handler_invoke_fn_t	_on_destroy_invoke_fn = nullptr;

				copy_handler_fn_t	_on_construct_fn_copy_fn = nullptr;
				copy_handler_fn_t	_on_destroy_fn_copy_fn = nullptr;

				delete_fn_t	 _on_construct_delete_fn = nullptr;
				delete_fn_t	 _on_destroy_delete_fn = nullptr;

				meta::type_info	_type_info;

				bool _populated = false;
			};
		}
	}
}
#endif
#ifndef KAWA_ECS_ENTITY_MANAGER
#define KAWA_ECS_ENTITY_MANAGER

#include <limits>

namespace kawa
{
	namespace ecs
	{
		constexpr inline entity_id nullent = std::numeric_limits<entity_id>::max();

		namespace _
		{
			class entity_manager
			{
			public:
				entity_manager(size_t capacity, const std::string& debug_name)
					: _debug_name(debug_name)
				{
					_capacity = capacity;

					_free_list = new size_t[capacity]();
					_entity_mask = new bool[capacity]();
					_entity_entries = new size_t[capacity]();
					_r_entity_entries = new size_t[capacity]();
				};

				entity_manager(const entity_manager& other)
					: _debug_name(other._debug_name)
					, _capacity(other._capacity)
					, _occupied(other._occupied)
					, _free_list_size(other._free_list_size)
					, _entries_counter(other._entries_counter)
				{

					_free_list = new size_t[_capacity];
					std::copy(other._free_list, other._free_list + _capacity, _free_list);

					_entity_mask = new bool[_capacity];
					std::copy(other._entity_mask, other._entity_mask + _capacity, _entity_mask);

					_entity_entries = new size_t[_capacity];
					std::copy(other._entity_entries, other._entity_entries + _capacity, _entity_entries);

					_r_entity_entries = new size_t[_capacity];
					std::copy(other._r_entity_entries, other._r_entity_entries + _capacity, _r_entity_entries);
				};

				~entity_manager()
				{
					delete[] _free_list;
					delete[] _entity_mask;
					delete[] _entity_entries;
					delete[] _r_entity_entries;
				};

				inline entity_manager& operator=(const entity_manager& other)
				{
					if (this != &other)
					{
						delete[] _free_list;
						delete[] _entity_mask;
						delete[] _entity_entries;
						delete[] _r_entity_entries;

						_debug_name = other._debug_name;
						_capacity = other._capacity;
						_occupied = other._occupied;
						_free_list_size = other._free_list_size;
						_entries_counter = other._entries_counter;

						_free_list = new size_t[_capacity];
						std::copy(other._free_list, other._free_list + _capacity, _free_list);

						_entity_mask = new bool[_capacity];
						std::copy(other._entity_mask, other._entity_mask + _capacity, _entity_mask);

						_entity_entries = new size_t[_capacity];
						std::copy(other._entity_entries, other._entity_entries + _capacity, _entity_entries);

						_r_entity_entries = new size_t[_capacity];
						std::copy(other._r_entity_entries, other._r_entity_entries + _capacity, _r_entity_entries);

					}
					return *this;
				}

			public:

				inline void clear() noexcept
				{
					std::fill(_entity_mask, _entity_mask + _capacity, false);
					_free_list_size = 0;
					_entries_counter = 0;
					_occupied = 0;

				}

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
					KAWA_DEBUG_EXPAND(_validate_entity(e));

					return _entity_mask[e];
				}

				inline void remove_unchecked(entity_id e) noexcept
				{
					KAWA_DEBUG_EXPAND(_validate_entity(e));

					_free_list[_free_list_size++] = e;
					_entity_mask[e] = false;

					size_t idx = _r_entity_entries[e];

					_entity_entries[idx] = _entity_entries[--_entries_counter];
					_r_entity_entries[_entity_entries[_entries_counter]] = idx;
				}


				inline void remove(entity_id e) noexcept
				{
					KAWA_DEBUG_EXPAND(_validate_entity(e));

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

				inline entity_id get(size_t i) noexcept
				{
					KAWA_ASSERT_MSG(i < _occupied, "[ {} ]: index out of bounds [ {} ]", _debug_name, i);

					return _entity_entries[i];
				}

				inline entity_id* begin() noexcept
				{
					return _entity_entries;
				}

				inline entity_id* end() noexcept
				{
					return _entity_entries + _occupied;
				}

				inline size_t occupied() noexcept
				{
					return _occupied;
				}

				inline entity_id operator[](size_t i) noexcept
				{
					return get(i);
				}

			private:
				inline void _validate_entity(entity_id id) const noexcept
				{
					KAWA_ASSERT_MSG(id != nullent, "[ {} ]: nullent usage", _debug_name);
					KAWA_ASSERT_MSG(id < _capacity, "[ {} ]: invalid entity_id [ {} ] usage", _debug_name, id);
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

				std::string _debug_name;
			};
		}
	}
}

#endif 


#ifndef KAWA_ECS_STORAGE_MANAGER
#define KAWA_ECS_STORAGE_MANAGER

#include <limits>

namespace kawa
{
	namespace ecs
	{
		namespace _
		{
			class storage_manager
			{

			public:
				inline storage_manager(size_t storage_capacity, size_t capacity, const std::string& debug_name) noexcept
					: _debug_name(debug_name)
				{
					_capacity = capacity;
					_storage_capacity = storage_capacity;

					_storages = new poly_storage[storage_capacity]();
					_mask = new bool[storage_capacity]();
					_entries = new size_t[storage_capacity]();
					_r_entries = new size_t[storage_capacity]();
				};

				inline storage_manager(const storage_manager& other) noexcept
					: _debug_name(other._debug_name)
					, _capacity(other._capacity)
					, _storage_capacity(other._storage_capacity)
					, _entries_counter(other._entries_counter)
				{
					_storages = new poly_storage[_storage_capacity];
					std::copy(other._storages, other._storages + _storage_capacity, _storages);

					_mask = new bool[_storage_capacity];
					std::copy(other._mask, other._mask + _storage_capacity, _mask);

					_entries = new size_t[_storage_capacity];
					std::copy(other._entries, other._entries + _storage_capacity, _entries);

					_r_entries = new size_t[_storage_capacity];
					std::copy(other._r_entries, other._r_entries + _storage_capacity, _r_entries);
				};

				inline storage_manager(storage_manager&& other) noexcept
					: _debug_name(std::move(other._debug_name))
					, _capacity(other._capacity)
					, _storage_capacity(other._storage_capacity)
					, _entries_counter(other._entries_counter)
					, _storages(other._storages)
					, _mask(other._mask)
					, _entries(other._entries)
					, _r_entries(other._r_entries)
				{
					other._storages = nullptr;
					other._mask = nullptr;
					other._entries = nullptr;
					other._r_entries = nullptr;
				};

				inline ~storage_manager() noexcept
				{
					delete[] _storages;
					delete[] _mask;
					delete[] _entries;
					delete[] _r_entries;
				};

				inline storage_manager& operator=(const storage_manager& other) noexcept
				{
					if (this != &other)
					{
						delete[] _storages;
						delete[] _mask;
						delete[] _entries;
						delete[] _r_entries;

						_debug_name = other._debug_name;
						_capacity = other._capacity;
						_storage_capacity = other._storage_capacity;
						_entries_counter = other._entries_counter;

						_storages = new poly_storage[_storage_capacity];
						std::copy(other._storages, other._storages + _storage_capacity, _storages);

						_mask = new bool[_storage_capacity];
						std::copy(other._mask, other._mask + _storage_capacity, _mask);

						_entries = new size_t[_storage_capacity];
						std::copy(other._entries, other._entries + _storage_capacity, _entries);

						_r_entries = new size_t[_storage_capacity];
						std::copy(other._r_entries, other._r_entries + _storage_capacity, _r_entries);
					}
					return *this;
				}

				inline storage_manager& operator=(storage_manager&& other) noexcept
				{
					if (this != &other)
					{
						delete[] _storages;
						delete[] _mask;
						delete[] _entries;
						delete[] _r_entries;

						_debug_name = std::move(other._debug_name);
						_capacity = other._capacity;
						_storage_capacity = other._storage_capacity;
						_entries_counter = other._entries_counter;
						_storages = other._storages;
						_mask = other._mask;
						_entries = other._entries;
						_r_entries = other._r_entries;

						other._storages = nullptr;
						other._mask = nullptr;
						other._entries = nullptr;
						other._r_entries = nullptr;
					}
					return *this;
				}

			public:
				inline void clear() noexcept
				{
					std::fill(_mask, _mask + _storage_capacity, false);

					for (size_t i = 0; i < _entries_counter; i++)
					{
						_storages[_entries[i]].clear();
					}

					_entries_counter = 0;
					_entries_counter = 0;
				}

				template<typename T>
				inline storage_id get_id() noexcept
				{
					static storage_id id = _id_counter++;
					return id;
				}

				template<typename...Args>
				inline void ensure() noexcept
				{
					(get_id<Args>(), ...);
				}

				template<typename T, typename...Args>
				inline T& emplace(size_t index, Args&&...args) noexcept
				{
					poly_storage& storage = get_storage<T>();

					return storage.emplace<T>(index, std::forward<Args>(args)...);
				}

				template<typename...Args>
				inline void erase(size_t index) noexcept
				{
					(([this, index]<typename T>()
					{

						get_storage<T>().erase(index);
					}.template operator() < Args > ()), ...);
				}

				template<typename...Args>
				inline bool has(size_t index) noexcept
				{
					return (([this, index]<typename T>()
					{
						return get_storage<T>().has(index);
					}.template operator() < Args > ()) && ...);
				}

				template<typename T>
				inline T& get(size_t index) noexcept
				{
					return get_storage<T>().template get<T>(index);
				}

				template<typename T>
				inline T* get_if_has(size_t index) noexcept
				{
					return get_storage<T>().template get_if_has<T>(index);
				}

				template<typename...Args>
				inline void copy(size_t from, size_t to) noexcept
				{
					if (from != to)
					{
						(([this, from, to]<typename T>()
						{
							get_storage<T>().copy(from, to);

						}. template operator() < Args > ()), ...);
					}
				}

				template<typename...Args>
				inline void move(size_t from, size_t to) noexcept
				{
					if (from != to)
					{
						(([this, from, to]<typename T>()
						{
							get_storage<T>().move(from, to);

						}. template operator() < Args > ()), ...);
					}
				}

				inline bool alive(storage_id e) noexcept
				{
					return _mask[e];
				}

				//inline void remove_unchecked(storage_id id) noexcept
				//{
				//	size_t idx = _r_entries[id];

				//	_entries[idx] = _entries[--_entries_counter];
				//	_r_entries[_entries[_entries_counter]] = idx;
				//}

				//inline void remove(storage_id id) noexcept
				//{
				//	bool& cell = _mask[id];

				//	if (cell)
				//	{
				//		size_t idx = _r_entries[id];

				//		_entries[idx] = _entries[--_entries_counter];
				//		_r_entries[_entries[_entries_counter]] = idx;
				//	}
				//}

				template<typename T>
				inline poly_storage& get_storage() noexcept
				{
					storage_id id = get_id<T>();

					poly_storage& storage = _storages[id];
					bool& cell = _mask[id];

					if (!cell)
					{
						storage.populate<T>(_capacity);
						cell = true;

						size_t idx = _entries_counter++;

						_entries[idx] = id;
						_r_entries[id] = idx;
					}

					return storage;
				}

				inline poly_storage& get_storage(storage_id id) noexcept
				{
					return _storages[id];
				}

				inline storage_id* begin() noexcept
				{
					return _entries;
				}

				inline storage_id* end() noexcept
				{
					return _entries + _entries_counter;
				}

				inline size_t occupied() noexcept
				{
					return _entries_counter;
				}

			private:
				inline void _validate_storage(storage_id id) const noexcept
				{
					KAWA_ASSERT_MSG(id < _storage_capacity, "[ {} ]: maximum amoount of unique component types reached [ {} ], increase max_component_types", _debug_name, _storage_capacity);
				}

			private:
				poly_storage* _storages = nullptr;
				bool* _mask = nullptr;

				storage_id* _entries = nullptr;
				size_t* _r_entries = nullptr;
				size_t			_entries_counter = 0;

				size_t			_capacity = 0;
				size_t			_storage_capacity = 0;

				std::string		_debug_name;

			private:
				static inline size_t _id_counter = 0;
			};
		}
	}
}

#endif 
#ifndef KAWA_THREAD_POOL
#define	KAWA_THREAD_POOL

#include <thread>
#include <barrier>
#include <new>
#include <functional>

namespace kawa
{
	class thread_pool
	{
	public:
		thread_pool(size_t thread_count)
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

								_task(_starts[i], _ends[i]);

								_barrier.arrive_and_wait();
							}
						}
					);
				}
			}
		}
		~thread_pool()
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
		void task(Fn&& query, size_t work)
		{
			_task = std::forward<Fn>(query);

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

			_task(_starts[_tasks_count - 1], _ends[_tasks_count - 1]);

			_barrier.arrive_and_wait();
		}


	private:
		std::thread* _threads = nullptr;
		const size_t							_thread_count = 0;
		const size_t							_tasks_count = 0;

		size_t* _starts = nullptr;
		size_t* _ends = nullptr;


		std::function<void(size_t, size_t)>		_task = nullptr;
		std::barrier<>							_barrier;

		bool									_should_join = false;
	};

}
#endif
namespace kawa
{
	namespace ecs
	{
		class registry
		{
		public:
			struct specification
			{
				std::string name = "unnamed";
				size_t max_entity_count = 512;
				size_t max_component_types = 32;
			};

			inline registry(const registry::specification& reg_spec) noexcept
				: _spec(reg_spec)
				, _storage_manager(reg_spec.max_component_types, reg_spec.max_entity_count, reg_spec.name)
				, _entity_manager(reg_spec.max_entity_count, reg_spec.name)
			{}

			inline registry(const registry& other) noexcept = default;
			inline registry(registry&& other) noexcept = default;

			inline registry& operator=(const registry& reg) noexcept = default;
			inline registry& operator=(registry&& reg) noexcept = default;

			inline ~registry() noexcept {};

		public:
			inline const specification& get_specs() const noexcept
			{
				return _spec;
			}

			inline std::string get_full_name() const noexcept
			{
				if (_owned_by_world)
				{
					return std::format("{}::{}", _world_name, _spec.name);
				}

				return _spec.name;

			}

			inline void clear() noexcept
			{
				_entity_manager.clear();
				_storage_manager.clear();
			}

			inline entity_id entity() noexcept
			{
				return _entity_manager.get_new();
			}

			template<meta::valid_component...Args>
			inline entity_id entity_with(Args&&...args) noexcept
			{
				entity_id id = entity();

				(emplace<Args>(id, std::forward<Args>(args)), ...);

				return id;
			}

			template<meta::valid_component T, typename...Args>
				requires std::constructible_from<T, Args...>
			inline T& emplace(entity_id entity, Args&&...args) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));
				KAWA_ASSERT_MSG(_entity_manager.alive(entity), "[ {} ] kawa::ecs::registry::emplace<{}> on non alive entity", get_full_name(), meta::type_name<T>());

				static_assert(!std::is_const_v<T>, "component can not have const qualifier");

				return _storage_manager.emplace<T>(entity, std::forward<Args>(args)...);
			}

			template<meta::valid_component...Args>
			inline void erase(entity_id entity) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				_storage_manager.erase<Args...>(entity);
			}

			template<meta::valid_component...Args>
			inline bool has(entity_id entity) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				return _storage_manager.has<Args...>(entity);
			}

			template<meta::valid_component T>
			inline T& get(entity_id entity) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				return _storage_manager.get<T>(entity);
			}

			template<meta::valid_component T>
			inline T* get_if_has(entity_id entity) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				if (!_entity_manager.alive(entity))
				{
					return nullptr;
				}

				return _storage_manager.get_if_has<T>(entity);
			}

			template<meta::valid_component...Args>
				requires ((std::copyable<Args> && ...))
			inline void copy(entity_id from, entity_id to) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(from));
				KAWA_DEBUG_EXPAND(_validate_entity(to));

				_storage_manager.copy<Args...>(from, to);
			}

			template<meta::valid_component...Args>
				requires ((std::movable<Args> && ...))
			inline void move(entity_id from, entity_id to) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(from));
				KAWA_DEBUG_EXPAND(_validate_entity(to));

				_storage_manager.move<Args...>(from, to);
			}

			inline entity_id clone(entity_id from) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(from));

				entity_id e = entity();

				if (e == nullent) return e;

				for (storage_id s : _storage_manager)
				{
					_storage_manager.get_storage(s).copy(from, e);
				}

				return e;
			}

			inline void clone(entity_id from, entity_id to) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(from));
				KAWA_DEBUG_EXPAND(_validate_entity(to));

				for (storage_id s : _storage_manager)
				{
					_storage_manager.get_storage(s).copy(from, to);
				}
			}

			inline void destroy(entity_id entity) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				if (_entity_manager.alive(entity))
				{
					for (storage_id s : _storage_manager)
					{
						_storage_manager.get_storage(s).erase(entity);
					}

					_entity_manager.remove_unchecked(entity);
				}
			}

			inline bool alive(entity_id entity) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				return _storage_manager.alive(entity);
			}

			inline bool is_valid(entity_id e) noexcept
			{
				if (e == nullent || e >= _spec.max_entity_count)
				{
					return false;
				}

				return true;
			}

			template<typename Fn>
				requires meta::ensure_entity_id<Fn, 0>
			inline void on_construct(Fn&& fn)
			{
				using ft = typename meta::function_traits<Fn>;

				using second_arg = typename ft::template arg_at<1>;

				_storage_manager.get_storage<std::remove_cvref_t<second_arg>>().set_on_construct(std::forward<Fn>(fn));
			}

			template<typename Fn>
				requires meta::ensure_entity_id<Fn, 0>
			inline void on_destroy(Fn&& fn)
			{
				using ft = typename meta::function_traits<Fn>;

				using second_arg = typename ft::template arg_at<1>;

				_storage_manager.get_storage<std::remove_cvref_t<second_arg>>().set_on_destroy(std::forward<Fn>(fn));
			}

			template<meta::valid_component...Args>
			inline void ensure() noexcept
			{
				_storage_manager.ensure<Args...>();
			}

			template<typename Fn, typename...Params>
				requires
			(
				meta::ensure_component_info<Fn, 0>&&
				meta::ensure_fallthrough_parameters<Fn, 1, Params...>
				)
				inline void query_with_info(entity_id entity, Fn&& fn, Params&&...params) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				if (_entity_manager.alive(entity))
				{
					for (storage_id s : _storage_manager)
					{
						auto& storage = _storage_manager.get_storage(s);
						if (storage.has(entity))
						{
							std::forward<Fn>(fn)(std::forward<Params>(params)..., storage.get_type_info());
						}
					}
				}
			}

			template<typename Fn, typename...Params>
				requires
			(
				meta::ensure_entity_id<Fn, 0>&&
				meta::ensure_component_info<Fn, 1>&&
				meta::ensure_fallthrough_parameters<Fn, 2, Params...>
				)
				inline void query_self_info(Fn&& fn, Params&&...params) noexcept
			{
				for (entity_id entity : _entity_manager)
				{
					for (storage_id s : _storage_manager)
					{

						auto& storage = _storage_manager.get_storage(s);

						if (storage.has(entity))
						{
							std::forward<Fn>(fn)(entity, std::forward<Params>(params)..., storage.get_type_info());
						}
					}
				}
			}

			template<typename Fn, typename...Params>
				requires meta::ensure_fallthrough_parameters<Fn, 0, Params...>
			inline void query(Fn&& fn, Params&&...params) noexcept
			{
				using query_traits = typename meta::query_traits<Fn, 0, Params...>;

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
			}

			template<typename Fn, typename...Params>
				requires meta::ensure_fallthrough_parameters<Fn, 0, Params...>
			inline void query_par(thread_pool& exec, Fn&& fn, Params&&...params) noexcept
			{
				KAWA_ASSERT_MSG(!_query_par_running, "[ {} ]: trying to invoke kawa::ecs::query_par inside another parallel query body", get_full_name());

				_query_par_running = true;

				using query_traits = typename meta::query_traits<Fn, 0, Params...>;

				if constexpr (query_traits::args_count == query_traits::params_count)
				{
					exec.task
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
							exec,
							std::forward<Fn>(fn),
							std::make_index_sequence<query_traits::no_params_args_count>{},
							std::make_index_sequence<query_traits::require_count>{},
							std::make_index_sequence<query_traits::optional_count>{},
							std::forward<Params>(params)...
						);
				}

				_query_par_running = false;
			}

			template<typename Fn, typename...Params>
				requires
			(
				meta::ensure_entity_id<Fn, 0>&&
				meta::ensure_fallthrough_parameters<Fn, 1, Params...>
				)
				inline void query_self(Fn&& fn, Params&&...params) noexcept
			{
				using query_self_traits = typename meta::query_self_traits<Fn, Params...>;

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
			}

			template<typename Fn, typename...Params>
				requires
			(
				meta::ensure_entity_id<Fn, 0>&&
				meta::ensure_fallthrough_parameters<Fn, 1, Params...>
				)
				inline void query_self_par(thread_pool& exec, Fn&& fn, Params&&...params) noexcept
			{
				KAWA_ASSERT_MSG(!_query_par_running, "[ {} ]: trying to invoke kawa::ecs::query_self_par inside another parallel query body", get_full_name());
				_query_par_running = true;

				using query_traits = typename meta::query_traits<Fn, 1, Params...>;

				if constexpr (query_traits::args_count == query_traits::params_count)
				{
					exec.task
					(
						[&](size_t start, size_t end)
						{
							for (size_t i = start; i < end; i++)
							{
								fn(_entity_manager[i], std::forward<Params>(params)...);
							}
						}
						, _entity_manager.occupied()
					);
				}
				else
				{
					_query_self_par_impl<Fn, typename query_traits::no_params_args_tuple>
						(
							exec,
							std::forward<Fn>(fn),
							std::make_index_sequence<query_traits::no_params_args_count>{},
							std::make_index_sequence<query_traits::require_count>{},
							std::make_index_sequence<query_traits::optional_count>{},
							std::forward<Params>(params)...
						);
				}

				_query_par_running = false;
			}

			template<typename Fn, typename...Params>
				requires meta::ensure_fallthrough_parameters<Fn, 0, Params...>
			inline void query_with(entity_id entity, Fn fn, Params&&...params) noexcept
			{
				KAWA_DEBUG_EXPAND(_validate_entity(entity));

				using query_traits = typename meta::query_traits<Fn, 0, Params...>;

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
			}

		private:
			template<typename Fn, typename args_tuple, size_t...args_idxs, size_t...req_idxs, size_t...opt_idxs, typename...Params>
			inline void _query_impl(Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t args_count = sizeof...(args_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);
				constexpr size_t req_count = sizeof...(req_idxs);

				auto args_storages = std::forward_as_tuple(_storage_manager.get_storage<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()...);

				std::array<_::poly_storage*, opt_count> optional_storages;
				_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

				if constexpr (req_count)
				{
					std::array<_::poly_storage*, req_count> require_storages = {};
					_populate_required_array<args_tuple, args_idxs...>(require_storages);

					_::poly_storage* smallest = require_storages[0];

					for (_::poly_storage* st : require_storages)
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
			inline void _query_par_impl(thread_pool& exec, Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t args_count = sizeof...(args_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);
				constexpr size_t req_count = sizeof...(req_idxs);

				auto args_storages = std::forward_as_tuple(_storage_manager.get_storage<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()...);

				std::array<_::poly_storage*, opt_count> optional_storages;
				_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

				if constexpr (req_count)
				{
					std::array<_::poly_storage*, req_count> require_storages = {};
					_populate_required_array<args_tuple, args_idxs...>(require_storages);

					_::poly_storage* smallest = require_storages[0];

					for (_::poly_storage* st : require_storages)
					{
						if (st->occupied() < smallest->occupied())
						{
							smallest = st;
						}
					}

					exec.task
					(
						[&](size_t start, size_t end)
						{
							std::apply
							(
								[&](auto&&...storages)
								{
									for (size_t i = start; i < end; ++i)
									{
										entity_id e = smallest->get(i);
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
					exec.task
					(
						[&](size_t start, size_t end)
						{
							std::apply
							(
								[&](auto&&...storages)
								{
									for (size_t i = start; i < end; ++i)
									{
										entity_id e = _entity_manager[i];
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

				auto args_storages = std::forward_as_tuple(_storage_manager.get_storage<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()...);

				std::array<_::poly_storage*, opt_count> optional_storages;
				_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

				if constexpr (req_count)
				{
					std::array<_::poly_storage*, req_count> require_storages = {};
					_populate_required_array<args_tuple, args_idxs...>(require_storages);

					_::poly_storage* smallest = require_storages[0];

					for (_::poly_storage* st : require_storages)
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
			inline void _query_self_par_impl(thread_pool& exec, Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t args_count = sizeof...(args_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);
				constexpr size_t req_count = sizeof...(req_idxs);

				auto args_storages = std::forward_as_tuple(_storage_manager.get_storage<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()...);

				std::array<_::poly_storage*, opt_count> optional_storages;
				_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

				if constexpr (req_count)
				{
					std::array<_::poly_storage*, req_count> require_storages = {};
					_populate_required_array<args_tuple, args_idxs...>(require_storages);

					_::poly_storage* smallest = require_storages[0];

					for (_::poly_storage* st : require_storages)
					{
						if (st->occupied() < smallest->occupied())
						{
							smallest = st;
						}
					}

					exec.task
					(
						[&](size_t start, size_t end)
						{
							std::apply
							(
								[&](auto&&...storages)
								{
									for (size_t i = start; i < end; ++i)
									{
										entity_id e = smallest->get(i);
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
					exec.task
					(
						[&](size_t start, size_t end)
						{
							std::apply
							(
								[&](auto&&...storages)
								{
									for (size_t i = start; i < end; ++i)
									{
										entity_id e = _entity_manager[i];
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

				auto args_storages = std::forward_as_tuple(_storage_manager.get_storage<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()...);

				std::array<_::poly_storage*, opt_count> optional_storages;
				_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

				if constexpr (req_count)
				{
					std::array<_::poly_storage*, req_count> require_storages = {};
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
			inline void _make_owned(const std::string& world_name) noexcept
			{
				_owned_by_world = true;
				_world_name = world_name;
			}


		private:
			template<typename args_tuple, size_t...I, size_t N>
			inline constexpr void _populate_optional_array(std::array<_::poly_storage*, N>& out) noexcept
			{
				size_t id = 0;

				(([&]<size_t i>()
				{
					using T = std::tuple_element_t<I, args_tuple>;

					if constexpr (std::is_pointer_v<T>)
					{
						using CleanT = std::remove_pointer_t<T>;
						auto key = get_id<CleanT>();
						out[id] = &_storage_manager.get_storage(key);
						id++;
					}
				}.template operator() < I > (), void(), 0), ...);
			}

			template<typename args_tuple, size_t...I, size_t N>
			inline constexpr void _populate_required_array(std::array<_::poly_storage*, N>& out) noexcept
			{
				size_t id = 0;

				(([&]<size_t i>()
				{
					using T = std::tuple_element_t<I, args_tuple>;

					if constexpr (std::is_reference_v<T>)
					{
						using CleanT = std::remove_reference_t<T>;
						auto key = get_id<CleanT>();
						out[id] = &_storage_manager.get_storage(key);
						id++;
					}
				}.template operator() < I > (), void(), 0), ...);
			}

		private:
			template<typename T>
			inline storage_id get_id() noexcept
			{
				static storage_id id = _storage_manager.get_id<T>();
				KAWA_DEBUG_EXPAND(_validate_storage(id));
				return id;
			}

			inline void _validate_entity(entity_id id) const noexcept
			{
				KAWA_ASSERT_MSG(id != nullent, "[ {} ]: nullent usage", get_full_name());
				KAWA_ASSERT_MSG(id < _spec.max_entity_count, "[ {} ]: invalid entity_id [ {} ] usage", get_full_name(), id);
			}

			inline void _validate_storage(storage_id id) const noexcept
			{
				KAWA_ASSERT_MSG(id < _spec.max_component_types, "[ {} ]: maximum amoount of unique component types reached [ {} ], increase max_component_types", get_full_name(), _spec.max_component_types);
			}

		private:
			specification				_spec;

			_::storage_manager			_storage_manager;
			_::entity_manager			_entity_manager;
			std::string					_world_name;

			bool						_query_par_running = false;
			bool						_owned_by_world = false;

		private:
			static inline storage_id	_storage_id_counter = 0;

		};
	}
}

#endif 

#endif