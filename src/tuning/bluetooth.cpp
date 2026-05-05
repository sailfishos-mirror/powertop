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
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <utility>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <format>

#include "../lib.h"
#include "bluetooth.h"

bt_tunable::bt_tunable(void) : tunable("", 1.0, _("Good"), _("Bad"), _("Unknown"))
{
	desc = _("Bluetooth device interface status");
	toggle_bad = "Enable Bluetooth (hci0)";
	toggle_good = "Disable Bluetooth (hci0)";
}



/* structure definitions copied from include/net/bluetooth/hci.h from the 2.6.20 kernel */
#define HCIGETDEVINFO   _IOR('H', 211, int)
#define HCIDEVUP        _IOW('H', 201, int)
#define HCIDEVDOWN      _IOW('H', 202, int)
#define BTPROTO_HCI     1

#define __u16 uint16_t
#define __u8 uint8_t
#define __u32 uint32_t

typedef struct {
        __u8 b[6];
} __attribute__((packed)) bdaddr_t;

struct hci_dev_stats {
        __u32 err_rx;
        __u32 err_tx;
        __u32 cmd_tx;
        __u32 evt_rx;
        __u32 acl_tx;
        __u32 acl_rx;
        __u32 sco_tx;
        __u32 sco_rx;
        __u32 byte_rx;
        __u32 byte_tx;
};


struct hci_dev_info {
	__u16 dev_id;
	char  name[8];

	bdaddr_t bdaddr;

	__u32 flags;
	__u8  type;

	__u8  features[8];

	__u32 pkt_type;
	__u32 link_policy;
	__u32 link_mode;

	__u16 acl_mtu;
	__u16 acl_pkts;
	__u16 sco_mtu;
	__u16 sco_pkts;

	struct hci_dev_stats stat;
};

static int previous_bytes = -1;

int bt_tunable::good_bad(void)
{
	struct hci_dev_info devinfo;
	int fd;
	int thisbytes;
	int ret;

	fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
	if (fd < 0)
		return TUNE_GOOD;

	memset(&devinfo, 0, sizeof(devinfo));
	strcpy(devinfo.name, "hci0");
	ret = ioctl(fd, HCIGETDEVINFO, (void *) &devinfo);
	close(fd);
	if (ret < 0)
		return TUNE_GOOD;

	/* Interface already down — already in a good power state */
	if ((devinfo.flags & 1) == 0)
		return TUNE_GOOD;

	thisbytes = devinfo.stat.byte_rx + devinfo.stat.byte_tx;

	/* Bytes changed since last check: BT is actively in use, leave it alone */
	if (thisbytes != previous_bytes) {
		previous_bytes = thisbytes;
		return TUNE_GOOD;
	}

	previous_bytes = thisbytes;

	/* No byte activity: BT is idle and should be turned off */
	return TUNE_BAD;
}

void bt_tunable::toggle(void)
{
	int good = good_bad();
	int dev_id = 0; /* hci0 */

	int fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
	if (fd < 0)
		return;

	ioctl(fd, good == TUNE_BAD ? HCIDEVDOWN : HCIDEVUP, dev_id);
	close(fd);
}


void add_bt_tunable(void)
{
	struct hci_dev_info devinfo;
	class bt_tunable *bt;
	int fd;
	int ret;

	/* first check if /sys/modules/bluetooth exists, if not, don't probe bluetooth because
	   it would trigger an autoload */

//	if (access("/sys/module/bluetooth",F_OK))
//		return;


	/* check if hci0 exists */
	fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
	if (fd < 0)
		return;

	memset(&devinfo, 0, sizeof(devinfo));
	strcpy(devinfo.name, "hci0");
	ret = ioctl(fd, HCIGETDEVINFO, (void *) &devinfo);
	close(fd);
	if (ret < 0)
		return;


	bt = new bt_tunable();
	all_tunables.push_back(bt);
}

void bt_tunable::collect_json_fields(std::string &_js)
{
    tunable::collect_json_fields(_js);
}
