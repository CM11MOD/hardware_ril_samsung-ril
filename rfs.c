/*
 * This file is part of Samsung-RIL.
 *
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

#define LOG_TAG "RIL-RFS"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

void ipc_rfs_nv_read_item(struct ipc_message_info *info)
{
	struct ipc_client *ipc_client;

	struct ipc_rfs_io *rfs_io;
	struct ipc_rfs_io_confirm *rfs_io_conf;

	void *rfs_data;
	int rc;

	if (info->data == NULL || info->length < sizeof(struct ipc_rfs_io))
		return;

	rfs_io = (struct ipc_rfs_io *) info->data;

	if (ril_data.ipc_rfs_client == NULL || ril_data.ipc_rfs_client->data == NULL)
		return;

	ipc_client = (struct ipc_client *) ril_data.ipc_rfs_client->data;

	rfs_io_conf = calloc(1, rfs_io->length + sizeof(struct ipc_rfs_io_confirm));
	rfs_data = rfs_io_conf + sizeof(struct ipc_rfs_io_confirm);

	RIL_LOGD("Asked to read 0x%x bytes at offset 0x%x", rfs_io->length, rfs_io->offset);
	rc = nv_data_read(ipc_client, rfs_io->offset, rfs_io->length, rfs_data);

	RIL_LOGD("Read rfs_data dump:");
	hex_dump(rfs_data, rfs_io->length > 0x100 ? 0x100 : rfs_io->length);

	RIL_LOGD("Sending RFS IO Confirm message (rc is %d)", rc);
	rfs_io_conf->confirm = rc < 0 ? 0 : 1;
	rfs_io_conf->offset = rfs_io->offset;
	rfs_io_conf->length = rfs_io->length;

	ipc_rfs_send(IPC_RFS_NV_READ_ITEM, (unsigned char *) rfs_io_conf, rfs_io->length + sizeof(struct ipc_rfs_io_confirm), info->aseq);

	free(rfs_io_conf);
}

void ipc_rfs_nv_write_item(struct ipc_message_info *info)
{
	struct ipc_client *ipc_client;

	struct ipc_rfs_io *rfs_io;
	struct ipc_rfs_io_confirm rfs_io_conf;

	void *rfs_data;
	int rc;

	if (info->data == NULL || info->length < sizeof(struct ipc_rfs_io))
		return;

	rfs_io = (struct ipc_rfs_io *) info->data;

	if (ril_data.ipc_rfs_client == NULL || ril_data.ipc_rfs_client->data == NULL)
		return;

	ipc_client = (struct ipc_client *) ril_data.ipc_rfs_client->data;

	memset(&rfs_io_conf, 0, sizeof(rfs_io_conf));
	rfs_data = info->data + sizeof(struct ipc_rfs_io);

	RIL_LOGD("Write rfs_data dump:");
	hex_dump(rfs_data, rfs_io->length > 0x100 ? 0x100 : rfs_io->length);

	RIL_LOGD("Asked to write 0x%x bytes at offset 0x%x", rfs_io->length, rfs_io->offset);
	rc = nv_data_write(ipc_client, rfs_io->offset, rfs_io->length, rfs_data);

	RIL_LOGD("Sending RFS IO Confirm message (rc is %d)", rc);
	rfs_io_conf.confirm = rc < 0 ? 0 : 1;
	rfs_io_conf.offset = rfs_io->offset;
	rfs_io_conf.length = rfs_io->length;

	ipc_rfs_send(IPC_RFS_NV_WRITE_ITEM, (unsigned char *) &rfs_io_conf, sizeof(struct ipc_rfs_io_confirm), info->aseq);
}
