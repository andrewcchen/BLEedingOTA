#include <stdlib.h>
#include <assert.h>

#include "nrf52/nrf52832.h"

#include "ble_phy.h"
#include "ble_ll.h"
#include "chsel.h"

#include "config.h"

#define T_RAMPUP 40

uint8_t *ble_phy_tx_ptr;
uint8_t *ble_phy_rx_ptr;

void (*ble_phy_tx_callback)(uint32_t);
void (*ble_phy_rx_callback)(uint32_t);
void (*ble_phy_rx_fail_callback)(void);

bool ble_phy_crcok;

void ble_phy_init(void) {
	NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_1Mbit;
	NRF_RADIO->MODECNF0 = RADIO_MODECNF0_RU_Fast | 0x0200;
	//NRF_RADIO->TIFS = 150;

	NRF_RADIO->PCNF0 =
		8 << RADIO_PCNF0_LFLEN_Pos |
		1 << RADIO_PCNF0_S0LEN_Pos |
		0 << RADIO_PCNF0_S1LEN_Pos;

	NRF_RADIO->PCNF1 =
		254 << RADIO_PCNF1_MAXLEN_Pos |
		3 << RADIO_PCNF1_BALEN_Pos |
		RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos |
		RADIO_PCNF1_WHITEEN_Msk;

	NRF_RADIO->TXADDRESS = 0;
	NRF_RADIO->RXADDRESSES = 1 << 0;

	NRF_RADIO->CRCCNF =
		RADIO_CRCCNF_LEN_Three |
		RADIO_CRCCNF_SKIPADDR_Msk;
	NRF_RADIO->CRCPOLY = 0x00065B;

	NRF_RADIO->SHORTS =
		RADIO_SHORTS_READY_START_Msk;
		//RADIO_SHORTS_END_DISABLE_Msk;
	NRF_RADIO->INTENSET =
		RADIO_INTENSET_END_Msk |
		RADIO_INTENSET_DISABLED_Msk;
	NVIC_EnableIRQ(RADIO_IRQn);

	NRF_PPI->CHENCLR = 1 << 20 | 1 << 21 | 1 << 22;
	NRF_PPI->CHENSET = 1<<26; // RADIO->EVENTS_ADDRESS ==> TIMER0->TASKS_CAPTURE[1]
	NRF_PPI->CHENSET = 1<<27; // RADIO->EVENTS_END ==> TIMER0->TASKS_CAPTURE[2]

	NRF_TIMER0->MODE = TIMER_MODE_MODE_Timer;
	NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
	NRF_TIMER0->PRESCALER = 4; // 1 Mhz
	NRF_TIMER0->TASKS_START = 1;
}

void ble_phy_disable(void) {
	abort();
}

void ble_phy_set_chn(int channel) {
	NRF_RADIO->DATAWHITEIV = channel;
	NRF_RADIO->FREQUENCY = ble_chn_freq[channel];
}

void ble_phy_set_addr(uint32_t address) {
	NRF_RADIO->PREFIX0 = address >> 24;
	NRF_RADIO->BASE0 = address << 8;
}

void ble_phy_set_crcinit(uint32_t crcinit) {
	NRF_RADIO->CRCINIT = crcinit;
}

static bool doing_tx;

/*
static void set_start_timer(uint32_t t) {
	// Ensure scheduled time is at least one timer tick away
	// so that the event is guarenteed to fire
	uint32_t t_comp = t - 1; // Timer could tick during the check
	__disable_irq();
	NRF_TIMER0->TASKS_CAPTURE[3] = 1;
	if (int32_t(t_comp - NRF_TIMER0->CC[3]) > 0) {
		NRF_TIMER0->CC[0] = t;
	} else {
		NRF_TIMER0->CC[0] = NRF_TIMER0->CC[3] + 2;
	}
	__enable_irq();
}
*/

static inline void set_start_timer(uint32_t t) {
	NRF_TIMER0->CC[0] = t;
}

void ble_phy_tx(uint32_t start_time) {
	doing_tx = 1;

	NRF_RADIO->INTENCLR = RADIO_INTENCLR_ADDRESS_Msk;
	NRF_RADIO->PACKETPTR = (uint32_t)ble_phy_tx_ptr;

	NRF_PPI->CHENSET = 1 << 20; // TIMER0->EVENTS_COMPARE[0] ==> RADIO->TASKS_TXEN

	set_start_timer(start_time - T_RAMPUP);
}

void ble_phy_rx(uint32_t start_time, uint32_t widening) {
	doing_tx = 0;

	// 40us for 5 bytes of preamable + address
	// 20us of buffer to account for various possible delays
	NRF_TIMER0->CC[1] = start_time + widening + 40 + 20;

	NRF_RADIO->EVENTS_ADDRESS = 0;
	NRF_RADIO->EVENTS_PAYLOAD = 0;
	NRF_RADIO->INTENSET = RADIO_INTENSET_ADDRESS_Msk;
	NRF_RADIO->PACKETPTR = (uint32_t)ble_phy_rx_ptr;

	NRF_PPI->CHENSET = 1 << 21; // TIMER0->EVENTS_COMPARE[0] ==> RADIO->TASKS_RXEN
	NRF_PPI->CHENSET = 1 << 22; // TIMER0->EVENTS_COMPARE[1] ==> RADIO->TASKS_DISABLE

	set_start_timer(start_time - widening - T_RAMPUP);
}

// IRQ Handlers

void RADIO_IRQHandler(void);
void RADIO_IRQHandler(void) {
	if (NRF_RADIO->EVENTS_ADDRESS) {
		NRF_RADIO->EVENTS_ADDRESS = 0;

		NRF_PPI->CHENCLR = 1 << 22; // TIMER0->EVENTS_COMPARE[1] ==> RADIO->TASKS_DISABLE
	}
	if (NRF_RADIO->EVENTS_END) {
		NRF_RADIO->EVENTS_END = 0;

		NRF_PPI->CHENCLR = 1 << 20 | 1 << 21 | 1 << 22;

		uint32_t end_time = NRF_TIMER0->CC[2];
		if (doing_tx) {
			ble_phy_tx_callback(end_time);
		} else {
			NRF_RADIO->TASKS_DISABLE = 1;
			ble_phy_crcok = NRF_RADIO->CRCSTATUS;
			ble_phy_rx_callback(end_time);
		}
	}
	if (NRF_RADIO->EVENTS_DISABLED) {
		NRF_RADIO->EVENTS_DISABLED = 0;

		if (!doing_tx && !NRF_RADIO->EVENTS_PAYLOAD) {
			NRF_PPI->CHENCLR = 1 << 20 | 1 << 21 | 1 << 22;

			ble_phy_rx_fail_callback();
		}
	}
}

/*
void TIMER0_IRQHandler(void) {
	if (ble_phy_state == PHY_ADVERTISING)
		ble_phy_adv_timer0_irqhandler();
}*/

// lookup table
const uint8_t ble_chn_freq[40] = {
	4,6,8,10,12,14,16,18,20,22,24,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,2,26,80
};
