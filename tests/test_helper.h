#pragma once

#include <iostream>
#include <string>

static int pt_pass_count = 0;
static int pt_fail_count = 0;

#define PT_ASSERT_EQ(a, b) do { \
	auto _a = (a); auto _b = (b); \
	if (_a != _b) { \
		std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
		          << ": " #a " != " #b "\n" \
		          << "  got:      [" << _a << "]\n" \
		          << "  expected: [" << _b << "]\n"; \
		pt_fail_count++; \
	} else { \
		pt_pass_count++; \
	} \
} while (0)

#define PT_ASSERT_TRUE(expr) do { \
	if (!(expr)) { \
		std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
		          << ": " #expr " was false\n"; \
		pt_fail_count++; \
	} else { \
		pt_pass_count++; \
	} \
} while (0)

#define PT_FAIL(msg) do { \
	std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
	          << ": " << (msg) << "\n"; \
	pt_fail_count++; \
} while (0)

#define PT_RUN_TEST(fn) do { \
	std::cout << "  " #fn " ... "; \
	std::cout.flush(); \
	fn(); \
	std::cout << "ok\n"; \
} while (0)

static inline int pt_test_summary()
{
	std::cout << "\nResults: " << pt_pass_count << " passed, "
	          << pt_fail_count << " failed\n";
	return pt_fail_count ? 1 : 0;
}
