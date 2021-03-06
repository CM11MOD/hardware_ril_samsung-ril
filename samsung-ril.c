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

#define LOG_TAG "RIL"
#include "samsung-ril.h"
#include "util.h"

/*
 * RIL data
 */

struct ril_data ril_data;

/*
 * RIL requests
 */

int ril_request_id_get(void)
{
	ril_data.request_id++;
	ril_data.request_id %= 0xff;

	return ril_data.request_id;
}

int ril_request_id_set(int id)
{
	id %= 0xff;

	while (ril_data.request_id < id) {
		ril_data.request_id++;
		ril_data.request_id %= 0xff;
	}

	return ril_data.request_id;
}

int ril_request_register(RIL_Token t, int id)
{
	struct ril_request_info *request;
	struct list_head *list_end;
	struct list_head *list;

	request = calloc(1, sizeof(struct ril_request_info));
	if (request == NULL)
		return -1;

	request->token = t;
	request->id = id;
	request->canceled = 0;

	list_end = ril_data.requests;
	while (list_end != NULL && list_end->next != NULL)
		list_end = list_end->next;

	list = list_head_alloc((void *) request, list_end, NULL);

	if (ril_data.requests == NULL)
		ril_data.requests = list;

	return 0;
}

void ril_request_unregister(struct ril_request_info *request)
{
	struct list_head *list;

	if (request == NULL)
		return;

	list = ril_data.requests;
	while (list != NULL) {
		if (list->data == (void *) request) {
			memset(request, 0, sizeof(struct ril_request_info));
			free(request);

			if (list == ril_data.requests)
				ril_data.requests = list->next;

			list_head_free(list);

			break;
		}
list_continue:
		list = list->next;
	}
}

struct ril_request_info *ril_request_info_find_id(int id)
{
	struct ril_request_info *request;
	struct list_head *list;

	list = ril_data.requests;
	while (list != NULL) {
		request = (struct ril_request_info *) list->data;
		if (request == NULL)
			goto list_continue;

		if (request->id == id)
			return request;

list_continue:
		list = list->next;
	}

	return NULL;
}

struct ril_request_info *ril_request_info_find_token(RIL_Token t)
{
	struct ril_request_info *request;
	struct list_head *list;

	list = ril_data.requests;
	while (list != NULL) {
		request = (struct ril_request_info *) list->data;
		if (request == NULL)
			goto list_continue;

		if (request->token == t)
			return request;

list_continue:
		list = list->next;
	}

	return NULL;
}

int ril_request_set_canceled(RIL_Token t, int canceled)
{
	struct ril_request_info *request;

	request = ril_request_info_find_token(t);
	if (request == NULL)
		return -1;

	request->canceled = canceled ? 1 : 0;

	return 0;
}

int ril_request_get_canceled(RIL_Token t)
{
	struct ril_request_info *request;

	request = ril_request_info_find_token(t);
	if (request == NULL)
		return -1;

	return request->canceled;
}

RIL_Token ril_request_get_token(int id)
{
	struct ril_request_info *request;

	request = ril_request_info_find_id(id);
	if (request == NULL)
		return RIL_TOKEN_NULL;

	return request->token;
}

int ril_request_get_id(RIL_Token t)
{
	struct ril_request_info *request;
	int id, rc;

	request = ril_request_info_find_token(t);
	if (request != NULL)
		return request->id;

	id = ril_request_id_get();

	// Unregister a previous request with the same id
	request = ril_request_info_find_id(id);
	if (request != NULL)
		ril_request_unregister(request);

	rc = ril_request_register(t, id);
	if (rc < 0)
		return -1;

	return id;	
}

void ril_request_complete(RIL_Token t, RIL_Errno e, void *data, size_t length)
{
	struct ril_request_info *request;
	int canceled = 0;

	request = ril_request_info_find_token(t);
	if (request == NULL)
		goto complete;

	canceled = ril_request_get_canceled(t);
	ril_request_unregister(request);

	if (canceled)
		return;

complete:
	ril_data.env->OnRequestComplete(t, e, data, length);
}

void ril_request_unsolicited(int request, void *data, size_t length)
{
	ril_data.env->OnUnsolicitedResponse(request, data, length);
}

void ril_request_timed_callback(RIL_TimedCallback callback, void *data, const struct timeval *time)
{
	ril_data.env->RequestTimedCallback(callback, data, time);
}

/*
 * RIL radio state
 */

