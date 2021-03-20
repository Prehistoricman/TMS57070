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

void MAC::updateOutputShift() {
	switch (dsp->CR1.MOSM) {
	case 0: output_shift = 0; break;
	case 1: output_shift = 2; break;
	case 2: output_shift = 4; break;
	case 3: output_shift = -8; break;
	}
}

uint24_t MAC::getUpper() {
	uint32_t upper = value.upper;
	updateOutputShift();
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
	updateOutputShift();
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

void MAC::shift(int8_t amount) {
	if (amount < 0) { //Right shift
		value.raw = value.raw >> -amount;
	} else { //left shift
		value.raw = value.raw << amount;
	}
}

constexpr uint64_t SHIFT_52 = 48;
constexpr uint64_t FACTOR_52 = (int64_t)1 << (SHIFT_52 - 1);
constexpr uint64_t SHIFT_24 = 24;
constexpr uint64_t FACTOR_24 = (int64_t)1 << (SHIFT_24 - 1);

int64_t MAC::mult_internal(int64_t lhs, int64_t rhs) { //Incoming lhs and rhs are aligned like a MACC
	//TODO: saturation logic
	//TODO: flags
	double lhs_double = ((double)lhs) / FACTOR_52;
	double rhs_double = ((double)rhs) / FACTOR_52;
	
	double dresult = lhs_double * rhs_double;
	int64_t result = (int64_t)(dresult * FACTOR_52);
	
	printf("Multiplication resulted in %f raw: %llX\n", dresult, result);
	return result;
}

int64_t MAC::unsign(int64_t value) {
	//Convert a signed MACC-like number into an unsigned one
	return value;
}

void MAC::multiply(int24_t rhs, signs_t signs) {
	int64_t lhs_aligned;
	int64_t rhs_aligned;

	if (signs == signs_t::SS || signs == signs_t::SU) {
		lhs_aligned = value.raw;
	} else {
		lhs_aligned = unsign(value.raw);
	}
	if (signs == signs_t::SS || signs == signs_t::US) {
		rhs_aligned = ((int64_t)rhs.value) << SHIFT_24;
	} else {
		rhs_aligned = unsign(((int64_t)rhs.value) << SHIFT_24);
	}
	
	value.raw = mult_internal(lhs_aligned, rhs_aligned);
}
void MAC::multiply(int24_t lhs, int24_t rhs, signs_t signs) {
	int64_t lhs_aligned;
	int64_t rhs_aligned;

	if (signs == signs_t::SS || signs == signs_t::SU) {
		lhs_aligned = ((int64_t)lhs.value) << SHIFT_24;
	} else {
		lhs_aligned = unsign(((int64_t)lhs.value) << SHIFT_24);
	}
	if (signs == signs_t::SS || signs == signs_t::US) {
		rhs_aligned = ((int64_t)rhs.value) << SHIFT_24;
	} else {
		rhs_aligned = unsign(((int64_t)rhs.value) << SHIFT_24);
	}
	
	value.raw = mult_internal(lhs_aligned, rhs_aligned);
}
void MAC::multiply(MAC mac, signs_t signs) {
	int64_t lhs_aligned;
	int64_t rhs_aligned;

	if (signs == signs_t::SS || signs == signs_t::SU) {
		lhs_aligned = value.raw;
	} else {
		lhs_aligned = unsign(value.raw);
	}
	if (signs == signs_t::SS || signs == signs_t::US) {
		rhs_aligned = mac.value.raw;
	} else {
		rhs_aligned = unsign(mac.value.raw);
	}
	
	value.raw = mult_internal(lhs_aligned, rhs_aligned);
}

void MAC::mac(int24_t rhs, signs_t signs) {
	int64_t current = value.raw;
	//Now use the normal multiply function
	multiply(rhs, signs);
	value.raw += current;
	//TODO: calculate flags after accumulation
}
void MAC::mac(int24_t lhs, int24_t rhs, signs_t signs) {
	int64_t current = value.raw;
	//Now use the normal multiply function
	multiply(lhs, rhs, signs);
	value.raw += current;
	//TODO: calculate flags after accumulation
}
void MAC::mac(MAC mac, signs_t signs) {
	int64_t current = value.raw;
	//Now use the normal multiply function
	multiply(mac, signs);
	value.raw += current;
	//TODO: calculate flags after accumulation
}

#if 0
int64_t MAC::mult_internal(double lhs, double rhs) {
	//TODO: saturation logic
	//TODO: flags
	
	double dresult = lhs * rhs;
	int64_t result = (int64_t)(dresult * FACTOR_52);
	
	printf("Multiplication resulted in %f raw: %llX\n", dresult, result);
}

void MAC::multiply(int24_t rhs, signs_t signs) {
	double lhs_double = (double)value.raw / FACTOR_52;
	double rhs_double = (double)((int64_t)rhs.value) / FACTOR_24;
	printf("Multiplication %f x %f\n", lhs_double, rhs_double);

	value.raw = mult_internal(lhs_double, rhs_double);
}
void MAC::multiply(int24_t lhs, int24_t rhs, signs_t signs) {
	double lhs_double = (double)((int64_t)lhs.value) / FACTOR_24;
	double rhs_double = (double)((int64_t)rhs.value) / FACTOR_24;
	printf("Multiplication %f x %f\n", lhs_double, rhs_double);

	value.raw = mult_internal(lhs_double, rhs_double);
}
void MAC::multiply(MAC mac, signs_t signs) {
	double lhs_double = (double)value.raw / FACTOR_52;
	double rhs_double = (double)mac.value.raw / FACTOR_52;
	printf("Multiplication %f x %f\n", lhs_double, rhs_double);

	value.raw = mult_internal(lhs_double, rhs_double);
}

void MAC::mac(int24_t rhs, signs_t signs) {
	double lhs_double = (double)value.raw / FACTOR_52;
	double rhs_double = (double)((int64_t)rhs.value) / FACTOR_24;
	printf("MAC %llX + %f x %f\n", value.raw, lhs_double, rhs_double);

	value.raw += mult_internal(lhs_double, rhs_double);;
}
void MAC::mac(int24_t lhs, int24_t rhs, signs_t signs) {
	double lhs_double = (double)((int64_t)lhs.value) / FACTOR_24;
	double rhs_double = (double)((int64_t)rhs.value) / FACTOR_24;
	printf("MAC %llX + %f x %f\n", value.raw, lhs_double, rhs_double);

	value.raw += mult_internal(lhs_double, rhs_double);;
}
void MAC::mac(MAC mac, signs_t signs) {
	double lhs_double = (double)value.raw / FACTOR_52;
	double rhs_double = (double)mac.value.raw / FACTOR_52;
	printf("MAC %llX + %f x %f\n", value.raw, lhs_double, rhs_double);

	value.raw += mult_internal(lhs_double, rhs_double);;
}
#endif