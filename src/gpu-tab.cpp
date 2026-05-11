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
#include "report/report.h"
#include "report/report-maker.h"
#include "report/report-data-html.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <ncurses.h>

/* Internal key used for tab_windows lookup — constant regardless of GPU type. */
static const char * const GPU_TAB_KEY = "GPU";

static constexpr int BAR_WIDTH = 50;

/* Color-pair IDs for the four-segment frequency bar. */
static constexpr int GPU_BAR_FLOOR    = 10;  /* 0 → pol_min: guaranteed floor */
static constexpr int GPU_BAR_ACTIVE   = 11;  /* pol_min → cur: currently active */
static constexpr int GPU_BAR_HEADROOM = 12;  /* cur → pol_max: within-policy headroom */
static constexpr int GPU_BAR_BEYOND   = 13;  /* pol_max → hw_max: beyond policy */
static constexpr int GPU_BAR_BUSY     = 14;  /* C0 / active portion of idle bar */
static constexpr int GPU_BAR_IDLE     = 15;  /* C6 / idle portion of idle bar */

/*
 * Draw a horizontal progress bar with scale labels and optional policy markers.
 *
 * Outputs up to four lines into win:
 *   1.  " <label>  <value_str>"
 *   2.  "  [bar]"
 *   3.  "  <scale labels>"
 *   4.  "  <marker caret '<' at marker_hi>"  (only when marker_lo is NAN and marker_hi is not)
 *
 * When marker_lo is provided (not NAN) the bar uses four colour segments
 * (guaranteed floor / active / headroom / beyond-policy) and the marker
 * line is suppressed — the policy range is encoded directly in the bar.
 * When marker_lo is NAN but marker_hi is not, a '<' caret is printed
 * below the bar at marker_hi (used for the power TDP cap).
 *
 * color_filled / color_empty: ncurses COLOR_PAIR IDs used in the simple
 * two-segment mode (marker_lo == NAN).  Pass 0 for default terminal color.
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
			      int color_empty = 0,
			      int attr_filled = 0,
			      int attr_empty = 0)
{
	const double range = scale_max - scale_min;
	if (range <= 0.0)
		return;

	/* Line 1: label + current value — two-space indent to align with bar */
	wprintw(win, "  %s  %s\n", label.c_str(), value_str.c_str());

	/* Line 2: bar.
	 * Four-segment colour mode (marker_lo is set):
	 *   [scale_min, marker_lo)  █ bright-green  guaranteed floor
	 *   [marker_lo,  value)     █ green          currently active
	 *   [value,      pol_max)   ░ yellow         within-policy headroom
	 *   [pol_max,    scale_max] ░ dim            beyond policy (case 2 only)
	 * When pol_max == scale_max (case 1) the headroom segment runs to the
	 * end and there is no beyond-policy segment.
	 * Simple two-segment mode (marker_lo is NAN): solid █ / light ░. */
	{
		const double clamped = std::clamp(value, scale_min, scale_max);
		wprintw(win, "  ");

		if (!std::isnan(marker_lo)) {
			const int pos_min = std::clamp(
				static_cast<int>(std::round((marker_lo - scale_min) / range * bar_width)),
				0, bar_width);
			const int pos_cur = std::clamp(
				static_cast<int>(std::round((clamped - scale_min) / range * bar_width)),
				0, bar_width);
			const bool has_beyond = !std::isnan(marker_hi) &&
						std::abs(marker_hi - scale_max) > 0.5;
			int pos_max = bar_width;
			if (has_beyond)
				pos_max = std::clamp(
					static_cast<int>(std::round((marker_hi - scale_min) / range * bar_width)),
					0, bar_width);

			for (int i = 0; i < bar_width; i++) {
				if (i < pos_min) {
					wattron(win, COLOR_PAIR(GPU_BAR_FLOOR) | A_BOLD);
					wprintw(win, "█");
					wattroff(win, COLOR_PAIR(GPU_BAR_FLOOR) | A_BOLD);
				} else if (i < pos_cur) {
					wattron(win, COLOR_PAIR(GPU_BAR_ACTIVE));
					wprintw(win, "█");
					wattroff(win, COLOR_PAIR(GPU_BAR_ACTIVE));
				} else if (i < pos_max) {
					wattron(win, COLOR_PAIR(GPU_BAR_HEADROOM));
					wprintw(win, "░");
					wattroff(win, COLOR_PAIR(GPU_BAR_HEADROOM));
				} else {
					wattron(win, COLOR_PAIR(GPU_BAR_BEYOND) | A_DIM);
					wprintw(win, "░");
					wattroff(win, COLOR_PAIR(GPU_BAR_BEYOND) | A_DIM);
				}
			}
		} else {
			const int filled = std::clamp(
				static_cast<int>(std::round((clamped - scale_min) / range * bar_width)),
				0, bar_width);
			if (color_filled)
				wattron(win, COLOR_PAIR(color_filled) | attr_filled);
			for (int i = 0; i < filled; i++)
				wprintw(win, "█");
			if (color_filled)
				wattroff(win, COLOR_PAIR(color_filled) | attr_filled);
			if (color_empty)
				wattron(win, COLOR_PAIR(color_empty) | attr_empty);
			for (int i = filled; i < bar_width; i++)
				wprintw(win, "░");
			if (color_empty)
				wattroff(win, COLOR_PAIR(color_empty) | attr_empty);
		}
		wprintw(win, "\n");
	}

	/* Line 3: scale labels.
	 * Double the interval until adjacent labels are at least 6 chars apart. */
	{
		const double min_gap_chars = 6.0;
		while ((label_interval / range * bar_width) < min_gap_chars)
			label_interval *= 2.0;

		std::string scale_line(bar_width + 2, ' ');

		/* Reserve space for the max label, right-aligned at the end. */
		const std::string max_str =
			std::to_string(static_cast<int>(std::round(scale_max)));
		const int max_start =
			static_cast<int>(scale_line.size()) - static_cast<int>(max_str.size());

		const double first =
			std::ceil(scale_min / label_interval) * label_interval;
		for (double v = first;
		     v <= scale_max + label_interval * 0.01;
		     v += label_interval) {
			const int pos = std::clamp(
				static_cast<int>(std::round((v - scale_min) / range * bar_width)),
				0, bar_width - 1);
			const std::string num =
				std::to_string(static_cast<int>(std::round(v)));
			const int write_pos = pos + 2; /* +2 for leading "  " */
			/* Skip labels that would overlap the max-value label. */
			if (write_pos + static_cast<int>(num.size()) > max_start)
				continue;
			if (write_pos + static_cast<int>(num.size()) <=
			    static_cast<int>(scale_line.size()))
				scale_line.replace(write_pos, num.size(), num);
		}

		/* Always show the maximum value, right-aligned. */
		scale_line.replace(max_start, max_str.size(), max_str);

		wprintw(win, "%s\n", scale_line.c_str());
	}

	/* Line 4: marker caret — only when marker_lo is NAN and marker_hi is set.
	 * (When marker_lo is set the bar's colour segments carry that information.) */
	if (std::isnan(marker_lo) && !std::isnan(marker_hi)) {
		std::string marker_line(bar_width + 2, ' ');
		const int pos = std::clamp(
			static_cast<int>(std::round((marker_hi - scale_min) / range * bar_width)),
			0, bar_width - 1);
		marker_line[pos + 2] = '<';
		wprintw(win, "%s\n", marker_line.c_str());
	}

	wprintw(win, "\n");
}

