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
		result = DMEM[dmemAddressing()];
		break;
	case 1:
		result = CMEM[cmemAddressing()];
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
	switch (src_code) {
	case 0: //DMEM op ACCx
		lhs.value = DMEM[dmemAddressing()];
		if (opcode1_flag8) {
			rhs.value = ACC2.value;
		} else {
			rhs.value = ACC1.value;
		}
		break;
	case 1: //DMEM op MACCx
		lhs.value = DMEM[dmemAddressing()];
		if (opcode1_flag8) {
			rhs.value = MACC2.getUpper().value;
		} else {
			rhs.value = MACC1.getUpper().value;
		}
		break;
	case 2: //CMEM op ACCx
		lhs.value = CMEM[cmemAddressing()];
		if (opcode1_flag8) {
			rhs.value = ACC2.value;
		} else {
			rhs.value = ACC1.value;
		}
		break;
	case 3: //CMEM op MACCx
		lhs.value = CMEM[cmemAddressing()];
		if (opcode1_flag8) {
			rhs.value = MACC2.getUpper().value;
		} else {
			rhs.value = MACC1.getUpper().value;
		}
		break;
	default:
		assert(false); //Should not happen
		lhs.value = 0;
		rhs.value = 0;
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
		if ((result < INT24_MIN) || (result > INT24_MAX)) {
			CR1.AOV = 1;
		}
		if (result == 0) {
			CR1.ACCZ = 1;
		} else if (result < 0) {
			CR1.ACCN = 1;
		}
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
			printf("processACCValue: saturating value %d (%X) at -ve\n", acc, acc);
			acc = INT24_MIN;
			CR1.AOV = 1;
		} else if (acc > INT24_MAX) {
			printf("processACCValue: saturating value %d (%X) at +ve\n", acc, acc);
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
		break;


	case 0x4: //Load accumulator unsigned
	case 0x5:
	case 0x6:
	case 0x7: {
		int24_t* dst = loadACC();
		if (dst->value >= 0x800000) {
			dst->value = ~dst->value;
		}

		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x8: //Load accumulator complemented
	case 0x9:
	case 0xA:
	case 0xB:
	{
		int24_t* dst = loadACC();
		dst->value = ~dst->value + 1; //TODO: saturation logic

		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x10: //Load accumulator
	case 0x11:
	case 0x12:
	case 0x13: {
		int24_t* dst = loadACC();

		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x14: //Increment and load accumulator
	case 0x15:
	case 0x16:
	case 0x17: {
		int24_t* dst = loadACC();
		dst->value++;
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x18: //Decrement and load accumulator
	case 0x19:
	case 0x1A:
	case 0x1B: {
		int24_t* dst = loadACC();
		dst->value--;
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x20: //Add accumulator
	case 0x21:
	case 0x22:
	case 0x23: {
		int24_t* dst = arith(ArithOperation::Add);
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x24: //Sub accumulator
	case 0x25:
	case 0x26:
	case 0x27: {
		int24_t* dst = arith(ArithOperation::Sub);
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x28: //AND accumulator
	case 0x29:
	case 0x2A:
	case 0x2B: {
		int24_t* dst = arith(ArithOperation::And);
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x2C: //OR accumulator
	case 0x2D:
	case 0x2E:
	case 0x2F: {
		int24_t* dst = arith(ArithOperation::Or);
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x30: //XOR accumulator
	case 0x31:
	case 0x32:
	case 0x33: {
		int24_t* dst = arith(ArithOperation::Xor);
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x34: //CMP compare
	case 0x35:
	case 0x36:
	case 0x37: {
		int24_t* dst = arith(ArithOperation::Cmp);
	} break;

	case 0xC1: //Load register with immediate
		switch ((insn >> 16) & 0xFF) {
		case 0x00:
			DA.one.value = insn;
			break;
		case 0x08:
			DA.two.value = insn;
			break;
		case 0x10:
			DIR.one.value = insn;
			break;
		case 0x18:
			DIR.two.value = insn;
			break;
		case 0x20:
			CA.one.value = insn;
			break;
		case 0x28:
			CA.two.value = insn;
			break;
		case 0x30:
			CIR.one.value = insn;
			break;
		case 0x38:
			CIR.two.value = insn;
			break;
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
			assert(false); //unknown
		}
		break;

	case 0xC2: //Load DA imm
		DA.value = insn;
		break;
	case 0xC3: //Load DIR imm
		DIR.value = insn;
		break;
	case 0xC4: //Load CA imm
		CA.value = insn;
		break;
	case 0xC5: //Load CIR imm
		CIR.value = insn;
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
	case 0xCE: { //Load CR2 imm
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
				CMEM[cmemAddressing()] = ACC2.value;
			} else {
				DMEM[dmemAddressing()] = ACC2.value;
			}
		} else {
			//ACC1
			if (opcode2_flag8) {
				CMEM[cmemAddressing()] = ACC1.value;
			} else {
				DMEM[dmemAddressing()] = ACC1.value;
			}
		}
		break;
	case 0x02: //Save MACC high to MEM
		if (opcode2_flag4) {
			//MACC2
			if (opcode2_flag8) {
				CMEM[cmemAddressing()] = MACC2_delayed2.getUpper().value;
			} else {
				DMEM[dmemAddressing()] = MACC2_delayed2.getUpper().value;
			}
		} else {
			//MACC1
			if (opcode2_flag8) {
				CMEM[cmemAddressing()] = MACC1_delayed2.getUpper().value;
			} else {
				DMEM[dmemAddressing()] = MACC1_delayed2.getUpper().value;
			}
		}
		break;
	case 0x03: //Save MACC low to MEM
		if (opcode2_flag4) {
			//MACC2
			if (opcode2_flag8) {
				CMEM[cmemAddressing()] = MACC2_delayed2.getLower().value;
			} else {
				DMEM[dmemAddressing()] = MACC2_delayed2.getLower().value;
			}
		} else {
			//MACC1
			if (opcode2_flag8) {
				CMEM[cmemAddressing()] = MACC1_delayed2.getLower().value;
			} else {
				DMEM[dmemAddressing()] = MACC1_delayed2.getLower().value;
			}
		}
		break;

	case 0x06: //Load dual from CMEM
		//Load: (arg = 0-3) DA, DIR, CA, DIR
		switch (opcode2_args) {
		case 0:
			DA.value = CMEM[cmemAddressing()];
			break;
		case 1:
			DIR.value = CMEM[cmemAddressing()];
			break;
		case 2:
			CA.value = CMEM[cmemAddressing()];
			break;
		case 3:
			CIR.value = CMEM[cmemAddressing()];
			break;
		}
		break;
	case 0x08: //Dereference CMEM ptr to CMEM addressing register
		if (opcode2_flag4) { //If 4 set, store in incrementing register
			if (opcode2_flag8) {
				CIR.two.value = CMEM[cmemAddressing()];
			} else {
				CIR.two.value = CMEM[cmemAddressing()];
			}
		} else {
			if (opcode2_flag8) {
				CA.two.value = CMEM[cmemAddressing()];
			} else {
				CA.two.value = CMEM[cmemAddressing()];
			}
		}
		break;
	case 0x09: //Dereference CMEM ptr to DMEM addressing register //FIXME: combine with 0x08?
		if (opcode2_flag4) { //If 4 set, store in incrementing register
			if (opcode2_flag8) {
				DIR.two.value = CMEM[cmemAddressing()];
			} else {
				DIR.two.value = CMEM[cmemAddressing()];
			}
		} else {
			if (opcode2_flag8) {
				DA.two.value = CMEM[cmemAddressing()];
			} else {
				DA.two.value = CMEM[cmemAddressing()];
			}
		}
		break;

	case 0x0C: //Audio input
	case 0x0D:
		if (opcode2_flag8) { //right channel
			if (opcode2 == 0x0C) {
				DMEM[dmemAddressing()] = AR1R.value;
			} else {
				DMEM[dmemAddressing()] = AR2R.value;
			}
		} else { //left channel
			if (opcode2 == 0x0C) {
				DMEM[dmemAddressing()] = AR1L.value;
			} else {
				DMEM[dmemAddressing()] = AR2L.value;
			}
		}

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

	case 0x23:
		switch (opcode2_args) {
		case 0:
			CMEM[cmemAddressing()] = CR0.value;
			printf("CR0 is %06X\n", CR0.value);
			break;
		case 1:
			CMEM[cmemAddressing()] = CR1.value;
			printf("CR1 is %06X\n", CR1.value);
			break;
		case 2:
			CMEM[cmemAddressing()] = CR2.value;
			printf("CR2 is %06X\n", CR2.value);
			break;
		case 3:
			CMEM[cmemAddressing()] = CR3.value;
			printf("CR3 is %06X\n", CR3.value);
			break;
		}
		break;

	case 0x26: //Load HIR with a value from C/DMEM
		if (opcode2_flag8) {
			HIR.value = CMEM[cmemAddressing()];
		} else {
			HIR.value = DMEM[dmemAddressing()];
		}
		break;

	case 0x2C:
		if (opcode2_flag8) {
			//unknown
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

	if (CR1.EXT && !CR1.EXTMEM) { //DMEM is 512 words
		return addr & 0x1FF;
	} else { //DMEM is 256 words
		return addr & 0xFF;
	}
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
	case 0x38: //if ??? - CR1 bit 1
		condition_pass = CR1.ACCsomething;
		break;
	case 0x40: //if ??? - CR1 bit 4
		condition_pass = CR1.MACOV2;
		break;
	case 0x48: //if MAC overflow - CR1 bit 5
		condition_pass = CR1.MACOV;
		break;
	case 0x50: //if ??? - CR1 bit 6
		condition_pass = CR1.MACOV4;
		break;
	case 0x58: //if BIO low TODO
		
		break;
	default:
		assert(false); //Unhandled jump type
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