int ril_radio_state_complete(RIL_RadioState radio_state, RIL_Token token)
{
	RIL_Errno error = RIL_E_SUCCESS;

	// This goes from best case of failure to worst case of failure
	switch (radio_state) {
		case RADIO_STATE_SIM_NOT_READY:
			if (ril_data.state.radio_state == RADIO_STATE_SIM_NOT_READY)
				error = RIL_E_GENERIC_FAILURE;
		case RADIO_STATE_SIM_LOCKED_OR_ABSENT:
			if (ril_data.state.radio_state == RADIO_STATE_SIM_LOCKED_OR_ABSENT)
				error = RIL_E_GENERIC_FAILURE;
		case RADIO_STATE_OFF:
			if (ril_data.state.radio_state == RADIO_STATE_OFF)
				error = RIL_E_RADIO_NOT_AVAILABLE;
		case RADIO_STATE_UNAVAILABLE:
		default:
			if (ril_data.state.radio_state == RADIO_STATE_UNAVAILABLE)
				error = RIL_E_RADIO_NOT_AVAILABLE;
			break;
	}

	if (error  != RIL_E_SUCCESS) {
		if (token != RIL_TOKEN_NULL)
			ril_request_complete(token, error, NULL, 0);

		return 1;
	}

	return 0;
}

void ril_radio_state_update(RIL_RadioState radio_state)
{
	RIL_LOGD("Setting radio state to %d", radio_state);
	ril_data.state.radio_state = radio_state;

	ril_request_unsolicited(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);

	ril_tokens_check();
}

/*
 * RIL tokens
 */

void ril_tokens_check(void)
{
	RIL_Token t;

	if (ril_data.tokens.baseband_version != 0) {
		if (ril_data.state.radio_state != RADIO_STATE_OFF) {
			t = ril_data.tokens.baseband_version;
			ril_data.tokens.baseband_version = 0;
			ril_request_baseband_version(t);
		}
	}

	if (ril_data.tokens.get_imei != 0 && ril_data.tokens.get_imeisv != 0) {
		if (ril_data.state.radio_state != RADIO_STATE_OFF) {
			t = ril_data.tokens.get_imei;
			ril_data.tokens.get_imei = 0;
			ril_request_get_imei(t);
		}
	}
}

/*
 * Clients dispatch functions
 */

