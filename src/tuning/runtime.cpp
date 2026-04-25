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
#include "unistd.h"
#include "runtime.h"
#include <string.h>
#include <utility>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <format>

#include "../lib.h"
#include "../devices/runtime_pm.h"

runtime_tunable::runtime_tunable(const char *path, const char *bus, const char *dev, const char *port) : tunable("", 0.4, _("Good"), _("Bad"), _("Unknown"))
{
	ifstream file;
	runtime_path = std::format("{}/power/control", path);


	desc = std::format(_("Runtime PM for {} device {}"), bus, dev);
	if (!device_has_runtime_pm(path)) {
		desc = std::format(_("{} device {} has no runtime power management"), bus, dev);
	}

	if (strcmp(bus, "pci") == 0) {
		char filename[PATH_MAX];
		uint16_t vendor = 0, device = 0;

		snprintf(filename, sizeof(filename), "/sys/bus/%s/devices/%s/vendor", bus, dev);

		file.open(filename, ios::in);
		if (file) {
			file >> hex >> vendor;
			file.close();
		}


		snprintf(filename, sizeof(filename), "/sys/bus/%s/devices/%s/device", bus, dev);
		file.open(filename, ios::in);
		if (file) {
			file >> hex >> device;
			file.close();
		}

		if (vendor && device) {
			if (!device_has_runtime_pm(path)) {
				desc = std::format(_("PCI Device {} has no runtime power management"), pci_id_to_name(vendor, device, filename, 4095));
			} else {
				desc = std::format(_("Runtime PM for PCI Device {}"), pci_id_to_name(vendor, device, filename, 4095));
			}
		}

		if (string(path).find("ata") != string::npos) {
			desc = std::format(_("Runtime PM for port {} of PCI device: {}"), port, pci_id_to_name(vendor, device, filename, 4095));
		}

		if (string(path).find("block") != string::npos) {
			desc = std::format(_("Runtime PM for disk {}"), port);
		}

	}
	toggle_good = std::format("echo 'auto' > '{}';", runtime_path);
	toggle_bad = std::format("echo 'on' > '{}';", runtime_path);
}

int runtime_tunable::good_bad(void)
{
	std::string content;

	content = read_sysfs_string(runtime_path.c_str());

	if (content == "auto")
		return TUNE_GOOD;
	if (content == "on")
		return TUNE_GOOD;

	return TUNE_BAD;
}

void runtime_tunable::toggle(void)
{
	int good;
	good = good_bad();

	if (good == TUNE_GOOD) {
		write_sysfs(runtime_path.c_str(), "on");
		return;
	}

	write_sysfs(runtime_path.c_str(), "auto");
}

const char *runtime_tunable::toggle_script(void)
{
	int good;
	good = good_bad();

	if (good == TUNE_GOOD) {
		return toggle_bad.c_str();
	}

	return toggle_good.c_str();
}


void add_runtime_tunables(const char *bus)
{
	struct dirent *entry;
	DIR *dir;
	char filename[PATH_MAX], port[PATH_MAX];
	int max_ports = 32, count=0;

	snprintf(filename, sizeof(filename), "/sys/bus/%s/devices/", bus);
	dir = opendir(filename);
	if (!dir)
		return;
	while (1) {
		class runtime_tunable *runtime, *runtime_ahci_port, *runtime_ahci_disk;

		entry = readdir(dir);

		if (!entry)
			break;
		if (entry->d_name[0] == '.')
			continue;

		snprintf(filename, sizeof(filename), "/sys/bus/%s/devices/%s/power/control", bus, entry->d_name);

		if (access(filename, R_OK) != 0)
			continue;


		snprintf(filename, sizeof(filename), "/sys/bus/%s/devices/%s", bus, entry->d_name);

		runtime = new class runtime_tunable(filename, bus, entry->d_name, NULL);

		if (!device_has_runtime_pm(filename))
			all_untunables.push_back(runtime);
		else
			all_tunables.push_back(runtime);

		for (int i=0; i < max_ports; i++) {
			snprintf(port, sizeof(port), "ata%d", i);
			snprintf(filename, sizeof(filename), "/sys/bus/%s/devices/%s/%s/power/control", bus, entry->d_name, port);

			if (access(filename, R_OK) != 0)
				continue;

			snprintf(filename, sizeof(filename), "/sys/bus/%s/devices/%s/%s", bus, entry->d_name, port);
			runtime_ahci_port = new class runtime_tunable(filename, bus, entry->d_name, port);

			if (!device_has_runtime_pm(filename))
				all_untunables.push_back(runtime_ahci_port);
			else
				all_tunables.push_back(runtime_ahci_port);
		}

		for (char blk = 'a'; blk <= 'z'; blk++)
		{
			if (count != 0)
				break;

			snprintf(filename, sizeof(filename), "/sys/block/sd%c/device/power/control", blk);

			if (access(filename, R_OK) != 0)
				continue;

			snprintf(port, sizeof(port), "sd%c", blk);
			snprintf(filename, sizeof(filename), "/sys/block/%s/device", port);
			runtime_ahci_disk = new class runtime_tunable(filename, bus, entry->d_name, port);
			if (!device_has_runtime_pm(filename))
				all_untunables.push_back(runtime_ahci_disk);
			else
				all_tunables.push_back(runtime_ahci_disk);
		}
		count = 1;

	}
	closedir(dir);
}
