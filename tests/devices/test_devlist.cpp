#include <string>
#include <iostream>

#include "devlist.h"
#include "devices/device.h"
#include "test_framework.h"
#include "../test_helper.h"

static const std::string DATA_DIR = TEST_DATA_DIR;

static void begin_replay(const std::string &fixture)
{
	test_framework_manager::get().reset();
	test_framework_manager::get().set_replay(fixture);
}

static void reset_replay()
{
	test_framework_manager::get().reset();
}

/* ── clean_open_devices on empty state ──────────────────────────── */

static void test_clean_open_devices_empty()
{
	clean_open_devices();
	/* no crash = pass */
}

/* ── collect_open_devices fills device list ─────────────────────── */

static void test_collect_open_devices_basic()
{
	clean_open_devices();
	begin_replay(DATA_DIR + "/devlist_proc.ptrecord");

	collect_open_devices();

	/*
	 * Verify collection via charge_device_to_openers:
	 * pid 123 has /dev/sda open; pid 456 has only /dev/null (excluded).
	 * Both one[] and two[] are searched; after the first call the entry
	 * is in whichever vector phase selected.
	 */
	device dev;
	int openers = charge_device_to_openers("sda", 10.0, &dev);
	PT_ASSERT_EQ(openers, 1);

	clean_open_devices();
	reset_replay();
}

/* ── excluded device paths are not collected ────────────────────── */

static void test_collect_open_devices_excluded_paths()
{
	clean_open_devices();

	/* Fixture: pid 99 has only excluded /dev paths */
	begin_replay(DATA_DIR + "/devlist_excluded.ptrecord");
	collect_open_devices();

	device dev;
	int openers = charge_device_to_openers("dev", 1.0, &dev);
	PT_ASSERT_EQ(openers, 0);

	clean_open_devices();
	reset_replay();
}

/* ── charge_device returns 0 when no matching device ────────────── */

static void test_charge_device_no_match()
{
	clean_open_devices();
	begin_replay(DATA_DIR + "/devlist_proc.ptrecord");

	collect_open_devices();

	device dev;
	int openers = charge_device_to_openers("nvme", 5.0, &dev);
	PT_ASSERT_EQ(openers, 0);

	clean_open_devices();
	reset_replay();
}

/* ── register_devpower + run_devpower_list hides device with openers */

static void test_register_and_run_devpower_hides_device()
{
	clean_open_devices();
	begin_replay(DATA_DIR + "/devlist_proc.ptrecord");
	collect_open_devices();

	device dev;
	dev.hide = false;
	register_devpower("sda", 10.0, &dev);
	run_devpower_list();

	/* /dev/sda is open → hide=true */
	PT_ASSERT_TRUE(dev.hide);

	clear_devpower();
	clean_open_devices();
	reset_replay();
}

/* ── register_devpower + run_devpower_list leaves device visible ── */

static void test_register_and_run_devpower_no_openers()
{
	clean_open_devices();
	begin_replay(DATA_DIR + "/devlist_proc.ptrecord");
	collect_open_devices();

	device dev;
	dev.hide = true;
	register_devpower("nvme", 5.0, &dev);
	run_devpower_list();

	/* nvme has no openers → hide=false */
	PT_ASSERT_TRUE(!dev.hide);

	clear_devpower();
	clean_open_devices();
	reset_replay();
}

int main()
{
	std::cout << "=== devlist tests ===\n";
	PT_RUN_TEST(test_clean_open_devices_empty);
	PT_RUN_TEST(test_collect_open_devices_basic);
	PT_RUN_TEST(test_collect_open_devices_excluded_paths);
	PT_RUN_TEST(test_charge_device_no_match);
	PT_RUN_TEST(test_register_and_run_devpower_hides_device);
	PT_RUN_TEST(test_register_and_run_devpower_no_openers);
	return pt_test_summary();
}
