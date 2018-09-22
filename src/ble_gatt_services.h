#pragma pack(push, 1)

struct gatt_service {
	uint16_t decl_handle;
	uint16_t end_handle;
	uint16_t uuid;
} gatt_services[] = {
	// GAP Service
	{ 0x0001, 0x0005, 0x1800 },
};

struct gatt_characteristic {
	uint16_t decl_handle;
	uint8_t properties;
	uint16_t value_handle;
	uint16_t uuid;
} gatt_characteristics[] = {
	// GAP Service
	{ 0x0002, 0x02, 0x0003, 0x2a00 }, // Device name
	{ 0x0004, 0x02, 0x0005, 0x2a01 }, // Apperance
};

#pragma pack(pop)
