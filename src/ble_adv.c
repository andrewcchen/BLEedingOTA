
#include <stdlib.h>
#include <assert.h>

#include "nrf52/nrf52832.h"

#include "ble_adv.h"
#include "ble_phy.h"
#include "ble_ll.h"

#include "config.h"


static uint8_t adv_ind_pdu[] = {
	0x40, 0x0f, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
	0x02, 0x01, 0x02,
	0x05, 0x08, 't', 'e', 's', 't',
};

static uint8_t scan_rsp_pdu[] = {
	0x44, 0x0f, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
	0x08, 0x09, 't', 'e', 's', 't', 'i', 'n', 'g',
};

static ble_addr adv_addr = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11};

static uint8_t rx_buf[256] __attribute__ ((aligned (4)));

static int cur_chn;
static uint32_t last_event_time;

static void next_channel(uint32_t end_time) {
	ble_phy_tx_ptr = adv_ind_pdu;
	if (cur_chn == 39) {
		cur_chn = 37;
		ble_phy_set_chn(cur_chn);
		last_event_time += 5000; // 20ms advertising interval
		last_event_time += rand() % 1000;
		ble_phy_tx(last_event_time);
	} else {
		cur_chn++;
		ble_phy_set_chn(cur_chn);
		ble_phy_tx(end_time + 150);
	}
}

static void tx_handler(uint32_t end_time) {
	ble_phy_rx(end_time + 146, 0);
}

static void rx_handler(uint32_t end_time) {
	if (ble_phy_crcok) {
		struct ble_advertising_pdu *pkt = (void*)rx_buf;

		if (pkt->pdu_type == SCAN_REQ) {
			struct ble_scan_req *req = (void*)&pkt->payload;

			if (ble_addr_eq(req->adv_addr, adv_addr)) {
				ble_phy_tx_ptr = scan_rsp_pdu;
				ble_phy_tx(end_time + 150);
				return;
			}
		}
		else if (pkt->pdu_type == CONNECT_IND) {
			struct ble_connect_req *req = (void*)&pkt->payload;
			ble_ll_enter_connection(req, end_time);
			return;
		}
	}
	
	next_channel(end_time);
}

static void rx_fail_handler(void) {
	NRF_TIMER0->TASKS_CAPTURE[0] = 1;
	next_channel(NRF_TIMER0->CC[0]);
}

void ble_adv_start(void) {
	ble_phy_tx_ptr = adv_ind_pdu;
	ble_phy_rx_ptr = rx_buf;

	ble_phy_tx_callback = &tx_handler;
	ble_phy_rx_callback = &rx_handler;
	ble_phy_rx_fail_callback = &rx_fail_handler;

	ble_phy_set_addr(0x8E89BED6);
	ble_phy_set_crcinit(0x555555);

	NRF_TIMER0->TASKS_CLEAR = 1;

	last_event_time = 0;
	cur_chn = 39;
	next_channel(0);
}

#if 0
static void ble_adv_on_chn(int chn) {
	assert(NRF_RADIO->STATE == RADIO_STATE_STATE_Disabled);

	ble_phy_adv_state = SENDING_ADV_IND;
	NRF_RADIO->PACKETPTR = (uintptr_t) adv_ind_pdu;
	NRF_RADIO->DATAWHITEIV = chn;
	NRF_RADIO->FREQUENCY = ble_chn_freq[chn];
	NRF_RADIO->TASKS_TXEN = 1;
}

static void ble_phy_do_adv(void) {
		uint32_t endtime = NRF_TIMER0->CC[2];
		// timer to disable the radio if a response is not recieved
		NRF_TIMER0->CC[1] = endtime + 300;
		NRF_PPI->CHENSET = 1 << 22; // TIMER0->EVENTS_COMPARE[1] ==> RADIO->TASKS_DISABLE
	
		// Put radio into receiving
		NRF_RADIO->PACKETPTR = (uintptr_t) ble_phy_rx_buf;
		NRF_RADIO->TASKS_RXEN = 1;
	} else if (ble_phy_adv_state == SENDING_SCAN_RSP) {
		NRF_PPI->CHENCLR = 1 << 20; // TIMER0->EVENTS_COMPARE[0] ==> RADIO->TASKS_TXEN
		NRF_RADIO->TASKS_DISABLE = 1;
	} else {
		abort();
	}
}

static void ble_phy_adv_pkt_recvd(void) {
	if (NRF_RADIO->CRCSTATUS) { // crc ok
		struct ble_advertising_pdu *pkt = ble_phy_rx_buf;

		if (pkt->pdu_type == SCAN_REQ) {
			struct ble_scan_req *req = &pkt->payload;
			if (ble_addr_eq(req->adv_addr, adv_addr)) {
				ble_phy_adv_state = SENDING_SCAN_RSP;
				NRF_RADIO->PACKETPTR = (uintptr_t) scan_rsp_pdu;
				NRF_PPI->CHENSET = 1 << 20; // TIMER0->EVENTS_COMPARE[0] ==> RADIO->TASKS_TXEN
				NRF_TIMER0->CC[0] = NRF_TIMER0->CC[2] + 110;
				return;
			}
		}

		if (pkt->pdu_type == CONNECT_IND) {
			struct ble_connect_req *req = &pkt->payload;
			//ble_phy_state = PHY_CONNECTING;

			uint32_t endtime = NRF_TIMER0->CC[2];
			// TODO
			//return;
		}
	}

	// unknown or invalid packet recieved, ignore
	NRF_RADIO->TASKS_DISABLE = 1;
}

void ble_phy_adv_timer0_irqhandler(void) {
	if (NRF_TIMER0->EVENTS_COMPARE[3]) {
		NRF_TIMER0->EVENTS_COMPARE[3] = 0;

		ble_phy_do_adv();
	} else {
		abort();
	}
}

void ble_phy_adv_radio_irqhandler(void) {
	if (NRF_RADIO->EVENTS_END) {
		NRF_RADIO->EVENTS_END = 0;

		if (NRF_RADIO->STATE == RADIO_STATE_STATE_RxIdle) {
			ble_phy_adv_pkt_recvd();
		} else if (NRF_RADIO->STATE == RADIO_STATE_STATE_TxIdle) {
			ble_phy_adv_pkt_sent();
		} else {
			abort();
		}
	} else if (NRF_RADIO->EVENTS_DISABLED) {
		NRF_RADIO->EVENTS_DISABLED = 0;

		NRF_PPI->CHENCLR = 1 << 22; // TIMER0->EVENTS_COMPARE[1] ==> RADIO->TASKS_DISABLE
		
		if (ble_phy_cur_chn == 39) {
			ble_phy_cur_chn = 37;

			NRF_TIMER0->TASKS_CAPTURE[1] = 1;
			uint32_t now = NRF_TIMER0->CC[1];
			uint32_t adv_intv = 20000;
			uint32_t rand_delay = rand() % 10000;
			NRF_TIMER0->CC[3] = now + adv_intv + rand_delay;
		} else {
			ble_phy_cur_chn++;
			ble_phy_do_adv();
		}
	} else if (NRF_RADIO->EVENTS_ADDRESS) {
		NRF_RADIO->EVENTS_ADDRESS = 0;

		NRF_PPI->CHENCLR = 1 << 22; // TIMER0->EVENTS_COMPARE[1] ==> RADIO->TASKS_DISABLE
	} else {
		abort();
	}
}
#endif
