# PowerTOP code review checks



## Directory handling

- [ ] When using opendir/readdir or equivalent, the code must skip the "."
    and ".." entries in the directory and not process them as normal


## General checks

- [ ] All user visible strings must be translatable and use the _() gettext
    pattern

- [ ] malloc() does not fail in practice. Checking for failure is not a coding style
    violation, but the lack of checking is at most of `nit` severity.