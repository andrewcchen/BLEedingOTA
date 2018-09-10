#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ble_l2cap.h"
#include "ble_ll.h"

#include "config.h"


uint8_t ble_l2cap_rx_buf[251] __attribute__ ((aligned (4)));
bool ble_l2cap_rx_full;
static int rx_filled_bytes;


void ble_l2cap_poll(void) {
	if (ble_l2cap_rx_full) return;
	if (!ble_ll_rx_full) return;

	struct ble_data_pdu *pdu = (void*)ble_ll_rx_buf;
	struct ble_l2cap_pkt *pkt = (void*)ble_l2cap_rx_buf;

	if (pdu->sn == 2) { // l2cap start
		// TODO check overflow

		memcpy(pkt, pdu->payload, pdu->length);

		if (pdu->length < pkt->length + 4) {
			// need continuation
			rx_filled_bytes = pdu->length;
		} else {
			ble_l2cap_rx_full = true;
		}

		ble_ll_rx_full = false;
	} else if (pdu->sn == 1) { // l2cap continuation
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
