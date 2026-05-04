/*
 * Tests for power_consumer base class: serialize(), Witts(), usage(),
 * usage_units().
 *
 * get_parameter_value() is stubbed to return 0.0, so Witts() cost is
 * driven entirely by power_charge / measurement_time.
 */
#include <cmath>
#include <string>
#include "process/powerconsumer.h"
#include "../test_helper.h"

double measurement_time = 1.0;

static void test_power_consumer_defaults()
{
	power_consumer pc;

	std::string got = pc.serialize();

	/* base class name()/type() return translated "abstract" */
	PT_ASSERT_TRUE(got.find("\"accumulated_runtime\":0") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"child_runtime\":0") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"wake_ups\":0") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"gpu_ops\":0") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"disk_hits\":0") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"hard_disk_hits\":0") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"xwakes\":0") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"power_charge\":0") != std::string::npos);
}

static void test_power_consumer_fields()
{
	power_consumer pc;
	pc.accumulated_runtime = 500000;
	pc.child_runtime       = 100000;
	pc.wake_ups            = 42;
	pc.gpu_ops             = 7;
	pc.disk_hits           = 3;
	pc.hard_disk_hits      = 1;
	pc.xwakes              = 2;
	pc.power_charge        = 1.5;

	std::string got = pc.serialize();

	PT_ASSERT_TRUE(got.find("\"accumulated_runtime\":500000") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"child_runtime\":100000") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"wake_ups\":42") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"gpu_ops\":7") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"disk_hits\":3") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"hard_disk_hits\":1") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"xwakes\":2") != std::string::npos);
	PT_ASSERT_TRUE(got.find("\"power_charge\":1.5") != std::string::npos);
}

/* ── Witts() ─────────────────────────────────────────────────────────────── */

static void test_witts_zero_measurement_time()
{
	power_consumer pc;
	measurement_time = 0.000001;   /* < 0.00001 threshold */
	double w = pc.Witts();
	measurement_time = 1.0;
	PT_ASSERT_EQ(w, 0.0);
}

static void test_witts_power_charge()
{
	/* With all parameter stubs returning 0.0, cost = power_charge */
	power_consumer pc;
	pc.power_charge = 5.0;
	measurement_time = 1.0;
	double w = pc.Witts();
	PT_ASSERT_TRUE(std::fabs(w - 5.0) < 1e-9);
}

static void test_witts_child_runtime_clamp()
{
	/* child_runtime > accumulated_runtime is clamped to 0 — no negative cost */
	power_consumer pc;
	pc.accumulated_runtime = 100;
	pc.child_runtime       = 999;   /* bogus — exceeds accumulated */
	measurement_time = 1.0;
	/* Should not crash and must not return a large negative value */
	double w = pc.Witts();
	PT_ASSERT_TRUE(w >= 0.0);
}

/* ── usage() ─────────────────────────────────────────────────────────────── */

static void test_usage_zero_measurement_time()
{
	power_consumer pc;
	measurement_time = 0.0;
	double u = pc.usage();
	measurement_time = 1.0;
	PT_ASSERT_EQ(u, 0.0);
}

static void test_usage_ms_range()
{
	/* t = 1000000 ns / 1000000 / 1.0 s = 1.0  ≥ 0.7  → ms/s path */
	power_consumer pc;
	pc.accumulated_runtime = 1000000;
	pc.child_runtime       = 0;
	measurement_time = 1.0;
	double u = pc.usage();
	PT_ASSERT_TRUE(std::fabs(u - 1.0) < 1e-9);
}

static void test_usage_us_range()
{
	/* t = 100000 / 1000000 / 1.0 = 0.1  < 0.7  → multiply × 1000 → 100 */
	power_consumer pc;
	pc.accumulated_runtime = 100000;
	pc.child_runtime       = 0;
	measurement_time = 1.0;
	double u = pc.usage();
	PT_ASSERT_TRUE(std::fabs(u - 100.0) < 1e-9);
}

/* ── usage_units() ───────────────────────────────────────────────────────── */

static void test_usage_units_zero_measurement_time()
{
	power_consumer pc;
	measurement_time = 0.0;
	std::string u = pc.usage_units();
	measurement_time = 1.0;
	PT_ASSERT_EQ(u, std::string(""));
}

static void test_usage_units_ms_range()
{
	power_consumer pc;
	pc.accumulated_runtime = 1000000;   /* t = 1.0 ≥ 0.7 → ms/s */
	measurement_time = 1.0;
	std::string u = pc.usage_units();
	PT_ASSERT_EQ(u, std::string(" ms/s"));
}

static void test_usage_units_us_range()
{
	power_consumer pc;
	pc.accumulated_runtime = 100000;   /* t = 0.1 < 0.7 → µs/s or us/s */
	measurement_time = 1.0;
	std::string u = pc.usage_units();
	PT_ASSERT_TRUE(u.find("s/s") != std::string::npos);   /* µs/s or us/s */
}

int main()
{
	std::cout << "=== power_consumer base class tests ===\n";
	PT_RUN_TEST(test_power_consumer_defaults);
	PT_RUN_TEST(test_power_consumer_fields);
	PT_RUN_TEST(test_witts_zero_measurement_time);
	PT_RUN_TEST(test_witts_power_charge);
	PT_RUN_TEST(test_witts_child_runtime_clamp);
	PT_RUN_TEST(test_usage_zero_measurement_time);
	PT_RUN_TEST(test_usage_ms_range);
	PT_RUN_TEST(test_usage_us_range);
	PT_RUN_TEST(test_usage_units_zero_measurement_time);
	PT_RUN_TEST(test_usage_units_ms_range);
	PT_RUN_TEST(test_usage_units_us_range);
	return pt_test_summary();
}
