#ifndef KAWA_ANY
#define KAWA_ANY

#include <concepts>
#include "macros.h"
#include "meta.h"

namespace kawa
{

struct lifetime_vtable
{
	using deleter_fn_t = void(void*);
	using dtor_fn_t = void(void*);
	using copy_ctor_fn_t = void(void*, void*);
	using move_ctor_fn_t = void(void*, void*);

	deleter_fn_t* deleter_fn = nullptr;
	dtor_fn_t* dtor_fn = nullptr;
	copy_ctor_fn_t* copy_ctor_fn = nullptr;
	move_ctor_fn_t* move_ctor_fn = nullptr;
	meta::type_info type_info;

	void release()
	{
		type_info.reset();
		deleter_fn = nullptr;
		dtor_fn = nullptr;
		copy_ctor_fn = nullptr;
		move_ctor_fn = nullptr;
	}

	template<typename T>
	void refresh()
	{
		release();

		type_info.refresh<T>();

		deleter_fn = +[](void* ptr)
			{
				delete reinterpret_cast<T*>(ptr);
			};

		dtor_fn = +[](void* ptr)
			{
				reinterpret_cast<T*>(ptr)->~T();
			};
		
		copy_ctor_fn = nullptr;

		if constexpr (std::is_copy_constructible_v<T>)
		{
			copy_ctor_fn =+[](void* from, void* to)
				{
					new (to) T(*reinterpret_cast<T*>(from));
				};
		}

		move_ctor_fn = nullptr;

		if constexpr (std::is_move_constructible_v<T>)
		{
			move_ctor_fn = +[](void* from, void* to)
				{
					new (to) T(std::move(*reinterpret_cast<T*>(from)));
				};
		}
	}

	void deleter(void* ptr)
	{
		kw_assert(type_info && ptr);
		deleter_fn(ptr);
	}

	void dtor(void* ptr)
	{
		kw_assert(type_info && ptr);
		dtor_fn(ptr);
	}

	void dtor_offset(void* ptr, usize offset)
	{
		dtor((u8*)(ptr)+offset * type_info.size);
	}


	void copy_ctor(void* from, void* to)
	{
		kw_assert(type_info && from && to);
		kw_assert_msg(copy_ctor_fn, "trying to copy uncopyable type");
		copy_ctor_fn(from, to);
	}
	
	void copy_ctor_offset(void* from, usize from_offset, void* to, usize to_offset)
	{
		copy_ctor((u8*)(from)+from_offset * type_info.size, (u8*)(to)+to_offset * type_info.size);
	}

	void move_ctor(void* from, void* to)
	{
		kw_assert(type_info && from && to);
		kw_assert_msg(move_ctor_fn, "trying to move unmovalbe type");
		move_ctor_fn(from, to);
	}

	void move_ctor_offset(void* from, usize from_offset, void* to, usize to_offset)
	{
		move_ctor((u8*)(from) + from_offset * type_info.size, (u8*)(to)+to_offset * type_info.size);
	}
};

template<typename T>
struct any_construct_tag {};

template<usize storage_size>
class sized_any
{
public:

	sized_any() = default;

	template<typename T, typename...Args>
		requires (sizeof(T) <= storage_size)
	sized_any(any_construct_tag<T>, Args&&...args)
	{
		refresh<T>(std::forward<Args>(args)...);
	}

	sized_any(sized_any&& other) noexcept
	{
		_vtable.type_info = other._vtable.type_info;
		_storage = other._storage;
		other._storage = nullptr;
	}

	sized_any(const sized_any& other) = delete;

	~sized_any()
	{
		release();
	}

	void release()
	{
		if (_vtable.type_info)
		{
			_vtable.dtor(_storage);
			_vtable.type_info.reset();
		}
	}	

	template<typename...Fn>
	bool try_match(Fn&&...funcs)
	{
		bool matched = false;
		(([&]()
		{
			if (matched) return;

			using expected_t = std::remove_cvref_t<typename meta::function_traits<Fn>::template arg_at<0>>;

			if (auto v = try_unwrap<expected_t>())
			{
				funcs(*v);
				matched = true;
			}
		}()), ...);

		return matched;
	}

