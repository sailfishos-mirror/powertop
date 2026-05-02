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
#include <dirent.h>
#include <utility>
#include <iostream>
#include <fstream>
#include <limits.h>
#include <format>
#include <filesystem>


#include "../lib.h"

static std::vector<std::string> get_matching_files(const std::string& path) {
	std::vector<std::string> files;
	size_t star_pos = path.find('*');
	if (star_pos == std::string::npos) {
		files.push_back(path);
		return files;
	}

	size_t dir_end = path.rfind('/', star_pos);
	if (dir_end == std::string::npos) return files;

	std::string base_dir = path.substr(0, dir_end + 1);
	std::string match_prefix = path.substr(dir_end + 1, star_pos - dir_end - 1);
	std::string suffix = path.substr(star_pos + 1);

	std::error_code ec;
	if (!std::filesystem::exists(base_dir, ec)) return files;

	for (const auto& entry : std::filesystem::directory_iterator(base_dir, ec)) {
		std::string filename = entry.path().filename().string();
		if (filename.starts_with(match_prefix)) {
			std::string full_path = entry.path().string() + suffix;
			files.push_back(full_path);
		}
	}
	return files;
}

sysfs_tunable::sysfs_tunable(const std::string &str, const std::string &_sysfs_path, const std::string &target_content) : tunable(str, 1.0, _("Good"), _("Bad"), _("Unknown"))
{
	sysfs_path = _sysfs_path;
	target_value = target_content;

	if (sysfs_path.find('*') != std::string::npos) {
		toggle_good = std::format("for i in {}; do echo '{}' > $i; done;", sysfs_path, target_value);
	} else {
		toggle_good = std::format("echo '{}' > '{}';", target_value, sysfs_path);
	}
}

int sysfs_tunable::good_bad(void)
{
	std::vector<std::string> files = get_matching_files(sysfs_path);
	if (files.empty())
		return TUNE_BAD;

	bool all_good = true;
	for (const auto& file : files) {
		std::string content = read_sysfs_string(file);
		if (content != target_value) {
			bad_value = content;
			all_good = false;
		}
	}

	if (all_good)
		return TUNE_GOOD;

	if (sysfs_path.find('*') != std::string::npos) {
		toggle_bad = std::format("for i in {}; do echo '{}' > $i; done;", sysfs_path, bad_value);
	} else {
		toggle_bad = std::format("echo '{}' > '{}';", bad_value, sysfs_path);
	}

	return TUNE_BAD;
}

void sysfs_tunable::toggle(void)
{
	int good = good_bad();
	std::vector<std::string> files = get_matching_files(sysfs_path);

	if (good == TUNE_GOOD) {
		if (!bad_value.empty()) {
			for (const auto& file : files)
				write_sysfs(file, bad_value);
		}
		return;
	}

	for (const auto& file : files)
		write_sysfs(file, target_value);
}


void add_sysfs_tunable(const std::string &str, const std::string &_sysfs_path, const std::string &_target_content)
{
	std::vector<std::string> files = get_matching_files(_sysfs_path);
	bool any_accessible = false;
	for (const auto& file : files) {
		if (access(file.c_str(), R_OK) == 0) {
			any_accessible = true;
			break;
		}
	}
	if (!any_accessible)
		return;
	class sysfs_tunable *st;

	st = new sysfs_tunable(str, _sysfs_path, _target_content);
	all_tunables.push_back(st);
}

static void add_sata_callback(const std::string &d_name)
{
	std::string filename;
	filename = std::format("/sys/class/scsi_host/{}/link_power_management_policy", d_name);
	if (access(filename.c_str(), R_OK) != 0)
		return;

	add_sysfs_tunable(pt_format(_("Enable SATA link power management for {}"), d_name), filename, "med_power_with_dipm");
}

void add_sata_tunables(void)
{
	process_directory("/sys/class/scsi_host/", add_sata_callback);
}

void sysfs_tunable::collect_json_fields(std::string &_js)
{
    tunable::collect_json_fields(_js);
    JSON_FIELD(sysfs_path);
    JSON_FIELD(target_value);
    JSON_FIELD(bad_value);
}
