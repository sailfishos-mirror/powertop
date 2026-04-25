/*
 * Copyright 2010, Intel Corporation
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

#include "tuning.h"
#include "tunable.h"
#include <unistd.h>
#include "tuningsysfs.h"
#include <string.h>
#include <dirent.h>
#include <utility>
#include <iostream>
#include <fstream>
#include <limits.h>
#include <format>

#include "../lib.h"

sysfs_tunable::sysfs_tunable(const char *str, const char *_sysfs_path, const char *_target_content) : tunable(str, 1.5, _("Good"), _("Bad"), _("Unknown"))
{
	sysfs_path = _sysfs_path;
	target_value = _target_content;
	bad_value = read_sysfs_string(_sysfs_path);

	toggle_good = std::format("echo '{}' > '{}';", target_value, sysfs_path);
	toggle_bad = std::format("echo '{}' > '{}';", bad_value, sysfs_path);
}

int sysfs_tunable::good_bad(void)
{
	std::string content;

	content = read_sysfs_string(sysfs_path.c_str());

	if (content == target_value)
		return TUNE_GOOD;

	return TUNE_BAD;
}

void sysfs_tunable::toggle(void)
{
	int good;
	good = good_bad();

	if (good == TUNE_GOOD) {
		write_sysfs(sysfs_path.c_str(), bad_value.c_str());
		return;
	}

	write_sysfs(sysfs_path.c_str(), target_value.c_str());
}

const char *sysfs_tunable::toggle_script(void)
{
	int good;
	good = good_bad();

	if (good == TUNE_GOOD) {
		return toggle_bad.c_str();
	}

	return toggle_good.c_str();
}


void add_sysfs_tunable(const char *str, const char *_sysfs_path, const char *_target_content)
{
	class sysfs_tunable *st;

	st = new class sysfs_tunable(str, _sysfs_path, _target_content);
	all_tunables.push_back(st);
}

static void add_sata_callback(const char *d_name)
{
	char filename[PATH_MAX];
	snprintf(filename, sizeof(filename), "/sys/class/scsi_host/%s/link_power_management_policy", d_name);
	if (access(filename, R_OK) != 0)
		return;

	add_sysfs_tunable(_("SATA link power management for host"), filename, "min_power");
}

void add_sata_tunables(void)
{
	process_directory("/sys/class/scsi_host/", add_sata_callback);
}
