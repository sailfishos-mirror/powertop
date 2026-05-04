This repository is for PowerTOP, a linux tool to find the sources of power
consumption in a system.

Do **NOT** update this file to record the results of code review or fixes
for code review comments!

The code review rules for this project are in `review/review.md` and this
includes a style guide in `review/style.md`.

The class hiearchy is documented in `review/class.md` and this document
needs to be kept uptodate as changes to the class hierarchy are made.

Also read `review/tools.md` and `tests/testdesign.md` into the context when
working on the testsuite.


# Global code change process

1. Collect pre-change code coverage on the test suite (or reuse from context)
2. Make the change
3. Build the change (including the test suite) -- requirement: no new warnings
4a. Run the test suite -- requirement: no failure
4b. Collect post-change code coverage on the test stuite
5. Review the change against the intent and the rultes in `review/review.md`
6. Make a git commit with a descriptive commit message 

# [[maybe_unused]] placement rule

`[[maybe_unused]]` must appear **before** the full parameter declaration,
not after a `*` or `&` qualifier or between type and name.

Correct:   `[[maybe_unused]] struct foo *name`
Correct:   `[[maybe_unused]] const std::string &name`
Correct:   `[[maybe_unused]] int name`
Wrong:     `struct foo *[[maybe_unused]] name`
Wrong:     `const std::string &[[maybe_unused]] name`
Wrong:     `int [[maybe_unused]] name`

GCC warnings triggered by the wrong placement:
- `'maybe_unused' on a type other than class or enumeration definition`
- `attribute ignored`
- `unused parameter`

# Git commit notes

Git commits in this repo must use `--no-gpg-sign` flag to avoid hanging due
to GPG agent unavailability:
```
git commit --no-gpg-sign -F commitmsg.txt
```
Write the commit message to a file in the project directory (NOT /tmp), then
remove it after committing. Use `git add -A` carefully — always check for
untracked files (like DEADJOE, temp files) that should NOT be committed; use
`git restore --staged <file>` to unstage them before committing.
The repo has a global `user.signingkey` set, but the GPG agent is not running
in the terminal session. Setting `commit.gpgsign=false` locally is not
sufficient — `--no-gpg-sign` on the command line is required.

# Derived class serialize tests (tests/base/ and tests/devices/)

Three additional derived classes now have snapshot tests in `tests/base/`:

- `test_process.cpp` — kernel_thread (N cmdline) and user_process (R cmdline).
  Pass `tid == pid` (non-zero) to skip `/proc/status` read; only one fixture
  record per scenario. Link: `process.cpp + powerconsumer.cpp +
  powerconsumer_stubs.cpp + lib.cpp + test_framework.cpp`.

- `test_usb_wakeup.cpp` — enabled and disabled wakeup states.
  Constructor reads nothing; `wakeup_value()` reads `usb_path` once per
  `serialize()` call (via `wakeup::collect_json_fields`). One fixture record
  per scenario. Link: `wakeup_usb.cpp + wakeup.cpp + lib.cpp + test_framework.cpp`.

- `test_sysfs_power_meter.cpp` — charging, discharging_direct, discharging_fallback.
  `end_measurement()` triggers `measure()` which reads: present, status,
  power_now, energy_now (or fallback voltage/current/charge). Fixture record
  order must exactly match the read sequence. Link: `sysfs.cpp +
  measurement.cpp + lib.cpp + test_framework.cpp`.

# collect_json_fields pattern

When adding `collect_json_fields(std::string &_js)` to derived classes:
1. Add declaration to header public section: `void collect_json_fields(std::string &_js) override;`
2. Append implementation to the .cpp file
3. Always call parent's `collect_json_fields(_js)` first
4. Use `JSON_FIELD(x)` for simple fields, `JSON_KV(k, v)` for computed/cast values,
   `JSON_ARRAY(k, vec)` for `vector<T*>` where T has `serialize()`
