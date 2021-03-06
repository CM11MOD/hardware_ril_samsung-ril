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

#ifndef _SAMSUNG_RIL_H_
#define _SAMSUNG_RIL_H_

#include <errno.h>
#include <string.h>
#include <pthread.h>

#include <utils/Log.h>
#include <telephony/ril.h>

#include <samsung-ipc.h>

#include "ipc.h"
#include "srs.h"

/*
 * Defines
 */

#ifdef ALOGI
#define RIL_LOGI ALOGI
#else
#define RIL_LOGI LOGI
#endif

#ifdef ALOGD
#define RIL_LOGD ALOGD
#else
#define RIL_LOGD LOGD
#endif

#ifdef ALOGE
#define RIL_LOGE ALOGE
#else
#define RIL_LOGE LOGE
#endif

#define RIL_VERSION_STRING "Samsung RIL"

#define RIL_LOCK() pthread_mutex_lock(&ril_data.mutex)
#define RIL_UNLOCK() pthread_mutex_unlock(&ril_data.mutex)
#define RIL_CLIENT_LOCK(client) pthread_mutex_lock(&(client->mutex))
#define RIL_CLIENT_UNLOCK(client) pthread_mutex_unlock(&(client->mutex))

#define RIL_TOKEN_DATA_WAITING	(RIL_Token) 0xff
#define RIL_TOKEN_NULL		(RIL_Token) 0x00

#define RIL_SMS_TPID		0xff
#define RIL_SMS_NUMBER		"0123456789"

#define RIL_CLIENT_MAX_TRIES	7

/*
 * RIL client
 */

struct ril_client;

struct ril_client_funcs {
	int (*create)(struct ril_client *client);
	int (*destroy)(struct ril_client *client);
	int (*read_loop)(struct ril_client *client);
};

typedef enum {
	RIL_CLIENT_NULL		= 0,
	RIL_CLIENT_CREATED	= 1,
	RIL_CLIENT_READY	= 2,
	RIL_CLIENT_DESTROYED	= 3,
	RIL_CLIENT_ERROR	= 4,
} ril_client_state;

struct ril_client {
	struct ril_client_funcs funcs;
	ril_client_state state;

	void *data;

	pthread_t thread;
	pthread_mutex_t mutex;
};

struct ril_client *ril_client_new(struct ril_client_funcs *client_funcs);
int ril_client_free(struct ril_client *client);
int ril_client_create(struct ril_client *client);
int ril_client_destroy(struct ril_client *client);
int ril_client_thread_start(struct ril_client *client);

/*
 * RIL requests
 */

struct ril_request_info {
	RIL_Token token;
	int id;
	int canceled;
};

int ril_request_id_get(void);
int ril_request_id_set(int id);
int ril_request_register(RIL_Token t, int id);
void ril_request_unregister(struct ril_request_info *request);
struct ril_request_info *ril_request_info_find_id(int id);
struct ril_request_info *ril_request_info_find_token(RIL_Token t);
int ril_request_set_canceled(RIL_Token t, int canceled);
int ril_request_get_canceled(RIL_Token t);
RIL_Token ril_request_get_token(int id);
int ril_request_get_id(RIL_Token t);

void ril_request_complete(RIL_Token t, RIL_Errno e, void *data, size_t length);
void ril_request_unsolicited(int request, void *data, size_t length);
void ril_request_timed_callback(RIL_TimedCallback callback, void *data, const struct timeval *time);

/*
 * RIL radio state
 */

int ril_radio_state_complete(RIL_RadioState radio_state, RIL_Token token);
void ril_radio_state_update(RIL_RadioState radio_state);

/*
 * RIL tokens
 */

struct ril_tokens {
	RIL_Token radio_power;
	RIL_Token pin_status;
	RIL_Token get_imei;
	RIL_Token get_imeisv;
	RIL_Token baseband_version;

	RIL_Token registration_state;
	RIL_Token gprs_registration_state;
	RIL_Token operator;

	RIL_Token outgoing_sms;
	RIL_Token sim_io;
};

void ril_tokens_check(void);

/*
 * RIL state
 */

typedef enum {
	SIM_STATE_ABSENT			= 0,
	SIM_STATE_NOT_READY			= 1,
	SIM_STATE_READY				= 2,
	SIM_STATE_PIN				= 3,
	SIM_STATE_PUK				= 4,
	SIM_STATE_BLOCKED			= 5,
	SIM_STATE_NETWORK_PERSO 		= 6,
	SIM_STATE_NETWORK_SUBSET_PERSO		= 7,
	SIM_STATE_CORPORATE_PERSO		= 8,
	SIM_STATE_SERVICE_PROVIDER_PERSO	= 9,
} ril_sim_state;