	template<typename...Fn>
	void ensure_match(Fn&&...funcs)
	{
		bool matched = false;
		(([&]()
			{
				if (matched) return;

				using expected_t = std::remove_cvref_t<typename meta::function_traits<Fn>::template arg_at<0>>;

				if (auto v = try_unwrap<expected_t>())
				{
					funcs(*v);
					matched = true;
				}
			}()), ...);

		kw_assert_msg(matched, "wasnt able to match");
	}

	template<typename T, typename...Args>
		requires (sizeof(T) <= storage_size)
	T& refresh(Args&&...args)
	{
		release();

		auto v = new(_storage) T(std::forward<Args>(args)...);
		_vtable.refresh<T>();

		return *v;
	}	

	template<typename T>
		requires (sizeof(T) <= storage_size)
	T& unwrap()	noexcept
	{
		kw_assert(_vtable.type_info.is<T>());
		return *reinterpret_cast<T*>(_storage);
	}

	template<typename T>
		requires (sizeof(T) <= storage_size)
	T* try_unwrap()	noexcept
	{
		if (_vtable.type_info.is<T>())
		{
			return reinterpret_cast<T*>(_storage);
		}
		else
		{
			return nullptr;
		}
	}

	template<typename T>
		requires (sizeof(T) <= storage_size)
	bool is() noexcept { return _vtable.type_info.is<T>(); }

private:
	alignas(std::max_align_t) u8 _storage[storage_size];
	lifetime_vtable _vtable;
};

class unsized_any
{
public:

	unsized_any() = default;

	template<typename T, typename...Args>
	unsized_any(any_construct_tag<T>, Args&&...args)
	{
		refresh<T>(std::forward<Args>(args)...);
	}

	unsized_any& operator=(unsized_any&& other)
	{
		if (this != &other)
		{
			_vtable.type_info = other._vtable.type_info;
			if (_vtable.type_info)
			{
				_vtable.move_ctor(other._storage, _storage);
			}
		}

		return *this;
	}

	unsized_any(unsized_any&& other) noexcept
	{
		*this = std::move(other);
	}

	unsized_any& operator=(const unsized_any& other)
	{
		if (this != &other)
		{
			_vtable = other._vtable;
			if (_vtable.type_info)
			{
				_vtable.copy_ctor(other._storage, _storage);
			}
		}

		return *this;
	}

	unsized_any(const unsized_any& other) noexcept
	{
		*this = other;
	}

	~unsized_any()
	{
		release();
	}

	void release()
	{
		if (_storage)
		{
			_vtable.deleter(_storage);
			_storage = nullptr;
		}
	}

	template<typename...Fn>
	bool try_match(Fn&&...funcs)
	{
		bool matched = false;
		(([&]()
			{
				if (matched) return;

				using expected_t = std::remove_cvref_t<typename meta::function_traits<Fn>::template arg_at<0>>;

				if (auto v = try_unwrap<expected_t>())
				{
					funcs(*v);
					matched = true;
				}
			}()), ...);

		return matched;
	}

	template<typename...Fn>
	void ensure_match(Fn&&...funcs)
	{
		bool matched = false;
		(([&]()
			{
				if (matched) return;

				using expected_t = std::remove_cvref_t<typename meta::function_traits<Fn>::template arg_at<0>>;

				if (auto v = try_unwrap<expected_t>())
				{
					funcs(*v);
					matched = true;
				}
			}()), ...);

		kw_assert_msg(matched, "wasnt able to match");
	}

	template<typename T, typename...Args>
	T& refresh(Args&&...args)
	{
		release();

		auto v = new T(std::forward<Args>(args)...);
		_storage = v;
		_vtable.refresh<T>();

		return *v;
	}

	template<typename T>
	T& unwrap()	noexcept
	{
		kw_assert(_storage);
		kw_assert(_vtable.type_info.is<T>());
		return *reinterpret_cast<T*>(_storage);
	}

	template<typename T>
	T* try_unwrap()	noexcept
	{
		kw_assert(_storage);

		if (_vtable.type_info.is<T>())
		{
			return reinterpret_cast<T*>(_storage);
		}
		else
		{
			return nullptr;
		}
	}

	template<typename T>
	bool is() noexcept { return _vtable.type_info.is<T>(); }

private:
	void* _storage = nullptr;
	lifetime_vtable _vtable;
};

}
#endif // !KAWA_ANY
