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
#include <fstream>
#include <format>
#include <unistd.h>

extern "C" {
#include <tracefs.h>
}

#include "../lib.h"
#include "device.h"
#include "xe-gpu.h"
#include "../parameters/parameters.h"
#include "../process/powerconsumer.h"

/*
 * Find the hwmon energy counter for the Xe GPU, if present.
 *
 * The xe driver exposes a hwmon device named "xe" under the DRM card's PCI
 * device directory.  Some hardware additionally exposes energy1_input (in
 * micro-joules) which lets us compute actual GPU power directly; other
 * hardware only exposes power1_crit (the TDP ceiling).  Return the path to
 * energy1_input if readable, or an empty string otherwise.
 */
static std::string find_xe_hwmon_energy_path(void)
{
	for (const auto &entry : list_directory("/sys/class/drm")) {
		if (!entry.starts_with("card"))
			continue;
		if (entry.find('-') != std::string::npos)
			continue;

		const std::string hwmon_base =
			std::format("/sys/class/drm/{}/device/hwmon", entry);

		for (const auto &hwmon : list_directory(hwmon_base)) {
			const std::string name_path =
				std::format("{}/{}/name", hwmon_base, hwmon);
			if (read_file_content(name_path) != "xe\n")
				continue;

			const std::string energy_path =
				std::format("{}/{}/energy1_input",
					    hwmon_base, hwmon);
			if (access(energy_path.c_str(), R_OK) == 0)
				return energy_path;
		}
	}
	return {};
}

xegpu::xegpu(const std::string &energy_path)
	: device(), hwmon_energy_path(energy_path)
{
	index  = get_param_index("xe-gpu-operations");
	rindex = get_result_index("xe-gpu-operations");
	if (!hwmon_energy_path.empty()) {
		last_time = pt_gettime();
		last_energy = read_sysfs_uint64(hwmon_energy_path, nullptr);
	}
}

void xegpu::start_measurement(void)
{
	if (hwmon_energy_path.empty())
		return;
	last_time   = pt_gettime();
	last_energy = read_sysfs_uint64(hwmon_energy_path, nullptr);
}

void xegpu::end_measurement(void)
{
	if (hwmon_energy_path.empty())
		return;

	const struct timeval now   = pt_gettime();
	const double delta = (now.tv_sec  - last_time.tv_sec)
			   + (now.tv_usec - last_time.tv_usec) / 1e6;

	consumed_power = 0.0;
	if (delta >= 1e-5) {
		/* energy1_input is in micro-joules */
		const double energy =
			read_sysfs_uint64(hwmon_energy_path, nullptr);
		consumed_power  = (energy - last_energy) / 1e6 / delta;
		last_energy     = energy;
		last_time       = now;
	}
}

double xegpu::utilization(void) const
{
	return get_result_value(rindex);
}

double xegpu::power_usage(struct result_bundle *result,
			   struct parameter_bundle *bundle)
{
	if (!hwmon_energy_path.empty())
		return consumed_power;

	const double factor = get_parameter_value(index, bundle);
	const double util   = get_result_value(rindex, result);
	return util * factor / 100.0;
}

void xegpu::collect_json_fields(std::string &_js)
{
	device::collect_json_fields(_js);
	JSON_FIELD(index);
	JSON_FIELD(rindex);
	JSON_FIELD(consumed_power);
}

void create_xe_gpu(void)
{
	if (!tracefs_event_file_exists(nullptr, "xe",
				       "xe_sched_job_exec", "format"))
		return;

	register_parameter("xe-gpu-operations");

	const std::string energy_path = find_xe_hwmon_energy_path();
	auto *gpu = new xegpu(energy_path);
	all_devices.push_back(gpu);
}