struct ril_state {
	RIL_RadioState radio_state;
	ril_sim_state sim_state;

	struct ipc_sec_sim_status_response sim_pin_status;
	struct ipc_sec_sim_icc_type sim_icc_type;

	struct ipc_net_regist_response netinfo;
	struct ipc_net_regist_response gprs_netinfo;
	struct ipc_net_current_plmn_response plmndata;

	struct ipc_call_status call_status;

	int gprs_last_failed_cid;

	unsigned char dtmf_tone;
	unsigned char ussd_state;

	unsigned char sms_incoming_msg_tpid;
	unsigned char ril_sms_tpid;
};

/*
 * RIL data
 */

struct ril_data {
	struct RIL_Env *env;

	struct ril_state state;
	struct ril_tokens tokens;
	struct ril_oem_hook_svc_session *oem_hook_svc_session;
	struct list_head *gprs_connections;
	struct list_head *incoming_sms;
	struct list_head *outgoing_sms;
	struct list_head *sim_io;
	struct list_head *generic_responses;
	struct list_head *requests;
	int request_id;

	struct ril_client *ipc_fmt_client;
	struct ril_client *ipc_rfs_client;
	struct ril_client *srs_client;

	pthread_mutex_t mutex;
};

extern struct ril_data ril_data;

/*
 * Dispatch functions
 */

void ipc_fmt_dispatch(struct ipc_message_info *info);
void ipc_rfs_dispatch(struct ipc_message_info *info);
void srs_dispatch(struct srs_message *message);

/* GEN */

struct ipc_gen_phone_res_expect_info {
	unsigned char aseq;
	unsigned short command;
	void (*func)(struct ipc_message_info *info);
	int complete;
	int abort;
};

int ipc_gen_phone_res_expect_register(unsigned char aseq, unsigned short command,
	void (*func)(struct ipc_message_info *info), int complete, int abort);
void ipc_gen_phone_res_expect_unregister(struct ipc_gen_phone_res_expect_info *expect);
struct ipc_gen_phone_res_expect_info *ipc_gen_phone_res_expect_find_aseq(unsigned char aseq);
int ipc_gen_phone_res_expect_to_func(unsigned char aseq, unsigned short command,
	void (*func)(struct ipc_message_info *info));
int ipc_gen_phone_res_expect_to_complete(unsigned char aseq, unsigned short command);
int ipc_gen_phone_res_expect_to_abort(unsigned char aseq, unsigned short command);

void ipc_gen_phone_res(struct ipc_message_info *info);

/* PWR */

void ipc_pwr_phone_pwr_up(void);
void ipc_pwr_phone_reset(void);
void ipc_pwr_phone_state(struct ipc_message_info *info);
void ril_request_radio_power(RIL_Token t, void *data, int length);

/* DISP */

void ril_request_signal_strength(RIL_Token t);
void ipc_disp_icon_info(struct ipc_message_info *info);
void ipc_disp_rssi_info(struct ipc_message_info *info);

/* MISC */

void ril_request_get_imei(RIL_Token t);
void ril_request_get_imeisv(RIL_Token t);
void ipc_misc_me_sn(struct ipc_message_info *info);
void ril_request_baseband_version(RIL_Token t);
void ipc_misc_me_version(struct ipc_message_info *info);
void ril_request_get_imsi(RIL_Token t);
void ipc_misc_me_imsi(struct ipc_message_info *info);
void ipc_misc_time_info(struct ipc_message_info *info);

/* SAT */
void ril_request_report_stk_service_is_running(RIL_Token t);
void ipc_sat_proactive_cmd(struct ipc_message_info *info);
void ril_request_stk_send_terminal_response(RIL_Token t, void *data, size_t length);
void ril_request_stk_send_envelope_command(RIL_Token t, void *data, size_t length);
void ipc_sat_envelope_cmd(struct ipc_message_info *info);

/* SS */

void ril_request_send_ussd(RIL_Token t, void *data, size_t datalen);
void ril_request_cancel_ussd(RIL_Token t, void *data, size_t datalen);
void ipc_ss_ussd(struct ipc_message_info *info);

/* SEC */

