#include "TMS57070.h"
#include <cassert>

using namespace TMS57070;

//Decodes a load instruction and sets the ACC accordingly
//Returns pointer to the destination ACC
int24_t* Emulator::loadACC() {
	int32_t result;
	//Where is the data destination?
	int24_t* dst = &ACC1;
	if (opcode1_flag4) {
		dst = &ACC2;
	}
	//Where is the data source?
	uint8_t src_code = opcode1 & 3;
	switch (src_code) {
	case 0:
		result = DMEM[dmemAddressing()].value;
		break;
	case 1:
		result = CMEM[cmemAddressing()].value;
		break;
	case 2:
		if (opcode1_flag8) {
			result = ACC2.value;
		} else {
			result = ACC1.value;
		}
		break;
	case 3:
		if (opcode1_flag8) {
			result = MACC2_delayed2.getUpper().value;
		} else {
			result = MACC1_delayed2.getUpper().value;
		}
		break;
	default:
		result = 0;
		assert(false); //Should not happen
	}

	//Apply flags
	dst->value = processACCValue(result);

	return dst;
}

//Returns pointer to the destination ACC
int24_t* Emulator::arith(ArithOperation operation) {
	//Where is the data destination?
	int24_t* dst = &ACC1;
	if (opcode1_flag4) {
		dst = &ACC2;
	}
	//Where is the data source?
	uint8_t src_code = opcode1 & 3;
	int24_t lhs;
	int24_t rhs;
	if ((opcode1 == 0x3C) || (opcode1 == 0x3D) || (opcode1 == 0x3E)) {
		//These instructions use CMEM and DMEM always
		lhs.value = DMEM[dmemAddressing()].value;
		rhs.value = CMEM[cmemAddressing()].value;
	} else { //Normal ALU instructions
		switch (src_code) {
		case 0: //DMEM op ACCx
			lhs.value = DMEM[dmemAddressing()].value;
			if (opcode1_flag8) {
				rhs.value = ACC2.value;
			} else {
				rhs.value = ACC1.value;
			}
			break;
		case 1: //DMEM op MACCx
			lhs.value = DMEM[dmemAddressing()].value;
			if (opcode1_flag8) {
				rhs.value = MACC2_delayed2.getUpper().value;
			} else {
				rhs.value = MACC1_delayed2.getUpper().value;
			}
			break;
		case 2: //CMEM op ACCx
			lhs.value = CMEM[cmemAddressing()].value;
			if (opcode1_flag8) {
				rhs.value = ACC2.value;
			} else {
				rhs.value = ACC1.value;
			}
			break;
		case 3: //CMEM op MACCx
			lhs.value = CMEM[cmemAddressing()].value;
			if (opcode1_flag8) {
				rhs.value = MACC2_delayed2.getUpper().value;
			} else {
				rhs.value = MACC1_delayed2.getUpper().value;
			}
			break;
		default:
			assert(false); //Should not happen
			lhs.value = 0;
			rhs.value = 0;
		}
	}

	int32_t result;
	switch (operation) {
	case ArithOperation::Add:
		result = lhs.value + rhs.value;
		break;
	case ArithOperation::Sub:
		result = lhs.value - rhs.value;
		break;
	case ArithOperation::And:
		result = lhs.value & rhs.value;
		break;
	case ArithOperation::Or:
		result = lhs.value | rhs.value;
		break;
	case ArithOperation::Xor:
		result = lhs.value ^ rhs.value;
		break;
	case ArithOperation::Cmp:
		//Quit early to avoid writing to ACC
		result = lhs.value - rhs.value;
		CR1.AOV = (result < INT24_MIN) || (result > INT24_MAX);
		CR1.ACCZ = result == 0;
		CR1.ACCN = result < 0;
		return nullptr; //This value doesn't get saved to ACC
		break;
	default:
		assert(false);
	}

	//Set flags
	result = processACCValue(result);

	dst->value = result;
	return dst;
}

//Takes in a new ACC value as a result of a calculation and:
//applies saturation logic, truncates, and sets overflow flag
//returns the correct value to load
int32_t Emulator::processACCValue(int32_t acc) {
	if (CR1.AOVM) { //saturation logic on
		if (acc < INT24_MIN) {
			//Saturate at minimum
			tms_printf("processACCValue: saturating value %d (%X) at -ve\n", acc, acc);
			acc = INT24_MIN;
			CR1.AOV = 1;
		} else if (acc > INT24_MAX) {
			tms_printf("processACCValue: saturating value %d (%X) at +ve\n", acc, acc);
			acc = INT24_MAX;
			CR1.AOV = 1;
		}
	} else { //saturation logic off
		//nothing to do?
		//TODO: can acc overflow with sat. logic off?
		if ((acc < INT24_MIN) || (acc > INT24_MAX)) {
			CR1.AOV = 1;
		}
	}
	
	int24_t acc_i24{acc}; //Value is truncated here. Sign is re-calculated, not preserved

	CR1.ACCZ = acc_i24.value == 0;
	CR1.ACCN = acc_i24.value < 0;

	return acc_i24.value;
}

