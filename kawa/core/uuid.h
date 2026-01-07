#ifndef KAWA_UUID
#define KAWA_UUID

#include <functional>
#include "macros.h"
#include "core_types.h"
#include "rng.h"

namespace kawa
{
	class uuid
	{
	public:
		inline uuid() noexcept : _val(0) {};

		inline uuid(u64 val) noexcept : _val(val) {};
			
		inline void nullify() noexcept
		{
			_val = 0;
		}

		inline void refresh() noexcept
		{
			_val = rng::randu64();
		}

		inline u64 val() const noexcept
		{
			return _val;
		}

		operator u64() const noexcept
		{
			return _val;
		}

	private:
		u64 _val;

	};
}

namespace std
{
	template<>
	struct hash<kawa::uuid>			
	{
		std::size_t operator()(const kawa::uuid& uuid) const noexcept
		{
			return uuid.val();
		}
	};
}

#endif // !KAWA_UUID