struct ril_request_sim_io_info {
	unsigned char command;
	unsigned short fileid;
	unsigned char p1;
	unsigned char p2;
	unsigned char p3;
	void *data;
	int length;

	int waiting;
	RIL_Token token;
};

void ril_state_update(ril_sim_state status);
void ipc_sec_sim_status(struct ipc_message_info *info);
void ril_request_get_sim_status(RIL_Token t);
void ipc_sec_sim_icc_type(struct ipc_message_info *info);
void ril_request_sim_io_next(void);
void ril_request_sim_io_complete(RIL_Token t, unsigned char command, unsigned short fileid,
	unsigned char p1, unsigned char p2, unsigned char p3, void *data, int length);
void ril_request_sim_io(RIL_Token t, void *data, int length);
void ipc_sec_rsim_access(struct ipc_message_info *info);
void ipc_sec_sim_status_complete(struct ipc_message_info *info);
void ipc_sec_lock_info(struct ipc_message_info *info);
void ril_request_enter_sim_pin(RIL_Token t, void *data, size_t length);
void ril_request_change_sim_pin(RIL_Token t, void *data, size_t length);
void ril_request_enter_sim_puk(RIL_Token t, void *data, size_t length);
void ril_request_query_facility_lock(RIL_Token t, void *data, size_t length);
void ipc_sec_phone_lock(struct ipc_message_info *info);
void ipc_sec_phone_lock_complete(struct ipc_message_info *info);
void ril_request_set_facility_lock(RIL_Token t, void *data, size_t length);

/* SVC */

typedef enum {
	RIL_OEM_HOOK_TAG_SVC	= 1,
} RIL_OEMHookTag;

typedef enum {
	RIL_OEM_COMMAND_SVC_ENTER_MODE	= 1,
	RIL_OEM_COMMAND_SVC_END_MODE	= 2,
	RIL_OEM_COMMAND_SVC_KEY		= 3,
} RIL_OEMCommandSvc;

typedef struct {
	unsigned char tag;
	unsigned char command;
	unsigned short length;
} RIL_OEMHookHeader;

typedef struct {
	unsigned char mode;
	unsigned char type;
	unsigned char query;
} RIL_OEMHookSvcEnterMode;

typedef struct {
	unsigned char mode;
} RIL_OEMHookSvcEndMode;

typedef struct {
	unsigned char key;
	unsigned char query;
} RIL_OEMHookSvcKey;

struct ril_oem_hook_svc_session {
	RIL_Token token;
	void *display_screen;
	size_t display_screen_length;
};

void ipc_svc_display_screen(struct ipc_message_info *info);
void ril_request_oem_hook_raw(RIL_Token t, void *data, int length);

/* NET */

void ril_plmn_split(char *plmn_data, char **plmn, unsigned int *mcc, unsigned int *mnc);
void ril_plmn_string(char *plmn_data, char *response[3]);
unsigned char ril_plmn_act_get(char *plmn_data);
void ril_request_operator(RIL_Token t);
void ipc_net_current_plmn(struct ipc_message_info *message);
#if RIL_VERSION >= 6
void ril_request_voice_registration_state(RIL_Token t);
void ril_request_data_registration_state(RIL_Token t);
#else
void ril_request_registration_state(RIL_Token t);
void ril_request_gprs_registration_state(RIL_Token t);
#endif
void ipc_net_regist(struct ipc_message_info *message);
void ril_request_query_available_networks(RIL_Token t);
void ipc_net_plmn_list(struct ipc_message_info *info);
void ril_request_get_preferred_network_type(RIL_Token t);
void ril_request_set_preferred_network_type(RIL_Token t, void *data, size_t length);
void ipc_net_mode_sel(struct ipc_message_info *info);
void ril_request_query_network_selection_mode(RIL_Token t);
void ipc_net_plmn_sel(struct ipc_message_info *info);
void ril_request_set_network_selection_automatic(RIL_Token t);
void ril_request_set_network_selection_manual(RIL_Token t, void *data, size_t length);

/* SMS */

struct ipc_sms_incoming_msg_info {
	char *pdu;
	int length;

	unsigned char type;
	unsigned char tpid;
};

struct ril_request_send_sms_info {
	char *pdu;
	int pdu_length;
	unsigned char *smsc;
	int smsc_length;

	RIL_Token token;
};

int ril_request_send_sms_register(char *pdu, int pdu_length, unsigned char *smsc, int smsc_length, RIL_Token t);
void ril_request_send_sms_unregister(struct ril_request_send_sms_info *send_sms);
struct ril_request_send_sms_info *ril_request_send_sms_info_find(void);
struct ril_request_send_sms_info *ril_request_send_sms_info_find_token(RIL_Token t);

