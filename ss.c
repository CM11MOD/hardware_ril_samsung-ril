/*
 * This file is part of Samsung-RIL.
 *
 * Copyright (C) 2013 Paul Kocialkowski <contact@paulk.fr>
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

#define LOG_TAG "RIL-SS"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

void ipc_ss_ussd_complete(struct ipc_message_info *info)
{
	struct ipc_gen_phone_res *phone_res;
	int rc;

	phone_res = (struct ipc_gen_phone_res *) info->data;

	rc = ipc_gen_phone_res_check(phone_res);
	if (rc < 0) {
		RIL_LOGE("There was an error, aborting USSD request");

		ril_request_complete(ril_request_get_token(info->aseq), RIL_E_GENERIC_FAILURE, NULL, 0);
		ril_data.state.ussd_state = 0;

		return;
	}

	ril_request_complete(ril_request_get_token(info->aseq), RIL_E_SUCCESS, NULL, 0);
}

void ril_request_send_ussd(RIL_Token t, void *data, size_t length)
{
	char *data_enc = NULL;
	int data_enc_len = 0;

	char *message =NULL;
	struct ipc_ss_ussd *ussd = NULL;

	int message_size = 0xc0;

	if (data == NULL || length < (int) sizeof(char *))
		goto error;

	if (ril_radio_state_complete(RADIO_STATE_OFF, t))
		return;

	switch (ril_data.state.ussd_state) {
		case 0:
		case IPC_SS_USSD_NO_ACTION_REQUIRE:
		case IPC_SS_USSD_TERMINATED_BY_NET:
		case IPC_SS_USSD_OTHER_CLIENT:
		case IPC_SS_USSD_NOT_SUPPORT:
		case IPC_SS_USSD_TIME_OUT:
			RIL_LOGD("USSD Tx encoding is GSM7");

			data_enc_len = ascii2gsm7_ussd(data, (unsigned char**)&data_enc, length);
			if (data_enc_len > message_size) {
				RIL_LOGE("USSD message size is too long, aborting");
				ril_request_complete(t, RIL_E_GENERIC_FAILURE, NULL, 0);

				free(data_enc);

				return;
			}

			message = malloc(message_size);
			memset(message, 0, message_size);

			ussd = (struct ipc_ss_ussd *) message;
			ussd->state = IPC_SS_USSD_NO_ACTION_REQUIRE;
			ussd->dcs = 0x0f; // GSM7 in that case
			ussd->length = data_enc_len;

			memcpy((void *) (message + sizeof(struct ipc_ss_ussd)), data_enc, data_enc_len);

			free(data_enc);

			break;
		case IPC_SS_USSD_ACTION_REQUIRE:
		default:
			RIL_LOGD("USSD Tx encoding is ASCII");

			data_enc_len = asprintf(&data_enc, "%s", (char*)data);

			if (data_enc_len > message_size) {
				RIL_LOGE("USSD message size is too long, aborting");
				ril_request_complete(t, RIL_E_GENERIC_FAILURE, NULL, 0);

				free(data_enc);

				return;
			}

			message = malloc(message_size);
			memset(message, 0, message_size);

			ussd = (struct ipc_ss_ussd *) message;
			ussd->state = IPC_SS_USSD_ACTION_REQUIRE;
			ussd->dcs = 0x0f; // ASCII in that case
			ussd->length = data_enc_len;

			memcpy((void *) (message + sizeof(struct ipc_ss_ussd)), data_enc, data_enc_len);

			free(data_enc);

			break;
	}

	if (message == NULL) {
		RIL_LOGE("USSD message is empty, aborting");

		ril_request_complete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	ipc_gen_phone_res_expect_to_func(ril_request_get_id(t), IPC_SS_USSD,
		ipc_ss_ussd_complete);

	ipc_fmt_send(IPC_SS_USSD, IPC_TYPE_EXEC, (void *) message, message_size, ril_request_get_id(t));

	return;

error:
	ril_request_complete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ril_request_cancel_ussd(RIL_Token t, void *data, size_t length)
{
	struct ipc_ss_ussd ussd;

	if (ril_radio_state_complete(RADIO_STATE_OFF, t))
		return;

	memset(&ussd, 0, sizeof(ussd));

	ussd.state = IPC_SS_USSD_TERMINATED_BY_NET;
	ril_data.state.ussd_state = IPC_SS_USSD_TERMINATED_BY_NET;

	ipc_gen_phone_res_expect_to_complete(ril_request_get_id(t), IPC_SS_USSD);

	ipc_fmt_send(IPC_SS_USSD, IPC_TYPE_EXEC, (void *) &ussd, sizeof(ussd), ril_request_get_id(t));
}

void ipc2ril_ussd_state(struct ipc_ss_ussd *ussd, char *message[2])
{
	if (ussd == NULL || message == NULL)
		return;

	switch (ussd->state) {
		case IPC_SS_USSD_NO_ACTION_REQUIRE:
			asprintf(&message[0], "%d", 0);
			break;
		case IPC_SS_USSD_ACTION_REQUIRE:
			asprintf(&message[0], "%d", 1);
			break;
		case IPC_SS_USSD_TERMINATED_BY_NET:
			asprintf(&message[0], "%d", 2);
			break;
		case IPC_SS_USSD_OTHER_CLIENT:
			asprintf(&message[0], "%d", 3);
			break;
		case IPC_SS_USSD_NOT_SUPPORT:
			asprintf(&message[0], "%d", 4);
			break;
		case IPC_SS_USSD_TIME_OUT:
			asprintf(&message[0], "%d", 5);
			break;
	}
}

void ipc_ss_ussd(struct ipc_message_info *info)
{
	char *data_dec = NULL;
	int data_dec_len = 0;
	SmsCodingScheme codingScheme;

	char *message[2];

	struct ipc_ss_ussd *ussd = NULL;
	unsigned char state;

	if (info->data == NULL || info->length < sizeof(struct ipc_ss_ussd))
		goto error;

	memset(message, 0, sizeof(message));

	ussd = (struct ipc_ss_ussd *) info->data;

	ipc2ril_ussd_state(ussd, message);

	ril_data.state.ussd_state = ussd->state;

	if (ussd->length > 0 && info->length > 0 && info->data != NULL) {
		codingScheme = sms_get_coding_scheme(ussd->dcs);
		switch (codingScheme) {
			case SMS_CODING_SCHEME_GSM7:
				RIL_LOGD("USSD Rx encoding is GSM7");

				data_dec_len = gsm72ascii(info->data
					+ sizeof(struct ipc_ss_ussd), &data_dec, info->length - sizeof(struct ipc_ss_ussd));
				asprintf(&message[1], "%s", data_dec);
				message[1][data_dec_len] = '\0';

				break;
			case SMS_CODING_SCHEME_UCS2:
				RIL_LOGD("USSD Rx encoding %x is UCS2", ussd->dcs);

				data_dec_len = info->length - sizeof(struct ipc_ss_ussd);
				message[1] = malloc(data_dec_len * 4 + 1);

				int i, result = 0;
				char *ucs2 = (char*)info->data + sizeof(struct ipc_ss_ussd);
				for (i = 0; i < data_dec_len; i += 2) {
					int c = (ucs2[i] << 8) | ucs2[1 + i];
					result += utf8_write(message[1], result, c);
				}
				message[1][result] = '\0';
				break;
			default:
				RIL_LOGD("USSD Rx encoding %x is unknown, assuming ASCII",
					ussd->dcs);

				data_dec_len = info->length - sizeof(struct ipc_ss_ussd);
				asprintf(&message[1], "%s", info->data + sizeof(struct ipc_ss_ussd));
				message[1][data_dec_len] = '\0';
				break;
		}
	}

	ril_request_unsolicited(RIL_UNSOL_ON_USSD, message, sizeof(message));

	return;

error:
	ril_request_complete(ril_request_get_token(info->aseq), RIL_E_GENERIC_FAILURE, NULL, 0);
}
