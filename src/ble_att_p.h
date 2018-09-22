#pragma once

enum att_opcode {
	ERROR_RSP = 0x01,
	EXCHANGE_MTU_REQ = 0x02,
	EXCHANGE_MTU_RSP = 0x03,
	READ_BY_GROUP_TYPE_REQ = 0x10,
	READ_BY_GROUP_TYPE_RSP = 0x11,
};

enum att_error_code {
	REQUEST_NOT_SUPPORTED = 0x06,
};

#pragma pack(push, 1)

struct att_pdu {
	uint8_t opcode;
	uint8_t params[];
};

struct error_rsp_param {
	uint8_t req_opcode;
	uint16_t handle;
	uint8_t code;
};

struct read_by_group_type_req {
	uint16_t start;
	uint16_t end;
	uint8_t uuid[];
}

#pragma pack(pop)
