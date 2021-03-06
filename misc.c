/*
 * This file is part of Samsung-RIL.
 *
 * Copyright (C) 2010-2011 Joerie de Gram <j.de.gram@gmail.com>
 * Copyright (C) 2011-2013 Paul Kocialkowski <contact@paulk.fr>
 *
 * Samsung-RIL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Samsung-RIL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Samsung-RIL.  If not, see <http://www.gnu.org/licenses/>.
 */

#define LOG_TAG "RIL-MISC"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

void ril_request_get_imei_send(RIL_Token t)
{
	unsigned char data;

	data = IPC_MISC_ME_SN_SERIAL_NUM;
	ipc_fmt_send(IPC_MISC_ME_SN, IPC_TYPE_GET, (unsigned char *) &data, sizeof(data), ril_request_get_id(t));
}

void ril_request_get_imei(RIL_Token t)
{
	if (ril_radio_state_complete(RADIO_STATE_OFF, t))
		return;

	if (ril_data.tokens.get_imei) {
		RIL_LOGD("Another IMEI request is waiting, aborting");
		ril_request_complete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	ril_data.tokens.get_imei = t;

	if (ril_data.tokens.get_imeisv) {
		RIL_LOGD("IMEISV token found: %p", ril_data.tokens.get_imeisv);

		ril_request_get_imei_send(ril_data.tokens.get_imei);
	} else {
		RIL_LOGD("Waiting for IMEISV token");
	}
}

void ril_request_get_imeisv(RIL_Token t)
{
	if (ril_radio_state_complete(RADIO_STATE_OFF, t))
		return;

	if (ril_data.tokens.get_imeisv) {
		RIL_LOGD("Another IMEISV request is waiting, aborting");
		ril_request_complete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	ril_data.tokens.get_imeisv = t;

	if (ril_data.tokens.get_imei) {
		RIL_LOGD("IMEI token found: %p", ril_data.tokens.get_imei);

		ril_request_get_imei_send(ril_data.tokens.get_imei);
	} else {
		RIL_LOGD("Waiting for IMEI token");
	}
}

void ipc_misc_me_sn_imei(struct ipc_message_info *info)
{
	struct ipc_misc_me_sn *imei_info;
	RIL_Token t;
	char imei[33];
	char imeisv[3];

	if (info->type != IPC_TYPE_RESP)
		goto error;

	if (info->data == NULL || info->length < sizeof(struct ipc_misc_me_sn))
		goto error;

	imei_info = (struct ipc_misc_me_sn *) info->data;
	t = ril_request_get_token(info->aseq);

	if (ril_data.tokens.get_imei != t) 
		RIL_LOGE("IMEI tokens mismatch (%p and %p)",
			ril_data.tokens.get_imei, t);

	if (imei_info->length > 32)
		return;

	memset(imei, 0, sizeof(imei));
	memset(imeisv, 0, sizeof(imeisv));

	memcpy(imei, imei_info->data, imei_info->length);

	// Last two bytes of IMEI in imei_info are the SV bytes
	memcpy(imeisv, (imei_info->data + imei_info->length - 2), 2);

	// In case of token mismatch, complete both requests
	if (t && ril_data.tokens.get_imei != t) {
		ril_request_complete(t, RIL_E_SUCCESS, imei, sizeof(char *));
	}

	// IMEI
	if (ril_data.tokens.get_imei) {
		ril_request_complete(ril_data.tokens.get_imei,
			RIL_E_SUCCESS, imei, sizeof(char *));
		ril_data.tokens.get_imei = 0;
	}

	// IMEI SV
	if (ril_data.tokens.get_imeisv) {
		ril_request_complete(ril_data.tokens.get_imeisv,
			RIL_E_SUCCESS, imeisv, sizeof(char *));
		ril_data.tokens.get_imeisv = 0;
	}

	return;

error:
	ril_request_complete(ril_request_get_token(info->aseq), RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ipc_misc_me_sn(struct ipc_message_info *info)
{
	struct ipc_misc_me_sn *me_sn_info;

	if (info->type != IPC_TYPE_RESP)
		return;

	if (info->data == NULL || info->length < sizeof(struct ipc_misc_me_sn))
		goto error;

	me_sn_info = (struct ipc_misc_me_sn *) info->data;

	switch (me_sn_info->type) {
		case IPC_MISC_ME_SN_SERIAL_NUM:
			ipc_misc_me_sn_imei(info);
			break;
		case IPC_MISC_ME_SN_SERIAL_NUM_SERIAL:
			RIL_LOGD("Got IPC_MISC_ME_SN_SERIAL_NUM_SERIAL: %s\n",
				me_sn_info->data);
			break;
	}

	return;

error:
	ril_request_complete(ril_request_get_token(info->aseq), RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ril_request_baseband_version(RIL_Token t)
{
	unsigned char data;

	if (ril_radio_state_complete(RADIO_STATE_OFF, t))
		return;

	if (ril_data.tokens.baseband_version) {
		RIL_LOGD("Another Baseband version request is waiting, aborting");
		ril_request_complete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	ril_data.tokens.baseband_version = t;

	data = 0xff;

	ipc_fmt_send(IPC_MISC_ME_VERSION, IPC_TYPE_GET, (unsigned char *) &data, sizeof(data), ril_request_get_id(t));
}

void ipc_misc_me_version(struct ipc_message_info *info)
{
	char sw_version[33];
	struct ipc_misc_me_version *version;
	RIL_Token t;

	if (info->type != IPC_TYPE_RESP)
		return;

	if (info->data == NULL || info->length < sizeof(struct ipc_misc_me_version))
		goto error;

	version = (struct ipc_misc_me_version *) info->data;
	t = ril_request_get_token(info->aseq);

	if (ril_data.tokens.baseband_version != t) 
		RIL_LOGE("Baseband tokens mismatch (%p and %p)",
			ril_data.tokens.baseband_version, t);

	memcpy(sw_version, version->sw_version, 32);
	sw_version[32] = '\0';

	ril_request_complete(t, RIL_E_SUCCESS, sw_version, sizeof(sw_version));
	ril_data.tokens.baseband_version = 0;

	return;

error:
	ril_request_complete(ril_request_get_token(info->aseq), RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ril_request_get_imsi(RIL_Token t)
{
	if (ril_radio_state_complete(RADIO_STATE_OFF, t))
		return;

	ipc_fmt_send_get(IPC_MISC_ME_IMSI, ril_request_get_id(t));
}

void ipc_misc_me_imsi(struct ipc_message_info *info)
{
	unsigned char imsi_length;
	char *imsi;

	if (info->type != IPC_TYPE_RESP)
		return;

	if (info->data == NULL || info->length < sizeof(unsigned char))
		goto error;

	imsi_length = *((unsigned char *) info->data);

	if (((int) info->length) < imsi_length + 1) {
		RIL_LOGE("%s: missing IMSI data", __func__);
		ril_request_complete(ril_request_get_token(info->aseq),
			RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	imsi = (char *) calloc(1, imsi_length + 1);
	memcpy(imsi, ((unsigned char *) info->data) + sizeof(unsigned char), imsi_length);
	imsi[imsi_length] = '\0';

	ril_request_complete(ril_request_get_token(info->aseq), RIL_E_SUCCESS, imsi, imsi_length + 1);

	free(imsi);

	return;

error:
	ril_request_complete(ril_request_get_token(info->aseq), RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ipc_misc_time_info(struct ipc_message_info *info)
{
	struct ipc_misc_time_info *nitz;
	char str[128];

	if (info->data == NULL || info->length < sizeof(struct ipc_misc_time_info))
		return;

	nitz = (struct ipc_misc_time_info *) info->data;

	sprintf(str, "%02u/%02u/%02u,%02u:%02u:%02u%c%02d,%02d",
		nitz->year, nitz->mon, nitz->day, nitz->hour,
		nitz->min, nitz->sec, nitz->tz < 0 ? '-' : '+',
		nitz->tz < 0 ? -nitz->tz : nitz->tz, nitz->dl);

	ril_request_unsolicited(RIL_UNSOL_NITZ_TIME_RECEIVED,
		str, strlen(str) + 1);
}
