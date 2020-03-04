
#include <stdint.h>

#define _SERVER_PORT 4500

constexpr uint32_t _MAX_USER = 10;
constexpr uint32_t _MAX_BUFFER = 1024;

enum class EPacketType : uint8_t {
	simplechat = 0,
	id
};

struct PACKET_ID {
	uint8_t size_;
	EPacketType type_;
	uint16_t id_;
};

struct PACKET_SIMPLE {
	uint8_t size_;
	EPacketType type_;
	uint16_t id_;
	int8_t c_;
};