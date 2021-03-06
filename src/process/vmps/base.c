/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/**
 * $Id$
 * @file src/process/vmps/base.c
 * @brief VMPS processing.
 *
 * @copyright 2018 The Freeradius server project.
 * @copyright 2018 Alan DeKok (aland@deployingradius.com)
 */
#include <freeradius-devel/io/application.h>
#include <freeradius-devel/server/protocol.h>
#include <freeradius-devel/server/process.h>
#include <freeradius-devel/unlang/base.h>
#include <freeradius-devel/util/dict.h>
#include <freeradius-devel/util/debug.h>
#include <freeradius-devel/vmps/vmps.h>

#include <freeradius-devel/protocol/vmps/vmps.h>

static fr_dict_t const *dict_vmps;

extern fr_dict_autoload_t process_vmps_dict[];
fr_dict_autoload_t process_vmps_dict[] = {
	{ .out = &dict_vmps, .proto = "vmps" },
	{ NULL }
};

static fr_dict_attr_t const *attr_packet_type;

extern fr_dict_attr_autoload_t process_vmps_dict_attr[];
fr_dict_attr_autoload_t process_vmps_dict_attr[] = {
	{ .out = &attr_packet_type, .name = "Packet-Type", .type = FR_TYPE_UINT32, .dict = &dict_vmps },
	{ NULL }
};

