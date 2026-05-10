/*
 * Tests for devices/xe-gpu.cpp  (xegpu class)
 *
 * Exercises: constructor with no hwmon, constructor with one energy channel,
 * start/end measurement (power calculation), zero-delta guard,
 * power_usage(), and collect_json_fields().
 *
 * create_xe_gpu() and xe_fan_device are not tested here
 * (both require libtracefs probing).
 */

#include <string>
#include <cstdint>
#include <iostream>

#include "devices/xe-gpu.h"
#include "parameters/parameters.h"
#include "test_framework.h"
#include "../test_helper.h"

/* Stub: required by device.cpp */
void (*ui_notify_user)(const std::string &) = nullptr;

/* ── no hwmon: constructor finds nothing ────────────────────────── */

static void test_xegpu_no_hwmon()
{
	test_framework_manager::get().set_replay(
		TEST_DATA_DIR "/xe_gpu_no_hwmon.ptrecord");

	xegpu gpu;

	test_framework_manager::get().reset();

	PT_ASSERT_TRUE(gpu.power_channels.empty());
}

/* ── one channel, TDP cap only (no energy counter) ─────────────── */

static void test_xegpu_no_energy_channel()
{
	test_framework_manager::get().set_replay(
		TEST_DATA_DIR "/xe_gpu_no_energy_ch.ptrecord");

	xegpu gpu;

	test_framework_manager::get().reset();

	PT_ASSERT_TRUE(gpu.power_channels.size() == 1);
	PT_ASSERT_TRUE(gpu.power_channels[0].label == "pkg");
	/* No energy path → current_watts stays -1.0 */
	PT_ASSERT_TRUE(gpu.power_channels[0].current_watts < -0.5);
	/* power*_crit present → tdp_cap_watts == 45.0 */
	PT_ASSERT_TRUE(gpu.power_channels[0].tdp_cap_watts > 44.9 &&
		       gpu.power_channels[0].tdp_cap_watts < 45.1);
}

/* ── start + end measurement: 0.5 W computed ───────────────────── */

static void test_xegpu_measurement()
{
	test_framework_manager::get().set_replay(
		TEST_DATA_DIR "/xe_gpu_measurement.ptrecord");

	xegpu gpu;
	gpu.start_measurement();
	gpu.end_measurement();

	test_framework_manager::get().reset();

	/* energy delta = 500,000 µJ over 1 s → 0.5 W */
	PT_ASSERT_TRUE(gpu.power_channels[0].current_watts > 0.49 &&
		       gpu.power_channels[0].current_watts < 0.51);
}

/* ── zero time-delta: current_watts stays -1.0 ──────────────────── */

static void test_xegpu_zero_delta()
{
	test_framework_manager::get().set_replay(
		TEST_DATA_DIR "/xe_gpu_zero_delta.ptrecord");

	xegpu gpu;
	gpu.start_measurement();
	gpu.end_measurement();

	test_framework_manager::get().reset();

	PT_ASSERT_TRUE(gpu.power_channels[0].current_watts < -0.5);
}

/* ── collect_json_fields ────────────────────────────────────────── */

static void test_xegpu_collect_json_fields()
{
	test_framework_manager::get().set_replay(
		TEST_DATA_DIR "/xe_gpu_measurement.ptrecord");

	xegpu gpu;
	gpu.start_measurement();
	gpu.end_measurement();

	test_framework_manager::get().reset();

	std::string js = gpu.serialize();

	PT_ASSERT_TRUE(js.find("\"index\":") != std::string::npos);
	PT_ASSERT_TRUE(js.find("\"rindex\":") != std::string::npos);
	PT_ASSERT_TRUE(js.find("\"consumed_power\":") != std::string::npos);
	PT_ASSERT_TRUE(js.find("\"channel0_label\":") != std::string::npos);
	PT_ASSERT_TRUE(js.find("\"channel0_watts\":") != std::string::npos);
	PT_ASSERT_TRUE(js.find("\"channel0_tdp\":") != std::string::npos);
}

int main()
{
	std::cout << "=== xegpu device tests ===\n";
	PT_RUN_TEST(test_xegpu_no_hwmon);
	PT_RUN_TEST(test_xegpu_no_energy_channel);
	PT_RUN_TEST(test_xegpu_measurement);
	PT_RUN_TEST(test_xegpu_zero_delta);
	PT_RUN_TEST(test_xegpu_collect_json_fields);
	return pt_test_summary();
}
