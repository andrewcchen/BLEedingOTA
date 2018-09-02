#pragma once

#include <stdbool.h>

extern void (*ble_phy_tx_callback)(uint32_t end_time);
extern void (*ble_phy_rx_callback)(uint32_t end_time);
extern void (*ble_phy_rx_fail_callback)(void);

extern bool ble_phy_crcok;

void ble_phy_init(void);
void ble_phy_disable(void);

void ble_phy_set_chn(int channel);
void ble_phy_set_addr(uint32_t address);
void ble_phy_set_crcinit(uint32_t crcinit);
void ble_phy_set_maxlen(uint8_t maxlen);
void ble_phy_set_ptr(void *ptr);

uint32_t ble_phy_get_time(void);

void ble_phy_tx(uint32_t start_time);
void ble_phy_rx(uint32_t start_time, uint32_t length);