void ipc_fmt_dispatch(struct ipc_message_info *info)
{
	if (info == NULL)
		return;

	RIL_LOCK();

	ril_request_id_set(info->aseq);

	switch (IPC_COMMAND(info)) {
		/* GEN */
		case IPC_GEN_PHONE_RES:
			ipc_gen_phone_res(info);
			break;
		/* PWR */
		case IPC_PWR_PHONE_PWR_UP:
			ipc_pwr_phone_pwr_up();
			break;
		case IPC_PWR_PHONE_RESET:
			ipc_pwr_phone_reset();
			break;
		case IPC_PWR_PHONE_STATE:
			ipc_pwr_phone_state(info);
			break;
		/* DISP */
		case IPC_DISP_ICON_INFO:
			ipc_disp_icon_info(info);
			break;
		case IPC_DISP_RSSI_INFO:
			ipc_disp_rssi_info(info);
			break;
		/* MISC */
		case IPC_MISC_ME_SN:
			ipc_misc_me_sn(info);
			break;
		case IPC_MISC_ME_VERSION:
			ipc_misc_me_version(info);
			break;
		case IPC_MISC_ME_IMSI:
			ipc_misc_me_imsi(info);
			break;
		case IPC_MISC_TIME_INFO:
			ipc_misc_time_info(info);
			break;
		/* SAT */
#ifndef DISABLE_STK
		case IPC_SAT_PROACTIVE_CMD:
			ipc_sat_proactive_cmd(info);
			break;
		case IPC_SAT_ENVELOPE_CMD:
			ipc_sat_envelope_cmd(info);
			break;
#endif
		/* SS */
		case IPC_SS_USSD:
			ipc_ss_ussd(info);
			break;
		/* SEC */
		case IPC_SEC_SIM_STATUS:
			ipc_sec_sim_status(info);
			break;
		case IPC_SEC_SIM_ICC_TYPE:
			ipc_sec_sim_icc_type(info);
			break;
		case IPC_SEC_LOCK_INFO:
			ipc_sec_lock_info(info);
			break;
		case IPC_SEC_RSIM_ACCESS:
			ipc_sec_rsim_access(info);
			break;
		case IPC_SEC_PHONE_LOCK:
			ipc_sec_phone_lock(info);
			break;
		/* NET */
		case IPC_NET_CURRENT_PLMN:
			ipc_net_current_plmn(info);
			break;
		case IPC_NET_REGIST:
			ipc_net_regist(info);
			break;
		case IPC_NET_PLMN_LIST:
			ipc_net_plmn_list(info);
			break;
		case IPC_NET_PLMN_SEL:
			ipc_net_plmn_sel(info);
			break;
		case IPC_NET_MODE_SEL:
			ipc_net_mode_sel(info);
			break;
		/* SMS */
		case IPC_SMS_INCOMING_MSG:
			ipc_sms_incoming_msg(info);
			break;
		case IPC_SMS_DELIVER_REPORT:
			ipc_sms_deliver_report(info);
			break;
		case IPC_SMS_SVC_CENTER_ADDR:
			ipc_sms_svc_center_addr(info);
			break;
		case IPC_SMS_SEND_MSG:
			ipc_sms_send_msg(info);
			break;
		case IPC_SMS_DEVICE_READY:
			ipc_sms_device_ready(info);
			break;
		case IPC_SMS_SAVE_MSG:
			ipc_sms_save_msg(info);
			break;
		case IPC_SMS_DEL_MSG:
			ipc_sms_del_msg(info);
			break;
		/* SVC */
		case IPC_SVC_DISPLAY_SCREEN:
			ipc_svc_display_screen(info);
			break;
		/* CALL */
		case IPC_CALL_INCOMING:
			ipc_call_incoming(info);
			break;
		case IPC_CALL_LIST:
			ipc_call_list(info);
			break;
		case IPC_CALL_STATUS:
			ipc_call_status(info);
			break;
		case IPC_CALL_BURST_DTMF:
			ipc_call_burst_dtmf(info);
			break;
		/* GPRS */
		case IPC_GPRS_IP_CONFIGURATION:
			ipc_gprs_ip_configuration(info);
			break;
		case IPC_GPRS_CALL_STATUS:
			ipc_gprs_call_status(info);
			break;
		case IPC_GPRS_PDP_CONTEXT:
			ipc_gprs_pdp_context(info);
			break;
		default:
			RIL_LOGE("%s: Unhandled request: %s (%04x)", __func__, ipc_command_to_str(IPC_COMMAND(info)), IPC_COMMAND(info));
			break;
	}

	RIL_UNLOCK();
}

void ipc_rfs_dispatch(struct ipc_message_info *info)
{
	if (info == NULL)
		return;

	RIL_LOCK();

	switch (IPC_COMMAND(info)) {
		case IPC_RFS_NV_READ_ITEM:
			ipc_rfs_nv_read_item(info);
			break;
		case IPC_RFS_NV_WRITE_ITEM:
			ipc_rfs_nv_write_item(info);
			break;
		default:
			RIL_LOGE("%s: Unhandled request: %s (%04x)", __func__, ipc_command_to_str(IPC_COMMAND(info)), IPC_COMMAND(info));
			break;
	}

	RIL_UNLOCK();
}

void srs_dispatch(struct srs_message *message)
{
	if (message == NULL)
		return;

	RIL_LOCK();

	switch (message->command) {
		case SRS_CONTROL_PING:
			srs_control_ping(message);
			break;
		case SRS_SND_SET_CALL_CLOCK_SYNC:
			srs_snd_set_call_clock_sync(message);
			break;
		case SRS_SND_SET_CALL_VOLUME:
			srs_snd_set_call_volume(message);
			break;
		case SRS_SND_SET_CALL_AUDIO_PATH:
			srs_snd_set_call_audio_path(message);
			break;
		default:
			RIL_LOGE("%s: Unhandled request: (%04x)", __func__, message->command);
			break;
	}

	RIL_UNLOCK();
}

/*
 * RIL interface
 */

