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

		void multiply(int24_t value);
		void multiply(MAC mac);

		void mac(int24_t value);
		void mac(MAC mac);

		int16_t output_shift;

	private:
		mac_value_internal_t value;

		Emulator* dsp;
	};

}