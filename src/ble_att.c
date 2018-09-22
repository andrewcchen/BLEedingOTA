#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ble_att.h"
#include "ble_l2cap.h"
#include "ble_gatt_server.h"
#include "ble_ll.h"

#include "config.h"

static inline void memcpy2(void *dst, void *src) {
	uint8_t *d = dst, *s = src;
	*d = *s;
	*(d+1) = *(s+1);
}

#define READ2(p) (*(uint8_t*)(p) | (*((uint8_t*)(p)+1) << 8))
#define WRITE2(p,v) *(uint8_t*)(p) = (v)&0xff; *((uint8_t*)(p)+1) = (v)>>8

#define RSP_ERROR(handle, code) \
	*(struct error_rsp*)rsp_p = \
		(struct error_rsp) { ERROR_RSP, op, handle, code }; \
	rsp_len = 5

void ble_att_process_request(void *req_p, int req_len) {
	(void)req_len;

	struct att_pdu *req_pdu = req_p;
	int op = req_pdu->opcode;
	//int param_len = req_len - 1;

	struct att_pdu* rsp_pdu = ble_l2cap_prepare_tx();
	if (rsp_pdu == NULL) return;
	void *rsp_p = rsp_pdu;

	int rsp_len;

	if (op == EXCHANGE_MTU_REQ) {
		rsp_pdu->opcode = EXCHANGE_MTU_RSP;
		WRITE2(rsp_pdu->params, BLE_L2CAP_RX_MTU - 4);
		rsp_len = 3;

	} else if (op == READ_BY_GROUP_TYPE_REQ) {
		struct read_by_group_type_req *req = req_p;
		struct read_by_group_type_rsp *rsp = rsp_p;

		/*
		uint16_t uuid;
		if (req_len == 7) {
			uuid = READ2(&req->uuid);
		} else if (req_len == 21) {
			uuid = READ2(&req->uuid + 12);
		} else {
			assert(false);
			// error
		}

		if (uuid != 0x2800) {
			assert(false);
			// todo error not found
		}
		*/

		uint16_t start = READ2(&req->start);
		uint16_t end = READ2(&req->end);

		int l = ble_gatt_server_get_services(start, end, rsp->data);
		if (l == 0) {
			// error not found
			RSP_ERROR(start, ATTRIBUTE_NOT_FOUND);
		} else {
			rsp->opcode = READ_BY_GROUP_TYPE_RSP;
			rsp->length = 6;
			rsp_len = sizeof(*rsp) + l;
		}
		// todo

	} else {
		//assert(false);
		RSP_ERROR(0, REQUEST_NOT_SUPPORTED);
	}

	ble_l2cap_ready_tx(rsp_len);
}