void ril_on_request(int request, void *data, size_t length, RIL_Token t)
{
	RIL_LOCK();

	switch (request) {
		/* PWR */
		case RIL_REQUEST_RADIO_POWER:
			ril_request_radio_power(t, data, length);
			break;
		case RIL_REQUEST_BASEBAND_VERSION:
			ril_request_baseband_version(t);
			break;
		/* DISP */
		case RIL_REQUEST_SIGNAL_STRENGTH:
			ril_request_signal_strength(t);
			break;
		/* MISC */
		case RIL_REQUEST_GET_IMEI:
			ril_request_get_imei(t);
			break;
		case RIL_REQUEST_GET_IMEISV:
			ril_request_get_imeisv(t);
			break;
		case RIL_REQUEST_GET_IMSI:
			ril_request_get_imsi(t);
			break;
		/* SAT */
#ifndef DISABLE_STK
		case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING:
			ril_request_report_stk_service_is_running(t);
			break;
		case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE:
			ril_request_stk_send_terminal_response(t, data, length);
			break;
		case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND:
			ril_request_stk_send_envelope_command(t, data, length);
			break;
		case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM:
			ril_request_complete(t, RIL_E_SUCCESS, NULL, 0);
			break;
#endif
		/* SS */
		case RIL_REQUEST_SEND_USSD:
			ril_request_send_ussd(t, data, length);
			break;
		case RIL_REQUEST_CANCEL_USSD:
			ril_request_cancel_ussd(t, data, length);
			break;
		/* SEC */
		case RIL_REQUEST_GET_SIM_STATUS:
			ril_request_get_sim_status(t);
			break;
		case RIL_REQUEST_SIM_IO:
			ril_request_sim_io(t, data, length);
			break;
		case RIL_REQUEST_ENTER_SIM_PIN:
			ril_request_enter_sim_pin(t, data, length);
			break;
		case RIL_REQUEST_CHANGE_SIM_PIN:
			ril_request_change_sim_pin(t, data, length);
			break;
		case RIL_REQUEST_ENTER_SIM_PUK:
			ril_request_enter_sim_puk(t, data, length);
			break;
		case RIL_REQUEST_QUERY_FACILITY_LOCK:
			ril_request_query_facility_lock(t, data, length);
			break;
		case RIL_REQUEST_SET_FACILITY_LOCK:
			ril_request_set_facility_lock(t, data, length);
			break;
		/* SVC */
		case RIL_REQUEST_OEM_HOOK_RAW:
			ril_request_oem_hook_raw(t, data, length);
			break;
		/* NET */
		case RIL_REQUEST_OPERATOR:
			ril_request_operator(t);
			break;
#if RIL_VERSION >= 6
		case RIL_REQUEST_VOICE_REGISTRATION_STATE:
			ril_request_voice_registration_state(t);
			break;
		case RIL_REQUEST_DATA_REGISTRATION_STATE:
			ril_request_data_registration_state(t);
			break;
#else
		case RIL_REQUEST_REGISTRATION_STATE:
			ril_request_registration_state(t);
			break;
		case RIL_REQUEST_GPRS_REGISTRATION_STATE:
			ril_request_gprs_registration_state(t);
			break;
#endif
		case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS:
			ril_request_query_available_networks(t);
			break;
		case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE:
			ril_request_get_preferred_network_type(t);
			break;
		case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:
			ril_request_set_preferred_network_type(t, data, length);
			break;
		case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:
			ril_request_query_network_selection_mode(t);
			break;
		case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:
			ril_request_set_network_selection_automatic(t);
			break;
		case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:
			ril_request_set_network_selection_manual(t, data, length);
			break;
		/* SMS */
		case RIL_REQUEST_SEND_SMS:
			ril_request_send_sms(t, data, length);
			break;
		case RIL_REQUEST_SEND_SMS_EXPECT_MORE:
			ril_request_send_sms_expect_more(t, data, length);
			break;
		case RIL_REQUEST_SMS_ACKNOWLEDGE:
			ril_request_sms_acknowledge(t, data, length);
			break;
		case RIL_REQUEST_WRITE_SMS_TO_SIM:
			ril_request_write_sms_to_sim(t, data, length);
			break;
		case RIL_REQUEST_DELETE_SMS_ON_SIM:
			ril_request_delete_sms_on_sim(t, data, length);
			break;
		/* CALL */
		case RIL_REQUEST_DIAL:
			ril_request_dial(t, data, length);
			break;
		case RIL_REQUEST_GET_CURRENT_CALLS:
			ril_request_get_current_calls(t);
			break;
		case RIL_REQUEST_HANGUP:
		case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:
		case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:
			ril_request_hangup(t);
			break;
		case RIL_REQUEST_ANSWER:
			ril_request_answer(t);
			break;
		case RIL_REQUEST_LAST_CALL_FAIL_CAUSE:
			ril_request_last_call_fail_cause(t);
			break;
		case RIL_REQUEST_DTMF:
			ril_request_dtmf(t, data, length);
			break;
		case RIL_REQUEST_DTMF_START:
			ril_request_dtmf_start(t, data, length);
			break;
		case RIL_REQUEST_DTMF_STOP:
			ril_request_dtmf_stop(t);
			break;
		/* GPRS */
		case RIL_REQUEST_SETUP_DATA_CALL:
			ril_request_setup_data_call(t, data, length);
			break;
		case RIL_REQUEST_DEACTIVATE_DATA_CALL:
			ril_request_deactivate_data_call(t, data, length);
			break;
		case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE:
			ril_request_last_data_call_fail_cause(t);
			break;
		case RIL_REQUEST_DATA_CALL_LIST:
			ril_request_data_call_list(t);
			break;
		/* SND */
		case RIL_REQUEST_SET_MUTE:
			ril_request_set_mute(t, data, length);
			break;
		/* OTHER */
		case RIL_REQUEST_SCREEN_STATE:
			/* This doesn't affect anything */
			ril_request_complete(t, RIL_E_SUCCESS, NULL, 0);
			break;
		default:
			RIL_LOGE("%s: Unhandled request: %d", __func__, request);
			ril_request_complete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
			break;
	}

	RIL_UNLOCK();
}