static unlang_action_t mod_process(rlm_rcode_t *p_result, UNUSED module_ctx_t const *mctx, request_t *request)
{
	rlm_rcode_t		rcode;
	CONF_SECTION		*unlang;
	fr_dict_enum_t const	*dv;

	REQUEST_VERIFY(request);

	switch (request->request_state) {
	case REQUEST_INIT:
		RDEBUG("Received %s ID %08x", fr_vmps_codes[request->packet->code], request->packet->id);
		log_request_proto_pair_list(L_DBG_LVL_1, request, NULL, &request->request_pairs, NULL);

		request->component = "vmps";

		dv = fr_dict_enum_by_value(attr_packet_type, fr_box_uint32(request->packet->code));
		if (!dv) {
			REDEBUG("Failed to find value for &request.VMPS-Packet-Type");
			RETURN_MODULE_FAIL;
		}

		unlang = cf_section_find(request->server_cs, "recv", dv->name);
		if (!unlang) {
			RWDEBUG("Failed to find 'recv %s' section", dv->name);
			request->reply->code = FR_PACKET_TYPE_VALUE_DO_NOT_RESPOND;
			goto send_reply;
		}

		RDEBUG("Running 'recv %s' from file %s", cf_section_name2(unlang), cf_filename(unlang));
		if (unlang_interpret_push_section(request, unlang, RLM_MODULE_NOOP, UNLANG_TOP_FRAME) < 0) {
			RETURN_MODULE_FAIL;
		}

		request->request_state = REQUEST_RECV;
		FALL_THROUGH;

	case REQUEST_RECV:
		rcode = unlang_interpret(request);

		if (request->master_state == REQUEST_STOP_PROCESSING) {
			*p_result = RLM_MODULE_HANDLED;
			return UNLANG_ACTION_STOP_PROCESSING;
		}

		if (rcode == RLM_MODULE_YIELD) RETURN_MODULE_YIELD;

		switch (rcode) {
		case RLM_MODULE_NOOP:
		case RLM_MODULE_OK:
		case RLM_MODULE_UPDATED:
			if (request->packet->code == FR_PACKET_TYPE_VALUE_JOIN_REQUEST) {
				request->reply->code = FR_PACKET_TYPE_VALUE_JOIN_RESPONSE;

			} else if (request->packet->code == FR_PACKET_TYPE_VALUE_RECONFIRM_REQUEST) {
				request->reply->code = FR_PACKET_TYPE_VALUE_RECONFIRM_RESPONSE;

			} else {
				request->reply->code = FR_PACKET_TYPE_VALUE_DO_NOT_RESPOND;
			}
			break;

		default:
		case RLM_MODULE_REJECT:
		case RLM_MODULE_FAIL:
		case RLM_MODULE_HANDLED:
			request->reply->code = FR_PACKET_TYPE_VALUE_DO_NOT_RESPOND;
			break;
		}

		dv = fr_dict_enum_by_value(attr_packet_type, fr_box_uint32(request->reply->code));
		unlang = NULL;
		if (dv) unlang = cf_section_find(request->server_cs, "send", dv->name);

		if (!unlang) goto send_reply;

	rerun_nak:
		RDEBUG("Running 'send %s' from file %s", cf_section_name2(unlang), cf_filename(unlang));
		if (unlang_interpret_push_section(request, unlang, RLM_MODULE_NOOP, UNLANG_TOP_FRAME) < 0) {
			RETURN_MODULE_FAIL;
		}

		request->request_state = REQUEST_SEND;
		FALL_THROUGH;

	case REQUEST_SEND:
		rcode = unlang_interpret(request);

		if (request->master_state == REQUEST_STOP_PROCESSING) {
			*p_result = RLM_MODULE_HANDLED;
			return UNLANG_ACTION_STOP_PROCESSING;
		}

		if (rcode == RLM_MODULE_YIELD) RETURN_MODULE_YIELD;

		switch (rcode) {
		case RLM_MODULE_NOOP:
		case RLM_MODULE_OK:
		case RLM_MODULE_UPDATED:
		case RLM_MODULE_HANDLED:
			/* reply is already set */
			break;

		default:
			/*
			 *	If we over-ride an ACK with a NAK, run
			 *	the NAK section.
			 */
			if (request->reply->code != FR_PACKET_TYPE_VALUE_DO_NOT_RESPOND) {
				dv = fr_dict_enum_by_value(attr_packet_type,
							   fr_box_uint32(request->reply->code));
				RWDEBUG("Failed running 'send %s', trying 'send Do-Not-Respond'.", dv->name);

				request->reply->code = FR_PACKET_TYPE_VALUE_DO_NOT_RESPOND;

				dv = fr_dict_enum_by_value(attr_packet_type,
							   fr_box_uint32(request->reply->code));
				unlang = NULL;
				if (!dv) goto send_reply;

				unlang = cf_section_find(request->server_cs, "send", dv->name);
				if (unlang) goto rerun_nak;

				RWDEBUG("Not running 'send %s' section as it does not exist", dv->name);
			}
			break;
		}

	send_reply:
		/*
		 *	Check for "do not respond".
		 */
		if (request->reply->code == FR_PACKET_TYPE_VALUE_DO_NOT_RESPOND) {
			RDEBUG("Not sending reply to client.");
			RETURN_MODULE_HANDLED;
		}

		/*
		 *	Overwrite the src ip address on the outbound packet
		 *	with the one specified by the client.
		 *	This is useful to work around broken DSR implementations
		 *	and other routing issues.
		 */
		if (request->client && (request->client->src_ipaddr.af != AF_UNSPEC)) {
			request->reply->socket.inet.src_ipaddr = request->client->src_ipaddr;
		}

		if (RDEBUG_ENABLED) common_packet_debug(request, request->reply, &request->reply_pairs, false);
		break;

	default:
		RETURN_MODULE_FAIL;
	}

	RETURN_MODULE_OK;
}


static const virtual_server_compile_t compile_list[] = {
	{
		.name = "recv",
		.name2 = "Join-Request",
		.component = MOD_AUTHORIZE,
	},
	{
		.name = "send",
		.name2 = "Join-Response",
		.component = MOD_POST_AUTH,
	},
	{
		.name = "recv",
		.name2 = "Reconfirm-Request",
		.component = MOD_AUTHORIZE,
	},
	{
		.name = "send",
		.name2 = "Reconfirm-Response",
		.component = MOD_POST_AUTH,
	},
	{
		.name = "send",
		.name2 = "Do-Not-Respond",
		.component = MOD_POST_AUTH,
	},

	COMPILE_TERMINATOR
};


extern fr_process_module_t process_vmps;
fr_process_module_t process_vmps = {
	.magic		= RLM_MODULE_INIT,
	.name		= "process_vmps",
	.process	= mod_process,
	.compile_list	= compile_list,
	.dict		= &dict_vmps,
};
