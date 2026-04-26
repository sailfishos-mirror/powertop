# Buffer Overflow Vulnerability Report for PowerTOP

This report summarizes potential buffer overflow vulnerabilities identified in the `src/` directory, primarily due to the use of legacy C-style string functions without adequate bounds checking.

## 1. Tunable and Wakeup Description Buffers

Many classes derived from `tunable` and `wakeup` use `sprintf` to populate a `desc` buffer defined in the base class.

*   **Vulnerability**: `sprintf(desc, ...)`
*   **Buffer Size**: `desc` is 4096 bytes (defined in `src/tuning/tunable.h` and `src/wakeup/wakeup.h`).
*   **Risk**: If the arguments passed to `sprintf` (such as interface names, paths, or device descriptions) are long, they can easily exceed the 4096-byte limit.
*   **Examples**:
    *   `src/tuning/wifi.cpp:48`: `sprintf(desc, _("Wireless Power Saving for interface %s"), iface);` where `iface` is also 4096 bytes.
    *   `src/tuning/runtime.cpp`: Multiple `sprintf` calls to `desc` using `path`, `bus`, `dev`, and `pci_id_to_name` results.
    *   `src/wakeup/wakeup_usb.cpp:51` and `src/wakeup/wakeup_ethernet.cpp:51`.

**Recommendation**: Replace `sprintf` with `snprintf(desc, sizeof(desc), ...)` to ensure no overflow occurs.

## 2. Path Buffer Overflows

Several locations use `sprintf` to construct file paths.

*   **Vulnerability**: `sprintf(runtime_path, "%s/power/control", path);`
*   **File**: `src/tuning/runtime.cpp:45`
*   **Buffer Size**: `runtime_path` is `PATH_MAX` (typically 4096 on Linux).
*   **Risk**: If `path` is already close to `PATH_MAX`, adding `/power/control` will overflow the buffer.

**Recommendation**: Use `snprintf` with `sizeof(runtime_path)`.

## 3. String Concatenation in `devlist.cpp`

The `guilty` buffer in the `device` class is used to accumulate process names.

*   **Vulnerability**: `strcat(_dev->guilty, one[i]->comm);`
*   **File**: `src/devlist.cpp:221, 233`
*   **Buffer Size**: `_dev->guilty` is 4096 bytes.
*   **Risk**: While there is a check `if (strlen(_dev->guilty) < 2000)`, it only checks if the *current* length is less than 2000. If `one[i]->comm` (or many small ones) eventually exceeds the remaining 2096 bytes, an overflow will occur.

**Recommendation**: Use `strncat` or check `strlen(_dev->guilty) + strlen(one[i]->comm) + 1 < sizeof(_dev->guilty)`.

## 4. General `sprintf` Usage on Fixed-Size Buffers

There are numerous instances of `sprintf` into buffers that are assumed to be "large enough".

*   `src/devices/network.cpp:147`: `sprintf(humanname, "nic:%s", _name);`
*   `src/cpu/cpu_linux.cpp`, `src/cpu/cpu_core.cpp`, `src/cpu/cpu_package.cpp`: Frequent use of `sprintf(buffer, ...)` where `buffer` is passed as a `char *`.

**Recommendation**: Update function signatures to include buffer size and use `snprintf`.

## Summary of Risks

The identified patterns represent a "classic" set of buffer overflow risks. While many may not be exploitable under normal conditions (as they depend on system-provided strings like interface names), they violate secure coding practices and could lead to crashes or undefined behavior if system paths or device names are unusually long.
