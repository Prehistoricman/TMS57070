#pragma once
#include <cstdint>

namespace TMS57070 {

	struct uint24_t;
	struct int24_t;
	class Emulator;

	union mac_value_internal_t {
		int64_t raw : 52;
		uint64_t raw_unsigned : 52;
		struct {
			uint32_t lower : 24;
			uint32_t upper : 24;
			uint32_t ext : 4;
		};
	};

	enum class signs_t {
		SS,
		SU,
		US,
		UU
	};

	class MAC {
	public:
		MAC(Emulator* dsp);

		uint24_t getUpper();
		uint24_t getLower();
		void set(uint64_t value);
		void set(MAC mac); //Copy value from another MAC
		void setUpper(uint32_t value);
		void setLower(uint32_t value);
		void clear();
		void clearUpper();
		void clearLower();

		void multiply(int24_t value, signs_t signs);
		void multiply(int24_t lhs, int24_t rhs, signs_t signs);
		void multiply(MAC mac, signs_t signs);

		void mac(int24_t value, signs_t signs);
		void mac(int24_t lhs, int24_t rhs, signs_t signs);
		void mac(MAC mac, signs_t signs);

		void shift(int8_t amount);

		int16_t output_shift;

	private:
		void updateOutputShift();
		int64_t mult_internal(int64_t lhs, int64_t rhs);
		int64_t unsign(int64_t value);

		mac_value_internal_t value;

		Emulator* dsp;
	};

}