/* ------------------------------------------------------------------ */

/* Read the active xe power profile from the first GT found.
 * The sysfs file uses kernel "selected list" format, e.g.:
 *   [power_saving]  balanced  performance  */
static std::string xe_power_profile(void)
{
	const std::string card = find_xe_card_path();
	if (card.empty())
		return {};

	const std::string dev = card + "/device";
	for (const auto &tile : list_directory(dev)) {
		if (!tile.starts_with("tile"))
			continue;
		const std::string tile_path = std::format("{}/{}", dev, tile);
		for (const auto &gt : list_directory(tile_path)) {
			if (!gt.starts_with("gt"))
				continue;
			const std::string raw = read_sysfs_string(
				std::format("{}/{}/freq0/power_profile",
					    tile_path, gt));
			if (!raw.empty())
				return extract_bracket_selection(raw);
		}
	}
	return {};
}

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


			const double hw_max =
				static_cast<double>(read_sysfs(freq + "/rp0_freq"));
			if (hw_max <= 0.0)
				continue;

			const double cur =
				static_cast<double>(read_sysfs(freq + "/cur_freq"));
			const double pol_min =
				static_cast<double>(read_sysfs(freq + "/min_freq"));
			const double pol_max =
				static_cast<double>(read_sysfs(freq + "/max_freq"));

			const std::string label =
				std::format("{} {}", tile, gt);
			const std::string value_str =
				hz_to_human(static_cast<unsigned long>(cur) * 1000UL);
			const int bar_width =
				std::min(COLS - 4, 180);

			draw_progress_bar(win, label, cur,
					  0.0, hw_max,
					  pol_min, pol_max,
					  value_str, 500.0, bar_width);
		}
	}
}

