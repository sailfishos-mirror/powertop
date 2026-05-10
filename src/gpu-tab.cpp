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
#include "cpu/intel_cpus.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <ncurses.h>

/* Internal key used for tab_windows lookup — constant regardless of GPU type. */
static const char * const GPU_TAB_KEY = "GPU";

static constexpr int BAR_WIDTH = 50;

/*
 * Draw a horizontal progress bar with scale labels and optional policy markers.
 *
 * Outputs up to four lines into win:
 *   1.  " <label>  <value_str>"
 *   2.  "  [bar: █ filled, ░ empty]"
 *   3.  "  <scale labels at multiples of label_interval>"
 *   4.  "  <marker carets (^) at marker_lo / marker_hi>"  (omitted if no markers)
 *
 * marker_lo / marker_hi: pass NAN to omit either marker.
 * color_filled / color_empty: ncurses COLOR_PAIR IDs (0 = default terminal color).
 *
 * If labels at label_interval would overlap (gap < ~6 chars), the interval is
 * doubled repeatedly until labels fit.
 */
static void draw_progress_bar(WINDOW *win,
			      const std::string &label,
			      double value,
			      double scale_min, double scale_max,
			      double marker_lo, double marker_hi,
			      const std::string &value_str,
			      double label_interval,
			      int bar_width = BAR_WIDTH,
			      int color_filled = 0,
			      int color_empty = 0)
{
	const double range = scale_max - scale_min;
	if (range <= 0.0)
		return;

	/* Line 1: label + current value — two-space indent to align with bar */
	wprintw(win, "  %s  %s\n", label.c_str(), value_str.c_str());

	/* Line 2: bar */
	const double clamped = std::clamp(value, scale_min, scale_max);
	const int filled = std::clamp(
		(int)std::round((clamped - scale_min) / range * bar_width),
		0, bar_width);

	wprintw(win, "  ");
	if (color_filled)
		wattron(win, COLOR_PAIR(color_filled));
	for (int i = 0; i < filled; i++)
		wprintw(win, "█");
	if (color_filled)
		wattroff(win, COLOR_PAIR(color_filled));
	if (color_empty)
		wattron(win, COLOR_PAIR(color_empty));
	for (int i = filled; i < bar_width; i++)
		wprintw(win, "░");
	if (color_empty)
		wattroff(win, COLOR_PAIR(color_empty));
	wprintw(win, "\n");

	/* Line 3: scale labels.
	 * Double the interval until adjacent labels are at least 6 chars apart. */
	{
		const double min_gap_chars = 6.0;
		while ((label_interval / range * bar_width) < min_gap_chars)
			label_interval *= 2.0;

		std::string scale_line(bar_width + 2, ' ');
		const double first =
			std::ceil(scale_min / label_interval) * label_interval;
		for (double v = first;
		     v <= scale_max + label_interval * 0.01;
		     v += label_interval) {
			const int pos = std::clamp(
				(int)std::round((v - scale_min) / range * bar_width),
				0, bar_width - 1);
			const std::string num =
				std::to_string((int)std::round(v));
			const int write_pos = pos + 2; /* +2 for leading "  " */
			if (write_pos + (int)num.size() <=
			    (int)scale_line.size())
				scale_line.replace(write_pos, num.size(), num);
		}
		wprintw(win, "%s\n", scale_line.c_str());
	}

	/* Line 4: marker carets — only emitted when at least one is set. */
	{
		std::string marker_line(bar_width + 2, ' ');
		bool any = false;

		auto place = [&](double mv) {
			if (std::isnan(mv))
				return;
			const int pos = std::clamp(
				(int)std::round((mv - scale_min) / range * bar_width),
				0, bar_width - 1);
			marker_line[pos + 2] = '^';
			any = true;
		};
		place(marker_lo);
		place(marker_hi);

		if (any)
			wprintw(win, "%s\n", marker_line.c_str());
	}

	wprintw(win, "\n");
}

/* ------------------------------------------------------------------ */

static void show_frequency_section(WINDOW *win)
{
	const std::string card = find_xe_card_path();
	if (card.empty())
		return;

	wprintw(win, "%s\n\n", _("Frequency Overview"));

	const std::string dev = card + "/device";

	for (const auto &tile : list_directory(dev)) {
		if (!tile.starts_with("tile"))
			continue;
		const std::string tile_path = std::format("{}/{}", dev, tile);

		for (const auto &gt : list_directory(tile_path)) {
			if (!gt.starts_with("gt"))
				continue;
			const std::string freq =
				std::format("{}/{}/freq0", tile_path, gt);

			const double hw_min =
				(double)read_sysfs(freq + "/rpn_freq");
			const double hw_max =
				(double)read_sysfs(freq + "/rp0_freq");
			if (hw_max <= 0.0)
				continue;

			const double cur =
				(double)read_sysfs(freq + "/cur_freq");
			const double pol_min =
				(double)read_sysfs(freq + "/min_freq");
			const double pol_max =
				(double)read_sysfs(freq + "/max_freq");

			const std::string label =
				std::format("{} {}", tile, gt);
			const std::string value_str =
				hz_to_human((unsigned long)cur * 1000UL);
			const int bar_width =
				std::min(COLS - 4, 180);

			draw_progress_bar(win, label, cur,
					  hw_min, hw_max,
					  pol_min, pol_max,
					  value_str, 500.0, bar_width);
		}
	}
}

/* ------------------------------------------------------------------ */

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
	show_frequency_section(w);
	wprintw(w, "%s\n\n", _("Idle / Busy"));
	wprintw(w, "%s\n\n", _("Fan Speeds"));
}

/* ------------------------------------------------------------------ */

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
