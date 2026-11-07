#include "TMS57070_MAC.h"
#include "TMS57070.h"
#include <cassert>

using namespace TMS57070;

constexpr uint64_t MAC_BITS_LOWER_MASK = 0x0000000FFFFFF;
constexpr uint64_t MAC_BITS_UPPER_MASK = 0x0FFFFFF000000;
constexpr uint64_t MAC_BITS_EXTENDED_MASK = 0xF000000000000;

MAC::MAC(Emulator* dsp) {
	this->dsp = dsp;
	output_shift = 0;
	value.raw = 0;
}

int24_t MAC::getUpper() {
	//Apply output shifter
	int64_t raw_shifted = value.raw;
	if (output_shift >= 0) {
		raw_shifted = raw_shifted << output_shift;
	} else {
		raw_shifted = raw_shifted >> (-output_shift);
	}

	//Apply output rounder (step 1)
	int64_t mask = 0;
	if (bit_count) {
		int64_t carry = raw_shifted & (static_cast<int64_t>(1) << (bit_count - 1)); //Most significant bit which is about to be deleted
		mask = (static_cast<int64_t>(1) << bit_count) - 1; //Mask of bits to be deleted
		raw_shifted += carry << 1;
	}

	//Apply overflow limiter
	int64_t upper = raw_shifted >> 24;
	if (dsp->CR1.MOVM) {
		if (upper > INT24_MAX) {
			upper = INT24_MAX;
		} else if (upper < INT24_MIN) {
			upper = INT24_MIN;
		}
	}

	//Apply output rounder (step 2)
	if (bit_count) {
		upper = upper & ~(mask >> 24);
	}

	int24_t retval{ (int32_t)upper };
	return retval;
}

uint24_t MAC::getLower() {
	int64_t raw_shifted = value.raw;
	if (output_shift >= 0) {
		raw_shifted = raw_shifted << output_shift;
	} else {
		raw_shifted = raw_shifted >> (-output_shift);
	}

	uint32_t lower = raw_shifted & UINT24_MAX;
	
	if (dsp->CR1.MOVM) {
		int64_t upper = raw_shifted >> 24; //lower will saturate with the upper
		if (upper > INT24_MAX) {
			lower = UINT24_MAX;
		} else if (upper < INT24_MIN) {
			lower = 0;
		}
	}

	uint24_t retval{ lower };
	return retval;
}

void MAC::set(uint64_t value) {
	this->value.raw = value;
}

void MAC::set(MAC mac) {
	this->value = mac.value;
}

void MAC::setUpper(int32_t upper) {
	//Assume this also clears the > 1.0 bits
	value.raw_unsigned &= MAC_BITS_LOWER_MASK; //clear upper + ext
	value.raw_unsigned |= (int64_t)upper << 24;
}

void MAC::setLower(uint32_t lower) {
	value.raw_unsigned &= (MAC_BITS_UPPER_MASK | MAC_BITS_EXTENDED_MASK); //Clear lower
	value.raw_unsigned |= lower & UINT24_MAX;
}

void MAC::clear() {
	value.raw = 0;
}

void MAC::clearUpper() {
	value.raw_unsigned &= MAC_BITS_LOWER_MASK;
}

