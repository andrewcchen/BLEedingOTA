#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ble_l2cap.h"
#include "ble_ll.h"
#include "ble_att.h"

#include "config.h"

static uint8_t rx_buf[BLE_L2CAP_RX_MTU] __attribute__ ((aligned (4)));
static bool rx_full;

static inline void poll_ll(void) {
	if (!ble_ll_rx_full) return;

	static int rx_filled;

	struct ble_data_pdu *ll_pdu = (void*)ble_ll_rx_buf;
	struct ble_l2cap_pdu *pdu = (void*)rx_buf;
	
	if (ll_pdu->llid != 1 && ll_pdu->llid != 2)
		return;
	// TODO check llid validity?

	int new_filled = rx_filled + ll_pdu->length;
	if (new_filled > BLE_L2CAP_RX_MTU) {
		// mtu exceeded error
		abort();
	}

	memcpy(rx_buf + rx_filled, ll_pdu->payload, rx_filled);
	rx_filled = new_filled;

	ble_ll_rx_full = false;

	if (pdu->length <= new_filled) {
		rx_filled = 0;
		rx_full = 1;
	}
}

static inline void process_packet(void) {
	if (ble_ll_tx_full > 0) return;

	struct ble_l2cap_pdu *pdu = (void*)rx_buf;

	if (pdu->cid == 4) {
		ble_att_process_request(pdu->payload, pdu->length);
	} else {
		abort();
	}
}

void ble_l2cap_poll(void) {
	poll_ll();

	if (rx_full) process_packet();
}

void* ble_l2cap_prepare_tx(void) {
	void *ptr = ble_ll_prepare_tx();
	if (ptr == NULL) return NULL;
	else return (ptr + 4);
}

void ble_l2cap_ready_tx(int length) {
	struct ble_l2cap_pdu *pdu = (void*)(ble_ll_tx_buf + 2);
	pdu->length = length;
	pdu->cid = 4; // Attribute Protocol
	ble_ll_ready_tx(length + 4);
}

/*
void ble_l2cap_poll(void) {
	if (ble_l2cap_rx_full) return;
	if (!ble_ll_rx_full) return;

	struct ble_data_pdu *pdu = (void*)ble_ll_rx_buf;
	struct ble_l2cap_pkt *pkt = (void*)ble_l2cap_rx_buf;

	if (pdu->llid == 2) { // l2cap start
		// TODO check overflow

		memcpy(pkt, pdu->payload, pdu->length);

		if (pdu->length < pkt->length + 4) {
			// need continuation
			rx_filled_bytes = pdu->length;
		} else {
			ble_l2cap_rx_full = true;
		}

		ble_ll_rx_full = false;
	} else if (pdu->llid == 1) { // l2cap continuation
		// TODO check overflow

		void *tgt = ble_l2cap_rx_buf + rx_filled_bytes;
		memcpy(tgt, pdu->payload, pdu->length);

		rx_filled_bytes += pdu->length;

		if (rx_filled_bytes >= pkt->length + 4) {
			// full packet recieved
			ble_l2cap_rx_full = true;
		}

		ble_ll_rx_full = false;
	}
}
*/
