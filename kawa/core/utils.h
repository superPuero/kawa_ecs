#ifndef KAWA_UTILS
#define KAWA_UTILS

#include "macros.h"
#include "core_types.h"

#include <fstream>

namespace kawa
{     
    template <usize N>
    struct static_string
    {
        char value[N];

        consteval static_string(const char(&str)[N])
        {
            std::copy_n(str, N, value);
        }

        template<usize i>
        consteval bool operator==(const static_string<i>& other) const
        {
            for (usize i = 0; i < N; ++i)
                if (value[i] != other.value[i]) return false;
            return true;
        }
    };

    template<usize i>
    static_string(const char(&)[i]) -> static_string<i>;

	inline string read_file(const string& path)
	{
		std::ifstream file(path);

		kw_verify_msg(file.is_open(), "failed to open file: {}", path);

		std::ostringstream buffer;
		buffer << file.rdbuf();  

		return buffer.str();
	}

    inline dyn_array<u8> read_file_bytes(const string& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);

        kw_verify_msg(file.is_open(), "failed to open file: {}", path);

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        dyn_array<u8> buffer(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
        {
            kw_panic_msg("failed to read file: {}", path);
        }

        return buffer;
    }

    template <typename T, usize N>
    constexpr usize array_size(const T(&)[N]) noexcept { return N; }

    template <typename T>
    usize dyn_array_byte_size(const dyn_array<T>& arr) noexcept { return arr.size() * sizeof(T); }
}
#endif // !KAWA_UTILS
