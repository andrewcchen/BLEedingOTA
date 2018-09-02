#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "nrf52/nrf52832.h"

#include "ble_phy.h"

#include "ble_ll.h"
#include "chsel.h"

#include "config.h"


static struct ble_chsel1 chsel;

static uint32_t transmit_window_size;
static uint32_t conn_interval;
//static uint32_t connSlaveLatency;
static uint32_t conn_supervision_timeout;
static uint32_t anchor_point;
static uint32_t expected_anchor_point;

static uint8_t transmit_seq_num;
static uint8_t next_expected_seq_num;

#define TX_BUF_LEN 256
#define RX_BUF_LEN 256

static uint8_t tx_buf[TX_BUF_LEN] __attribute__ ((aligned (4)));
static uint8_t rx_buf[RX_BUF_LEN] __attribute__ ((aligned (4)));
static struct ble_data_pdu rx_overflow_buf;
static struct ble_data_pdu tx_empty_buf;

static bool tx_buf_full;
static bool rx_buf_full;
static struct ble_data_pdu *rx_ptr;
static struct ble_data_pdu *tx_ptr;

static bool last_crcfail;
//static bool last_tx_full;
static bool rx_md;
//static bool tx_md;

static bool connected;
static bool first_pkt_in_evt;


static void end_event(void);
static void process_packet(void);

static inline uint32_t cal_win_widen(uint32_t t) {
	// assume worst case 1000ppm
	return (t / 1024) + 16;
}

static void set_rx_ptr(void) {
	if (rx_buf_full) {
		ble_phy_set_maxlen(0);
		rx_ptr = &rx_overflow_buf;
	} else {
		rx_buf_full = true;
		ble_phy_set_maxlen(254);
		rx_ptr = (void*)rx_buf;
	}
	ble_phy_set_ptr(rx_ptr);
}

static void set_tx_ptr(void) {
	ble_phy_set_maxlen(254);
	if (tx_buf_full) {
		tx_ptr = &tx_empty_buf;
	} else {
		tx_ptr = (void*)tx_buf;
	}
	tx_ptr->nesn = next_expected_seq_num;
	tx_ptr->sn = transmit_seq_num;
	tx_ptr->md = 0;
	ble_phy_set_ptr(tx_ptr);
}

static void tx_handler(uint32_t end_time) {
	if (!rx_md) {
		end_event();
	} else {
		set_rx_ptr();
		ble_phy_rx(end_time + 146, 8);
	}
}

static void rx_handler(uint32_t end_time) {
	connected = 1;
	//struct ble_data_pdu *rx_pkt = (void*)ble_phy_rx_ptr;

	if (first_pkt_in_evt) {
		first_pkt_in_evt = 0;
		uint32_t pkt_time = (10 + rx_ptr->length) * 8;
		anchor_point = end_time - pkt_time;
		//expected_anchor_point = anchor_point;
	}

	bool should_process_pkt = false;

	if (ble_phy_crcok) {
		last_crcfail = false;

		if (rx_ptr->sn == next_expected_seq_num) { // new data
			rx_md = rx_ptr->md;

			if (rx_ptr != &rx_overflow_buf) {
				next_expected_seq_num ^= 1;
				if (rx_ptr->length == 0)
					rx_buf_full = false;
				else
					should_process_pkt = true;
			}
			if (rx_ptr->nesn != transmit_seq_num) { // ack
				transmit_seq_num ^= 1;
				tx_buf_full = 0;
			}
		}
	} else {
		if (last_crcfail) {
			last_crcfail = false;
			end_event();
			return;
		} else {
			last_crcfail = true;
		}
	}

	set_tx_ptr();
	ble_phy_tx(end_time + 150);

	if (should_process_pkt)
		process_packet();
}

static void rx_fail_handler(void) {
	end_event();
}

static void end_event(void) {
	first_pkt_in_evt = 1;
	
	int chn = ble_chsel1_next_chn(&chsel);
	ble_phy_set_chn(chn);

	set_rx_ptr();

	expected_anchor_point = anchor_point + conn_interval;
	uint32_t w = cal_win_widen(expected_anchor_point - anchor_point);
	if (connected)
		ble_phy_rx(expected_anchor_point - w, w);
	else
		ble_phy_rx(expected_anchor_point - w, transmit_window_size + w);
}

void ble_ll_enter_connection(struct ble_connect_req *req, uint32_t end_time) {
	ble_chsel1_init(&chsel, req->hop, req->channel_map);

	ble_phy_set_addr(req->access_addr);
	ble_phy_set_crcinit(req->crc_init);

	ble_phy_tx_callback = &tx_handler;
	ble_phy_rx_callback = &rx_handler;
	ble_phy_rx_fail_callback = &rx_fail_handler;

	transmit_window_size = req->win_size * 1250;
	conn_interval = req->interval * 1250;
	conn_supervision_timeout = req->timeout * 10000;
	uint32_t transmit_window_offset = req->win_offset * 1250 + 1250; // transmitWindowDelay = 1.25ms

	anchor_point = end_time;
	expected_anchor_point = anchor_point + transmit_window_offset;

	transmit_seq_num = 0;
	next_expected_seq_num = 0;

	tx_empty_buf.llid = 1;
	tx_empty_buf.md = 0;
	tx_empty_buf.length = 0;

	rx_buf_full = 0;
	tx_buf_full = 0;

	connected = 0;
	first_pkt_in_evt = 1;

	int chn = ble_chsel1_next_chn(&chsel);
	ble_phy_set_chn(chn);

	set_rx_ptr();

	uint32_t w = cal_win_widen(transmit_window_offset);
	ble_phy_rx(expected_anchor_point - w, transmit_window_size + w);
}


static void process_ctrl_pkt(uint8_t opcode, void *data) {
	(void)data;

	if (tx_buf_full) return;
	tx_buf_full = 1;

	struct ble_data_pdu *tx = (void*)tx_buf;
	tx->llid = 3;

	switch (opcode) {
	case LL_FEATURE_REQ:
		tx->length = 9;
		memset(tx->payload, 0, 9);
		tx->payload[0] = LL_FEATURE_RSP;
		tx->payload[1] = 0;
		break;
	case LL_VERSION_IND:
		tx->length = 6;
		memset(tx->payload, 0, 6);
		tx->payload[0] = LL_VERSION_IND;
		tx->payload[1] = 8; // bluetooth 4.2
		break;
	case LL_TERMINATE_IND:
		abort();
	default:
		tx->length = 2;
		tx->payload[0] = LL_UNKNOWN_RSP;
		tx->payload[1] = opcode;
	}

	rx_buf_full = 0;
}

static void process_packet(void) {
	struct ble_data_pdu *pdu = rx_ptr;

	if (pdu->llid == 3) { // control pdu
		uint8_t opcode = pdu->payload[0];
		void *data = pdu->payload+1;
		process_ctrl_pkt(opcode, data);
	} else {
		rx_buf_full = 0;
	}
}