/* ------------------------------------------------------------------ */

static void show_fan_section(WINDOW *win)
{
	/* Global max RPM seen across all fans — shared scale, minimum 1000. */
	static double max_rpm_seen = 1000.0;

	std::vector<device *> fans;
	for (auto *d : all_devices) {
		if (d->class_name() == "GPU" &&
		    d->util_units().find("RPM") != std::string::npos)
			fans.push_back(d);
	}

	if (fans.empty())
		return;

	for (auto *d : fans) {
		const double rpm = d->utilization();
		if (rpm > max_rpm_seen)
			max_rpm_seen = rpm;
	}

	wprintw(win, "%s\n\n", _("Fan Speeds"));

	const int bar_width = std::min(COLS - 4, 180);

	for (auto *d : fans) {
		const std::string value_str =
			std::format("{} RPM", static_cast<int>(d->utilization()));

		draw_progress_bar(win, d->human_name(), d->utilization(),
				  0.0, max_rpm_seen,
				  NAN, NAN,
				  value_str, 500.0, bar_width);
	}
}

/* ------------------------------------------------------------------ */

static void show_idle_section(WINDOW *win)
{
	const xe_core *xc = get_xe_core();
	if (!xc || xc->per_gt_busy_pct.empty())
		return;

	wprintw(win, "%s\n\n", _("Idle / Busy"));

	const int bar_width = std::min(COLS - 4, 180);

	for (size_t i = 0; i < xc->per_gt_busy_pct.size(); i++) {
		const double pct = xc->per_gt_busy_pct[i];
		if (pct < 0.0)
			continue; /* first measurement not yet complete */

		/* C0 is computed as 100-C6; show both in one bar:
		 * red (█) for the busy/C0 portion, green (░) for idle/C6. */
		const double c6_pct = 100.0 - pct;
		const std::string value_str =
			std::format("C0: {:.1f}%  C6: {:.1f}%", pct, c6_pct);
		draw_progress_bar(win, xc->gt_labels[i], pct,
				  0.0, 100.0, NAN, NAN,
				  value_str, 25.0, bar_width,
				  GPU_BAR_BUSY, GPU_BAR_IDLE, 0, A_BOLD);
	}
}

/* ------------------------------------------------------------------ */

