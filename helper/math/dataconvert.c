#include <math.h>
#include <stdint.h>

uint64_t math_bytes_to_kilo(uint64_t bytes) {
	uint64_t kilobytes = bytes / (uint64_t)1024;
	return kilobytes;
}

uint64_t math_bytes_to_mega(uint64_t bytes) {
	uint64_t kilobytes = bytes / (uint64_t)1024;
	uint64_t megabytes = kilobytes / (uint64_t)1024;
	return megabytes;
}

uint64_t math_kilo_to_mega(uint64_t kilo) {
	uint64_t megabytes = kilo / (uint64_t)1024;
	return megabytes;
}

uint64_t math_mega_to_bytes(uint64_t mega) {
	uint64_t bytes = mega * (uint64_t)1024 * (uint64_t)1024;
	return bytes;
}

uint64_t math_mega_to_kilo(uint64_t mega) { 
	uint64_t kilo = mega * (uint64_t)1024;
	return kilo;
}

uint64_t math_kilo_to_bytes(uint64_t kilo) {
	uint64_t bytes = kilo * (uint64_t)1024;
	return bytes;
}