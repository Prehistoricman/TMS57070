#include "TMS57070.h"
#include <cassert>

using namespace TMS57070;

void Emulator::reset() {
	PC.value = 0; //Reset vector
	SP = 0;
	RPTC = 0;
	

	//testing only
	CA.one.value = 5;
	CA.two.value = 9;
	DA.one.value = 7;
	DA.two.value = 1;
	DMEM[7] = 0x111111;
	DMEM[1] = 0x888888;
	DMEM[0x10] = 0xDAD;
	MACC1.value = 0xBADBEEF;
	MACC2.value = 0xBEAD;
	ACC2.value = 0xCAFE;
}

void Emulator::step() {
	/* Tasks:
	Read PMEM at PC
	inc PC
	decode & execute
	*/

	insn = PMEM[PC.value];
	tms_printf("Read instruction %08X from %03X\n", insn, PC.value);

	if (RPTC) { //Are we in a repeat?
		if (PC.value == rep_end_PC.value) {
			tms_printf("PC matched repeat end: %X. Setting PC back to %X\n", rep_end_PC.value, rep_start_PC.value);
			PC.value = rep_start_PC.value;
			RPTC--;
		} else {
			PC.value++;
		}
	} else {
		PC.value++;
	}

	opcode1 = insn >> 24;
	opcode1_flag4 = insn & 0x00400000;
	opcode1_flag8 = insn & 0x00800000;
	opcode2 = (insn >> 16) & 0x3F;
	opcode2_flag4 = insn & 0x00004000;
	opcode2_flag8 = insn & 0x00008000;
	opcode2_args = (insn >> 14) & 3; //This encompasses the above 2 flags

	if ((insn >> 24) >= 0x80) {
		//Only primary instruction
		exec1st();
	} else {
		exec2nd();
		exec1st();
		//TODO: handle post-increments here?
	}

	//Apply MACC pipeline
	MACC1_delayed2.value = MACC1_delayed1.value;
	MACC2_delayed2.value = MACC2_delayed1.value;
	MACC1_delayed1.value = MACC1.value;
	MACC2_delayed1.value = MACC2.value;

	//Check if there is an interrupt to jump to
	//Are we FREE?
	if (CR2.FREE) {
		//AND interrupt flags and enables
		//NOT the enabled to get 1 = enabled
		uint8_t pending_interrupts = CR2.bytes[0]/*flags*/ & ~CR2.bytes[1]/*enables*/;
		if (pending_interrupts) {
			//There is an interrupt to jump to
			assert(SP != 4); //Stack overflow!
			stack[SP].value = PC.value;
			SP++;
			CR2.FREE = 0;

			interrupt_vector_t vector = int_vector_decode(pending_interrupts);
			PC.value = vector.PC.value; //Set PC
			CR2.bytes[0] &= ~vector.flag; //Clear flag

			tms_printf("Interrupted! Going to PC %08X\n", PC.value);
		}
	}
}

//Decodes an interrupt flag/enable value to an interrupt vector location
interrupt_vector_t Emulator::int_vector_decode(uint8_t flags) {
	//This mask, once applied, will select the least-significant '1' bit in the input number
	//uint8_t flags_bitmask = flags ^ (flags - 1);
	//uint8_t flags_lowest = flags & flags_bitmask;

	uint8_t vectors[8] = { //Decode map
		1, //ARI1
		3, //ARI1A
		5, //HIR
		6, //INT1
		2, //ARI2
		4, //ARI2A
		7, //guess
		8  //guess
	};

	//Shift bits until we find the first '1'
	for (uint8_t vector = 0; vector < 8; vector++) {
		if (flags & 1) {
			return interrupt_vector_t{ vectors[vector], (uint8_t)(1 << vector) };
		}
		flags = flags >> 1;
	}
	assert(false); //Should not happen
	return interrupt_vector_t{};
}

void Emulator::sample_in(Channel channel, int32_t value) {
	//Set input register and raise flag
	switch (channel) {
	case Channel::in_1L:
		AR1L.value = value;
		CR2.ARI1_IF = 1; //TODO: receive both left and right samples before raising flag?
		break;
	case Channel::in_1R:
		AR1R.value = value;
		CR2.ARI1_IF = 1; //TODO: receive both left and right samples before raising flag?
		break;
	case Channel::in_2L:
		AR2L.value = value;
		CR2.ARI2_IF = 1; //TODO: receive both left and right samples before raising flag?
		break;
	case Channel::in_2R:
		AR2R.value = value;
		CR2.ARI2_IF = 1; //TODO: receive both left and right samples before raising flag?
		break;
	default:
		assert(false); //Wrong channel
	}
}

void Emulator::register_sample_out_callback(sample_out_callback_t cb) {
	sample_out_cb = cb;
}

uint32_t Emulator::hir_out() {
	uint32_t value = HIR.value;
	HIR.value = 0;
	return value;
}

void Emulator::ext_interrupt(uint8_t interrupt) {
	CR2.INT2_IF;
	switch (interrupt) {
	case 1:
		CR2.INT1_IF = 1;
		break;
	case 2:
		CR2.INT2_IF = 1;
		break;
	case 3:
		CR2.INT3_IF = 1;
		break;
	default:
		assert(false); //Invalid external interrupt number
	}
}

void Emulator::hir_interrupt(uint24_t input) {
	CR2.HIR_IF = 1;
	HIR.value = input.value;
}