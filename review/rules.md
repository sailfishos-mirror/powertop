# PowerTOP code review checks



## Directory handling

- [ ] When using opendir/readdir or equivalent, the code must skip the "."
    and ".." entries in the directory and not process them as normal


## C++ style

- [ ] All virtual method overrides in derived classes must use the `override`
    specifier. The `virtual` keyword may be kept or omitted on overrides, but
    `override` is mandatory. Pure virtual declarations in base classes (`= 0`)
    do not need `override`. The build is configured with `-Wsuggest-override`
    to enforce this.

- [ ] Use `nullptr` instead of `NULL` in all C++ code (`.cpp` and `.h` files).


## General checks

- [ ] All user visible strings must be translatable and use the `_()` gettext
    pattern.

- [ ] When formatting a translated string that contains `{}` placeholders, you
    **must** use `pt_format` instead of `std::format`. `std::format` requires a
    compile-time format string; `_()` returns a runtime `const char*` from
    gettext, so `std::format(_("..."), x)` will not compile. `pt_format` wraps
    `std::vformat` which accepts runtime strings.

    **Use `std::format` when the string is not translated** (internal/technical
    strings, data-derived identifiers, log output not shown to the user):
    ```cpp
    // OK — not user-visible, no translation needed
    std::string path = std::format("{}/actual_brightness", sysfs_path);
    ```

    **Use `pt_format(_(...))` when the string is user-visible and needs
    translation:**
    ```cpp
    // OK — user-visible label with a runtime argument
    humanname = pt_format(_("USB device: {}"), product);
    ```

- [ ] malloc() does not fail in practice. Checking for failure is not a coding style
    violation, but the lack of checking is at most of `nit` severity.


## Numeric safety

- [ ] **Every floating-point division must have an explicit near-zero guard
    before the `/` operator.**  A zero or near-zero divisor produces `Inf` or
    `NaN`; because NaN comparisons always return `false`, post-hoc clamps
    (e.g. `if (x < 0.0) x = 0.0`) silently pass NaN through to formatted
    output or further arithmetic.  The guard must come *before* the division:

    ```cpp
    // CORRECT
    if (time_factor < 1.0)
        return "";
    return std::format("{:5.1f}%", percentage(duration_delta / time_factor));

    // WRONG — NaN escapes the clamp
    double pct = duration_delta / time_factor;   // NaN when time_factor == 0
    if (pct < 0.0) pct = 0.0;                   // NaN < 0.0 is false → no effect
    ```

    Use the established thresholds (see `style.md` §7.1):
    `measurement_time < 0.00001`, `time_factor < 1.0`, `time_delta < 1.0`.

    Missing or post-hoc-only guards are **high** severity; a completely
    unguarded division that is reachable in normal operation is **critical**.