RIL_RadioState ril_on_state_request(void)
{
	return ril_data.state.radio_state;
}

int ril_on_supports(int request)
{
	return 1;
}

void ril_on_cancel(RIL_Token t)
{
	ril_request_set_canceled(t, 1);
}

const char *ril_get_version(void)
{
	return RIL_VERSION_STRING;
}

/*
 * RIL init
 */

void ril_data_init(void)
{
	memset(&ril_data, 0, sizeof(ril_data));

	pthread_mutex_init(&ril_data.mutex, NULL);

	ril_data.state.radio_state = RADIO_STATE_UNAVAILABLE;
}

/*
 * RIL interface
 */

static const RIL_RadioFunctions ril_ops = {
	RIL_VERSION >= 6 ? 6 : RIL_VERSION,
	ril_on_request,
	ril_on_state_request,
	ril_on_supports,
	ril_on_cancel,
	ril_get_version
};

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
	struct ril_client *ipc_fmt_client;
	struct ril_client *ipc_rfs_client;
	struct ril_client *srs_client;
	int rc;

	if (env == NULL)
		return NULL;

	ril_data_init();
	ril_data.env = (struct RIL_Env *) env;

	RIL_LOCK();

	RIL_LOGD("Creating IPC FMT client");

	ipc_fmt_client = ril_client_new(&ipc_fmt_client_funcs);
	rc = ril_client_create(ipc_fmt_client);

	if (rc < 0) {
		RIL_LOGE("IPC FMT client creation failed.");
		goto ipc_rfs;
	}

	rc = ril_client_thread_start(ipc_fmt_client);

	if (rc < 0) {
		RIL_LOGE("IPC FMT thread creation failed.");
		goto ipc_rfs;
	}

	ril_data.ipc_fmt_client = ipc_fmt_client;
	RIL_LOGD("IPC FMT client ready");

ipc_rfs:
	RIL_LOGD("Creating IPC RFS client");

	ipc_rfs_client = ril_client_new(&ipc_rfs_client_funcs);
	rc = ril_client_create(ipc_rfs_client);

	if (rc < 0) {
		RIL_LOGE("IPC RFS client creation failed.");
		goto srs;
	}

	rc = ril_client_thread_start(ipc_rfs_client);

	if (rc < 0) {
		RIL_LOGE("IPC RFS thread creation failed.");
		goto srs;
	}

	ril_data.ipc_rfs_client = ipc_rfs_client;
	RIL_LOGD("IPC RFS client ready");

srs:
	RIL_LOGD("Creating SRS client");

	srs_client = ril_client_new(&srs_client_funcs);
	rc = ril_client_create(srs_client);

	if (rc < 0) {
		RIL_LOGE("SRS client creation failed.");
		goto end;
	}

	rc = ril_client_thread_start(srs_client);

	if (rc < 0) {
		RIL_LOGE("SRS thread creation failed.");
		goto end;
	}

	ril_data.srs_client = srs_client;
	RIL_LOGD("SRS client ready");

end:
	RIL_UNLOCK();

	return &ril_ops;
}
