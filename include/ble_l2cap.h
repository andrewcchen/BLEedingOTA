#pragma once

#include "ble_ll.h"

#define BLE_L2CAP_RX_MTU 270
#define BLE_L2CAP_TX_MTU 27

void ble_l2cap_poll(void);

void* ble_l2cap_prepare_tx(void);
void ble_l2cap_ready_tx(int length);

struct ble_l2cap_pdu {
	uint16_t length;
	uint16_t cid;
	uint8_t payload[];
};
