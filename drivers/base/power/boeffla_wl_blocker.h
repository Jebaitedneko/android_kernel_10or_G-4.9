/*
 * Author: andip71, 01.09.2017
 *
 * Version 1.1.0
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define BOEFFLA_WL_BLOCKER_VERSION	"1.1.0"

#define LIST_WL_DEFAULT				"ApmAudio;ApmOutput;IPA_WS;IPA_RM12;CHG_PLCY_MAIN_WL;CHG_PLCY_Legacy_WL;NETLINK;[timerfd];fp_wakelock;event0;event1;event2;event3;event4;event5;eventpoll;KeyEvents;video0;video1;video2;video3;video32;video33;qcom_rx_wakelock;wlan;wlan_wow_wl;wlan_extscan_wl;netmgr_wl;"

#define LENGTH_LIST_WL				512
#define LENGTH_LIST_WL_DEFAULT		512
#define LENGTH_LIST_WL_SEARCH		LENGTH_LIST_WL + LENGTH_LIST_WL_DEFAULT + 5
