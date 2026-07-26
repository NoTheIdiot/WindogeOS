#include <basicutil.h>
#include <boot/kernel.h>
#include <stdint.h>

uint16_t lPci_Config_Read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
	uint32_t address;
	uint32_t lbus	= (uint32_t)bus;
	uint32_t lslot  = (uint32_t)slot;
	uint32_t lfunc  = (uint32_t)func;

	address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
	
}