void MAC::clearLower() {
	value.raw_unsigned &= (MAC_BITS_UPPER_MASK | MAC_BITS_EXTENDED_MASK);
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

int64_t MAC::mult_internal(int64_t lhs, int64_t rhs, bool negate) { //Incoming lhs and rhs are aligned like a MACC
	//TODO: saturation logic
	//TODO: flags
	double lhs_double = ((double)lhs) / FACTOR_52;
	double rhs_double = ((double)rhs) / FACTOR_52;
	
	double dresult = lhs_double * rhs_double;
	if (negate) {
		dresult = -dresult;
	}
	int64_t result = (int64_t)(dresult * FACTOR_52);
	
	tms_printf("Multiplication resulted in %f raw: %llX\n", dresult, result);
	return result;
}

int64_t MAC::unsign(int64_t value) {
	//Convert a signed MACC-like number into an unsigned one
	return value;
}

void MAC::multiply(int24_t rhs, MACSigns signs, bool negate) {
	int64_t lhs_aligned;
	int64_t rhs_aligned;

	if (signs == MACSigns::SS || signs == MACSigns::SU) {
		lhs_aligned = value.raw;
	} else {
		lhs_aligned = unsign(value.raw);
	}
	if (signs == MACSigns::SS || signs == MACSigns::US) {
		rhs_aligned = ((int64_t)rhs.value) << SHIFT_24;
	} else {
		rhs_aligned = ((int64_t)(uint32_t)rhs.value) << SHIFT_24;
	}
	
	value.raw = mult_internal(lhs_aligned, rhs_aligned, negate);
}
void MAC::multiply(int24_t lhs, int24_t rhs, MACSigns signs, bool negate) {
	int64_t lhs_aligned;
	int64_t rhs_aligned;

	if (signs == MACSigns::SS || signs == MACSigns::SU) {
		lhs_aligned = ((int64_t)lhs.value) << SHIFT_24;
	} else {
		lhs_aligned = ((int64_t)((uint32_t)lhs.value & UINT24_MAX)) << SHIFT_24;
	}
	if (signs == MACSigns::SS || signs == MACSigns::US) {
		rhs_aligned = ((int64_t)rhs.value) << SHIFT_24;
	} else {
		rhs_aligned = ((int64_t)((uint32_t)rhs.value & UINT24_MAX)) << SHIFT_24;
	}
	
	value.raw = mult_internal(lhs_aligned, rhs_aligned, negate);
}
void MAC::multiply(MAC mac, MACSigns signs, bool negate) {
	int64_t lhs_aligned;
	int64_t rhs_aligned;

	if (signs == MACSigns::SS || signs == MACSigns::SU) {
		lhs_aligned = value.raw;
	} else {
		lhs_aligned = unsign(value.raw);
	}
	if (signs == MACSigns::SS || signs == MACSigns::US) {
		rhs_aligned = mac.value.raw;
	} else {
		rhs_aligned = unsign(mac.value.raw);
	}
	
	value.raw = mult_internal(lhs_aligned, rhs_aligned, negate);
}

void MAC::mac(int24_t rhs, MACSigns signs, bool negate) {
	int64_t current = value.raw;
	//Apply accumulation shifter
	switch (dsp->CR1.MASM) {
	case 1: current = current << 2;  break;
	case 2: current = current << 4;  break;
	case 3: current = current >> 24; break;
	}
	//Now use the normal multiply function
	multiply(rhs, signs, negate);
	value.raw += current;

	tms_printf("MAC accumulated with raw: %llX result: %llX\n", current, value.raw);
	//TODO: calculate flags after accumulation
}
void MAC::mac(int24_t lhs, int24_t rhs, MACSigns signs, bool negate) {
	int64_t current = value.raw;
	//Apply accumulation shifter
	switch (dsp->CR1.MASM) {
	case 1: current = current << 2;  break;
	case 2: current = current << 4;	 break;
	case 3: current = current >> 24; break;
	}
	//Now use the normal multiply function
	multiply(lhs, rhs, signs, negate);
	value.raw += current;

	tms_printf("MAC accumulated with raw: %llX result: %llX\n", current, value.raw);
	//TODO: calculate flags after accumulation
}
void MAC::mac(MAC mac, MACSigns signs, bool negate) {
	int64_t current = value.raw;
	//Apply accumulation shifter
	switch (dsp->CR1.MASM) {
	case 1: current = current << 2;  break;
	case 2: current = current << 4;	 break;
	case 3: current = current >> 24; break;
	}
	//Now use the normal multiply function
	multiply(mac, signs, negate);
	value.raw += current;
	
	tms_printf("MAC accumulated with raw: %llX result: %llX\n", current, value.raw);
	//TODO: calculate flags after accumulation
}

#if 0
int64_t MAC::mult_internal(double lhs, double rhs) {
	//TODO: saturation logic
	//TODO: flags
	
	double dresult = lhs * rhs;
	int64_t result = (int64_t)(dresult * FACTOR_52);
	
	tms_printf("Multiplication resulted in %f raw: %llX\n", dresult, result);
}

void MAC::multiply(int24_t rhs, MACSigns signs) {
	double lhs_double = (double)value.raw / FACTOR_52;
	double rhs_double = (double)((int64_t)rhs.value) / FACTOR_24;
	tms_printf("Multiplication %f x %f\n", lhs_double, rhs_double);

	value.raw = mult_internal(lhs_double, rhs_double);
}
void MAC::multiply(int24_t lhs, int24_t rhs, MACSigns signs) {
	double lhs_double = (double)((int64_t)lhs.value) / FACTOR_24;
	double rhs_double = (double)((int64_t)rhs.value) / FACTOR_24;
	tms_printf("Multiplication %f x %f\n", lhs_double, rhs_double);

	value.raw = mult_internal(lhs_double, rhs_double);
}
void MAC::multiply(MAC mac, MACSigns signs) {
	double lhs_double = (double)value.raw / FACTOR_52;
	double rhs_double = (double)mac.value.raw / FACTOR_52;
	tms_printf("Multiplication %f x %f\n", lhs_double, rhs_double);

	value.raw = mult_internal(lhs_double, rhs_double);
}

void MAC::mac(int24_t rhs, MACSigns signs) {
	double lhs_double = (double)value.raw / FACTOR_52;
	double rhs_double = (double)((int64_t)rhs.value) / FACTOR_24;
	tms_printf("MAC %llX + %f x %f\n", value.raw, lhs_double, rhs_double);

	value.raw += mult_internal(lhs_double, rhs_double);;
}
void MAC::mac(int24_t lhs, int24_t rhs, MACSigns signs) {
	double lhs_double = (double)((int64_t)lhs.value) / FACTOR_24;
	double rhs_double = (double)((int64_t)rhs.value) / FACTOR_24;
	tms_printf("MAC %llX + %f x %f\n", value.raw, lhs_double, rhs_double);

	value.raw += mult_internal(lhs_double, rhs_double);;
}
void MAC::mac(MAC mac, MACSigns signs) {
	double lhs_double = (double)value.raw / FACTOR_52;
	double rhs_double = (double)mac.value.raw / FACTOR_52;
	tms_printf("MAC %llX + %f x %f\n", value.raw, lhs_double, rhs_double);

	value.raw += mult_internal(lhs_double, rhs_double);;
}
#endif