#ifndef KAWA_PROFILING
#define KAWA_PROFILING

#include <unordered_map>
#include <source_location>
#include <chrono>
#include <mutex>
#include <thread>

#include "math.h"
#include "core_types.h"

#define KAWA_PROFILE(name)\
kawa::profiler_map::instance().mutex.lock();\
auto _scoped_timer = kawa::profiler_map::instance().create_scoped_entry(name);\
kawa::profiler_map::instance().mutex.unlock()

#define KAWA_PROFILE_AUTO() KAWA_PROFILE(string(std::source_location::current().function_name()))

#define KAWA_PROFILER() kawa::profiler_map::instance()

namespace kawa
{
    struct profiler_map
    { 
        struct scoped_entry
        { 
            scoped_entry(profiler_map& map, const string& name)
                : _entry(map.map[std::this_thread::get_id()][name])
                , _start(std::chrono::high_resolution_clock::now())
            {
            }

            ~scoped_entry()
            {
                _entry.first = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - _start).count() / 1000.f;
                _entry.second++;
            }
            
            std::pair<f32, u32>& _entry;
            std::chrono::time_point<std::chrono::high_resolution_clock> _start;
        };

        scoped_entry create_scoped_entry(const string& name)
        {
            return scoped_entry(*this, name);
        }

        static inline profiler_map& instance() noexcept
        {
            static profiler_map pm;
            return pm;
        }

        std::mutex mutex;
        umap<std::thread::id, umap<string, std::pair<f32, u32>>> map;
    };
   
}



#endif // !KAWA_PROFILING
