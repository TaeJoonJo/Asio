
#include <stdint.h>

#define _SERVER_PORT 4500

constexpr uint32_t _MAX_USER = 10;
constexpr uint32_t _MAX_BUFFER = 1024;

struct PACKET_SIMPLE {
	uint8_t size_;
	uint16_t id_;
	int8_t c_;
};