void ril_request_send_sms_next(void);
void ril_request_send_sms_complete(RIL_Token t, char *pdu, int pdu_length, unsigned char *smsc, int smsc_length);
void ril_request_send_sms(RIL_Token t, void *data, size_t length);
void ril_request_send_sms_expect_more(RIL_Token t, void *data, size_t length);
void ipc_sms_svc_center_addr(struct ipc_message_info *info);
void ipc_sms_send_msg_complete(struct ipc_message_info *info);
void ipc_sms_send_msg(struct ipc_message_info *info);

int ipc_sms_incoming_msg_register(char *pdu, int length, unsigned char type, unsigned char tpid);
void ipc_sms_incoming_msg_unregister(struct ipc_sms_incoming_msg_info *incoming_msg);
struct ipc_sms_incoming_msg_info *ipc_sms_incoming_msg_info_find(void);

void ipc_sms_incoming_msg_complete(char *pdu, int length, unsigned char type, unsigned char tpid);
void ipc_sms_incoming_msg(struct ipc_message_info *info);
void ril_request_sms_acknowledge(RIL_Token t, void *data, size_t length);
void ipc_sms_deliver_report(struct ipc_message_info *info);

void ril_request_write_sms_to_sim(RIL_Token token, void *data, size_t size);
void ipc_sms_save_msg(struct ipc_message_info *info);
void ril_request_delete_sms_on_sim(RIL_Token token, void *data, size_t size);
void ipc_sms_del_msg(struct ipc_message_info *info);

int ril_sms_send(char *number, char *message);

void ipc_sms_device_ready(struct ipc_message_info *info);

/* Call */

void ipc_call_incoming(struct ipc_message_info *info);
void ipc_call_status(struct ipc_message_info *info);
void ril_request_dial(RIL_Token t, void *data, size_t length);
void ril_request_get_current_calls(RIL_Token t);
void ipc_call_list(struct ipc_message_info *info);
void ril_request_hangup(RIL_Token t);
void ril_request_answer(RIL_Token t);
void ril_request_last_call_fail_cause(RIL_Token t);
void ril_request_dtmf(RIL_Token t, void *data, int length);
void ipc_call_burst_dtmf(struct ipc_message_info *info);
void ril_request_dtmf_start(RIL_Token t, void *data, int length);
void ril_request_dtmf_stop(RIL_Token t);

/* SND */

void ril_request_set_mute(RIL_Token t, void *data, int length);
void srs_snd_set_call_clock_sync(struct srs_message *message);
void srs_snd_set_call_volume(struct srs_message *message);
void srs_snd_set_call_audio_path(struct srs_message *message);

/* GPRS */

struct ril_gprs_connection {
	int cid;
	int enabled;
#if RIL_VERSION >= 6
	RIL_DataCallFailCause fail_cause;
#else
	RIL_LastDataCallActivateFailCause fail_cause;
#endif
	char *interface;

	RIL_Token token;
	struct ipc_gprs_pdp_context_set context;
	struct ipc_gprs_define_pdp_context define_context;
	struct ipc_gprs_ip_configuration ip_configuration;
};

int ril_gprs_connection_register(int cid);
void ril_gprs_connection_unregister(struct ril_gprs_connection *gprs_connection);
struct ril_gprs_connection *ril_gprs_connection_find_cid(int cid);
struct ril_gprs_connection *ril_gprs_connection_find_token(RIL_Token t);
struct ril_gprs_connection *ril_gprs_connection_start(void);
void ril_gprs_connection_stop(struct ril_gprs_connection *gprs_connection);

void ril_request_setup_data_call(RIL_Token t, void *data, int length);
void ril_request_deactivate_data_call(RIL_Token t, void *data, int length);
void ipc_gprs_ip_configuration(struct ipc_message_info *info);
void ipc_gprs_call_status(struct ipc_message_info *info);
void ril_request_last_data_call_fail_cause(RIL_Token t);
void ipc_gprs_pdp_context(struct ipc_message_info *info);
void ril_unsol_data_call_list_changed(void);
void ril_request_data_call_list(RIL_Token t);

/* RFS */

void ipc_rfs_nv_read_item(struct ipc_message_info *info);
void ipc_rfs_nv_write_item(struct ipc_message_info *info);

#endif
