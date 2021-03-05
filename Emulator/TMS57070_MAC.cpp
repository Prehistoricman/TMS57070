#include "TMS57070_MAC.h"
#include "TMS57070.h"

using namespace TMS57070;

constexpr uint64_t MAC_BITS_LOWER_MASK = 0x0000000FFFFFF;
constexpr uint64_t MAC_BITS_UPPER_MASK = 0x0FFFFFF000000;
constexpr uint64_t MAC_BITS_EXTENDED_MASK = 0xF000000000000;

MAC::MAC(Emulator* dsp) {
	this->dsp = dsp;
	output_shift = 0;
	value.raw = 0;
}

uint24_t MAC::getUpper() {
	uint32_t upper = value.upper;
	if (output_shift >= 0) {
		upper = upper << output_shift;
	} else {
		upper = upper >> (-output_shift);
	}
	uint24_t retval{ upper };
	return retval;
}

uint24_t MAC::getLower() {
	uint32_t lower = value.lower;
	if (output_shift >= 0) {
		lower = lower << output_shift;
	} else {
		lower = lower >> (-output_shift);
	}
	uint24_t retval{ lower };
	return retval;
}

void MAC::set(uint64_t value) {
	this->value.raw = value;
}

void MAC::set(MAC mac) {
	this->value.raw = mac.value.raw;
}

void MAC::setUpper(uint32_t upper) {
	//Assume this also clears the > 1.0 bits
	value.ext = 0;
	value.upper = upper;
}

void MAC::setLower(uint32_t lower) {
	value.lower = lower;
}

void MAC::clear() {
	value.raw = 0;
}

void MAC::clearUpper() {
	value.upper = 0;
}

void MAC::clearLower() {
	value.lower = 0;
}

const uint64_t SHIFT = 47;
const uint64_t FACTOR = (uint64_t)1 << SHIFT;

void MAC::multiply(int24_t rhs) {
	double dx = (double)value.raw / FACTOR;
	double dy = (double)((uint64_t)rhs.value << 24);

	double dresult = dx * dy;
	uint64_t result = (uint64_t)(dresult * FACTOR);

	//TODO: saturation logic
	//TODO: flags

	value.raw = result;
	printf("Multiplication resulted in %f raw: %X\n", dresult, value.raw);
}
void MAC::multiply(MAC mac) {
	double dx = (double)value.raw / FACTOR;
	double dy = (double)mac.value.raw / FACTOR;

	double dresult = dx * dy;
	uint64_t result = (uint64_t)(dresult * FACTOR);

	//TODO: saturation logic
	//TODO: flags

	value.raw = result;
	printf("Multiplication resulted in %f raw: %X\n", dresult, value.raw);
}

void MAC::mac(int24_t rhs) {
	double dx = (double)value.raw / FACTOR;
	double dy = (double)((uint64_t)rhs.value << 24);

	double dresult = dx * dy;
	uint64_t result = (uint64_t)(dresult * FACTOR);

	//TODO: saturation logic
	//TODO: flags

	value.raw += result;
	printf("Multiplication resulted in %f raw: %X\n", dresult, value.raw);
}
void MAC::mac(MAC mac) {
	double dx = (double)value.raw / FACTOR;
	double dy = (double)mac.value.raw / FACTOR;

	double dresult = dx * dy;
	uint64_t result = (uint64_t)(dresult * FACTOR);

	//TODO: saturation logic
	//TODO: flags

	value.raw += result;
	printf("Multiplication resulted in %f raw: %X\n", dresult, value.raw);
}