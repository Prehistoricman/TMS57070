#include "TMS57070.h"
#include <cassert>

using namespace TMS57070;

//Returns pointer to the destination ACC
uint24_t* Emulator::loadACC() {
	//Where is the data destination?
	uint24_t* dst = &ACC1;
	if (opcode1_flag4) {
		dst = &ACC2;
	}
	//Where is the data source?
	uint8_t src_code = opcode1 & 3;
	switch (src_code) {
	case 0:
		dst->value = DMEM[dmemAddressing()];
		break;
	case 1:
		dst->value = CMEM[cmemAddressing()];
		break;
	case 2:
		if (opcode1_flag8) {
			dst->value = ACC2.value;
		} else {
			dst->value = ACC1.value;
		}
		break;
	case 3:
		if (opcode1_flag8) {
			dst->value = getMAC(2, MACBits::Upper);
		} else {
			dst->value = getMAC(1, MACBits::Upper);
		}
		break;
	default:
		assert(false); //Should not happen
	}

	return dst;
}

//Returns pointer to the destination ACC
uint24_t* Emulator::arith(ArithOperation operation) {
	//Where is the data destination?
	uint24_t* dst = &ACC1;
	if (opcode1_flag4) {
		dst = &ACC2;
	}
	//Where is the data source?
	uint8_t src_code = opcode1 & 3;
	uint24_t lhs;
	uint24_t rhs;
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
			rhs.value = MACC2.value;
		} else {
			rhs.value = MACC1.value;
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
			rhs.value = MACC2.value;
		} else {
			rhs.value = MACC1.value;
		}
		break;
	default:
		assert(false); //Should not happen
		lhs.value = 0;
		rhs.value = 0;
	}

	uint32_t result;
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
		//TODO: apply flags
		return nullptr;
		break;
	default:
		assert(false);
	}

	//These operations can over/underflow
	if ((operation == ArithOperation::Add) || (operation == ArithOperation::Sub)) {
		if (CR1.AOVM) {
			//TODO: saturation logic
			CR1.AOV = 0;
		}
	}

	dst->value = result;
	return dst;
}