void Emulator::exec1st() {
	switch (opcode1) {
	case 0x00: //NOP
	case 0x01:
	case 0x02:
	case 0x03:
		break;

	case 0x4: //Load accumulator unsigned
	case 0x5:
	case 0x6:
	case 0x7:
	{
		int24_t* dst = loadACC();
		if (dst->value < 0) { //TODO: test this
			dst->value = ~dst->value + 1;
		}

		tms_printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x8: //Load accumulator complemented
	case 0x9:
	case 0xA:
	case 0xB:
	{
		int24_t* dst = loadACC();
		dst->value = ~dst->value + 1; //TODO: saturation logic

		tms_printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x10: //Load accumulator
	case 0x11:
	case 0x12:
	case 0x13:
	{
		int24_t* dst = loadACC();

		tms_printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x14: //Increment and load accumulator
	case 0x15:
	case 0x16:
	case 0x17:
	{
		int24_t* dst = loadACC();
		dst->value++;
		tms_printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x18: //Decrement and load accumulator
	case 0x19:
	case 0x1A:
	case 0x1B:
	{
		int24_t* dst = loadACC();
		dst->value--;
		tms_printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x1C:
	{ //SHACC shift accumulator
		int24_t* dst = &ACC1;
		if (opcode1_flag4) {
			dst = &ACC2;
		}
		int32_t value = dst->value;
		if (opcode1_flag8) {
			//shift left
			value = value << 1;
		} else {
			//Shift right
			value = value >> 1;
		}
		value = processACCValue(value);
		dst->value = value;
	} break;

	case 0x1D: //ZACC Zero accumulator (and something else)
		if (opcode1_flag8) {
			if (UNKNOWN_STRICT) {
				assert(false); //idk
			}
		} else {
			//ZACC zero accumulator
			int24_t* dst = &ACC1;
			if (opcode1_flag4) {
				dst = &ACC2;
			}
			dst->value = 0;
			processACCValue(dst->value); //Set flags
		}
		break;

	case 0x20: //Add accumulator
	case 0x21:
	case 0x22:
	case 0x23:
	{
		int24_t* dst = arith(ArithOperation::Add);
		tms_printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x24: //Sub accumulator
	case 0x25:
	case 0x26:
	case 0x27:
	{
		int24_t* dst = arith(ArithOperation::Sub);
		tms_printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x28: //AND accumulator
	case 0x29:
	case 0x2A:
	case 0x2B:
	{
		int24_t* dst = arith(ArithOperation::And);
		tms_printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x2C: //OR accumulator
	case 0x2D:
	case 0x2E:
	case 0x2F:
	{
		int24_t* dst = arith(ArithOperation::Or);
		tms_printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x30: //XOR accumulator
	case 0x31:
	case 0x32:
	case 0x33:
	{
		int24_t* dst = arith(ArithOperation::Xor);
		tms_printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x34: //CMP compare
	case 0x35:
	case 0x36:
	case 0x37:
	{
		int24_t* dst = arith(ArithOperation::Cmp);
	} break;

	case 0x39:
		//TODO: 16-bit truncation for the 16-bit mode
		if (opcode1_flag4) { //WRE
			//FIXME: write is not immediate and can clash with other XMEM operations
			XMEM[xmemAddressing(CMEM[cmemAddressing()].value)].value = DMEM[dmemAddressing()].value;
		} else { //RDE
			XMEM_read_addr = xmemAddressing(CMEM[cmemAddressing()].value);

			switch (CR3.XBUS) {
			case 0: //4-bit
				XMEM_read_cycles = CR3.XWORD ? 21 : 15;
				break;
			case 1: //8-bit
				XMEM_read_cycles = CR3.XWORD ? 12 : 9;
				break;
			case 2: //12-bit
				XMEM_read_cycles = CR3.XWORD ? 9 : 0;
				break;
			case 3: //16-bit
				XMEM_read_cycles = CR3.XWORD ? 0 : 6;
				break;
			}
		}
		break;


	case 0x3A: //NOP
	case 0x3B:
		break;

	case 0x3C: //ADD CMEM + DMEM
		if (opcode1_flag8) {
			arith(ArithOperation::Sub);
		} else {
			arith(ArithOperation::Add);
		}
		break;
	case 0x3D: //AND bitwise CMEM AND DMEM
		if (opcode1_flag8) {
			arith(ArithOperation::Or);
		} else {
			arith(ArithOperation::And);
		}
		break;
	case 0x3E: //XOR bitwise CMEM AND DMEM
		if (opcode1_flag8) {
			if (UNKNOWN_STRICT) {
				assert(false); //idk
			}
		} else {
			arith(ArithOperation::Xor);
		}
		break;

	case 0x3F: //NOP
		break;

	case 0x40: //Multiply CMEM by ACCx
	case 0x41: //ACC2
	case 0x44: //Unsigned CMEM
	case 0x45: //ACC2
	case 0x48: //Unsigned ACC
	case 0x49: //ACC2
	case 0x4C: //Unsigned both
	case 0x4D: //ACC2
	{
		MAC* MACx = &MACC1;
		if (opcode1_flag4) MACx = &MACC2;

		bool negate = opcode1_flag8;

		int24_t* ACCx = &ACC1;
		if ((opcode1 & 1) == 1) ACCx = &ACC2;

		int24_t* cmem_word = &CMEM[cmemAddressing()];

		MACSigns signs;
		switch (opcode1) {
		case 0x40: case 0x41: signs = MACSigns::SS; break;
		case 0x44: case 0x45: signs = MACSigns::US; break;
		case 0x48: case 0x49: signs = MACSigns::SU; break;
		case 0x4C: case 0x4D: signs = MACSigns::UU; break;
		}

		MACx->multiply(*cmem_word, *ACCx, signs, negate);
	} break;

	case 0x42: //Multiply CMEM by DMEM
	case 0x46: //unsigned CMEM
	case 0x4A: //unsigned DMEM
	case 0x4E: //unsigned both
	{
		MAC* MACx = &MACC1;
		if (opcode1_flag4) MACx = &MACC2;

		bool negate = opcode1_flag8;
		
		int24_t* cmem_word = &CMEM[cmemAddressing()];
		int24_t* dmem_word = &DMEM[dmemAddressing()];

		MACSigns signs;
		switch (opcode1) {
		case 0x42: signs = MACSigns::SS; break;
		case 0x46: signs = MACSigns::US; break;
		case 0x4A: signs = MACSigns::SU; break;
		case 0x4E: signs = MACSigns::UU; break;
		}

		MACx->multiply(*cmem_word, *dmem_word, signs, negate);
	} break;

	case 0x50: //MAC CMEM by ACCx
	case 0x51: //ACC2
	case 0x52: //MAC DMEM by ACCx
	case 0x53: //ACC2
	{
		MAC* MACx = &MACC1;
		if (opcode1_flag4) MACx = &MACC2;

		bool negate = opcode1_flag8;

		int24_t* ACCx = &ACC1;
		if ((opcode1 & 1) == 1) ACCx = &ACC2;

		int24_t* word;
		if ((opcode1 & 2) == 0) {
			word = &CMEM[cmemAddressing()];
		} else {
			word = &DMEM[dmemAddressing()];
		}
		MACx->mac(*ACCx, *word, MACSigns::SS, negate);
	} break;

	case 0x54: //MAC unsigned CMEM by ACCx
	case 0x55: //ACC2
	case 0x56: //MAC DMEM by unsigned ACCx
	case 0x57: //ACC2
	{
		MAC* MACx = &MACC1;
		if (opcode1_flag4) MACx = &MACC2;

		bool negate = opcode1_flag8;

		int24_t* ACCx = &ACC1;
		if ((opcode1 & 1) == 1) ACCx = &ACC2;

		int24_t* word;
		MACSigns signs;
		if ((opcode1 & 2) == 0) {
			word = &CMEM[cmemAddressing()];
			signs = MACSigns::SU;
		} else {
			word = &DMEM[dmemAddressing()];
			signs = MACSigns::US;
		}
		MACx->mac(*ACCx, *word, signs, negate);
	} break;

	case 0x58: //MAC CMEM by unsigned ACCx
	case 0x59: //ACC2
	case 0x5A: //MAC unsigned DMEM by ACCx
	case 0x5B: //ACC2
	{
		MAC* MACx = &MACC1;
		if (opcode1_flag4) MACx = &MACC2;

		bool negate = opcode1_flag8;

		int24_t* ACCx = &ACC1;
		if ((opcode1 & 1) == 1) ACCx = &ACC2;

		int24_t* word;
		MACSigns signs;
		if ((opcode1 & 2) == 0) {
			word = &CMEM[cmemAddressing()];
			signs = MACSigns::US;
		} else {
			word = &DMEM[dmemAddressing()];
			signs = MACSigns::SU;
		}
		MACx->mac(*ACCx, *word, signs, negate);
	} break;

	case 0x5C: //MAC unsigned CMEM by unsigned ACCx
	case 0x5D: //ACC2
	case 0x5E: //MAC unsigned DMEM by unsigned ACCx
	case 0x5F: //ACC2
	{
		MAC* MACx = &MACC1;
		if (opcode1_flag4) MACx = &MACC2;

		bool negate = opcode1_flag8;

		int24_t* ACCx = &ACC1;
		if ((opcode1 & 1) == 1) ACCx = &ACC2;

		int24_t* word;
		if ((opcode1 & 2) == 0) {
			word = &CMEM[cmemAddressing()];
		} else {
			word = &DMEM[dmemAddressing()];
		}
		MACx->mac(*ACCx, *word, MACSigns::UU, negate);
	} break;

	case 0x60: //Multiply CMEM by ACC (and accumulate shifted MAC)
	case 0x61: //ACC2
	case 0x62: //Multiply DMEM by ACC
	case 0x63: //ACC2
	{
		MAC* MACx = &MACC1;
		if (opcode1_flag4) MACx = &MACC2;

		bool negate = opcode1_flag8;

		int24_t* ACCx = &ACC1;
		if ((opcode1 & 1) == 1) ACCx = &ACC2;

		int24_t* word;
		if ((opcode1 & 2) == 0) {
			word = &CMEM[cmemAddressing()];
		} else {
			word = &DMEM[dmemAddressing()];
		}

		//Shift MAC right by 24
		if (CR1.MASM == 0) {
			MACx->shift(-24);
		}

		MACx->mac(*ACCx, *word, MACSigns::SS, negate);
	} break;

	case 0x64: //Multiply unsigned CMEM by ACC (and accumulate shifted MAC)
	case 0x65: //ACC2
	case 0x66: //Multiply DMEM by unsigned ACC
	case 0x67: //ACC2
	{
		MAC* MACx = &MACC1;
		if (opcode1_flag4) MACx = &MACC2;

		bool negate = opcode1_flag8;

		int24_t* ACCx = &ACC1;
		if ((opcode1 & 1) == 1) ACCx = &ACC2;

		int24_t* word;
		MACSigns signs;
		if ((opcode1 & 2) == 0) {
			word = &CMEM[cmemAddressing()];
			signs = MACSigns::SU;
		} else {
			word = &DMEM[dmemAddressing()];
			signs = MACSigns::US;
		}

		//Shift MAC right by 24
		if (CR1.MASM == 0) {
			MACx->shift(-24);
		}

		MACx->mac(*ACCx, *word, signs, negate);
	} break;

	case 0x68: //NOP
	case 0x69:
	case 0x6A:
	case 0x6B:
		break;

	case 0x6C: //MAC CMEM by DMEM
	case 0x6D: //unsigned CMEM
	case 0x6E: //unsigned DMEM
	case 0x6F: //unsigned both
	{
		MAC* MACx = &MACC1;
		if (opcode1_flag4) MACx = &MACC2;

		bool negate = opcode1_flag8;
		
		int24_t* cmem_word = &CMEM[cmemAddressing()];
		int24_t* dmem_word = &DMEM[dmemAddressing()];

		MACSigns signs;
		switch (opcode1) {
		case 0x6C: signs = MACSigns::SS; break;
		case 0x6D: signs = MACSigns::US; break;
		case 0x6E: signs = MACSigns::SU; break;
		case 0x6F: signs = MACSigns::UU; break;
		}

		MACx->mac(*cmem_word, *dmem_word, signs, negate);
	} break;

	case 0x70: //Multiply CMEM with DMEM (and accumulate with shifted MAC)
	case 0x71: //Unsigned DMEM
	{
		MAC* MACx = &MACC1;
		if (opcode1_flag4) MACx = &MACC2;

		bool negate = opcode1_flag8;

		int24_t* cmem_word = &CMEM[cmemAddressing()];
		int24_t* dmem_word = &DMEM[dmemAddressing()];

		MACSigns signs = MACSigns::SS;
		if (opcode1 == 0x71) {
			signs = MACSigns::US;
		}

		//Shift MAC right by 24
		if (CR1.MASM == 0) {
			MACx->shift(-24);
		}

		MACx->mac(*cmem_word, *dmem_word, signs, negate);
	} break;

	case 0x72: //SHMAC shift MACC
	{
		MAC* MACx = &MACC1;
		if (opcode1_flag4) MACx = &MACC2;

		if (opcode1_flag8) { //Left shift
			MACx->shift(1);
		} else { //Right shift
			MACx->shift(-1);
		}
	} break;

	case 0x73: //Zero MACC (and something else)
		if (opcode1_flag8) {
			if (UNKNOWN_STRICT) {
				assert(false); //idk
			}
		} else {
			//Zero MACC
			MAC* MACx = &MACC1;
			if (opcode1_flag4) MACx = &MACC2;

			MACx->set(0);
		}
		break;

	case 0x75: //NOP
	case 0x76:
	case 0x77:
		break;

	case 0x78:
	case 0x79: //Load MAC high and clear
		if (opcode1_flag4) {
			MACC2.setLower(0);
		} else {
			MACC1.setLower(0);
		}
		//Let this fall through to set MAC high
	case 0x7A:
	case 0x7B: //Load MAC high
	case 0x7C:
	case 0x7D: //Load MAC low
	{
		int24_t load_word;
		if ((opcode1 & 0x1) == 0) { //Instructions 78, 7A, 7C: DMEM and ACC1
			if (opcode1_flag8) {
				load_word.value = ACC1.value;
			} else {
				load_word.value = DMEM[dmemAddressing()].value;
			}
		} else { //Instructions 79, 7B, 7D: CMEM and ACC2
			if (opcode1_flag8) {
				load_word.value = ACC2.value;
			} else {
				load_word.value = CMEM[cmemAddressing()].value;
			}
		}
		if (opcode1 < 0x7C) { //load MAC high
			if (opcode1_flag4) {
				MACC2.setUpper(load_word.value);
			} else {
				MACC1.setUpper(load_word.value);
			}
		} else { //load MAC low
			if (opcode1_flag4) {
				MACC2.setLower(load_word.value);
			} else {
				MACC1.setLower(load_word.value);
			}
		}
	} break;

	case 0x7E: //NOP
	case 0x7F:
		break;

	case 0xC1: //Load register with immediate
		switch ((insn >> 16) & 0xFF) {
		case 0x00: DA.one.value = insn; break;
		case 0x08: DA.two.value = insn; break;
		case 0x10: DIR.one.value = insn; break;
		case 0x18: DIR.two.value = insn; break;
		case 0x20: CA.one.value = insn; break;
		case 0x28: CA.two.value = insn; break;
		case 0x30: CIR.one.value = insn; break;
		case 0x38: CIR.two.value = insn; break;
		case 0x40: //LIRAE
			if (!CR1.ACCN) {
				CA.one.value = insn;
			}
			break;
		case 0x48: //LIRAE
			if (!CR1.ACCN) {
				CA.two.value = insn;
			}
			break;
		default:
			if (UNKNOWN_STRICT) {
				assert(false); //unknown
			}
			break;
		}
		break;

	case 0xC2: //Load DA imm
		DA.one.value = (insn & 0xFFF);
		DA.two.value = ((insn >> 12) & 0xFFF);
		break;
	case 0xC3: //Load DIR imm
		DIR.one.value = (insn & 0xFFF);
		DIR.two.value = ((insn >> 12) & 0xFFF);
		break;
	case 0xC4: //Load CA imm
		CA.one.value = (insn & 0xFFF);
		CA.two.value = ((insn >> 12) & 0xFFF);
		break;
	case 0xC5: //Load CIR imm
		CIR.one.value = (insn & 0xFFF);
		CIR.two.value = ((insn >> 12) & 0xFFF);
		break;

	case 0xC7: //Set DMEM circular registers
		DOFF.value = insn;
		DCIRC.value = insn >> 12;
		break;
	case 0xC8: //Set CMEM circular registers
		COFF.value = insn;
		CCIRC.value = insn >> 12;
		break;

	case 0xCA: //Load ACC1 imm
		ACC1.value = insn;
		processACCValue(ACC1.value); //Set flags
		break;
	case 0xCB: //Load ACC2 imm
		ACC2.value = insn;
		processACCValue(ACC2.value); //Set flags
		break;
	case 0xCC: //Load CR0 imm
		CR0.value = insn;
		break;
	case 0xCD: //Load CR1 imm
		CR1.value = insn;
		break;
	case 0xCE: //Load CR2 imm
	{
		//Handle flag behaviour
		cr2_t temp;
		temp.value = insn;
		uint8_t flagsToClear = temp.bytes[0] & CR2.bytes[0]; //Only clear flags that are common to both the input value and current value
		temp.bytes[0] = CR2.bytes[0] ^ flagsToClear; //Erase common bits
		CR2.value = temp.value;
	} break;
	case 0xCF: //Load CR3 imm
		CR3.value = insn;
		break;

	case 0xE0: //RPTK repeat next instruction
		RPTC = insn >> 16;
		rep_start_PC.value = PC.value;
		rep_end_PC.value = PC.value;
		break;
	case 0xE4: //RTPB repeat block
		RPTC = insn >> 16;
		rep_start_PC.value = PC.value;
		rep_end_PC.value = insn;
		if (rep_end_PC.value == PC.value) {
			RPTC = 0; //RPTB cannot do a single-instruction loop
		}
		break;

	case 0xEC: //RET
		assert(SP != 0); //Stack underflow!
		SP--;
		PC.value = stack[SP].value;
		break;
	case 0xEE: //RETI
		assert(SP != 0); //Stack underflow!
		SP--;
		PC.value = stack[SP].value;
		CR2.FREE = 1;
		RPTC = 0;
		break;

	case 0xF0: //Jmp
	case 0xF1: //Conditional jumps
	case 0xF2:
	case 0xF3:
	case 0xF4:
	case 0xF5:
	case 0xF6:
	case 0xF7:
	case 0xF8: //Call
	case 0xF9: //Conditional call
	case 0xFA:
	case 0xFB:
	case 0xFC:
	case 0xFD:
	case 0xFE:
	case 0xFF:
		execJmp();
		break;

	default:
		tms_printf("Unhandled 1st instruction: %08X\n", insn);
		if (UNKNOWN_STRICT) {
			printf("Unhandled 1st instruction: %08X\n", insn); //always print if we hit the assert
			assert(false);
		}
		break;
	}
}

void Emulator::exec2nd() {
	switch (opcode2) {
	case 0x00: //NOP
		break;

	case 0x01: //Save ACCx to MEM
		if (opcode2_flag4) {
			//ACC2
			if (opcode2_flag8) {
				CMEM[cmemAddressing()].value = ACC2.value;
			} else {
				DMEM[dmemAddressing()].value = ACC2.value;
			}
		} else {
			//ACC1
			if (opcode2_flag8) {
				CMEM[cmemAddressing()].value = ACC1.value;
			} else {
				DMEM[dmemAddressing()].value = ACC1.value;
			}
		}
		break;
	case 0x02: //Save MACC high to MEM
		if (opcode2_flag4) {
			//MACC2
			if (opcode2_flag8) {
				CMEM[cmemAddressing()].value = MACC2_delayed2.getUpper().value;
			} else {
				DMEM[dmemAddressing()].value = MACC2_delayed2.getUpper().value;
			}
		} else {
			//MACC1
			if (opcode2_flag8) {
				CMEM[cmemAddressing()].value = MACC1_delayed2.getUpper().value;
			} else {
				DMEM[dmemAddressing()].value = MACC1_delayed2.getUpper().value;
			}
		}
		break;
	case 0x03: //Save MACC low to MEM
		if (opcode2_flag4) {
			//MACC2
			if (opcode2_flag8) {
				CMEM[cmemAddressing()].value = MACC2_delayed2.getLower().value;
			} else {
				DMEM[dmemAddressing()].value = MACC2_delayed2.getLower().value;
			}
		} else {
			//MACC1
			if (opcode2_flag8) {
				CMEM[cmemAddressing()].value = MACC1_delayed2.getLower().value;
			} else {
				DMEM[dmemAddressing()].value = MACC1_delayed2.getLower().value;
			}
		}
		break;

	case 0x06: //Load dual from CMEM
	{
		//Load: (arg = 0-3) DA, DIR, CA, DIR
		uint32_t value = CMEM[cmemAddressing()].value;
		switch (opcode2_args) {
		case 0: //Load DA
			DA.one.value = (value & 0xFFF);
			DA.two.value = ((value >> 12) & 0xFFF);
			break;
		case 1: //Load DIR
			DIR.one.value = (value & 0xFFF);
			DIR.two.value = ((value >> 12) & 0xFFF);
			break;
		case 2: //Load CA
			CA.one.value = (value & 0xFFF);
			CA.two.value = ((value >> 12) & 0xFFF);
			break;
		case 3: //Load CIR
			CIR.one.value = (value & 0xFFF);
			CIR.two.value = ((value >> 12) & 0xFFF);
			break;
		}
	} break;
	case 0x08: //Dereference CMEM ptr to DMEM addressing register
		if (opcode2_flag4) { //If 4 set, store in incrementing register
			if (opcode2_flag8) {
				DIR.two.value = CMEM[cmemAddressing()].value;
			} else {
				DIR.one.value = CMEM[cmemAddressing()].value;
			}
		} else {
			if (opcode2_flag8) {
				DA.two.value = CMEM[cmemAddressing()].value;
			} else {
				DA.one.value = CMEM[cmemAddressing()].value;
			}
		}
		break;
	case 0x09: //Dereference CMEM ptr to CMEM addressing register //FIXME: combine with 0x08?
		if (opcode2_flag4) { //If 4 set, store in incrementing register
			if (opcode2_flag8) {
				CIR.two.value = CMEM[cmemAddressing()].value;
			} else {
				CIR.one.value = CMEM[cmemAddressing()].value;
			}
		} else {
			if (opcode2_flag8) {
				CA.two.value = CMEM[cmemAddressing()].value;
			} else {
				CA.one.value = CMEM[cmemAddressing()].value;
			}
		}
		break;

	case 0x0A: //Save addressing register to CMEM
	case 0x0B: //Save addressing register to CMEM
	{
		uint12_t* reg = nullptr;
		if (opcode2 == 0x0A) { //DMEM addressing regs
			switch (opcode2_args) {
			case 0: reg = &DA.one; break;
			case 1: reg = &DIR.one; break;
			case 2: reg = &DA.two; break;
			case 3: reg = &DIR.two; break;
			}
		} else { //CMEM addressing regs
			switch (opcode2_args) {
			case 0: reg = &CA.one; break;
			case 1: reg = &CIR.one; break;
			case 2: reg = &CA.two; break;
			case 3: reg = &CIR.two; break;
			}
		}
		assert(reg != nullptr);
		CMEM[cmemAddressing()].value = reg->value;
	} break;

	case 0x0C: //Audio input
	case 0x0D:
		if (opcode2_flag8) { //right channel
			if (opcode2 == 0x0C) {
				DMEM[dmemAddressing()].value = AR1R.value;
			} else {
				DMEM[dmemAddressing()].value = AR2R.value;
			}
		} else { //left channel
			if (opcode2 == 0x0C) {
				DMEM[dmemAddressing()].value = AR1L.value;
			} else {
				DMEM[dmemAddressing()].value = AR2L.value;
			}
		}
		break;

	case 0x18:
		if (opcode2_flag8) { //right channel
			if (opcode2_flag4) {
				AX1R.value = MACC2_delayed2.getUpper().value;
				sample_out_cb(Channel::out_1R, AX1R.value);
			} else {
				AX1R.value = MACC1_delayed2.getUpper().value;
				sample_out_cb(Channel::out_1R, AX1R.value);
			}
		} else { //left channel
			if (opcode2_flag4) {
				AX1L.value = MACC2_delayed2.getUpper().value;
				sample_out_cb(Channel::out_1L, AX1L.value);
			} else {
				AX1L.value = MACC1_delayed2.getUpper().value;
				sample_out_cb(Channel::out_1L, AX1L.value);
			}
		}
		break;
	case 0x19:
		if (opcode2_flag8) { //right channel
			if (opcode2_flag4) {
				AX2R.value = MACC2_delayed2.getUpper().value;
				sample_out_cb(Channel::out_2R, AX2R.value);
			} else {
				AX2R.value = MACC1_delayed2.getUpper().value;
				sample_out_cb(Channel::out_2R, AX2R.value);
			}
		} else { //left channel
			if (opcode2_flag4) {
				AX2L.value = MACC2_delayed2.getUpper().value;
				sample_out_cb(Channel::out_2L, AX2L.value);
			} else {
				AX2L.value = MACC1_delayed2.getUpper().value;
				sample_out_cb(Channel::out_2L, AX2L.value);
			}
		}
		break;
	case 0x1A:
		if (opcode2_flag8) { //right channel
			if (opcode2_flag4) {
				AX3R.value = MACC2_delayed2.getUpper().value;
				sample_out_cb(Channel::out_3R, AX3R.value);
			} else {
				AX3R.value = MACC1_delayed2.getUpper().value;
				sample_out_cb(Channel::out_3R, AX3R.value);
			}
		} else { //left channel
			if (opcode2_flag4) {
				AX3L.value = MACC2_delayed2.getUpper().value;
				sample_out_cb(Channel::out_3L, AX3L.value);
			} else {
				AX3L.value = MACC1_delayed2.getUpper().value;
				sample_out_cb(Channel::out_3L, AX3L.value);
			}
		}
		break;

	case 0x20:
		if (opcode2_flag8) { //Save XRD to DMEM
			if (opcode2_flag4) {
				if (UNKNOWN_STRICT) {
					assert(false); //Broken/idk
				}
			} else {
				DMEM[dmemAddressing()].value = XRD.value;
			}
		} else {
			if (UNKNOWN_STRICT) {
				assert(false); //Broken/idk
			}
		}
		break;

	case 0x23:
		switch (opcode2_args) {
		case 0:
			CMEM[cmemAddressing()].value = CR0.value;
			tms_printf("CR0 is %06X\n", CR0.value);
			break;
		case 1:
			CMEM[cmemAddressing()].value = CR1.value;
			tms_printf("CR1 is %06X\n", CR1.value);
			break;
		case 2:
			CMEM[cmemAddressing()].value = CR2.value;
			tms_printf("CR2 is %06X\n", CR2.value);
			break;
		case 3:
			CMEM[cmemAddressing()].value = CR3.value;
			tms_printf("CR3 is %06X\n", CR3.value);
			break;
		}
		break;

	case 0x26: //Load HIR with a value from C/DMEM
		if (opcode2_flag8) {
			HIR.value = CMEM[cmemAddressing()].value;
		} else {
			HIR.value = DMEM[dmemAddressing()].value;
		}
		break;

	case 0x27: //Handle circular memory
		if (!CR1.LCMEM) {
			uint32_t insn_temp = insn;
			insn = 0x00002000 + CCIRC.value; //ugly hack
			uint32_t current_end = CMEM[cmemAddressing()].value;
			COFF.value--;
			insn = 0x00002000; //ugly hack
			CMEM[cmemAddressing()].value = current_end; //Set new start to old end
		}
		if (!CR1.LDMEM) {
			uint32_t insn_temp = insn;
			insn = 0x00002000 + DCIRC.value; //ugly hack
			uint32_t current_end = DMEM[dmemAddressing()].value;
			DOFF.value--;
			insn = 0x00002000; //ugly hack
			DMEM[dmemAddressing()].value = current_end; //Set new start to old end

			DA.two.value = 0;
		}
		XOFF--; //TODO: is this conditional?
		break;

	case 0x28:
		CR1.MASM = opcode2_args;
		break;

	case 0x29: //MAC shifter mode
	{
		CR1.MOSM = opcode2_args;
		int8_t output_shift = 0;
		switch (CR1.MOSM) {
		case 0: output_shift = 0; break;
		case 1: output_shift = 2; break;
		case 2: output_shift = 4; break;
		case 3: output_shift = -8; break;
		default: assert(false);
		}
		MACC1.output_shift = output_shift;
		MACC2.output_shift = output_shift;
		MACC1_delayed2.output_shift = output_shift;
		MACC2_delayed2.output_shift = output_shift;
		MACC1_delayed1.output_shift = output_shift;
		MACC2_delayed1.output_shift = output_shift;
	} break;

	case 0x2C:
		if (opcode2_flag8) {
			//Clear overflow bits
			if (opcode1_flag4) {
				//MAC overflow bits
				CR1.MOV = 0;
				CR1.MOVL = 0;
			} else {
				//ACC overflow bits
				CR1.AOV = 0;
				CR1.AOVL = 0;
			}
		} else {
			CR2.FREE = opcode2_flag4;
		}
		break;
	case 0x2D:
		if (opcode2_flag8) {
			CR1.MOVM = opcode2_flag4;
		} else {
			CR1.AOVM = opcode2_flag4;
		}
		break;
	case 0x2E:
		if (opcode2_flag8) {
			CR1.LCMEM = opcode2_flag4;
		} else {
			CR1.LDMEM = opcode2_flag4;
		}
		break;

	default:
		tms_printf("Unhandled 2nd instruction: %08X\n", insn);
		if (UNKNOWN_STRICT) {
			printf("Unhandled 2nd instruction: %08X\n", insn); //Always print if we hit the assert
			assert(false);
		}
		break;
	}
}

//Returns CMEM address specified by the current instruction
uint32_t Emulator::cmemAddressing() {
	uint8_t mode = (insn >> 12) & 3;
	bool ca_switch;
	uint32_t addr = 0;

	switch (mode) {
	case 0:
		//bad
		break;
	case 1:
		//Indirect
		ca_switch = insn & 0x00000800;
		if (ca_switch) {
			addr = CA.two.value;
		} else {
			addr = CA.one.value;
		}
		break;
	case 2:
		//Direct
		addr = insn;
		break;
	case 3:
		//Indirect
		ca_switch = insn & 0x00000100;
		if (ca_switch) {
			addr = CA.two.value;
		} else {
			addr = CA.one.value;
		}
		break;
	default:
		assert(false); //Should not happen
	}

	if (!CR1.LCMEM) {
		addr += COFF.value;
	}
	
	if (CR1.EXT && CR1.EXTMEM) { //CMEM is 512 words
		return addr & 0x1FF;
	} else { //CMEM is 256 words
		return addr & 0xFF;
	}
}

//Returns DMEM address specified by the current instruction
uint32_t Emulator::dmemAddressing() {
	uint8_t mode = (insn >> 12) & 3;
	bool ca_switch;
	uint32_t addr = 0;

	switch (mode) {
	case 0:
		//bad
		break;
	case 1:
		//Direct
		addr = insn;
		break;
	case 2:
	case 3:
		//Indirect
		ca_switch = insn & 0x00000800;
		if (ca_switch) {
			addr = DA.two.value;
		} else {
			addr = DA.one.value;
		}
		break;
	default:
		assert(false); //Should not happen
	}

	if (!CR1.LDMEM) {
		addr += DOFF.value;
	}

	if (CR1.EXT && !CR1.EXTMEM) { //DMEM is 512 words
		return addr & 0x1FF;
	} else { //DMEM is 256 words
		return addr & 0xFF;
	}
}

uint32_t Emulator::xmemAddressing(uint32_t addr) {
	uint32_t xmem_size = (4096 * 4096) - 1; //TODO: replace this with the size as deduced from registers
	return (addr + XOFF) & 0xFFFF;
}

//Handle any jmp/call instruction
void Emulator::execJmp() {
	//Get the two nibbles after the first one. A jump instruction may be 0xF1800045 - in this case we want '18'
	//Also AND with 7F to give jumps (0xF0 based) and calls (0xF8 based) the same conditions
	uint8_t args = (insn >> (4 + 16)) & 0x7F;

	bool is_call = opcode1 >= 0xF8;
	bool condition_pass = false;
	uint16_t target_address = insn;

	switch (args) {
	case 0x00: //Unconditional
		condition_pass = true;
		break;
	case 0x08: //Unconditional indirect
		condition_pass = true;
		target_address = ACC1.value;
		break;
	case 0x0C: //Unconditional indirect
		condition_pass = true;
		target_address = ACC2.value;
		break;
	case 0x10: //if zero
		condition_pass = CR1.ACCZ;
		break;
	case 0x18: //if not zero
		condition_pass = !CR1.ACCZ;
		break;
	case 0x20: //if greater than zero
		condition_pass = !(CR1.ACCZ || CR1.ACCN);
		break;
	case 0x28: //if less than zero
		condition_pass = CR1.ACCN;
		break;
	case 0x30: //if ACC overflow
		condition_pass = CR1.AOV;
		break;
	case 0x38: //if ACC latched overflow - CR1 bit 1
		condition_pass = CR1.AOVL;
		break;
	case 0x40: //if MACC overflow - CR1 bit 4
		condition_pass = CR1.MOV;
		break;
	case 0x48: //if MAC latched overflow - CR1 bit 5
		condition_pass = CR1.MOVL;
		break;
	case 0x50: //if MAC range overflow - CR1 bit 6
		condition_pass = CR1.MOVR;
		break;
	case 0x58: //if BIO low TODO
		
		break;
	default:
		if (UNKNOWN_STRICT) {
			assert(false); //Unhandled jump type
		}
		break;
	}
	if (condition_pass) {
		if (is_call) {
			assert(SP != 4); //Stack overflow!
			stack[SP].value = PC.value;
			SP++;
		}

		PC.value = target_address;

		if (is_call)
			tms_printf("Calling %03X\n", PC.value);
		else
			tms_printf("Jumping to %03X\n", PC.value);
	}
}

void Emulator::execPostIncrements() {
	const uint8_t addrMode = (insn >> 12) & 3;
	const uint8_t nibble2 = (insn >> 8) & 0xF; //Often referred to as 'i' in my notes
	const uint8_t nibble1 = (insn >> 4) & 0xF; //Often referred to as 'z' in my notes

	//DA control
	if (addrMode & 2) {
		uint12_t* DAx = &DA.one;
		if (nibble2 & 8) { //Use DA2
			DAx = &DA.two;
		}
		if (nibble2 & 4) {
			//Increment with incrementing register
			uint12_t* DIRx = &DIR.one;
			if (nibble2 & 2) {
				//Use other incrementing register
				DIRx = &DIR.two;
			}
			DAx->value += DIRx->value;
		} else {
			if (nibble2 & 2) {
				//Increment
				DAx->value++;
			}
		}
	}

	//CA control
	if (addrMode == 1) {
		uint12_t* CAx = &CA.one;
		if (nibble2 & 8) { //Use CA2
			CAx = &CA.two;
		}
		if (nibble2 & 4) {
			//Increment with incrementing register
			uint12_t* CIRx = &CIR.one;
			if (nibble2 & 2) {
				//Use other incrementing register
				CIRx = &CIR.two;
			}
			CAx->value += CIRx->value;
		} else {
			if (nibble2 & 2) {
				//Increment
				CAx->value++;
			}
		}
	} else if (addrMode == 3) {
		uint12_t* CAx = &CA.one;
		if (nibble2 & 1) { //Use CA2
			CAx = &CA.two;
		}
		if (nibble1 & 8) {
			//Increment with incrementing register
			uint12_t* CIRx = &CIR.one;
			if (nibble1 & 4) {
				//Use other incrementing register
				CIRx = &CIR.two;
			}
			CAx->value += CIRx->value;
		} else {
			if (nibble1 & 4) {
				//Increment
				CAx->value++;
			}
		}
	}
}