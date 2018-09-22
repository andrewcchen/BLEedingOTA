#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "ble_gatt_server.h"
#include "ble_gatt_services.h"

#define NELEMS(a) (sizeof(a) / sizeof(a[0]))

int ble_gatt_server_get_services(uint16_t start, uint16_t end, void *dst) {
	void *p = dst;
	int count = 0;

	for (unsigned int i = 0; i < NELEMS(gatt_services); i++) {
		int h = gatt_services[i].decl_handle;
		if (h >= start && h <= end) {
			memcpy(p, &gatt_services[i], sizeof(gatt_services[0]));
			if (++count == 3) break;
			p += sizeof(gatt_services[0]);
		}
	}

	return (p - dst);
}
