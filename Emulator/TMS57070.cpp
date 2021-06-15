#include "TMS57070.h"
#include <cassert>

using namespace TMS57070;

void Emulator::reset() {
	PC.value = 0; //Reset vector
	SP = 0;
	RPTC = 0;

	//TODO: read these from the input file/host interface
	CR0.value = 0xAA9BAD;
	CR1.value = 0x810100;
	CR2.value = 0x30FF00;
	CR3.value = 0xE50000;
	
	addr_regs_pipeline.dual_ptr = nullptr;
	addr_regs_pipeline.single_ptr = nullptr;
	addr_regs_pipeline.dual_ptr_delayed1 = nullptr;
	addr_regs_pipeline.single_ptr_delayed1 = nullptr;
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
	
	addr_regs_pipeline_step();

	if ((insn >> 24) >= 0xC0) { //Only primary instruction
		execPrimary();
	} else if ((insn >> 24) >= 0x80) { //Class 2 instruction
		execClass2();
		execPrimary();
		execPostIncrements();
	} else { //Class 1 instruction
		execSecondary();
		execPrimary();
		execPostIncrements();
	}

	//Apply MACC pipeline
	MACC1_delayed2.set(MACC1_delayed1);
	MACC2_delayed2.set(MACC2_delayed1);
	MACC1_delayed1.set(MACC1);
	MACC2_delayed1.set(MACC2);

	//Handle background XMEM reading
	if (XMEM_read_cycles != 0) {
		XMEM_read_cycles--;
		if (XMEM_read_cycles == 0) {
			//Read is done
			XRD.value = XMEM[XMEM_read_addr].value;
			if (!CR3.XWORD) {
				XRD.value &= 0xFFFF00; //16-bit truncation
			}
			tms_printf("External read complete. addr=%06X data=%06X\n", XMEM_read_addr, XRD.value);
		}
	}

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

void Emulator::addr_regs_pipeline_step() {
	//Apply changes from two cycles ago
	if (addr_regs_pipeline.dual_ptr_delayed1 != nullptr) {
		*addr_regs_pipeline.dual_ptr_delayed1 = addr_regs_pipeline.dual_value_delayed1;
	}
	if (addr_regs_pipeline.single_ptr_delayed1 != nullptr) {
		*addr_regs_pipeline.single_ptr_delayed1 = addr_regs_pipeline.single_value_delayed1;
	}
	
	addr_regs_pipeline.dual_ptr_delayed1 = addr_regs_pipeline.dual_ptr;
	addr_regs_pipeline.dual_value_delayed1 = addr_regs_pipeline.dual_value;
	addr_regs_pipeline.single_ptr_delayed1 = addr_regs_pipeline.single_ptr;
	addr_regs_pipeline.single_value_delayed1 = addr_regs_pipeline.single_value;

	addr_regs_pipeline.dual_ptr = nullptr;
	addr_regs_pipeline.single_ptr = nullptr;
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

void Emulator::register_external_bus_in_callback(external_bus_in_callback_t cb) {
	ext_bus_in_cb = cb;
}
void Emulator::register_external_bus_out_callback(external_bus_out_callback_t cb) {
	ext_bus_out_cb = cb;
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

void Emulator::update_mac_modes() {
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

	uint8_t bit_count;
	switch (CR1.MRDM) {
	case 0: bit_count = 0; break;
	case 1: bit_count = 48 - 24; break;
	case 2: bit_count = 48 - 20; break;
	case 3: bit_count = 48 - 18; break;
	case 4: bit_count = 48 - 16; break;
	case 5: bit_count = 48 - 16; break;
	case 6: bit_count = 48 - 20; break; //TODO: implement the difference between this and setting 2
	case 7: bit_count = 48 - 18; break; //TODO: implement the difference between this and setting 3
	}
	MACC1.bit_count = bit_count;
	MACC2.bit_count = bit_count;
	MACC1_delayed2.bit_count = bit_count;
	MACC2_delayed2.bit_count = bit_count;
	MACC1_delayed1.bit_count = bit_count;
	MACC2_delayed1.bit_count = bit_count;
}

void Emulator::set_bio(bool value) {
	this->BIO = value;
}

static std::string jsonValue(const char* name, uint32_t value) {
	std::string build;
	constexpr int TEMP_LEN = 16;
	char temp[TEMP_LEN];
	
	build.append("\"");
	build.append(name);
	build.append("\":\"");
	snprintf(temp, TEMP_LEN, "%06lX", value);
	int temp_pos = 0;
	if (strlen(temp) > 6) {
		temp_pos = (int)strlen(temp) - 6;
	}
	assert(strlen(temp + temp_pos) == 6); //Hex length is always 6
	build.append(temp + temp_pos);
	build.append("\",");

	return build;
}

//Returns a JSON string with CMEM, DMEM, and all the registers
std::string Emulator::reportState() {
	std::string report;
	constexpr int TEMP_LEN = 16;
	char temp[TEMP_LEN];
	report.append("{");
	
	report.append(jsonValue("ACC1", ACC1.value));
	report.append(jsonValue("ACC2", ACC2.value));
	report.append(jsonValue("MAC1", MACC1.getUpper().value));
	report.append(jsonValue("MAC2", MACC2.getUpper().value));
	report.append(jsonValue("MAC1L", MACC1.getLower().value));
	report.append(jsonValue("MAC2L", MACC2.getLower().value));
	report.append(jsonValue("CA1", CA.one.value));
	report.append(jsonValue("CA2", CA.two.value));
	report.append(jsonValue("DA1", DA.one.value));
	report.append(jsonValue("DA2", DA.two.value));
	report.append(jsonValue("XRD", XRD.value));
	report.append(jsonValue("CR0", CR0.value));
	report.append(jsonValue("CR1", CR1.value));
	report.append(jsonValue("CR2", CR2.value));
	report.append(jsonValue("CR3", CR3.value));
	report.append(jsonValue("CIR1", CIR.one.value));
	report.append(jsonValue("CIR2", CIR.two.value));
	report.append(jsonValue("DIR1", DIR.one.value));
	report.append(jsonValue("DIR2", DIR.two.value));
	
	//report CMEM
	report.append("\"CMEM\":[");
	for (int i = 0; i < 255; i++) {
		report.append("\"");
		snprintf(temp, TEMP_LEN, "%06lX", CMEM[i].value);
		int temp_pos = 0;
		if (strlen(temp) > 6) {
			temp_pos = (int)strlen(temp) - 6;
		}
		assert(strlen(temp + temp_pos) == 6); //Hex length is always 6
		report.append(temp + temp_pos);
		report.append("\",");
	}
	report.pop_back(); //Delete last comma
	report.append("]");
	report.append(",");
	
	//report DMEM
	report.append("\"DMEM\":[");
	for (int i = 0; i < 255; i++) {
		report.append("\"");
		snprintf(temp, TEMP_LEN, "%06lX", DMEM[i].value);
		int temp_pos = 0;
		if (strlen(temp) > 6) {
			temp_pos = (int)strlen(temp) - 6;
		}
		assert(strlen(temp + temp_pos) == 6); //Hex length is always 6
		report.append(temp + temp_pos);
		report.append("\",");
	}
	report.pop_back(); //Delete last comma
	report.append("]");
	
	report.append("}");
	return report;
}