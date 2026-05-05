# PowerTOP

PowerTOP is a Linux tool used to diagnose issues with power consumption and
power management. In addition to being a diagnostic tool, PowerTOP also has an
interactive mode you can use to experiment with various power management
settings, for cases where the Linux distribution has not enabled those settings.


# Build dependencies

PowerTOP is written in C++ and targets Linux. It requires the
[Meson build system](https://mesonbuild.com/) and the following libraries:

* `ncurses` (required)
* `libnl-3` (required)
* `libtracefs` (required)
* `libpci` (optional — only needed if the system has PCI devices)
* `gettext` (optional — for translated messages)

Example packages to install on Ubuntu:

    sudo apt install meson ninja-build libpci-dev libnl-3-dev libnl-genl-3-dev \
    libncursesw5-dev libtracefs-dev libtraceevent-dev gettext pkg-config


## Building PowerTOP

To build PowerTOP from the cloned source:

    meson setup build
    ninja -C build

To run the test suite:

    meson setup -Denable-tests=true build_test
    ninja -C build_test test


# Running PowerTOP

The following sections go over basic operation of PowerTOP. This includes
kernel configuration options (or kernel patches) needed for full functionality.
Run `powertop --help` to see all options.


## Kernel parameters

PowerTOP needs some kernel configuration options enabled to function
properly. As of Linux 3.13, these are (the list may be incomplete):

    CONFIG_NO_HZ
    CONFIG_HIGH_RES_TIMERS
    CONFIG_HPET_TIMER
    CONFIG_CPU_FREQ_GOV_ONDEMAND
    CONFIG_USB_SUSPEND
    CONFIG_SND_AC97_POWER_SAVE
    CONFIG_PERF_EVENTS
    CONFIG_TRACEPOINTS
    CONFIG_TRACING
    CONFIG_X86_MSR
    CONFIG_DEBUG_FS
    CONFIG_POWERCAP
    CONFIG_INTEL_RAPL

The patches in the `patches/` subdirectory are optional but enable full
PowerTOP functionality.


## Outputting a report

When PowerTOP is executed as root and without arguments, it runs in
interactive mode. In this mode, PowerTOP most resembles `top`.

For generating reports, or for filing functional bug reports, there are two
output modes: CSV and HTML. You can set sample times, the number of iterations,
a workload over which to run PowerTOP, and whether to include
`debug`-level output.

For an HTML report, run:

    powertop --html=report.html

This creates a static `report.html` file, suitable for sharing.

For a Markdown report, run:

    powertop --markdown=report.md

This creates a `report.md` file (defaults to `powertop.md` if no filename
is given), suitable for viewing in any Markdown renderer.

For a CSV report, run:

    powertop --csv=report.csv

This creates a `report.csv` file, also suitable for sharing.

If you want to file a functional bug report, generate a `--debug` HTML report
and share it:

    powertop --debug --html=report.html

**Important:** Because PowerTOP is intended for privileged (`root`) use, reports
— especially those generated with `--debug` — will contain verbose system
information. PowerTOP **does not** sanitize or anonymize its reports. Be mindful
of this when sharing them.

**Developers:** If you make changes to the HTML reporting code, validate the
output using the W3C Markup Validation Service and the W3C CSS Validation
Service:
* https://validator.w3.org/#validate_by_upload
* https://jigsaw.w3.org/css-validator/#validate_by_upload


## Calibrating and power numbers

PowerTOP, when running on battery, tracks power consumption and activity on
the system. Once there are sufficient measurements, PowerTOP can start to
report power estimates for various activities. You can improve the accuracy
of these estimates by running a calibration cycle at least once:

    powertop --calibrate

Calibration cycles through various display brightness levels (including
"off"), USB device activities, and other workloads.


## Extech Power Analyzer / Datalogger support

PowerTOP supports the Extech Power Analyzer/Datalogger (model 380803) over a
serial connection. Pass the device node on the command line:

    powertop --extech=/dev/ttyUSB0

(where `ttyUSB0` is the device node of the serial-to-USB adapter)


# Contributing to PowerTOP and getting support

There are numerous ways to contribute to PowerTOP. See `CONTRIBUTE.md` for
details. The short version: fork the repository and send pull requests.

To file a bug or feature request:
* https://github.com/fenrus75/powertop/issues


# Code from other open source projects

PowerTOP contains code from other open source projects; we thank the authors
for their work. Specifically, PowerTOP contains code from:

```
Parse Event Library - Copyright 2009, 2010 Red Hat Inc  Steven Rostedt <srostedt@redhat.com>
nl80211 userspace tool - Copyright 2007, 2008	Johannes Berg <johannes@sipsolutions.net>
```


# Copyright and License

    PowerTOP
    Copyright (C) 2020  Intel Corporation

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

See `COPYING` file for a copy of the aforementioned (GPLv2) license.


## SPDX Tag

    /* SPDX-License-Identifier: GPL-2.0-only */

From: https://spdx.org/licenses/GPL-2.0-only.html
