#include <stdint.h>
#include <stdbool.h>

// Definitions for BLE link layer packets

typedef uint8_t ble_addr[6];

enum ble_pdu_type {
	ADV_IND = 0,
	ADV_DIRECT_IND = 1,
	ADV_NONCONN_IND = 2,
	SCAN_REQ = 3,
	SCAN_RSP = 4,
	CONNECT_IND = 5,
	ADV_SCAN_IND = 6,
};

enum ble_ctrl_opcode {
	LL_CONNECTION_UPDATE_REQ = 0x00,
	LL_CHANNEL_MAP_REQ = 0x01,
	LL_TERMINATE_IND = 0x02,
	LL_ENC_REQ = 0x03,
	LL_ENC_RSP = 0x04,
	LL_START_ENC_REQ = 0x05,
	LL_START_ENC_RSP = 0x06,
	LL_UNKNOWN_RSP = 0x07,
	LL_FEATURE_REQ = 0x08,
	LL_FEATURE_RSP = 0x09,
	LL_PAUSE_ENC_REQ = 0x0A,
	LL_PAUSE_ENC_RSP = 0x0B,
	LL_VERSION_IND = 0x0C,
	LL_REJECT_IND = 0x0D,
	LL_SLAVE_FEATURE_REQ = 0x0E,
	LL_CONNECTION_PARAM_REQ = 0x0F,
	LL_CONNECTION_PARAM_RSP = 0x10,
	LL_REJECT_IND_EXT = 0x11,
	LL_PING_REQ = 0x12,
	LL_PING_RSP = 0x13,
	LL_LENGTH_REQ = 0x14,
	LL_LENGTH_RSP = 0x15,
};

#pragma pack(push, 1)

struct ble_advertising_pdu {
	uint8_t pdu_type:4;
	uint8_t _reserved:1;
	uint8_t chsel:1;
	uint8_t txadd:1;
	uint8_t rxadd:1;
	uint8_t length;
	uint8_t payload[];
};

struct ble_adv_ind {
	ble_addr adv_addr;
	uint8_t adv_data[];
};

struct ble_scan_req {
	ble_addr scan_addr;
	ble_addr adv_addr;
};

struct ble_scan_rsp {
	ble_addr adv_addr;
	uint8_t scan_rsp_data[];
};

struct ble_connect_req {
	ble_addr init_addr;
	ble_addr adv_addr;

	uint32_t access_addr;
	uint32_t crc_init:24;
	uint8_t win_size;
	uint16_t win_offset;
	uint16_t interval;
	uint16_t latency;
	uint16_t timeout;
	uint8_t channel_map[5];
	uint8_t hop:5;
	uint8_t sca:3;
} __attribute__((packed));


struct ble_data_pdu {
	uint8_t llid:2;
	uint8_t nesn:1;
	uint8_t sn:1;
	uint8_t md:1;
	uint8_t _reserved:3;
	uint8_t length;
	uint8_t payload[];
};

struct ble_ctrl_pdu {
	uint8_t llid:2;
	uint8_t nesn:1;
	uint8_t sn:1;
	uint8_t md:1;
	uint8_t _reserved:3;
	uint8_t length;
	uint8_t opcode;
	uint8_t payload[];
};

struct ble_ll_feature_rsp {
	struct ble_ctrl_pdu header;
	uint8_t feature_set[8];
};

struct ble_ll_version_ind {
	struct ble_ctrl_pdu header;
	uint8_t vers_nr;
	uint16_t comp_id;
	uint16_t sub_vers_nr;
};

struct ble_ll_length_rsp {
	struct ble_ctrl_pdu header;
	uint16_t max_rx_octets;
	uint16_t max_rx_time;
	uint16_t max_tx_octets;
	uint16_t max_tx_time;
};

#pragma pack(pop)


static inline bool ble_addr_eq(ble_addr a, ble_addr b) {
	for (int i = 0; i < 6; i++) {
		if (a[i] != b[i]) return false;
	}
	return true;
}


extern uint8_t ble_ll_tx_buf[29];
extern bool ble_ll_tx_full;

extern uint8_t ble_ll_rx_buf[253];
extern bool ble_ll_rx_full;

void ble_ll_enter_connection(struct ble_connect_req *req, uint32_t end_time);
