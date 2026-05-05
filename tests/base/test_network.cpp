/*
 * Copyright 2025, Intel Corporation
 *
 * This file is part of PowerTOP
 *
 * This program file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program in a file named COPYING; if not, write to the
 * Free Software Foundation, Inc,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 * or just google for it.
 */

/*
 * Unit tests for network::start_measurement() and network::end_measurement().
 *
 * A fake_net subclass overrides get_iface_up() and get_iface_speed() so
 * tests run without real network hardware.
 *
 * /proc/net/dev is read via the test framework (read_file_content); since the
 * fake NIC is not registered in the nics map, pkts stays 0 throughout, which
 * is fine — we are testing the virtual dispatch, not packet counting.
 */

#include <iostream>
#include <string>
#include <vector>

#include "devices/network.h"
#include "devices/device.h"
#include "parameters/parameters.h"
#include "test_framework.h"
#include "../test_helper.h"

/* Stub for get_wifi_power_saving() — always reports power saving active */
extern "C" int get_wifi_power_saving(const char *) { return 1; }
/* Stubs needed by parameters.cpp */
double global_power() { return 0.0; }
void save_all_results(const std::string &) {}

/* ------------------------------------------------------------------ */
/* fake_net: controllable stand-in for the real network device         */
/* ------------------------------------------------------------------ */

class fake_net : public network {
public:
	int fake_up    = 1;
	int fake_speed = 0;

	int get_iface_up_calls    = 0;
	int get_iface_speed_calls = 0;

	explicit fake_net(const std::string &name)
		: network(name, "/sys/class/net/" + name) {}

	int get_iface_up() override
	{
		get_iface_up_calls++;
		return fake_up;
	}

	int get_iface_speed() override
	{
		get_iface_speed_calls++;
		return fake_speed;
	}
};

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

static void test_start_calls_virtual_helpers()
{
	fake_net dev("testnic");

	dev.start_measurement();

	PT_ASSERT_EQ(dev.get_iface_up_calls,    1);
	PT_ASSERT_EQ(dev.get_iface_speed_calls, 1);
}

static void test_end_calls_virtual_helpers()
{
	fake_net dev("testnic");

	dev.start_measurement();
	dev.end_measurement();

	/* start + end = 2 calls each */
	PT_ASSERT_EQ(dev.get_iface_up_calls,    2);
	PT_ASSERT_EQ(dev.get_iface_speed_calls, 2);
}

static void test_utilization_non_negative()
{
	fake_net dev("testnic");

	dev.start_measurement();
	dev.end_measurement();

	/* pkts unchanged (not in nics map), so utilization == 0 */
	PT_ASSERT_TRUE(dev.utilization() >= 0.0);
}

static void test_interface_down_reported()
{
	fake_net dev("testnic");
	dev.fake_up = 0;

	dev.start_measurement();
	dev.end_measurement();

	std::string js = dev.serialize();
	/* start_up and end_up should both be 0 in the JSON */
	PT_ASSERT_TRUE(js.find("\"start_up\":0") != std::string::npos);
	PT_ASSERT_TRUE(js.find("\"end_up\":0")   != std::string::npos);
}

static void test_speed_recorded()
{
	fake_net dev("testnic");
	dev.fake_speed = 1000;

	dev.start_measurement();
	dev.end_measurement();

	std::string js = dev.serialize();
	PT_ASSERT_TRUE(js.find("\"start_speed\":1000") != std::string::npos);
	PT_ASSERT_TRUE(js.find("\"end_speed\":1000")   != std::string::npos);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main()
{
	std::cout << "network tests:\n";

	PT_RUN_TEST(test_start_calls_virtual_helpers);
	PT_RUN_TEST(test_end_calls_virtual_helpers);
	PT_RUN_TEST(test_utilization_non_negative);
	PT_RUN_TEST(test_interface_down_reported);
	PT_RUN_TEST(test_speed_recorded);

	return pt_test_summary();
}