void Emulator::exec1st() {
	switch (opcode1) {
	case 0x00: //NOP
		break;


	case 0x4: //Load accumulator unsigned
	case 0x5:
	case 0x6:
	case 0x7: {
		uint24_t* dst = loadACC();
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
		uint24_t* dst = loadACC();
		dst->value = ~dst->value + 1; //TODO: saturation logic

		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x10: //Load accumulator
	case 0x11:
	case 0x12:
	case 0x13: {
		uint24_t* dst = loadACC();

		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x14: //Increment and load accumulator
	case 0x15:
	case 0x16:
	case 0x17: {
		uint24_t* dst = loadACC();
		dst->value++;
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x18: //Decrement and load accumulator
	case 0x19:
	case 0x1A:
	case 0x1B: {
		uint24_t* dst = loadACC();
		dst->value--;
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x20: //Add accumulator
	case 0x21:
	case 0x22:
	case 0x23: {
		uint24_t* dst = arith(ArithOperation::Add);
		dst->value--;
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x24: //Sub accumulator
	case 0x25:
	case 0x26:
	case 0x27: {
		uint24_t* dst = arith(ArithOperation::Sub);
		dst->value--;
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x28: //AND accumulator
	case 0x29:
	case 0x2A:
	case 0x2B: {
		uint24_t* dst = arith(ArithOperation::And);
		dst->value--;
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x2C: //OR accumulator
	case 0x2D:
	case 0x2E:
	case 0x2F: {
		uint24_t* dst = arith(ArithOperation::Or);
		dst->value--;
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x30: //XOR accumulator
	case 0x31:
	case 0x32:
	case 0x33: {
		uint24_t* dst = arith(ArithOperation::Xor);
		dst->value--;
		printf("Set ACC%d to %X\n", opcode1_flag4 + 1, dst->value);
	} break;

	case 0x34: //CMP compare
	case 0x35:
	case 0x36:
	case 0x37: {
		uint24_t* dst = arith(ArithOperation::Cmp);
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
			//TODO
			CA.one.value = insn;
			break;
		case 0x48: //LIRAE
			//TODO
			CA.two.value = insn;
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
		break;
	case 0xCB: //Load ACC2 imm
		ACC2.value = insn;
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

	case 0xF0: //JMP
		PC.value = insn;
		tms_printf("Jumping to %03X\n", PC.value & 0x1FF);
		break;

	case 0xF8: //CALL
		assert(SP != 4); //Stack overflow!
		stack[SP].value = PC.value;
		SP++;
		PC.value = insn;
		tms_printf("Calling %03X\n", PC.value & 0x1FF);
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
				CMEM[cmemAddressing()] = getMAC(2, MACBits::Upper);
			} else {
				DMEM[dmemAddressing()] = getMAC(2, MACBits::Upper);
			}
		} else {
			//MACC1
			if (opcode2_flag8) {
				CMEM[cmemAddressing()] = getMAC(1, MACBits::Upper);
			} else {
				DMEM[dmemAddressing()] = getMAC(1, MACBits::Upper);
			}
		}
		break;
	case 0x03: //Save MACC low to MEM
		if (opcode2_flag4) {
			//MACC2
			if (opcode2_flag8) {
				CMEM[cmemAddressing()] = getMAC(2, MACBits::Lower);
			} else {
				DMEM[dmemAddressing()] = getMAC(2, MACBits::Lower);
			}
		} else {
			//MACC1
			if (opcode2_flag8) {
				CMEM[cmemAddressing()] = getMAC(1, MACBits::Lower);
			} else {
				DMEM[dmemAddressing()] = getMAC(1, MACBits::Lower);
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
				AX1R.value = getMAC(2, MACBits::Upper);
				sample_out_cb(Channel::out_1R, AX1R.value);
			} else {
				AX1R.value = getMAC(1, MACBits::Upper);
				sample_out_cb(Channel::out_1R, AX1R.value);
			}
		} else { //left channel
			if (opcode2_flag4) {
				AX1L.value = getMAC(2, MACBits::Upper);
				sample_out_cb(Channel::out_1L, AX1L.value);
			} else {
				AX1L.value = getMAC(1, MACBits::Upper);
				sample_out_cb(Channel::out_1L, AX1L.value);
			}
		}
		break;
	case 0x19:
		if (opcode2_flag8) { //right channel
			if (opcode2_flag4) {
				AX2R.value = getMAC(2, MACBits::Upper);
				sample_out_cb(Channel::out_2R, AX2R.value);
			} else {
				AX2R.value = getMAC(1, MACBits::Upper);
				sample_out_cb(Channel::out_2R, AX2R.value);
			}
		} else { //left channel
			if (opcode2_flag4) {
				AX2L.value = getMAC(2, MACBits::Upper);
				sample_out_cb(Channel::out_2L, AX2L.value);
			} else {
				AX2L.value = getMAC(1, MACBits::Upper);
				sample_out_cb(Channel::out_2L, AX2L.value);
			}
		}
		break;
	case 0x1A:
		if (opcode2_flag8) { //right channel
			if (opcode2_flag4) {
				AX3R.value = getMAC(2, MACBits::Upper);
				sample_out_cb(Channel::out_3R, AX3R.value);
			} else {
				AX3R.value = getMAC(1, MACBits::Upper);
				sample_out_cb(Channel::out_3R, AX3R.value);
			}
		} else { //left channel
			if (opcode2_flag4) {
				AX3L.value = getMAC(2, MACBits::Upper);
				sample_out_cb(Channel::out_3L, AX3L.value);
			} else {
				AX3L.value = getMAC(1, MACBits::Upper);
				sample_out_cb(Channel::out_3L, AX3L.value);
			}
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

uint32_t Emulator::getMAC(uint8_t mac, MACBits bits) {
	//Apply MAC output shifter
	//TODO: everything
	if (mac == 1) {
		return MACC1_delayed2.value;
	} else {
		return MACC2_delayed2.value;
	}
}

//Returns CMEM address specified by the current instruction
uint32_t Emulator::cmemAddressing() { //TODO: wrap-around
	uint8_t mode = (insn >> 12) & 3;
	bool ca_switch;
	switch (mode) {
	case 0:
		//bad
		break;
	case 1:
		//Indirect
		ca_switch = insn & 0x00000800;
		if (ca_switch) {
			return CA.two.value;
		} else {
			return CA.one.value;
		}
		break;
	case 2:
		//Direct
		return insn & 0x1FF;
		break;
	case 3:
		//Indirect
		ca_switch = insn & 0x00000100;
		if (ca_switch) {
			return CA.two.value;
		} else {
			return CA.one.value;
		}
		break;
	default:
		assert(false); //Should not happen
	}
	return 0;
}

//Returns DMEM address specified by the current instruction
uint32_t Emulator::dmemAddressing() { //TODO: addressing wrap-around and 24-bit limit
	uint8_t mode = (insn >> 12) & 3;
	bool ca_switch;
	switch (mode) {
	case 0:
		//bad
		break;
	case 1:
		//Direct
		return insn & 0x1FF;
		break;
	case 2:
	case 3:
		//Indirect
		ca_switch = insn & 0x00000800;
		if (ca_switch) {
			return DA.two.value;
		} else {
			return DA.one.value;
		}
		break;
	default:
		assert(false); //Should not happen
	}
	return 0;
}