static void show_power_section(WINDOW *win)
{
	xegpu *gpu = nullptr;
	for (auto *d : all_devices) {
		gpu = dynamic_cast<xegpu *>(d);
		if (gpu)
			break;
	}
	if (!gpu || gpu->power_channels.empty())
		return;

	wprintw(win, "%s\n", _("Power Overview"));
	const std::string profile = xe_power_profile();
	if (!profile.empty())
		wprintw(win, "  %s\n",
			pt_format(_("Power profile: {}"), profile).c_str());
	wprintw(win, "\n");

	const int bar_width = std::min(COLS - 4, 180);

	for (const auto &ch : gpu->power_channels) {
		if (ch.current_watts >= 0.0) {
			/* We have a live measurement — draw a bar. */
			double scale_max = std::max(ch.current_watts * 1.5, 10.0);
			if (ch.tdp_cap_watts > 0.0)
				scale_max = ch.tdp_cap_watts;
			double marker = NAN;
			if (ch.tdp_cap_watts > 0.0)
				marker = ch.tdp_cap_watts;

			const std::string value_str =
				std::format("{:.1f} W", ch.current_watts);
			draw_progress_bar(win, ch.label, ch.current_watts,
					  0.0, scale_max,
					  NAN, marker,
					  value_str, 25.0, bar_width);
		} else if (ch.tdp_cap_watts > 0.0) {
			/* TDP cap only — no energy counter on this hardware. */
			wprintw(win, "  %s  %s\n\n",
				ch.label.c_str(),
				pt_format(_("TDP cap: {:.0f} W"),
					  ch.tdp_cap_watts).c_str());
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

	show_power_section(w);
	show_frequency_section(w);
	show_idle_section(w);
	show_fan_section(w);
}

/* ------------------------------------------------------------------ */

void report_gpu_stats(void)
{
	/* Check if any GPU is present before opening the report div. */
	xegpu *gpu = nullptr;
	for (auto *d : all_devices) {
		gpu = dynamic_cast<xegpu *>(d);
		if (gpu)
			break;
	}
	if (!gpu && find_xe_card_path().empty())
		return;

	tag_attr div_attr;
	init_div(&div_attr, "clear_block", "gpuinfo");

	tag_attr title_attr;
	init_title_attr(&title_attr);

	report.add_div(&div_attr);
	report.add_title(&title_attr, __("Intel GPU Report"));

	/* ---- Power ---- */
	if (gpu && !gpu->power_channels.empty()) {
		report.add_title(&title_attr, __("Power"));

		const int rows = static_cast<int>(gpu->power_channels.size()) + 1;
		table_attributes tbl;
		init_std_table_attr(&tbl, rows, 3);

		std::vector<std::string> data(rows * 3);
		data[0] = __("Channel");
		data[1] = __("Current (W)");
		data[2] = __("TDP cap (W)");

		int idx = 3;
		for (const auto &ch : gpu->power_channels) {
			data[idx++] = ch.label;
			if (ch.current_watts >= 0.0)
				data[idx++] = std::format("{:.1f}", ch.current_watts);
			else
				data[idx++] = __("N/A");
			if (ch.tdp_cap_watts > 0.0)
				data[idx++] = std::format("{:.0f}", ch.tdp_cap_watts);
			else
				data[idx++] = __("N/A");
		}
		report.add_table(data, &tbl);
	}

	/* ---- Frequency ---- */
	{
		const std::string card = find_xe_card_path();
		const std::string dev  = card + "/device";

		/* Collect per-GT frequency rows. */
		struct freq_row { std::string label; int cur, pol_min, pol_max, hw_min, hw_max; };
		std::vector<freq_row> freq_rows;

		for (const auto &tile : list_directory(dev)) {
			if (!tile.starts_with("tile"))
				continue;
			const std::string tile_path =
				std::format("{}/{}", dev, tile);
			for (const auto &gt : list_directory(tile_path)) {
				if (!gt.starts_with("gt"))
					continue;
				const std::string freq =
					std::format("{}/{}/freq0", tile_path, gt);
				const int hw_max = static_cast<int>(read_sysfs(freq + "/rp0_freq"));
				if (hw_max <= 0)
					continue;
				freq_rows.push_back({
					std::format("{} {}", tile, gt),
					static_cast<int>(read_sysfs(freq + "/cur_freq")),
					static_cast<int>(read_sysfs(freq + "/min_freq")),
					static_cast<int>(read_sysfs(freq + "/max_freq")),
					static_cast<int>(read_sysfs(freq + "/rpn_freq")),
					hw_max
				});
			}
		}

		if (!freq_rows.empty()) {
			report.add_title(&title_attr, __("Frequency (MHz)"));

			const int rows = static_cast<int>(freq_rows.size()) + 1;
			table_attributes tbl;
			init_std_table_attr(&tbl, rows, 6);

			std::vector<std::string> data(rows * 6);
			data[0] = __("GT");
			data[1] = __("Current");
			data[2] = __("Policy min");
			data[3] = __("Policy max");
			data[4] = __("HW min");
			data[5] = __("HW max");

			int idx = 6;
			for (const auto &r : freq_rows) {
				data[idx++] = r.label;
				data[idx++] = std::to_string(r.cur);
				data[idx++] = std::to_string(r.pol_min);
				data[idx++] = std::to_string(r.pol_max);
				data[idx++] = std::to_string(r.hw_min);
				data[idx++] = std::to_string(r.hw_max);
			}
			report.add_table(data, &tbl);
		}
	}

	/* ---- Idle / Busy ---- */
	{
		const xe_core *xc = get_xe_core();
		if (xc && !xc->per_gt_busy_pct.empty()) {
			/* Count GTs with valid data. */
			int valid = 0;
			for (double v : xc->per_gt_busy_pct)
				if (v >= 0.0)
					++valid;

			if (valid > 0) {
				report.add_title(&title_attr, __("Idle / Busy"));

				const int rows = valid + 1;
				table_attributes tbl;
				init_std_table_attr(&tbl, rows, 3);

				std::vector<std::string> data(rows * 3);
				data[0] = __("GT");
				data[1] = __("C0 / Active (%)");
				data[2] = __("C6 / Idle (%)");

				int idx = 3;
				for (size_t i = 0; i < xc->per_gt_busy_pct.size(); ++i) {
					if (xc->per_gt_busy_pct[i] < 0.0)
						continue;
					data[idx++] = xc->gt_labels[i];
					data[idx++] = std::format("{:.1f}",
						xc->per_gt_busy_pct[i]);
					data[idx++] = std::format("{:.1f}",
						100.0 - xc->per_gt_busy_pct[i]);
				}
				report.add_table(data, &tbl);
			}
		}
	}

	/* ---- Fan Speeds ---- */
	{
		std::vector<device *> fans;
		for (auto *d : all_devices) {
			if (d->class_name() == "GPU" &&
			    d->util_units().find("RPM") != std::string::npos)
				fans.push_back(d);
		}

		if (!fans.empty()) {
			report.add_title(&title_attr, __("Fan Speeds"));

			const int rows = static_cast<int>(fans.size()) + 1;
			table_attributes tbl;
			init_std_table_attr(&tbl, rows, 2);

			std::vector<std::string> data(rows * 2);
			data[0] = __("Fan");
			data[1] = __("Speed (RPM)");

			int idx = 2;
			for (auto *d : fans) {
				data[idx++] = d->human_name();
				data[idx++] = std::to_string(
					static_cast<int>(d->utilization()));
			}
			report.add_table(data, &tbl);
		}
	}

	report.end_div();
}

/* ------------------------------------------------------------------ */

/* Detect which GPU is present, create the tab with the appropriate translated
 * name, and register it between "Frequency stats" and "Device stats". */
void initialize_gpu_tab(void)
{
	bool found_xe = false;

	for (auto *d : all_devices) {
		if (dynamic_cast<xegpu *>(d)) {
			found_xe = true;
			break;
		}
	}

	if (!found_xe)
		return;

	/* One-time colour-pair setup for the frequency bar segments. */
	init_pair(GPU_BAR_FLOOR,    COLOR_GREEN,  -1);
	init_pair(GPU_BAR_ACTIVE,   COLOR_GREEN,  -1);
	init_pair(GPU_BAR_HEADROOM, COLOR_YELLOW, -1);
	init_pair(GPU_BAR_BEYOND,   -1,           -1);
	init_pair(GPU_BAR_BUSY,     COLOR_RED,    -1);
	init_pair(GPU_BAR_IDLE,     COLOR_GREEN,  -1);

	const char *translated = _("Intel Xe GPU");

	auto *w = new gpu_tab_window();
	create_tab(GPU_TAB_KEY, translated, w);
}
