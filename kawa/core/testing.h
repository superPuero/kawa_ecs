#ifndef KAWA_TESTING
#define KAWA_TESTING

#include "core_types.h"


namespace kawa
{
	template<typename...Args>
	struct plus_invoker
	{
		plus_invoker(Args&...args)
			: _args(args...)
		{
		}

		std::tuple<Args&...> _args;

		template<typename T>
		plus_invoker& operator+(T&& fn)
		{
			std::apply(std::forward<T>(fn), _args);
			return *this;
		}
	};

	template<typename...Args>
	plus_invoker(Args&...) -> plus_invoker<Args&...>;

	struct test_entry
	{
		string name;
		string fail_expr;
		bool result = true;
	};

	struct test_group
	{
		test_group(const string& n)
			: name(n)
		{
		}

		test_entry& make_entry(const string& name)
		{
			return entries.emplace_back(name);
		}

		void summary()
		{
			usize passed = 0;
			kw_println_colored(kw_ansi_color_cyan, kw_ansi_background_color_default, "#### <{}> test group summary ####", name);
			kw_println("");

			for (auto& e : entries)
			{
				if (e.result)
				{
					kw_print("[ {} ] ", e.name);
					kw_println_colored(kw_ansi_color_green, kw_ansi_background_color_default, "{}", "success");
					passed++;
				}
				else
				{
					kw_print("[ {} ] ", e.name);
					kw_print_colored(kw_ansi_color_red, kw_ansi_background_color_default, "{}", "fail");
					kw_println(" at [ {} ]", e.fail_expr);
				}
			}

			kw_println("");
			kw_println_colored(kw_ansi_color_cyan, kw_ansi_background_color_default, "### {}/{} ({}%) passsed ###", passed, entries.size(), ((f32)passed / (f32)entries.size()) * 100);
		}

		string name;
		dyn_array<test_entry> entries;
	};

	struct test_manager
	{
		static test_manager* instance()
		{
			if (!_instance)
			{
				_instance = new test_manager();
			}

			return _instance;
		}

		void summary()
		{
			kw_println_colored(kw_ansi_color_cyan, kw_ansi_background_color_default, "{} ", "#### test summary ####");
			kw_println("");

			for (auto& g : groups)
			{
				g.summary();
			}
		}

		test_group& make_group(const string& name)
		{
			return groups.emplace_back(name);
		}

		dyn_array<test_group> groups;
		static inline test_manager* _instance = nullptr;
	};
}

#define kw_tests_start_group(group_name) ::kawa::plus_invoker(::kawa::test_manager::instance()->make_group(#group_name))+[&](::kawa::test_group& _kw_test_group)

#define kw_test(test_name) ::kawa::plus_invoker(_kw_test_group.make_entry(#test_name))+[&](::kawa::test_entry& _kw_test_entry)

#define kw_test_require(expr)\
	if(_kw_test_entry.result)\
	{\
		_kw_test_entry.result = (expr);\
		_kw_test_entry.fail_expr = #expr;\
	}

#define kw_tests_print_summary() ::kawa::test_manager::instance()->summary()
#define kw_tests_rests() ::kawa::test_manager::instance()->groups.clear();


#endif // !KAWA_TESTING
