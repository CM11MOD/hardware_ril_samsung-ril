/**
 * This file is part of samsung-ril.
 *
 * Copyright (C) 2010-2011 Joerie de Gram <j.de.gram@gmail.com>
 * Copyright (C) 2011-2012 Paul Kocialkowski <contact@oaulk.fr>
 *
 * samsung-ril is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * samsung-ril is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with samsung-ril.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define LOG_TAG "RIL-DISP"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

/**
 * Converts IPC RSSI to Android RIL format
 */
void ipc2ril_rssi(unsigned char rssi, RIL_SignalStrength *ss)
{
	int ril_rssi;

	memset(ss, 0, sizeof(ss));

	if(rssi > 0x6f) {
		ril_rssi = 0;
	} else {
		ril_rssi = (((rssi - 0x71) * -1) - ((rssi - 0x71) * -1) % 2) / 2;
		if(ril_rssi > 31)
			ril_rssi = 31;
	}

	LOGD("Signal Strength is %d\n", ril_rssi);

	ss->GW_SignalStrength.signalStrength = ril_rssi;
	ss->GW_SignalStrength.bitErrorRate = 99;

	/* Send CDMA and EVDO levels even in GSM mode */
	ss->CDMA_SignalStrength.dbm = ril_rssi;
	ss->CDMA_SignalStrength.ecio = 200;

	ss->EVDO_SignalStrength.dbm = ril_rssi;
	ss->EVDO_SignalStrength.ecio = 200;
}

void ril_request_signal_strength(RIL_Token t)
{
	unsigned char request = 1;

	ipc_fmt_send(IPC_DISP_ICON_INFO, IPC_TYPE_GET, &request, sizeof(request), ril_request_get_id(t));
}

void ipc_disp_icon_info(struct ipc_message_info *info)
{
	struct ipc_disp_icon_info *icon_info = (struct ipc_disp_icon_info *) info->data;
	RIL_SignalStrength ss;

	/* Don't consider this if modem isn't in normal power mode. */
	if(ril_data.state.power_state != IPC_PWR_PHONE_STATE_NORMAL)
		return;

	if(info->type == IPC_TYPE_NOTI && icon_info->rssi == 0xff)
		return;

	ipc2ril_rssi(icon_info->rssi, &ss);

	if(info->type == IPC_TYPE_NOTI) {
		LOGD("Unsol request!");
		ril_request_unsolicited(RIL_UNSOL_SIGNAL_STRENGTH, &ss, sizeof(ss));
	} else if(info->type == IPC_TYPE_RESP) {
		LOGD("Sol request!");
		ril_request_complete(ril_request_get_token(info->aseq), RIL_E_SUCCESS, &ss, sizeof(ss));
	}
}

void ipc_disp_rssi_info(struct ipc_message_info *info)
{
	struct ipc_disp_rssi_info *rssi_info = (struct ipc_disp_rssi_info *) info->data;
	RIL_SignalStrength ss;
	int rssi;

	/* Don't consider this if modem isn't in normal power mode. */
	if(ril_data.state.power_state != IPC_PWR_PHONE_STATE_NORMAL)
		return;

	ipc2ril_rssi(rssi_info->rssi, &ss);

	ril_request_unsolicited(RIL_UNSOL_SIGNAL_STRENGTH, &ss, sizeof(ss));
}
