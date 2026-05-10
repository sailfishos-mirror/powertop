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
 *
 * Authors:
 *	Arjan van de Ven <arjan@linux.intel.com>
 */

#include "gpu-tab.h"
#include "lib.h"
#include "devices/device.h"
#include "devices/xe-gpu.h"
#include "devices/i915-gpu.h"

#include <ncurses.h>

/* Internal key used for tab_windows lookup — constant regardless of GPU type. */
static const char * const GPU_TAB_KEY = "GPU";

void gpu_tab_window::repaint(void)
{
	expose();
}

void gpu_tab_window::expose(void)
{
	WINDOW *w = win;
	if (!w)
		return;

	wclear(w);
	wmove(w, 2, 0);

	wprintw(w, "%s\n\n", _("Power Overview"));
	wprintw(w, "%s\n\n", _("Frequency Overview"));
	wprintw(w, "%s\n\n", _("Idle / Busy"));
	wprintw(w, "%s\n\n", _("Fan Speeds"));
}

/* Detect which GPU is present, create the tab with the appropriate translated
 * name, and register it between "Frequency stats" and "Device stats". */
void initialize_gpu_tab(void)
{
	bool found_xe   = false;
	bool found_i915 = false;

	for (auto *d : all_devices) {
		if (dynamic_cast<xegpu *>(d)) {
			found_xe = true;
			break;
		}
		if (dynamic_cast<i915gpu *>(d)) {
			found_i915 = true;
			break;
		}
	}

	if (!found_xe && !found_i915)
		return;

	const char *translated = found_xe ? _("Intel Xe GPU") : _("Intel GPU");

	auto *w = new gpu_tab_window();
	create_tab(GPU_TAB_KEY, translated, w);
}
