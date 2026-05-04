/*
 * Tests for cpu_linux cstate formatting methods (src/cpu/cpu_linux.cpp).
 *
 * Covers the time_factor zero-guard added to fill_cstate_line() and
 * fill_cstate_percentage(): when time_factor is 0.0 (default before the
 * first measurement_end()) both methods must return "" rather than
 * producing NaN or Inf via 0.0/0.0 or x/0.0.
 *
 * time_factor is protected in abstract_cpu, so it is exposed via a thin
 * test subclass.
 */
#include <iostream>
#include <string>

#include "cpu/cpu.h"
#include "cpu/frequency.h"
#include "../test_helper.h"

class test_cpu_linux : public cpu_linux {
public:
	explicit test_cpu_linux(int cpu_nr = 0) { number = cpu_nr; }
	void set_time_factor(double f) { time_factor = f; }
};

/* ── fill_cstate_line ─────────────────────────────────────────────────────── */

static void test_cpu_linux_cstate_header()
{
	test_cpu_linux cpu(2);
	std::string s = cpu.fill_cstate_line(LEVEL_HEADER);
	/* Header should mention the CPU number */
	PT_ASSERT_TRUE(s.find("2") != std::string::npos);
}

static void test_cpu_linux_cstate_line_c0_normal()
{
	test_cpu_linux cpu;
	/* linux_name "active" triggers line_level = LEVEL_C0 in insert_cstate */
	cpu.insert_cstate("active", "active", 0, 0, 0);
	cpu.cstates[0]->duration_delta = 750;
	cpu.set_time_factor(1000.0);   /* 75% */

	std::string s = cpu.fill_cstate_line(LEVEL_C0);
	PT_ASSERT_EQ(s, std::string(" 75.0%"));
}

static void test_cpu_linux_cstate_line_c0_zero_time_factor()
{
	test_cpu_linux cpu;
	cpu.insert_cstate("active", "active", 0, 0, 0);
	cpu.cstates[0]->duration_delta = 0;
	cpu.set_time_factor(0.0);

	std::string s = cpu.fill_cstate_line(LEVEL_C0);
	PT_ASSERT_EQ(s, std::string(""));
	PT_ASSERT_TRUE(s.find("nan") == std::string::npos);
	PT_ASSERT_TRUE(s.find("inf") == std::string::npos);
}

static void test_cpu_linux_cstate_line_cx_zero_time_factor()
{
	test_cpu_linux cpu;
	cpu.insert_cstate("C3", "C3", 0, 0, 1, 1);
	cpu.cstates[0]->duration_delta = 0;
	cpu.set_time_factor(0.0);

	std::string s = cpu.fill_cstate_line(1);
	PT_ASSERT_EQ(s, std::string(""));
	PT_ASSERT_TRUE(s.find("nan") == std::string::npos);
	PT_ASSERT_TRUE(s.find("inf") == std::string::npos);
}

/* ── fill_cstate_percentage ───────────────────────────────────────────────── */

static void test_cpu_linux_cstate_percentage_normal()
{
	test_cpu_linux cpu;
	cpu.insert_cstate("C6", "C6", 0, 0, 1, 2);
	cpu.cstates[0]->duration_delta = 300;
	cpu.set_time_factor(1000.0);   /* 30% */

	std::string s = cpu.fill_cstate_percentage(2);
	PT_ASSERT_EQ(s, std::string(" 30.0%"));
}

static void test_cpu_linux_cstate_percentage_zero_time_factor()
{
	test_cpu_linux cpu;
	cpu.insert_cstate("C6", "C6", 0, 0, 1, 2);
	cpu.cstates[0]->duration_delta = 0;
	cpu.set_time_factor(0.0);

	std::string s = cpu.fill_cstate_percentage(2);
	PT_ASSERT_EQ(s, std::string(""));
	PT_ASSERT_TRUE(s.find("nan") == std::string::npos);
	PT_ASSERT_TRUE(s.find("inf") == std::string::npos);
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main()
{
	std::cout << "=== cpu_linux: cstate formatting tests ===\n";
	PT_RUN_TEST(test_cpu_linux_cstate_header);
	PT_RUN_TEST(test_cpu_linux_cstate_line_c0_normal);
	PT_RUN_TEST(test_cpu_linux_cstate_line_c0_zero_time_factor);
	PT_RUN_TEST(test_cpu_linux_cstate_line_cx_zero_time_factor);
	PT_RUN_TEST(test_cpu_linux_cstate_percentage_normal);
	PT_RUN_TEST(test_cpu_linux_cstate_percentage_zero_time_factor);
	return pt_test_summary();
}
