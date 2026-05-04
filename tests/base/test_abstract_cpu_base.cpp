/*
 * Tests for abstract_cpu base class: serialize(), has_cstate_level(),
 * has_pstate_level(), total_pstate_time(), reset_pstate_data().
 *
 * abstract_cpu has no pure virtual methods so it can be instantiated
 * directly. collect_json_fields emits: type, number, first_cpu, idle,
 * has_intel_MSR, current_frequency, effective_frequency,
 * cstates[], pstates[], children[].
 */
#include <string>
#include "cpu/cpu.h"
#include "cpu/frequency.h"
#include "../test_helper.h"

static void test_abstract_cpu_defaults()
{
	abstract_cpu cpu;
	cpu.set_type("GenericCPU");
	cpu.number = 2;

	std::string got = cpu.serialize();

	PT_ASSERT_TRUE(got.find("\"type\":\"GenericCPU\"") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"number\":2") != std::string::npos);
	/* vectors empty → empty JSON arrays */
	PT_ASSERT_TRUE(got.find("\"cstates\":[]") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"pstates\":[]") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"children\":[]") != std::string::npos);
}

static void test_abstract_cpu_idle_flag()
{
	abstract_cpu cpu;
	cpu.set_type("IdleCPU");
	cpu.go_idle(0);

	std::string got = cpu.serialize();

	PT_ASSERT_TRUE(got.find("\"idle\":true") != std::string::npos);
}

/* ── has_cstate_level() ──────────────────────────────────────────────────── */

static void test_has_cstate_level_header()
{
	abstract_cpu cpu;
	PT_ASSERT_EQ(cpu.has_cstate_level(LEVEL_HEADER), 1);
}

static void test_has_cstate_level_miss()
{
	abstract_cpu cpu;
	PT_ASSERT_EQ(cpu.has_cstate_level(3), 0);
}

static void test_has_cstate_level_hit()
{
	abstract_cpu cpu;
	cpu.insert_cstate("C3", "C3", 0, 0, 1, 3);
	PT_ASSERT_EQ(cpu.has_cstate_level(3), 1);
}

/* ── has_pstate_level() ──────────────────────────────────────────────────── */

static void test_has_pstate_level_header()
{
	abstract_cpu cpu;
	PT_ASSERT_EQ(cpu.has_pstate_level(LEVEL_HEADER), 1);
}

static void test_has_pstate_level_miss()
{
	abstract_cpu cpu;   /* no pstates */
	PT_ASSERT_EQ(cpu.has_pstate_level(0), 0);
}

static void test_has_pstate_level_hit()
{
	abstract_cpu cpu;
	cpu.insert_pstate(1000000, "1.0 GHz", 0, 0);
	PT_ASSERT_EQ(cpu.has_pstate_level(0), 1);
}

/* ── total_pstate_time() / reset_pstate_data() ───────────────────────────── */

static void test_total_pstate_time()
{
	abstract_cpu cpu;
	cpu.insert_pstate(800000,  "800 MHz", 0, 0);
	cpu.insert_pstate(1200000, "1.2 GHz", 0, 0);
	cpu.pstates[0]->time_after = 300;
	cpu.pstates[1]->time_after = 700;
	PT_ASSERT_EQ(cpu.total_pstate_time(), 1000ULL);
}

static void test_reset_pstate_data()
{
	abstract_cpu cpu;
	cpu.insert_pstate(1000000, "1.0 GHz", 500, 10);
	cpu.pstates[0]->time_after = 999;
	cpu.reset_pstate_data();
	PT_ASSERT_EQ(cpu.pstates[0]->time_before, 0ULL);
	PT_ASSERT_EQ(cpu.pstates[0]->time_after,  0ULL);
}

int main()
{
	std::cout << "=== abstract_cpu base class tests ===\n";
	PT_RUN_TEST(test_abstract_cpu_defaults);
	PT_RUN_TEST(test_abstract_cpu_idle_flag);
	PT_RUN_TEST(test_has_cstate_level_header);
	PT_RUN_TEST(test_has_cstate_level_miss);
	PT_RUN_TEST(test_has_cstate_level_hit);
	PT_RUN_TEST(test_has_pstate_level_header);
	PT_RUN_TEST(test_has_pstate_level_miss);
	PT_RUN_TEST(test_has_pstate_level_hit);
	PT_RUN_TEST(test_total_pstate_time);
	PT_RUN_TEST(test_reset_pstate_data);
	return pt_test_summary();
}
