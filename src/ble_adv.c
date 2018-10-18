
#include <stdlib.h>
#include <assert.h>

#include "nrf52/nrf52832.h"

#include "ble_adv.h"
#include "ble_phy.h"
#include "ble_ll.h"

#include "config.h"


static uint8_t adv_ind_pdu[] = {
	0x40, 0x0f, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
	0x02, 0x01, 0x06,
	0x05, 0x08, 't', 'e', 's', 't',
};

static uint8_t scan_rsp_pdu[] = {
	0x44, 0x0f, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
	0x08, 0x09, 't', 'e', 's', 't', 'i', 'n', 'g',
};

static ble_addr adv_addr = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11};

static uint8_t rx_buf[256] __attribute__ ((aligned (4)));

static int last_chn;
static uint32_t last_event_time;

static void next_channel(uint32_t end_time) {
	ble_phy_set_ptr(adv_ind_pdu);

	if (last_chn == 39) {
		last_chn = 37;
		ble_phy_set_chn(37);
		last_event_time += 20000; // 20ms advertising interval
		last_event_time += rand() % 8192;
		ble_phy_tx(last_event_time);
	} else {
		ble_phy_set_chn(++last_chn);
		ble_phy_tx(end_time + 1000);
	}
}

static void tx_handler(uint32_t end_time) {
	ble_phy_set_ptr(rx_buf);
	ble_phy_rx(end_time + 140, 20);
}

static void rx_handler(uint32_t end_time) {
	if (ble_phy_crcok) {
		struct ble_advertising_pdu *pkt = (void*)rx_buf;

		if (pkt->pdu_type == SCAN_REQ) {
			struct ble_scan_req *req = (void*)pkt->payload;

			if (ble_addr_eq(req->adv_addr, adv_addr)) {
				ble_phy_set_ptr(scan_rsp_pdu);
				ble_phy_tx(end_time + 145);
				return;
			}
		} else if (pkt->pdu_type == CONNECT_IND) {
			struct ble_connect_req *req = (void*)pkt->payload;

			if (ble_addr_eq(req->adv_addr, adv_addr)) {
				ble_ll_enter_connection(req, end_time);
				return;
			}
		}
	}
	
	next_channel(end_time);
}

static void rx_fail_handler(void) {
	uint32_t t = ble_phy_get_time();
	next_channel(t);
}

void ble_adv_start(void) {
	//ble_phy_set_ptr(adv_ind_pdu);

	ble_phy_tx_callback = &tx_handler;
	ble_phy_rx_callback = &rx_handler;
	ble_phy_rx_fail_callback = &rx_fail_handler;

	ble_phy_set_addr(0x8E89BED6);
	ble_phy_set_crcinit(0x555555);

	NRF_TIMER0->TASKS_CLEAR = 1;

	last_event_time = 0;
	last_chn = 39;
	next_channel(0);
}
