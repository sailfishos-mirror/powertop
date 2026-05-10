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
#pragma once

#include <string>
#include <sys/time.h>
#include "device.h"

class xegpu: public device {
	int index = 0;
	int rindex = 0;

	/* Non-empty when the xe hwmon exposes an energy counter. */
	std::string hwmon_energy_path;
	double      last_energy    = 0.0;
	struct timeval last_time   = {};
	double      consumed_power = 0.0;

public:
	xegpu(const std::string &energy_path);

	virtual void start_measurement(void) override;
	virtual void end_measurement(void) override;

	virtual double utilization(void) const override;

	virtual std::string class_name(void) const override { return "GPU"; }
	virtual std::string device_name(void) const override { return "GPU"; }
	virtual std::string human_name(void) override { return _("Intel Xe GPU"); }
	virtual double power_usage(struct result_bundle *result,
				   struct parameter_bundle *bundle) override;
	virtual bool show_in_list(void) const override { return false; }
	virtual std::string util_units(void) const override { return _(" ops/s"); }

	void collect_json_fields(std::string &_js) override;
};

extern void create_xe_gpu(void);