5. For `vector<string>`, manually build JSON array (no pointer T, can't use JSON_ARRAY)
6. For `nhm_*` and `i965_core` classes: call `abstract_cpu::collect_json_fields(_js)`
   (not the intermediate cpu_package/cpu_core base — those lack the method)
7. Skip non-serializable members: mutex, atomic, pthread_t, pointer-to-interface


# Method tests: write_log, measurement cycles, scheduling

## test_framework write_log

`get_write_log()` returns all writes captured unconditionally in both
record and replay modes. Key behavior in `replay_write()`:
- `write_sequences.count(path) == 0` → no W records provided: silently
  capture in write_log (opt-in verification via test code assertions)
- `write_sequences.count(path) > 0` and queue empty → "TEST FAIL: Extra
  write" (queue exhausted — still an error)
- W records present → validate AND capture in write_log

## sysfs_tunable method tests (tests/base/)

`result_string()` in tunable base calls `good_bad()` internally — which
triggers a sysfs read. Do NOT call `serialize()` after `reset()` if the
object's `good_bad()` will read sysfs: the read will hit the real
filesystem. Either keep replay active during serialize, or test `good_bad()`
and write assertions separately without calling serialize.

`bad_value` is private in `sysfs_tunable` — verify via `serialize()` JSON
or `toggle_bad` (protected in tunable base). Use a derived test class to
expose `toggle_bad`.

## Fixture creation with trace_tool.py add

Use `trace_tool.py add FILE R PATH VALUE` to build fixture files from
scratch (creates file if it doesn't exist). Values are plain strings,
auto-encoded to base64. Validate after building with `trace_tool.py validate`.

Example for backlight (2 reads in start_measurement):
  trace_tool.py add backlight_start.ptrecord R /sys/class/backlight/lcd/max_brightness 100
  trace_tool.py add backlight_start.ptrecord R /sys/class/backlight/lcd/actual_brightness 60

## Test infrastructure: stub files

When a class pulls in a heavy dependency chain through one function:
- Create `tests/devices/stub_display.cpp` for `create_tab` / `get_ncurses_win`
- Create `tests/base/stub_runtime_pm.cpp` for `device_has_runtime_pm`
Both stubs inline the actual logic using intercepted `read_sysfs()` /
`read_file_content()` — so the stubs remain correctly intercepted in tests.

## Coverage workflow

`scripts/coverage_report.sh [label] [build_dir]` captures a named lcov
snapshot from the coverage build directory (default: `build_cov`).

One-time setup: `meson setup build_cov -Db_coverage=true -Denable-tests=true`

Before/after pattern:
  ninja -C build_cov test && scripts/coverage_report.sh before
  # add test, rebuild
  ninja -C build_cov test && scripts/coverage_report.sh after

`ninja coverage` is broken due to duplicate test_framework.cpp symbols;
use the script instead (it passes `--ignore-errors inconsistent`).

# NaN / division-by-zero guard patterns

Two thresholds are used to guard floating-point divisions:

- `measurement_time < 0.00001` (10 µs, seconds unit) — used in
  `do_process.cpp` `total_*()` functions and `powerconsumer.cpp`.
- `time_factor < 1.0` (1 µs, microseconds unit) — used in
  `fill_cstate_line()` / `fill_cstate_percentage()` in
  `cpu_core.cpp`, `cpu_linux.cpp`, `cpu_package.cpp`, and
  `i965_core::fill_cstate_line()` in `intel_gpu.cpp`.
- `time_delta < 1.0` (1 µs) — same as time_factor, used in
  `intel_gpu.cpp`.

`percentage()` in `lib.cpp` clamps negatives to 0 but does NOT cap
at 100 (commented out), and does NOT trap NaN — callers must guard
their divisors.

# list_directory() test infrastructure (commit 4adae58)

`list_directory(path)` in `src/lib.cpp` returns a sorted `std::vector<std::string>`
of filenames using `std::filesystem::directory_iterator`. It is interceptable
by the test framework via a new `D` record type in `.ptrecord` files.

**D record format:** `D path base64(newline-joined-entries)`
- Empty base64 → empty/not-found directory (returns `{}`)
- In `trace_tool.py add`: `add FILE D /some/dir "entry1 entry2"` — entries are
  space-separated, sorted then newline-joined before base64 encoding.

**Converted callers (all callers as of commit 12883a9 — no more opendir in codebase):**
- `process_directory()` in `lib.cpp`
- `parse_cstates_start()` / `parse_cstates_end()` in `cpu_linux.cpp`
- `do_bus()` in `runtime_pm.cpp`
- `add_wifi_tunables()` in `tuning/wifi.cpp`
- `add_runtime_tunables()` in `tuning/runtime.cpp`
- `add_usb_callback()` in `tuning/tuningusb.cpp`
- `disk_name()`, `model_name()`, `create_all_ahcis()` in `devices/ahci.cpp`
- `dpms_screen_on()` in `devices/backlight.cpp`
- `create_all_devfreq_devices()` in `devices/devfreq.cpp` (also removed `static DIR*`)
- `rapl_device` constructor in `cpu/rapl/rapl_interface.cpp`
- `byt_has_ahci()` in `cpu/intel_cpus.cpp` (uses `std::filesystem::exists()`)
- `collect_open_devices()` in `devlist.cpp` (nested /proc/ and /proc/<pid>/fd/)

**Note:** `byt_has_ahci()` uses `std::filesystem::exists()` rather than `list_directory()` since it only checks directory existence. The `DIR *dir` member was removed from `intel_util` class.

**Empty D record format:** `D path` (no trailing b64 token) = empty/missing directory.
`test_framework.cpp` and `trace_tool.py` both handle this correctly.

# Coverage baseline (as of commit 8906999)

Overall: 29.0% lines (2654/9140), 43.2% functions (418/967)

Well-covered files (>85%, essentially done):
- `process/powerconsumer.cpp` 98%
- `process/interrupt.cpp` 89%
- `report/*.cpp`, `lib.cpp`, `timer.cpp`, `measurement/sysfs.cpp` > 90%

Partially covered, more tests possible but require sysfs fixtures:
- `cpu/abstract_cpu.cpp` 43% — measurement_start/end, wiggle need sysfs
- `cpu/cpu_linux.cpp` 30% — parse_cstates/pstates_start/end need sysfs
- `process/process.cpp` 39%
- `devices/runtime_pm.cpp` 32%
- `tuning/runtime.cpp` 45%

Practically untestable without hardware (0% or near):
- `main.cpp`, `cpu.cpp`, `intel_cpus.cpp`, `do_process.cpp`,
  `calibrate.cpp`, `perf/`, `display.cpp`, `devlist.cpp`

# New tests added (commit 681baea) — list_directory coverage

- `test_cpu_linux_measurement_cycle` (tests/base/test_cpu_linux.cpp):
  Fixture `cpu_linux_cycle.ptrecord` drives a full measurement_start/end
  cycle, exercising parse_cstates_start/end with list_directory D records
  and verifying fill_cstate_name/time/percentage output.

- `test_dpms_screen_on_no_drm` / `test_dpms_screen_on_active` (tests/devices/test_backlight_serialize.cpp):
  Use real `backlight` class (not the test_backlight override) with D
  records for `/sys/class/drm/card0` to exercise dpms_screen_on().

- `test_ahci_human_name_disk_model` (tests/devices/test_ahci.cpp):
  Fixture `ahci_host0_disk.ptrecord` with nested D records for
  target/disk entries exercises model_name() and disk_name() paths.

- `test_add_wifi_tunables_filters_by_prefix` / `test_add_wifi_tunables_empty_dir`
  (tests/base/test_wifi_tunables.cpp):
  New test file with stub_iw.cpp, exercises add_wifi_tunables() prefix
  filtering (wlan/wlp/wlx added, eth0 skipped) via D record replay.

Test count: 44 → 45 (one new test executable for wifi_tunables).
