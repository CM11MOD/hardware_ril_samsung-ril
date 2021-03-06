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

#define LOG_TAG "RIL-PWR"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

/*
 * Modem lets us know it's powered on. Though, it's still in LPM and should
 * be considered as OFF. This request is used as a first indication that
 * we can communicate with the modem, so unlock RIL start from here.
 */

void ipc_pwr_phone_pwr_up(void)
{
	ril_radio_state_update(RADIO_STATE_OFF);
}

void ipc_pwr_phone_reset(void)
{
	ril_radio_state_update(RADIO_STATE_OFF);
}

void ipc_pwr_phone_state(struct ipc_message_info *info)
{
	unsigned char state;

	if (info->data == NULL || info->length < sizeof(unsigned char))
		return;

	state = *((unsigned char *) info->data);

	switch (state) {
		case IPC_PWR_R(IPC_PWR_PHONE_STATE_LPM):
			RIL_LOGD("Got power to LPM");

			if (ril_data.tokens.radio_power != RIL_TOKEN_NULL) {
				ril_request_complete(ril_data.tokens.radio_power, RIL_E_SUCCESS, NULL, 0);
				ril_data.tokens.radio_power = RIL_TOKEN_NULL;
			}

			ril_radio_state_update(RADIO_STATE_OFF);
			break;
		case IPC_PWR_R(IPC_PWR_PHONE_STATE_NORMAL):
			RIL_LOGD("Got power to NORMAL");

			if (ril_data.tokens.radio_power != RIL_TOKEN_NULL) {
				ril_request_complete(ril_data.tokens.radio_power, RIL_E_SUCCESS, NULL, 0);
				ril_data.tokens.radio_power = RIL_TOKEN_NULL;
			}

			ril_radio_state_update(RADIO_STATE_SIM_NOT_READY);
			break;
	}

	ril_tokens_check();
}

void ril_request_radio_power(RIL_Token t, void *data, int length)
{
	int power_state;
	unsigned short power_data;

	if (data == NULL || length < (int) sizeof(int))
		return;

	if (ril_radio_state_complete(RADIO_STATE_UNAVAILABLE, t))
		return;

	power_state = *((int *) data);

	RIL_LOGD("requested power_state is %d", power_state);

	if (power_state > 0) {
		RIL_LOGD("Request power to NORMAL");
		power_data = IPC_PWR_PHONE_STATE_NORMAL;

		ipc_gen_phone_res_expect_to_abort(ril_request_get_id(t), IPC_PWR_PHONE_STATE);
		ipc_fmt_send(IPC_PWR_PHONE_STATE, IPC_TYPE_EXEC, (void *) &power_data, sizeof(power_data), ril_request_get_id(t));

		ril_data.tokens.radio_power = t;
	} else {
		RIL_LOGD("Request power to LPM");
		power_data = IPC_PWR_PHONE_STATE_LPM;

		ipc_gen_phone_res_expect_to_abort(ril_request_get_id(t), IPC_PWR_PHONE_STATE);
		ipc_fmt_send(IPC_PWR_PHONE_STATE, IPC_TYPE_EXEC, (void *) &power_data, sizeof(power_data), ril_request_get_id(t));

		ril_data.tokens.radio_power = t;
	}
}
