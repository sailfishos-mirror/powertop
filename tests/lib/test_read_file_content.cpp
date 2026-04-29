/*
 * Tests for read_file_content() in src/lib.cpp
 *
 * Covers:
 *   - normal mode: existing file, non-existent file
 *   - recording mode: existing file, non-existent file
 *   - replay mode: file-found, file-not-found
 */
#include <cstdio>
#include <unistd.h>

#include "lib.h"
#include "test_framework.h"
#include "../test_helper.h"

/* Stub required because lib.cpp declares it extern (defined in main.cpp) */
void (*ui_notify_user)(const std::string &) = nullptr;

static const std::string DATA_DIR = TEST_DATA_DIR;

/* ── normal mode ─────────────────────────────────────────────────────────── */

static void test_normal_existing_file()
{
	test_framework_manager::get().reset();
	std::string content = read_file_content(DATA_DIR + "/sample.txt");
	PT_ASSERT_EQ(content, "PowerTOP test reference file\nline 2\nline 3\n");
}

static void test_normal_nonexistent_file()
{
	test_framework_manager::get().reset();
	std::string content = read_file_content("/nonexistent/path/does/not/exist");
	PT_ASSERT_EQ(content, "");
}

/* ── recording mode ──────────────────────────────────────────────────────── */

static void test_recording_existing_file()
{
	test_framework_manager::get().reset();

	char tmpfile[] = "/tmp/pt_test_record_XXXXXX";
	int fd = mkstemp(tmpfile);
	close(fd);

	test_framework_manager::get().set_record(tmpfile);
	std::string content = read_file_content(DATA_DIR + "/sample.txt");
	PT_ASSERT_EQ(content, "PowerTOP test reference file\nline 2\nline 3\n");

	/* Save and verify round-trip via replay */
	test_framework_manager::get().save();
	test_framework_manager::get().reset();
	test_framework_manager::get().set_replay(tmpfile);

	std::string replayed = read_file_content(DATA_DIR + "/sample.txt");
	PT_ASSERT_EQ(replayed, "PowerTOP test reference file\nline 2\nline 3\n");

	test_framework_manager::get().reset();
	unlink(tmpfile);
}

static void test_recording_nonexistent_file()
{
	test_framework_manager::get().reset();

	char tmpfile[] = "/tmp/pt_test_record_XXXXXX";
	int fd = mkstemp(tmpfile);
	close(fd);

	test_framework_manager::get().set_record(tmpfile);
	std::string content = read_file_content("/nonexistent/path/does/not/exist");
	PT_ASSERT_EQ(content, "");

	/* Replay should reproduce the empty result (NOT_FOUND sentinel) */
	test_framework_manager::get().save();
	test_framework_manager::get().reset();
	test_framework_manager::get().set_replay(tmpfile);

	std::string replayed = read_file_content("/nonexistent/path/does/not/exist");
	PT_ASSERT_EQ(replayed, "");

	test_framework_manager::get().reset();
	unlink(tmpfile);
}

/* ── replay mode ─────────────────────────────────────────────────────────── */

static void test_replay_existing_file()
{
	test_framework_manager::get().reset();
	test_framework_manager::get().set_replay(DATA_DIR + "/replay_exists.ptrecord");

	std::string content = read_file_content("/test/replay/file");
	PT_ASSERT_EQ(content, "PowerTOP test replay content\n");

	test_framework_manager::get().reset();
}

static void test_replay_nonexistent_file()
{
	test_framework_manager::get().reset();
	test_framework_manager::get().set_replay(DATA_DIR + "/replay_notfound.ptrecord");

	std::string content = read_file_content("/test/replay/notfound");
	PT_ASSERT_EQ(content, "");

	test_framework_manager::get().reset();
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main()
{
	std::cout << "=== read_file_content tests ===\n";
	PT_RUN_TEST(test_normal_existing_file);
	PT_RUN_TEST(test_normal_nonexistent_file);
	PT_RUN_TEST(test_recording_existing_file);
	PT_RUN_TEST(test_recording_nonexistent_file);
	PT_RUN_TEST(test_replay_existing_file);
	PT_RUN_TEST(test_replay_nonexistent_file);
	return pt_test_summary();
}
