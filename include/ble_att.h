#pragma once

void ble_att_process_request(void *req_p, int req_len);

enum att_opcode {
	ERROR_RSP = 0x01,
	EXCHANGE_MTU_REQ = 0x02,
	EXCHANGE_MTU_RSP = 0x03,
	READ_BY_GROUP_TYPE_REQ = 0x10,
	READ_BY_GROUP_TYPE_RSP = 0x11,
};

enum att_error_code {
	REQUEST_NOT_SUPPORTED = 0x06,
	ATTRIBUTE_NOT_FOUND = 0x0a,
};

#pragma pack(push, 1)

struct att_pdu {
	uint8_t opcode;
	uint8_t params[];
};

struct error_rsp {
	uint8_t opcode;
	uint8_t req_opcode;
	uint16_t handle;
	uint8_t code;
};

struct read_by_group_type_req {
	uint8_t opcode;
	uint16_t start;
	uint16_t end;
	uint8_t uuid[];
} __attribute__ ((packed));

struct read_by_group_type_rsp {
	uint8_t opcode;
	uint8_t length;
	uint8_t data[];
};

#pragma pack(pop)
