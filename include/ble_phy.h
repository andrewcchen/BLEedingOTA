#pragma once

#include <stdbool.h>

extern uint8_t *ble_phy_tx_ptr;
extern uint8_t *ble_phy_rx_ptr;

extern void (*ble_phy_tx_callback)(uint32_t end_time);
extern void (*ble_phy_rx_callback)(uint32_t end_time);
extern void (*ble_phy_rx_fail_callback)(void);

extern bool ble_phy_crcok;

void ble_phy_init(void);
void ble_phy_disable(void);

void ble_phy_set_chn(int channel);
void ble_phy_set_addr(uint32_t address);
void ble_phy_set_crcinit(uint32_t crcinit);

/**
 * Schedule a tx at time
 */
void ble_phy_tx(uint32_t start_time);

/**
 * Schedule a rx at time
 */
void ble_phy_rx(uint32_t start_time, uint32_t widening);

/*
void ble_phy_cancel_tx(void);
void ble_phy_cancel_rx(void);
*/



/**
 * Frequency lookup table
 * The frequency of the ith channel is (2400 + ble_chn_freq[i]) Mhz
 */
extern const uint8_t ble_chn_freq[40];
