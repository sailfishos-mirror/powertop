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
#include <format>
#include <algorithm>

#include "intel_cpus.h"
#include "../lib.h"
#include "../parameters/parameters.h"
#include "../display.h"

/*
 * Collect the sysfs idle_residency_ms paths for every GT on every tile of
 * every Xe DRM card present in the system.  Returns an empty vector if no
 * Xe card is found.
 */
static std::vector<std::string> find_xe_gt_idle_paths(void)
{
	std::vector<std::string> paths;

	for (const auto &card : list_directory("/sys/class/drm")) {
		if (!card.starts_with("card"))
			continue;
		if (card.find('-') != std::string::npos)
			continue;

		const std::string dev =
			std::format("/sys/class/drm/{}/device", card);

		for (const auto &tile : list_directory(dev)) {
			if (!tile.starts_with("tile"))
				continue;

			const std::string tile_path =
				std::format("{}/{}", dev, tile);

			for (const auto &gt : list_directory(tile_path)) {
				if (!gt.starts_with("gt"))
					continue;

				const std::string idle_path =
					std::format("{}/{}/gtidle/"
						    "idle_residency_ms",
						    tile_path, gt);

				if (access(idle_path.c_str(), R_OK) == 0)
					paths.push_back(idle_path);
			}
		}
	}
	return paths;
}

/*
 * Find a DRM card bound to the xe driver, used to confirm xe is present for
 * the CPU-tab component without duplicating the tracefs check.
 */
std::string find_xe_card_path(void)
{
	static const std::string cached = []() -> std::string {
		for (const auto &card : list_directory("/sys/class/drm")) {
			if (!card.starts_with("card"))
				continue;
			if (card.find('-') != std::string::npos)
				continue;
			const std::string dev =
				std::format("/sys/class/drm/{}/device", card);
			/* xe driver exposes tile* subdirectories */
			for (const auto &entry : list_directory(dev)) {
				if (entry.starts_with("tile"))
					return std::format(
						"/sys/class/drm/{}", card);
			}
		}
		return {};
	}();
	return cached;
}

xe_core::xe_core()
{
	gt_idle_paths = find_xe_gt_idle_paths();
	idle_before.assign(gt_idle_paths.size(), 0);
	idle_after.assign(gt_idle_paths.size(), 0);
}

void xe_core::measurement_start(void)
{
	before = pt_gettime();

	uint64_t total_idle = 0;
	for (size_t i = 0; i < gt_idle_paths.size(); ++i) {
		idle_before[i] =
			read_sysfs_uint64(gt_idle_paths[i], nullptr);
		total_idle += idle_before[i];
	}

	update_cstate("gpu c0",   "Powered On", 0, 0,          1, 0);
	update_cstate("gpu gt-c6","GT-C6",       0, total_idle, 1, 1);
}

void xe_core::measurement_end(void)
{
	after = pt_gettime();

	uint64_t total_idle_before = 0;
	uint64_t total_idle_after  = 0;
	for (size_t i = 0; i < gt_idle_paths.size(); ++i) {
		total_idle_before += idle_before[i];
		idle_after[i] =
			read_sysfs_uint64(gt_idle_paths[i], nullptr);
		total_idle_after += idle_after[i];
	}

	update_cstate("gpu c0",    "Powered On", 0, 0,               1, 0);
	update_cstate("gpu gt-c6", "GT-C6",      0, total_idle_after, 1, 1);
}

std::string xe_core::fill_cstate_line(int line_nr,
				       [[maybe_unused]] const std::string &sep)
{
	if (line_nr == LEVEL_HEADER)
		return _("  GPU ");

	const double time_delta =
		1e6 * (after.tv_sec  - before.tv_sec)
		    + (after.tv_usec - before.tv_usec);

	if (time_delta < 1.0)
		return "";

	/* Sum idle residency across all GTs (in milliseconds). */
	uint64_t total_idle_before = 0;
	uint64_t total_idle_after  = 0;
	for (size_t i = 0; i < gt_idle_paths.size(); ++i) {
		total_idle_before += idle_before[i];
		total_idle_after  += idle_after[i];
	}

	const double ratio = 100000.0 / time_delta; /* % per ms */
	double d = -1.0;

	switch (line_nr) {
	case 0: /* GT-C0: active */
		d = 100.0 - ratio * static_cast<double>(
			total_idle_after - total_idle_before);
		break;
	case 1: /* GT-C6: idle */
		d = ratio * static_cast<double>(
			total_idle_after - total_idle_before);
		break;
	default:
		return "";
	}

	d = std::clamp(d, 0.0, 100.0);
	return pt_format(_("{:5.1f}%"), d);
}

std::string xe_core::fill_pstate_line([[maybe_unused]] int line_nr)
{
	return "";
}

std::string xe_core::fill_pstate_name([[maybe_unused]] int line_nr)
{
	return "";
}

void xe_core::collect_json_fields(std::string &_js)
{
	abstract_cpu::collect_json_fields(_js);
	JSON_KV("before_sec",  (long)before.tv_sec);
	JSON_KV("before_usec", (long)before.tv_usec);
	JSON_KV("after_sec",   (long)after.tv_sec);
	JSON_KV("after_usec",  (long)after.tv_usec);
}
