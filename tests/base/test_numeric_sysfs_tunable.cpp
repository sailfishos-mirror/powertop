/*
 * Tests for numeric_sysfs_tunable: good_bad() with >= and <= comparisons.
 *
 * Uses /proc/sys/vm/dirty_ratio (HIB, target=50) as the concrete example:
 *   value=20 → Bad  (below target — the original string-match bug)
 *   value=50 → Good (exact match)
 *   value=60 → Good (exceeds target — would have been Bad with exact string match)
 *
 * Lower-is-better is verified with a synthetic LIB tunable (target=30):
 *   value=20 → Good (below target)
 *   value=40 → Bad  (above target)
 *
 * State assertions use serialize() so all internal fields are visible via JSON
 * without needing a test subclass to expose protected members.
 *
 * Toggle is verified via the write_log: toggle() on a bad tunable must write
 * the target value string to the sysfs path.
 */
#include <string>
#include "tuning/tuningsysfs.h"
#include "test_framework.h"
#include "../test_helper.h"

/* Stub: defined in main.cpp, not linked here */
void (*ui_notify_user)(const std::string &) = nullptr;

static const std::string DATA_DIR = TEST_DATA_DIR;
static const std::string PATH = "/proc/sys/vm/dirty_ratio";

/* ── HIB tests (higher_is_better=true, target=50) ─────────────────────── */

static void test_hib_below_target()
{
	test_framework_manager::get().reset();
	test_framework_manager::get().set_replay(DATA_DIR + "/numeric_tunable_20.ptrecord");

	numeric_sysfs_tunable t("VM dirty ratio", PATH, 50.0);
	std::string json = t.serialize();

	PT_ASSERT_TRUE(json.find("\"result\":\"Bad\"") != std::string::npos);
	PT_ASSERT_TRUE(json.find("\"bad_value\":\"20\"") != std::string::npos);
	PT_ASSERT_TRUE(json.find("\"target_double\":50") != std::string::npos);
	PT_ASSERT_TRUE(json.find("\"higher_is_better\":true") != std::string::npos);

	test_framework_manager::get().reset();
}

static void test_hib_exactly_at_target()
{
	test_framework_manager::get().reset();
	test_framework_manager::get().set_replay(DATA_DIR + "/numeric_tunable_50.ptrecord");

	numeric_sysfs_tunable t("VM dirty ratio", PATH, 50.0);
	std::string json = t.serialize();

	PT_ASSERT_TRUE(json.find("\"result\":\"Good\"") != std::string::npos);
	PT_ASSERT_TRUE(json.find("\"bad_value\":\"\"") != std::string::npos);

	test_framework_manager::get().reset();
}

static void test_hib_above_target()
{
	/* Key case: value=60 exceeds target=50 — must be Good, not Bad */
	test_framework_manager::get().reset();
	test_framework_manager::get().set_replay(DATA_DIR + "/numeric_tunable_60.ptrecord");

	numeric_sysfs_tunable t("VM dirty ratio", PATH, 50.0);
	std::string json = t.serialize();

	PT_ASSERT_TRUE(json.find("\"result\":\"Good\"") != std::string::npos);
	PT_ASSERT_TRUE(json.find("\"bad_value\":\"\"") != std::string::npos);

	test_framework_manager::get().reset();
}

/* ── LIB tests (higher_is_better=false, target=30) ────────────────────── */

static void test_lib_below_target()
{
	test_framework_manager::get().reset();
	test_framework_manager::get().set_replay(DATA_DIR + "/numeric_tunable_20.ptrecord");

	numeric_sysfs_tunable t("LIB tunable", PATH, 30.0, false);
	std::string json = t.serialize();

	PT_ASSERT_TRUE(json.find("\"result\":\"Good\"") != std::string::npos);
	PT_ASSERT_TRUE(json.find("\"higher_is_better\":false") != std::string::npos);

	test_framework_manager::get().reset();
}

static void test_lib_above_target()
{
	test_framework_manager::get().reset();
	test_framework_manager::get().set_replay(DATA_DIR + "/numeric_tunable_40.ptrecord");

	numeric_sysfs_tunable t("LIB tunable", PATH, 30.0, false);
	std::string json = t.serialize();

	PT_ASSERT_TRUE(json.find("\"result\":\"Bad\"") != std::string::npos);
	PT_ASSERT_TRUE(json.find("\"bad_value\":\"40\"") != std::string::npos);

	test_framework_manager::get().reset();
}

/* ── Toggle test ───────────────────────────────────────────────────────── */

static void test_toggle_from_bad_writes_target()
{
	/* value=20 is bad (< 50); toggle() should write "50" to sysfs */
	test_framework_manager::get().reset();
	test_framework_manager::get().set_replay(DATA_DIR + "/numeric_tunable_20.ptrecord");

	numeric_sysfs_tunable t("VM dirty ratio", PATH, 50.0);
	t.toggle();

	auto& log = test_framework_manager::get().get_write_log();
	PT_ASSERT_EQ((int)log.size(), 1);
	PT_ASSERT_EQ(log[0].first,  PATH);
	PT_ASSERT_EQ(log[0].second, std::string("50"));

	test_framework_manager::get().reset();
}

int main()
{
	std::cout << "=== numeric_sysfs_tunable tests ===\n";
	PT_RUN_TEST(test_hib_below_target);
	PT_RUN_TEST(test_hib_exactly_at_target);
	PT_RUN_TEST(test_hib_above_target);
	PT_RUN_TEST(test_lib_below_target);
	PT_RUN_TEST(test_lib_above_target);
	PT_RUN_TEST(test_toggle_from_bad_writes_target);
	return pt_test_summary();
}
