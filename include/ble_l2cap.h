#pragma once

struct ble_l2cap_pkt {
	uint16_t length;
	uint16_t cid;
	uint8_t payload[];
};


void ble_l2cap_poll(void);
