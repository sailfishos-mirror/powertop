/*
 * Tests for add_wifi_tunables() — exercises the list_directory path in wifi.cpp.
 *
 * Fixture supplies a directory listing for /sys/class/net/ containing:
 *   wlan0   (matches "wlan" prefix → added)
 *   eth0    (no wireless prefix → skipped)
 *   wlp2s0  (matches "wlp" prefix → added)
 *
 * After add_wifi_tunables() we expect exactly 2 new tunables whose
 * descriptions mention the interface names.
 */
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include "tuning/wifi.h"
#include "tuning/tunable.h"
#include "test_framework.h"
#include "../test_helper.h"

static const std::string DATA_DIR = TEST_DATA_DIR;

static void test_add_wifi_tunables_filters_by_prefix()
{
	auto &tf = test_framework_manager::get();
	tf.reset();
	tf.set_replay(DATA_DIR + "/wifi_net_dir.ptrecord");

	size_t before = all_tunables.size();

	add_wifi_tunables();

	tf.reset();

	size_t added = all_tunables.size() - before;

	/* Two wireless interfaces found, one ethernet skipped */
	PT_ASSERT_EQ((int)added, 2);

	/* Verify descriptions mention the right interfaces */
	bool found_wlan0 = false, found_wlp2s0 = false;
	for (size_t i = before; i < all_tunables.size(); i++) {
		std::string d = all_tunables[i]->desc;
		if (d.find("wlan0")  != std::string::npos) found_wlan0  = true;
		if (d.find("wlp2s0") != std::string::npos) found_wlp2s0 = true;
	}
	PT_ASSERT_TRUE(found_wlan0);
	PT_ASSERT_TRUE(found_wlp2s0);

	/* Clean up global state */
	for (size_t i = before; i < all_tunables.size(); i++)
		delete all_tunables[i];
	all_tunables.resize(before);
}

static void test_add_wifi_tunables_empty_dir()
{
	auto &tf = test_framework_manager::get();
	tf.reset();
	tf.set_replay(DATA_DIR + "/wifi_net_dir_empty.ptrecord");

	size_t before = all_tunables.size();

	add_wifi_tunables();

	tf.reset();

	/* No entries → nothing added */
	PT_ASSERT_EQ((int)(all_tunables.size() - before), 0);
}

int main()
{
	std::cout << "=== wifi tunables tests ===\n";
	PT_RUN_TEST(test_add_wifi_tunables_filters_by_prefix);
	PT_RUN_TEST(test_add_wifi_tunables_empty_dir);
	return pt_test_summary();
}
