#include <stdlib.h>
#include <assert.h>

#include "nrf52/nrf52832.h"

#include "ble_phy.h"

#include "ble_ll.h"
#include "chsel.h"

#include "config.h"

//static bool ll_connected;

static struct ble_chsel1 chsel;

static uint32_t transmit_window_size;
static uint32_t transmit_window_offset; // Not the same as in bluetooth specs
static uint32_t conn_interval;
//static uint32_t connSlaveLatency;
static uint32_t conn_supervision_timeout;
static uint32_t anchor_point;
static uint32_t expected_anchor_point;

static uint8_t transmit_seq_num;
static uint8_t next_expected_seq_num;

static inline uint32_t cal_win_widen(uint32_t t) {
	// assume worst case 1000ppm
	return (t / 1024) + 16;
}

static uint8_t tx_buf[256][2] __attribute__ ((aligned (4)));
static uint8_t rx_buf[256][2] __attribute__ ((aligned (4)));
static uint8_t rx_overflow_buf[256];
static struct ble_data_pdu tx_empty_buf;

static uint8_t tx_ind;
static uint8_t tx_filled;
static uint8_t rx_filled;

static bool last_crcfail;
static bool last_tx_non_empty;
static bool rx_md;
static bool tx_md;

static bool connected;
static bool first_event_pkt;

static void end_event(void);

static void tx_handler(uint32_t end_time) {
	if (rx_md || tx_md) {
		ble_phy_rx(end_time + 146, 0);
	} else {
		end_event();
	}
}

int fucking_counter;
static void rx_handler(uint32_t end_time) {
	connected = 1;
	struct ble_data_pdu *rx_pkt = (void*)ble_phy_rx_ptr;

	if (first_event_pkt) {
		first_event_pkt = 0;
		uint32_t ota_time = (10 + rx_pkt->length) * 8;
		anchor_point = end_time - ota_time;
		expected_anchor_point = anchor_point;
	}

	if (ble_phy_crcok) {
		fucking_counter++;

		if (rx_pkt->sn == next_expected_seq_num) {
			// new data
			if (ble_phy_rx_ptr != rx_overflow_buf) {
				next_expected_seq_num ^= 1;
			}
			rx_md = rx_pkt->md;

			if (rx_pkt->length > 0) {
				if (rx_filled == 0) {
					rx_filled = 1;
					ble_phy_rx_ptr = rx_buf[next_expected_seq_num];
				} else if (rx_filled == 1) {
					rx_filled = 2;
					ble_phy_rx_ptr = rx_overflow_buf;
				}
			}

			if (rx_pkt->nesn != transmit_seq_num) {
				// ack
				transmit_seq_num ^= 1;

				if (last_tx_non_empty) {
					tx_ind ^= 1;
					tx_filled--;
				}
			}
		}
	} else {
		if (last_crcfail) {
			// 2 crc failures, end connection event
			//TODO
		} else {
			last_crcfail = true;
		}
	}

	if (tx_filled == 0) {
		ble_phy_tx_ptr = (void*)&tx_empty_buf;
		last_tx_non_empty = false;
	} else {
		ble_phy_tx_ptr = tx_buf[tx_ind];
		last_tx_non_empty = true;
	}

	struct ble_data_pdu *tx_pkt = (void*)ble_phy_tx_ptr;
	tx_pkt->nesn = next_expected_seq_num;
	tx_pkt->sn = transmit_seq_num;
	tx_pkt->md = tx_md = tx_filled > 1;

	ble_phy_tx(end_time + 150);
}

int sad_counter;
static void rx_fail_handler(void) {
	sad_counter++;
	end_event();
}

static void end_event(void) {
	// end connection event
	first_event_pkt = 1;
	
	int chn = ble_chsel1_next_chn(&chsel);
	ble_phy_set_chn(chn);

	expected_anchor_point += conn_interval;
	uint32_t widening = cal_win_widen(expected_anchor_point - anchor_point);
	uint32_t win2 = transmit_window_size / 2;
	if (!connected) ble_phy_rx(expected_anchor_point + win2, widening + win2);
	else ble_phy_rx(expected_anchor_point, widening);
}

void ble_ll_enter_connection(struct ble_connect_req *req, uint32_t end_time) {
	ble_chsel1_init(&chsel, req->hop, req->channel_map);
	int chn = ble_chsel1_next_chn(&chsel);
	ble_phy_set_chn(chn);

	ble_phy_set_addr(req->access_addr);
	ble_phy_set_crcinit(req->crc_init);

	ble_phy_rx_ptr = rx_buf[0];

	ble_phy_tx_callback = &tx_handler;
	ble_phy_rx_callback = &rx_handler;
	ble_phy_rx_fail_callback = &rx_fail_handler;

	transmit_window_size = req->win_size * 1250;
	transmit_window_offset = req->win_offset * 1250 + 1250; // transmitWindowDelay = 1.25ms
	conn_interval = req->interval * 1250;
	conn_supervision_timeout = req->timeout * 10000;

	anchor_point = end_time;
	expected_anchor_point = anchor_point + transmit_window_offset;

	transmit_seq_num = 0;
	next_expected_seq_num = 0;

	tx_empty_buf.llid = 1;
	tx_empty_buf.length = 0;

	tx_ind = 0;
	rx_filled = 0;

	first_event_pkt = 1;

	//tx_buf[0][0] = 3;
	//tx_buf[0][1] = 2;
	//tx_buf[0][2] = 2;
	//tx_buf[0][3] = 3;
	//tx_filled = 1;

	connected = 0;

	uint32_t widening = cal_win_widen(transmit_window_offset);
	uint32_t win2 = transmit_window_size / 2;
	ble_phy_rx(expected_anchor_point + win2, widening + win2);
	//ble_phy_rx(expected_anchor_point + transmit_window_size / 2, widening + transmit_window_size / 2);
	//ble_phy_rx(end_time + 150 + 1000000, 1000000);
}

#if 0
void ble_ll_radio_irqhandler(void) {
	if (NRF_RADIO->EVENT_DISABLED) {
		NRF_RADIO->EVENT_DISABLED = 0;
		
		NRF_PPI->CHENCLR = 1 << 22; // TIMER0->EVENTS_COMPARE[1] == > RADIO->TASKS_DISABLE

		if (packet recieved) {

		} else {
			if (ll_connected) {
			} else {
				expected_anchor_point += conn_interval;
				uint32_t widening = (expected_anchor_point - anchor_point) / 1024 + 16;
				NRF_TIMER0->CC[0] = expected_anchor_point - widening - T_RAMPUP;
			}
		}
	}
}

void ble_ll_timer0_irqhandler(void) {
	if (NRF_TIMER0->EVENT_COMPARE[0]) {
		NRF_TIMER0->EVENT_COMPARE[0] = 0;

		uint32_t widening = (expected_anchor_point - anchor_point) / 1024 + 16;

		if (ll_connected) {
			
		} else {
			// Disable timer (if no packet recieved)
			NRF_TIMER0->CC[1] = expected_anchor_point + transmit_window_size + widening + 50;
			NRF_PPI->CHENSET = 1 << 22; // TIMER0->EVENTS_COMPARE[1] == > RADIO->TASKS_DISABLE
		}
	}
	if (NRF_TIMER0->EVENT_COMPARE[3]) {
		NRF_TIMER0->EVENT_COMPARE[3] = 0;
	}
}
#endif
