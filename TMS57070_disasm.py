#----------------------------------------------------------------------
# Processor module template script
# (c) Hex-Rays
import sys
import idc
import idaapi
import idautils
from idc import *
from idaapi import *
import ida_ida

import logging
logger = logging.getLogger(__name__)
logging.basicConfig(filename = "disasm.log", level = logging.ERROR)

# define RAM starting at 18000h of size 100h
# define ROM starting at 0 of size 12100h
# The extra 100 is for the first page of data memory

#integer to hex
#width can be used to set the width of the result
def hex(int, width = None):
    if (width == None):
        return "0x%X" % int
    else:
        return ("0x%0." + str(width) + "X") % int

# ----------------------------------------------------------------------
class TMS57070_processor(idaapi.processor_t):
    
    # IDP id ( Numbers above 0x8000 are reserved for the third-party modules)
    id = 0x8000 + 57070
    
    # Processor features
    flag = PR_DEFSEG32 | PRN_HEX | PR_RNAMESOK | PR_CNDINSNS
    
    # Number of bits in a byte for code segments (usually 8)
    # IDA supports values up to 32 bits
    cnbits = 32
    
    # Number of bits in a byte for non-code segments (usually 8)
    # IDA supports values up to 32 bits
    dnbits = 24
    
    #To size set (in bytes, hopefully) of dt_tbyte
    tbyte_size = 2
    
    # short processor names
    # Each name should be shorter than 9 characters
    psnames = ['TMS57070']
    
    # long processor names
    # No restriction on name lengthes.
    plnames = ['Texas Instruments TMS57070']
    
    assembler = {
        "header": [".???"],
        "flag": AS_NCHRE | ASH_HEXF0 | ASD_DECF0 | ASO_OCTF0 | ASB_BINF0 | AS_NOTAB,
        "uflag": 0,
        "name": "TMS57070 assembler",
        "origin": ".org",
        "end": ".end",
        "cmnt": ";",
        "ascsep": "'",
        "accsep": "'",
        "esccodes": "\"'",
        "a_ascii": ".ascii",
        "a_byte": ".word", #byte vs word definition :((
        "a_word": ".word",
        "a_dword": ".dword",
        "a_bss": "dfs %s",
        "a_seg": "seg",
        "a_curip": "PC",
        "a_public": "",
        "a_weak": "",
        "a_extrn": ".extern",
        "a_comdef": "",
        "a_align": ".align",
        "lbrace": "(",
        "rbrace": ")",
        "a_mod": "%",
        "a_band": "&",
        "a_bor": "|",
        "a_xor": "^",
        "a_bnot": "~",
        "a_shl": "<<",
        "a_shr": ">>",
        "a_sizeof_fmt": "size %s",
    }
    
    # register names
    reg_names = regNames = [
        "SP", #Required for IDA
        "CS",
        "DS",
        "ACC", #accumulators
        "ACC1",
        "ACC2",
        "MACC1",
        "MACC2",
        "MACC1L",
        "MACC2L",
        "CA", #CA and DA
        "CA1",
        "CA2",
        "DA",
        "DA1",
        "DA2",
        "CR0", #control registers
        "CR1",
        "CR2",
        "CR3",
        #Control register fields
        "CR1.MOSM",#MACC output shifter mode
        "CR1.MOVM",#MACC overflow clamp mode
        "CR1.AOVM",#ACC overflow clamp mode
        "CR2.FREE",#Interrupt busy flag
        "HIR", #Host interface register
        "CIR", #Incrementing registers
        "CIR1",
        "CIR2",
        "DIR",
        "DIR1",
        "DIR2",
        "XRD", #External memory read register
        "AX1R",#Right and left outputs
        "AX1L",
        "AX2R",
        "AX2L",
        "AX3R",
        "AX3L",
        "AR1R",#Right and left inputs
        "AR1L",
        "AR2R",
        "AR2L",
        "SS",#Signed and unsigned
        "US",
        "SU",
        "UU",
        "unkn" #placeholder for unknown register
    ]
    
    # Segment register information (use virtual CS and DS registers if your
    # processor doesn't have segment registers):
    reg_first_sreg = 1 # index of CS
    reg_last_sreg = 2 # index of DS
    
    # size of a segment register in bytes
    segreg_size = 0
    
    # You should define 2 virtual segment registers for CS and DS.
    # number of CS/DS registers
    reg_code_sreg = 1
    reg_data_sreg = 2
    
    '''
    CF_STOP = 0x00001 #  Instruction doesn't pass execution to the next instruction 
    CF_CALL = 0x00002 #  CALL instruction (should make a procedure here) 
    
    CF_CHG1 = 0x00004 #  The instruction modifies the first operand 
    CF_CHG2 = 0x00008 #  The instruction modifies the second operand 
    CF_CHG3 = 0x00010 #  The instruction modifies the third operand 
    CF_CHG4 = 0x00020 #  The instruction modifies 4 operand 
    CF_CHG5 = 0x00040 #  The instruction modifies 5 operand 
    CF_CHG6 = 0x00080 #  The instruction modifies 6 operand 
    
    CF_USE1 = 0x00100 #  The instruction uses value of the first operand 
    CF_USE2 = 0x00200 #  The instruction uses value of the second operand 
    CF_USE3 = 0x00400 #  The instruction uses value of the third operand 
    CF_USE4 = 0x00800 #  The instruction uses value of the 4 operand 
    CF_USE5 = 0x01000 #  The instruction uses value of the 5 operand 
    CF_USE6 = 0x02000 #  The instruction uses value of the 6 operand
    
    CF_JUMP = 0x04000 #  The instruction passes execution using indirect jump or call (thus needs additional analysis) 
    CF_SHFT = 0x08000 #  Bit-shift instruction (shl,shr...) 
    CF_HLL  = 0x10000 #  Instruction may be present in a high level language function. 
    '''
    
    # Array of instructions
    instruc = [
        {'name': 'NOP',    'feature': 0,                 'cmt': "No operation"}, #00
        {'name': 'CPML',   'feature': CF_USE1 | CF_USE2, 'cmt': "Load into ACC and two's complement"}, #09-07
        {'name': 'LACCU',  'feature': CF_USE1 | CF_USE2, 'cmt': "Load into ACC"}, #08-0B
        {'name': 'LACC',   'feature': CF_USE1 | CF_USE2, 'cmt': "Load into ACC"}, #10-13
        {'name': 'INC',    'feature': CF_USE1 | CF_USE2, 'cmt': "Load into ACC and increment"}, #14-17
        {'name': 'DEC',    'feature': CF_USE1 | CF_USE2, 'cmt': "Load into ACC and decrement"}, #18-1B
        {'name': 'SHACC',  'feature': CF_USE1 | CF_SHFT, 'cmt': "Shift ACC once left or right"}, #1C
        {'name': 'ZACC',   'feature': CF_USE1 | CF_USE2, 'cmt': "Zero ACC"}, #1D
        {'name': 'ADD',    'feature': CF_USE1 | CF_USE2, 'cmt': "Add MEM to ACC or MACC, store result in ACC"}, #20-23
        {'name': 'SUB',    'feature': CF_USE1 | CF_USE2, 'cmt': "Subtract ACC or MACC from MEM, store result in ACC"}, #24-27
        {'name': 'AND',    'feature': CF_USE1 | CF_USE2, 'cmt': "Bitwise logical AND ACC or MACC with MEM, store result in ACC"}, #28-2B
        {'name': 'OR',     'feature': CF_USE1 | CF_USE2, 'cmt': "Bitwise logical OR ACC or MACC with MEM, store result in ACC"}, #2C-2F
        {'name': 'XOR',    'feature': CF_USE1 | CF_USE2, 'cmt': "Bitwise logical XOR ACC or MACC with MEM, store result in ACC"}, #30-33
        {'name': 'CMP(1)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Compare ACC with MEM"}, #34, 36
        {'name': 'CMP(2)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Compare MACC with MEM"}, #35, 37
        {'name': 'RDE',    'feature': CF_USE1 | CF_USE2, 'cmt': "Read from external memory at address from CMEM"}, #39
        {'name': 'WRE',    'feature': CF_USE1 | CF_USE2, 'cmt': "Write to external memory at address from CMEM and data from DMEM"}, #39
        {'name': 'ADDD',   'feature': CF_USE1 | CF_USE2, 'cmt': "Add CMEM and DMEM, store result in ACC"}, #3C
        {'name': 'ANDD',   'feature': CF_USE1 | CF_USE2, 'cmt': "Bitwise AND CMEM and DMEM, store result in ACC"}, #3D0
        {'name': 'ORD',    'feature': CF_USE1 | CF_USE2, 'cmt': "Bitwise OR CMEM and DMEM, store result in ACC"}, #3D8
        {'name': 'XORD',   'feature': CF_USE1 | CF_USE2, 'cmt': "Bitwise XOR CMEM and DMEM, store result in ACC"}, #3E
        {'name': 'MPY(1)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM with ACC"}, #40, 41
        {'name': 'MPY(2)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by ACC"}, #44, 45
        {'name': 'MPY(3)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM by unsigned ACC"}, #48, 49
        {'name': 'MPY(4)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by unsigned ACC"}, #4C, 4D
        {'name': 'MPY(5)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM with DMEM"}, #42
        {'name': 'MPY(6)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by DMEM"}, #46
        {'name': 'MPY(7)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM by unsigned DMEM"}, #4A
        {'name': 'MPY(8)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by unsigned DMEM"}, #4E
        {'name': 'SHM',    'feature': CF_USE1 | CF_USE2, 'cmt': "Load DMEM into MACC and divide by 2"}, #43, 47
        {'name': 'SHMU',   'feature': CF_USE1 | CF_USE2, 'cmt': "Load unsigned DMEM into MACC and divide by 2"}, #4B, 4F
        {'name': 'MAC(1)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM by ACC, accumulate into MACC"}, #50, 51
        {'name': 'MAC(2)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply DMEM by ACC, accumulate into MACC"}, #52, 53
        {'name': 'MAC(3)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by ACC, accumulate into MACC"}, #5C, 5D
        {'name': 'MAC(4)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned DMEM by ACC, accumulate into MACC"}, #5E, 5F
        {'name': 'MAC(5)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM by DMEM, accumulate into MACC"}, #6C
        {'name': 'MAC(6)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by DMEM, accumulate into MACC"}, #6D
        {'name': 'MAC(7)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM by unsigned DMEM, accumulate into MACC"}, #6E
        {'name': 'MAC(8)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by unsigned DMEM, accumulate into MACC"}, #6F
        {'name': 'SHMAC',  'feature': CF_USE1 | CF_SHFT, 'cmt': "Shift MACC once left or right"}, #72
        {'name': 'ZMACC',  'feature': CF_USE1 | CF_USE2, 'cmt': "Zero both MACCs"}, #74
        {'name': 'LMH',    'feature': CF_USE1 | CF_USE2, 'cmt': "Load high MACC with MEM or ACC"}, #78, 79
        {'name': 'LMHC',   'feature': CF_USE1 | CF_USE2, 'cmt': "Load high MACC with MEM or ACC and clear low MACC"}, #7A, 7B
        {'name': 'LML',    'feature': CF_USE1 | CF_USE2, 'cmt': "Load low MACC with MEM or ACC"}, #7C, 7D
        {'name': 'LRI',    'feature': CF_USE1 | CF_USE2, 'cmt': "Load immediate into register"}, #Cx
        {'name': 'LRIAE',  'feature': CF_USE1 | CF_USE2, 'cmt': "Load immediate into register if accumulator greater than or equal to zero"}, #C14
        {'name': 'RPTK',   'feature': CF_USE1,           'cmt': "Repeat next instruction"}, #E0
        {'name': 'RPTB',   'feature': CF_USE1,           'cmt': "Repeat next block"}, #E4
        {'name': 'RET',    'feature': CF_STOP,           'cmt': "Return"}, #EC
        {'name': 'RETI',   'feature': CF_STOP,           'cmt': "Return from interrupt"}, #EE
        {'name': 'JMP',    'feature': CF_JUMP,           'cmt': "Jump unconditionally"}, #F0
        {'name': 'JZ',     'feature': CF_JUMP,           'cmt': "Jump if zero"}, #F10
        {'name': 'JNZ',    'feature': CF_JUMP,           'cmt': "Jump if not zero"}, #F18
        {'name': 'JGZ',    'feature': CF_JUMP,           'cmt': "Jump if greater than zero"}, #F20
        {'name': 'JLZ',    'feature': CF_JUMP,           'cmt': "Jump if less than zero"}, #F28
        {'name': 'JAOV',   'feature': CF_JUMP,           'cmt': "Jump if accumulator overflow"}, #F30
        {'name': 'JMOV',   'feature': CF_JUMP,           'cmt': "Jump if MACC overflow"}, #F48
        {'name': 'JBIOZ',  'feature': CF_JUMP,           'cmt': "Jump if BIO low"}, #F58
        {'name': 'JUNKN',  'feature': CF_JUMP,           'cmt': "Jump of unknown function"}, #F38, 40, 50
        {'name': 'CALL',   'feature': CF_CALL,           'cmt': "Call unconditionally"}, #F8
        {'name': 'CZ',     'feature': CF_CALL,           'cmt': "Call if zero"}, #F90
        {'name': 'CNZ',    'feature': CF_CALL,           'cmt': "Call if not zero"}, #F98
        {'name': 'CGZ',    'feature': CF_CALL,           'cmt': "Call if greater than zero"}, #FA0
        {'name': 'CLZ',    'feature': CF_CALL,           'cmt': "Call if less than zero"}, #FA8
        {'name': 'CAOV',   'feature': CF_CALL,           'cmt': "Call if accumulator overflow"}, #FB0
        {'name': 'CMOV',   'feature': CF_CALL,           'cmt': "Call if MACC overflow"}, #FB8
        {'name': 'CBIOZ',  'feature': CF_CALL,           'cmt': "Call if BIO low"}, #FC8
        {'name': 'CUNKN',  'feature': CF_CALL,           'cmt': "Call of unknown function"}, #F88, others
        {'name': 'UNKN',   'feature': 0,                 'cmt': "unknown opcode"}, #placeholder
    ]
    
    # icode of the first instruction
    instruc_start = 0
    
    # icode of the last instruction + 1
    instruc_end = len(instruc) + 1
    
    # Icode of return instruction. It is ok to give any of possible return
    # instructions
    # for x in instruc:
        # if x['name'] == 'RET':
            # icode_return = instruc.index(x)
    
    #Called at module initialization.
    def notify_init(self, idp_file):
        logging.info("notify_init")
        ida_ida.cvar.inf.set_wide_high_byte_first(True)  #big endian for words
        ida_ida.cvar.inf.set_be(True)  #AWFUL documentation
        return True
    
    def notify_newfile(self, filename):
        #Create PMEM, CMEM and DMEM segments
        
        PMEM = idaapi.segment_t()
        PMEM.startEA = 0
        PMEM.endEA = 0x200
        PMEM.bitness = 0 #16-bit addressing
        PMEM.type = SEG_CODE
        #PMEM.sel = find_free_selector() #???
        
        idaapi.add_segm_ex(PMEM, "PMEM", "CODE", ADDSEG_OR_DIE)
        
        CMEM = idaapi.segment_t()
        CMEM.startEA = 0x200
        CMEM.endEA = 0x400
        CMEM.bitness = 0 #16-bit addressing
        CMEM.type = SEG_DATA | SEG_XTRN
        CMEM.sel = idaapi.allocate_selector(CMEM.startEA >> 4) #???
        
        idaapi.add_segm_ex(CMEM, "CMEM", "DATA", ADDSEG_OR_DIE)
        
        DMEM = idaapi.segment_t()
        DMEM.startEA = 0x400
        DMEM.endEA = 0x600
        DMEM.bitness = 0 #16-bit addressing
        DMEM.type = SEG_DATA | SEG_XTRN
        DMEM.sel = idaapi.allocate_selector(DMEM.startEA >> 4) #???
        
        idaapi.add_segm_ex(DMEM, "DMEM", "DATA", ADDSEG_OR_DIE)
    
    repeats = {}
    def notify_emu(self, insn):
        """
        Emulate instruction, create cross-references, plan to analyze
        subsequent instructions, modify flags etc. Upon entrance to this function
        all information about the instruction is in 'insn' structure (insn_t).
        If zero is returned, the kernel will delete the instruction.
        """
        logging.info("notify_emu")
        
        feature = insn.get_canon_feature()
        mnemonic = insn.get_canon_mnem()
        
        #is it an unconditional jump?
        uncond_jmp = False
        if insn.itype == self.get_instruction('JMP'):
            uncond_jmp = True
        
        #is it a jump?
        if feature & CF_JUMP > 0: #If the instruction has the CF_JUMP flag
            insn.add_cref(insn[0].addr, 0, fl_JN)
        
        #is it a call?
        if feature & CF_CALL > 0: #If the instruction has the CF_CALL flag
            insn.add_cref(insn[0].addr, 0, fl_CN)
        
        #is it a repeat?
        reps = ["RPTB", "RPTK"]
        if mnemonic in reps:
            if insn[1].addr != 0x0:
                #Remember this repeat for later
                self.repeats[insn[1].addr] = insn.ea + insn.size
        
        #If this is the end of a repeat, add the flow to the start of the loop
        if insn.ea in self.repeats.keys():
            insn.add_cref(self.repeats[insn.ea], 0, fl_JN)
        
        #Does the processor go to the next instruction?
        flow = (feature & CF_STOP == 0) and not uncond_jmp
        if flow:
            insn.add_cref(insn.ea + insn.size, 0, fl_F)
        
        #add data xref
        if insn[0].specflag2 == 1:
            pass#ua_add_dref(0, insn[0].addr, dr_W)
        if insn[1].specflag2 == 1:
            pass#ua_add_dref(0, insn[1].addr, dr_W)
        
        #Add data reference
        #TODO read/write flag
        for i in range(6):
            op = insn[i]
            if op.type == o_mem:
                if (op.specval == 1):
                    insn.add_dref(op.addr + 0x400, 0, dr_R)
                elif (op.specval == 2):
                    insn.add_dref(op.addr + 0x200, 0, dr_R)
        
        return 1
        
    def notify_out_operand(self, ctx, op):
        """
        Generate text representation of an instructon operand.
        This function shouldn't change the database, flags or anything else.
        The output text is placed in the output buffer initialized with init_output_buffer()
        This function uses out_...() functions from ua.hpp to generate the operand text
        Invoked when out_one_operand is called
        Returns: 1-ok, 0-operand is hidden.
        """
        logging.info("notify_out_operand")
        
        optype = op.type
        
        if optype == o_reg:
            #Special case for SHACC and SHMAC
            if (ctx.insn.get_canon_mnem() in ["SHACC", "SHMAC"] and op.n == 0):
                ctx.out_register(self.regNames[op.reg])
                ctx.out_char(" ")
                if (op.specval == 1):
                    ctx.out_symbol("<")
                    ctx.out_symbol("<")
                elif (op.specval == 2):
                    ctx.out_symbol(">")
                    ctx.out_symbol(">")
                else:
                    logger.error("notify_out_operand No specval for SHACC (specval = " + str(op.specval) + ")")
                ctx.out_char(" ")
                ctx.out_symbol("1")
                
            #All other instructions
            else:
                if (op.specflag3 == 1):
                    ctx.out_symbol("-") #minus sign
                if (op.specflag1 == 1):
                    ctx.out_symbol("*") #pointer symbol
                ctx.out_register(self.regNames[op.reg])
                if (op.specflag2 == 1):
                    ctx.out_symbol("+") #post-increment symbol
                if op.specval == 1:
                    ctx.out_register("CIR1")
                elif op.specval == 2:
                    ctx.out_register("CIR2")
                elif op.specval == 3:
                    ctx.out_register("DIR1")
                elif op.specval == 4:
                    ctx.out_register("DIR2")
            
        elif optype == o_imm:
            ctx.out_symbol("#")
            if (op.specflag1 == 1):
                #Output as signed
                ctx.out_value(op, OOFW_IMM | OOF_SIGNED)
            else:
                ctx.out_value(op, OOFW_IMM)
            logging.debug("notify_out_operand immediate value: " + hex(ctx.insn[0].value))
            
        elif optype == o_near:
            ok = ctx.out_name_expr(op, op.addr, BADADDR)
            if not ok:
                #When op.addr is either indirect or references outside of the address space
                ctx.out_tagon(COLOR_ERROR)
                ctx.out_long(op.addr, 16)
                ctx.out_tagoff(COLOR_ERROR)
                #QueueMark(Q_noName, self.cmd.ea)
        
        elif optype == o_mem:
            #Output memory region
            if (op.specflag3 == 1):
                ctx.out_symbol("-") #minus sign
            if (op.specval == 1):
                ctx.out_printf("DMEM")
            elif (op.specval == 2):
                ctx.out_printf("CMEM")
            else:
                logging.error("notify_out_operand Wrong specval for memory region in operand")
            
            ctx.out_addr_tag(op.addr) #Does nothing??
            
            ctx.out_symbol("(")
            
            #output address
            #ok = ctx.out_name_expr(op, op.addr, BADADDR)
            #if not ok:
            #    #When op.addr is either indirect or references outside of the address space
            #    ctx.out_tagon(COLOR_ERROR)
            #    ctx.out_long(op.addr, 16)
            #    ctx.out_tagoff(COLOR_ERROR)
            #    #QueueMark(Q_noName, self.cmd.ea)
            ctx.out_value(op, OOF_ADDR | OOFW_IMM)
            ctx.out_symbol(")")
            
        else:
            logging.error("notify_out_operand op type " + str(optype) + " failed in outop")
            return False
        
        return True
    
    def notify_out_insn(self, ctx):
        """
        Generate text representation of an instruction in 'cmd' structure.
        This function shouldn't change the database, flags or anything else.
        All these actions should be performed only by u_emu() function.
        Returns: nothing
        """
        logging.info("notify_out_insn for " + self.instruc[ctx.insn.itype]["name"] + " at " + hex(ctx.insn.ea))
        
        ctx.out_mnemonic()
        insn = ctx.insn
        
        # output first operand
        if insn[0].type != o_void:
            ctx.out_one_operand(0)
        
        # output 1st instruction operands
        for i in xrange(1, 4):
            if insn[i].type == o_void:
                break
            ctx.out_symbol(",")
            ctx.out_char(" ")
            ctx.out_one_operand(i)
        
        
        #Check for 2nd instruction
        #output MOV and the rest of operands
        if insn[4].type != o_void:
            #print 2nd instruction
            ctx.out_char(" ") #At least one space between instructions
            ctx.out_spaces(39) #2nd instruc margin
            
            ctx.out_printf("MOV ")
            ctx.out_one_operand(4)
            ctx.out_symbol(",")
            ctx.out_char(" ")
            ctx.out_one_operand(5) #Should always be 2
        
        ctx.out_spaces(62) #put comments at the same position
        ctx.out_symbol("|")
        #ctx.set_gen_cmt() #Does nothing??
        ctx.flush_outbuf()
    
    def notify_get_autocmt(self, insn):
        name = insn.get_canon_mnem()
        #Search instruc for this name
        for entry in self.instruc:
            if entry["name"] == name:
                return entry["cmt"]
        return "No comment for this instruction"
    
    #gets an instruction table index from instruc by name
    def get_instruction(self, name):
        for x in self.instruc:
            if x['name'] == name:
                return self.instruc.index(x)
        raise Exception("Could not find instruction %s" % name)
    
    #returns name if name exists in regNames
    def get_register(self, name):
        for x in self.regNames:
            if x == name:
                return self.regNames.index(x)
        raise Exception("Could not find register %s" % name)
    
    
    def ana_jumps(self):
        logging.info("ana_jumps")
        self.insn[0].type = o_near #All jumps are intra-segment
        self.insn[0].addr = (self.b3 & 1) << 8 | self.b4 #immediate jump location
        
        logging.debug("ana_jumps address: 0x%0.3X" % self.insn[0].addr)
        
        #condition = lower 3 bits of self.b1 and upper 4 of self.b2
        condition = (self.b1 & 0x7) << 4 | self.b2 >> 4
        
        instr_name = ""
        if self.b1 >= 0xF8:
            instr_name = "C" #For calls
        else:
            instr_name = "J" #For jumps
        
        if condition == 0x00: #unconditional
            pass
        elif condition == 0x08: #indirect jump to ACC1
            self.insn[0].type = o_reg
            self.insn[0].reg = self.get_register("ACC1")
        elif condition == 0x0C: #indirect jump to ACC2
            self.insn[0].type = o_reg
            self.insn[0].reg = self.get_register("ACC2")
        elif condition == 0x10: #if zero
            instr_name += "Z"
        elif condition == 0x18: #if not zero
            instr_name += "NZ"
        elif condition == 0x20: #if greater than zero
            instr_name += "GZ"
        elif condition == 0x28: #if less than zero
            instr_name += "LZ"
        elif condition == 0x30: #if acc overflow
            instr_name += "AOV"
        elif condition == 0x48: #if macc overflow
            instr_name += "MOV"
        elif condition == 0x58: #if BIO low
            instr_name += "BIOZ"
        else:
            instr_name += "UNKN"
        
        if len(instr_name) == 1: #If name has not been set yet
            if self.b1 >= 0xF8:
                instr_name = "CALL"
            else:
                instr_name = "JMP"
        
        self.insn.itype = self.get_instruction(instr_name)
        
    def ana_lri_long(self):
        #load register with immediate
        #Op1 = imm, Op2 = reg
        self.insn.itype = self.get_instruction("LRI")
        
        #24-bit immediate
        self.insn[0].type = o_imm
        self.insn[0].dtype = dt_word
        self.insn[0].value = self.b2 << 16 | self.b3 << 8 | self.b4
        
        #destination register
        self.insn[1].type = o_reg
        if self.b1 == 0xCA:
            self.insn[1].reg = self.get_register("ACC1")
        elif self.b1 == 0xCB:
            self.insn[1].reg = self.get_register("ACC2")
        elif self.b1 == 0xCC:
            self.insn[1].reg = self.get_register("CR0")
        elif self.b1 == 0xCD:
            self.insn[1].reg = self.get_register("CR1")
        elif self.b1 == 0xCE:
            self.insn[1].reg = self.get_register("CR2")
        elif self.b1 == 0xCF:
            self.insn[1].reg = self.get_register("CR3")
            
    def ana_lri(self):
        logging.info("ana_lri")
        #load addressing register with immediate
        #Op1 = imm, Op2 = reg
        self.insn.itype = self.get_instruction("LRI")
        
        imm = ((self.b3 & 0xF) << 8) | self.b4 #12-bit immediate
            
        self.insn[0].type = o_imm
        self.insn[0].dtype = dt_word
        self.insn[0].value = imm
        
        logging.debug("ana_lri self.b2: " + hex(self.b2))
        #registers
        self.insn[1].type = o_reg
        if self.b2 == 0x00:
            self.insn[1].reg = self.get_register("DA1")
        elif self.b2 == 0x08:
            self.insn[1].reg = self.get_register("DA2")
        elif self.b2 == 0x10:
            self.insn[1].reg = self.get_register("DIR1")
        elif self.b2 == 0x18:
            self.insn[1].reg = self.get_register("DIR2")
        elif self.b2 == 0x20:
            self.insn[1].reg = self.get_register("CA1")
        elif self.b2 == 0x28:
            self.insn[1].reg = self.get_register("CA2")
        elif self.b2 == 0x30:
            self.insn[1].reg = self.get_register("CIR1")
        elif self.b2 == 0x38:
            self.insn[1].reg = self.get_register("CIR2")
        elif self.b2 == 0x40:
            self.insn.itype = self.get_instruction("LRIAE")
            self.insn[1].reg = self.get_register("CA1")
            
            self.insn[2].type = o_reg
            self.insn[2].reg = self.get_register("ACC")
        elif self.b2 == 0x48:
            self.insn.itype = self.get_instruction("LRIAE")
            self.insn[1].reg = self.get_register("CA2")
            
            self.insn[2].type = o_reg
            self.insn[2].reg = self.get_register("ACC")
        else:
            logging.error("ana_lri wrong operand to LRI: " + hex(self.b2))
            
    def ana_lri_dual(self):
        #load addressing registers with immediates
        #Op1 = imm1, Op2 = imm2, Op3 = reg
        self.insn.itype = self.get_instruction("LRI")
        
        #2x 12-bit immediates
        imm1 = self.b4 | (self.b3 & 0xF) << 8
        imm2 = self.b3 >> 4 | (self.b2 << 4)
        
        self.insn[0].type = o_imm
        self.insn[0].dtype = dt_word #16-bit, close enough
        self.insn[0].value = imm1
        
        self.insn[1].type = o_imm
        self.insn[1].dtype = dt_word
        self.insn[1].value = imm2
        
        #registers
        self.insn[2].type = o_reg
        if self.b1 == 0xC2:
            self.insn[2].reg = self.get_register("DA")
        elif self.b1 == 0xC4:
            self.insn[2].reg = self.get_register("CA")
        elif self.b1 == 0xC3:
            self.insn[2].reg = self.get_register("DIR")
        elif self.b1 == 0xC5:
            self.insn[2].reg = self.get_register("CIR")
    
    def ana_dmem_addressing(self, operand):
        arg = self.b3 & 0x30
        if (arg == 0x30 or arg == 0x20):
            self.insn[operand].type = o_reg
            self.insn[operand].specflag1 = 1 #Output *
            if (self.b3 & 8 > 0):
                self.insn[operand].reg = self.get_register("DA2")
            else:
                self.insn[operand].reg = self.get_register("DA1")
            
            #post-incrementing
            if self.b3 & 4 > 0:
                #DIR post-increment
                self.insn[operand].specflag2 = 1 #Print plus sign
                if self.b3 & 2 > 0:
                    self.insn[operand].specval = 4 #DIR2
                else:
                    self.insn[operand].specval = 3 #DIR1
            else:
                self.insn[operand].specflag2 = self.b3 & 2 > 0 and 1 or 0
                
        elif (arg == 0x10):
            self.insn[operand].type = o_mem
            self.insn[operand].addr = 0x000 + (self.b3 & 1) << 8 | self.b4 #0x400
            self.insn[operand].specval = 1 #DMEM
            
            #TODO CA control
    
    def ana_cmem_addressing(self, operand):
        arg = self.b3 & 0x30
        if (arg == 0x30):
            self.insn[operand].type = o_reg
            self.insn[operand].specflag1 = 1 #Output *
            if (self.b3 & 1 > 0):
                self.insn[operand].reg = self.get_register("CA2")
            else:
                self.insn[operand].reg = self.get_register("CA1")
            
            #post-incrementing
            if self.b4 & 0x80 > 0:
                #CIR post-increment
                self.insn[operand].specflag2 = 1 #Print plus sign
                if self.b4 & 0x40 > 0:
                    self.insn[operand].specval = 2 #CIR2
                else:
                    self.insn[operand].specval = 1 #CIR1
            else:
                self.insn[operand].specflag2 = self.b4 & 0x40 > 0 and 1 or 0
                
        elif (arg == 0x10):
            self.insn[operand].type = o_reg
            self.insn[operand].specflag1 = 1 #Output *
            if (self.b3 & 8 > 0):
                self.insn[operand].reg = self.get_register("CA2")
            else:
                self.insn[operand].reg = self.get_register("CA1")
            
            #post-incrementing
            if self.b3 & 4 > 0:
                #CIR post-increment
                self.insn[operand].specflag2 = 1 #Print plus sign
                if self.b3 & 2 > 0:
                    self.insn[operand].specval = 2 #CIR2
                else:
                    self.insn[operand].specval = 1 #CIR1
            else:
                self.insn[operand].specflag2 = self.b3 & 2 > 0 and 1 or 0
                
        elif (arg == 0x20):
            self.insn[operand].type = o_mem
            self.insn[operand].addr = 0x000 + (self.b3 & 1) << 8 | self.b4 #0x200
            self.insn[operand].specval = 2 #CMEM
        
        elif (arg == 0x00):
            self.insn[operand].type = o_reg
            self.insn[operand].reg = self.get_register("unkn")
        
            #TODO DA control
    
    def ana_load(self):
        logging.info("ana_load")
        
        op = self.b1 & 3
        
        #Destination operand
        self.insn[1].type = o_reg
        if (self.b2 & 0x40 > 0):
            self.insn[1].reg = self.get_register("ACC2")
        else:
            self.insn[1].reg = self.get_register("ACC1")
        
        #Source operand
        if (op == 0): #DMEM
            self.ana_dmem_addressing(0)
                
        elif (op == 1): #CMEM
            self.ana_cmem_addressing(0)
        
        elif (op == 2): #ACC
            self.insn[0].type = o_reg
            if (self.b2 & 0x80 > 0):
                self.insn[0].reg = self.get_register("ACC2")
            else:
                self.insn[0].reg = self.get_register("ACC1")
        
        elif (op == 3): #MACC
            self.insn[0].type = o_reg
            if (self.b2 & 0x80 > 0):
                self.insn[0].reg = self.get_register("MACC2")
            else:
                self.insn[0].reg = self.get_register("MACC1")
    
    def ana_lac(self):
        logging.info("ana_lac")
        self.insn.itype = self.get_instruction("LACC")
        self.ana_load()
    
    def ana_inc(self):
        logging.info("ana_inc")
        self.insn.itype = self.get_instruction("INC")
        self.ana_load()
    
    def ana_dec(self):
        logging.info("ana_dec")
        self.insn.itype = self.get_instruction("DEC")
        self.ana_load()
        
    def ana_lacu(self):
        logging.info("ana_dec")
        self.insn.itype = self.get_instruction("LACCU")
        self.ana_load()
        
    def ana_cpml(self):
        logging.info("ana_dec")
        self.insn.itype = self.get_instruction("CPML")
        self.ana_load()
    
    def ana_repeat(self):
        logging.info("ana_repeat")
        
        self.insn[0].type = o_imm
        self.insn[0].value = self.b2
        
        self.insn[1].type = o_near #2nd operand is loop end
        
        if (self.b1 == 0xE0):
            self.insn.itype = self.get_instruction("RPTK")
            self.insn[1].addr = self.insn.ea + self.insn.size
        elif (self.b1 == 0xE4):
            self.insn.itype = self.get_instruction("RPTB")
            self.insn[1].addr = (self.b3 & 1) << 8 | self.b4
        else:
            logging.error("ana_repeat called for non-repeat instruction")
        
        logging.debug("ana_repeat end of loop: " + hex(self.insn[1].addr))
    
    def ana_shacc(self):
        logging.info("ana_shacc")
        self.insn.itype = self.get_instruction("SHACC")
        
        self.insn[0].type = o_reg
        if (self.b2 & 0x40 > 0):
            self.insn[0].reg = self.get_register("ACC2")
        else:
            self.insn[0].reg = self.get_register("ACC1")
        
        if (self.b2 & 0x80 > 0):
            self.insn[0].specval = 1
        else:
            self.insn[0].specval = 2
    
    def ana_shmac(self):
        logging.info("ana_shmac")
        self.insn.itype = self.get_instruction("SHMAC")
        
        self.insn[0].type = o_reg
        if (self.b2 & 0x40 > 0):
            self.insn[0].reg = self.get_register("MACC2")
        else:
            self.insn[0].reg = self.get_register("MACC1")
        
        if (self.b2 & 0x80 > 0):
            self.insn[0].specval = 1
        else:
            self.insn[0].specval = 2
    
    def ana_zacc(self):
        logging.info("ana_zacc")
        self.insn.itype = self.get_instruction("ZACC")
        
        self.insn[0].type = o_reg
        
        if self.b1 == 0x1D: #Single accumulator
            if (self.b2 & 0x40 > 0):
                self.insn[0].reg = self.get_register("ACC2")
            else:
                self.insn[0].reg = self.get_register("ACC1")
        elif self.b1 == 0x1F: #Both accumulators
            self.insn[0].reg = self.get_register("ACC1")
            self.insn[1].type = o_reg
            self.insn[1].reg = self.get_register("ACC2")
        else:
            logging.error("ana_zacc used on non-zacc instruction (opcode = " + str(self.b1) + ")")
    
    def ana_zmacc(self):
        logging.info("ana_zmacc")
        self.insn.itype = self.get_instruction("ZMACC")
        
        self.insn[0].type = o_reg
        
        if self.b1 == 0x73: #For singular zmacc
            if self.b2 & 0x40 > 0:
                self.insn[0].reg = self.get_register("MACC2")
            else:
                self.insn[0].reg = self.get_register("MACC1")
        else: #For dual zmacc
            self.insn[0].reg = self.get_register("MACC1")
            self.insn[1].type = o_reg
            self.insn[1].reg = self.get_register("MACC2")
    
    def ana_arith(self):
        logging.info("ana_arith")
        
        op = self.b1 & 3
        
        if (op <= 1): #DMEM
            self.ana_dmem_addressing(0)
        else: #CMEM
            self.ana_cmem_addressing(0)
        
        self.insn[1].type = o_reg
        if (op == 0 or op == 2): #ACC is second operand
            if (self.b2 & 0x80 > 0):
                self.insn[1].reg = self.get_register("ACC2")
            else:
                self.insn[1].reg = self.get_register("ACC1")
        else: #MACC is second operand
            self.insn[1].type = o_reg
            if (self.b2 & 0x80 > 0):
                self.insn[1].reg = self.get_register("MACC2")
            else:
                self.insn[1].reg = self.get_register("MACC1")
        
        self.insn[2].type = o_reg
        if (self.b2 & 0x40 > 0):
            self.insn[2].reg = self.get_register("ACC2")
        else:
            self.insn[2].reg = self.get_register("ACC1")
    
    def ana_arith_dual(self, opcode):
        logging.info("ana_arith_dual")
        
        self.insn.itype = self.get_instruction(opcode)
        
        self.ana_cmem_addressing(0);
        self.ana_dmem_addressing(1);
        
        if self.b1 == 0x3C and (self.b2 & 0x80 > 0): #Add
            self.insn[0].specflag3 = 1 #negate CMEM
        
        self.insn[2].type = o_reg
        if (self.b2 & 0x40 > 0):
            self.insn[2].reg = self.get_register("ACC2")
        else:
            self.insn[2].reg = self.get_register("ACC1")
    
    def ana_add(self):
        logging.info("ana_add")
        self.insn.itype = self.get_instruction("ADD")
        self.ana_arith()
    
    def ana_sub(self):
        logging.info("ana_sub")
        self.insn.itype = self.get_instruction("SUB")
        self.ana_arith()
    
    def ana_and(self):
        logging.info("ana_and")
        self.insn.itype = self.get_instruction("AND")
        self.ana_arith()
    
    def ana_or(self):
        logging.info("ana_or")
        self.insn.itype = self.get_instruction("OR")
        self.ana_arith()
    
    def ana_xor(self):
        logging.info("ana_xor")
        self.insn.itype = self.get_instruction("XOR")
        self.ana_arith()
    
    def ana_mult_single(self):
        self.insn[2].type = o_reg
        if (self.b1 & 0x01 > 0):
            self.insn[2].reg = self.get_register("ACC2")
        else:
            self.insn[2].reg = self.get_register("ACC1")
        if (self.b2 & 0x80 > 0):
            self.insn[2].specflag3 = 1 #Disply minus sign
        
        self.insn[3].type = o_reg
        if (self.b2 & 0x40 > 0):
            self.insn[3].reg = self.get_register("MACC2")
        else:
            self.insn[3].reg = self.get_register("MACC1")
    
    def ana_mult_cmem(self, opcode):
        logging.info("ana_mult_cmem")
        self.insn.itype = self.get_instruction(opcode)
        
        self.insn[0].type = o_reg
        if self.b1 == 0x40 or self.b1 == 0x41:
            self.insn[0].reg = self.get_register("SS")
        elif self.b1 == 0x44 or self.b1 == 0x45:
            self.insn[0].reg = self.get_register("US")
        elif self.b1 == 0x48 or self.b1 == 0x49:
            self.insn[0].reg = self.get_register("SU")
        elif self.b1 == 0x4C or self.b1 == 0x4D:
            self.insn[0].reg = self.get_register("UU")
        elif self.b1 == 0x50 or self.b1 == 0x51:
            self.insn[0].reg = self.get_register("SS")
        elif self.b1 == 0x5C or self.b1 == 0x5D:
            self.insn[0].reg = self.get_register("US")
        
        self.ana_cmem_addressing(1)
        self.ana_mult_single()
        
    def ana_mult_dmem(self, opcode):
        logging.info("ana_mult_dmem")
        self.insn.itype = self.get_instruction(opcode)
        
        self.insn[0].type = o_reg
        if self.b1 == 0x52 or self.b1 == 0x53:
            self.insn[0].reg = self.get_register("SS")
        elif self.b1 == 0x5E or self.b1 == 0x5F:
            self.insn[0].reg = self.get_register("US")
        
        self.ana_dmem_addressing(1)
        self.ana_mult_single()
    
    def ana_mult_dual(self, opcode):
        logging.info("ana_mult_dual")
        self.insn.itype = self.get_instruction(opcode)
        
        self.insn[0].type = o_reg
        if self.b1 == 0x42:
            self.insn[0].reg = self.get_register("SS")
        elif self.b1 == 0x46:
            self.insn[0].reg = self.get_register("US")
        elif self.b1 == 0x4A:
            self.insn[0].reg = self.get_register("SU")
        elif self.b1 == 0x4E:
            self.insn[0].reg = self.get_register("UU")
        elif self.b1 == 0x6C:
            self.insn[0].reg = self.get_register("SS")
        elif self.b1 == 0x6D:
            self.insn[0].reg = self.get_register("US")
        elif self.b1 == 0x6E:
            self.insn[0].reg = self.get_register("SU")
        elif self.b1 == 0x6F:
            self.insn[0].reg = self.get_register("UU")
        
        self.ana_cmem_addressing(1)
        self.ana_dmem_addressing(2)
        
        if (self.b2 & 0x80 > 0):
            self.insn[1].specflag3 = 1 #Negate product
        
        self.insn[3].type = o_reg
        if (self.b2 & 0x40 > 0):
            self.insn[3].reg = self.get_register("MACC2")
        else:
            self.insn[3].reg = self.get_register("MACC1")
    
    def ana_shm(self, opcode):
        logging.info("ana_shm")
        self.insn.itype = self.get_instruction(opcode)
        
        self.ana_dmem_addressing(0)
        
        if (self.b2 & 0x80 > 0):
            self.insn[0].specflag3 = 1 #Negate product
        
        self.insn[1].type = o_reg
        if (self.b2 & 0x40 > 0):
            self.insn[1].reg = self.get_register("MACC2")
        else:
            self.insn[1].reg = self.get_register("MACC1")
    
    def ana_extern(self):
        logging.info("ana_extern")
        
        if self.b2 & 0x40 > 0:
            self.insn.itype = self.get_instruction("WRE")
            self.ana_dmem_addressing(0)
            self.ana_cmem_addressing(1)
        else:
            self.insn.itype = self.get_instruction("RDE")
            self.ana_cmem_addressing(0)
            
            self.insn[1].type = o_reg
            self.insn[1].reg = self.get_register("XRD")
            # TODO: Investigation and addressing improvements
        
    def ana_loadmacc(self, opcode):
        logging.info("ana_loadmacc")
        self.insn.itype = self.get_instruction(opcode)
        
        #MEM or ACC
        if (self.b2 & 0x80 > 0):
            #ACC
            self.insn[0].type = o_reg
            if (self.b1 & 0x01 > 0):
                self.insn[0].reg = self.get_register("ACC2")
            else:
                self.insn[0].reg = self.get_register("ACC1")
        else:
            #MEM
            if (self.b1 & 0x01 > 0):
                self.ana_cmem_addressing(0)
            else:
                self.ana_dmem_addressing(0)
        
        self.insn[1].type = o_reg
        if (self.b2 & 0x40 > 0):
            self.insn[1].reg = self.get_register("MACC2")
        else:
            self.insn[1].reg = self.get_register("MACC1")
    
    def ana_cmp(self):
        logging.info("ana_cmp")
        
        if self.b1 & 2 == 0:
            #34 and 35
            self.ana_dmem_addressing(0)
        else:
            #36 and 37
            self.ana_cmem_addressing(0)
        
        self.insn[1].type = o_reg
        if self.b1 & 1 == 0:
            #34 and 36 = ACC
            self.insn.itype = self.get_instruction("CMP(1)")
            
            if (self.b2 & 0x80 > 0):
                self.insn[1].reg = self.get_register("ACC2")
            else:
                self.insn[1].reg = self.get_register("ACC1")
        else:
            #35 and 37 = MACC
            self.insn.itype = self.get_instruction("CMP(2)")
            
            if (self.b2 & 0x80 > 0):
                self.insn[1].reg = self.get_register("MACC2")
            else:
                self.insn[1].reg = self.get_register("MACC1")
    
    def ana2_store(self, regname1, regname2):
        self.insn[4].type = o_reg
        if (self.b3 & 0x40 > 0):
            self.insn[4].reg = self.get_register(regname2)
        else:
            self.insn[4].reg = self.get_register(regname1)
        
        if (self.b3 & 0x80 > 0):
            self.ana_cmem_addressing(5)
        else:
            self.ana_dmem_addressing(5)
    
    def ana2_store_cmem(self, reg1, reg2, reg3, reg4):
        arg = self.b3 >> 6
        regs = [reg1, reg2, reg3, reg4]
        
        self.insn[4].type = o_reg
        self.insn[4].reg = self.get_register(regs[arg])
        
        self.ana_cmem_addressing(5)
    
    def ana2_output(self):
        self.insn[4].type = o_reg
        if (self.b3 & 0x40 > 0):
            self.insn[4].reg = self.get_register("MACC2")
        else:
            self.insn[4].reg = self.get_register("MACC1")
        
        output_port = ""
        if self.opcode2 == 0x18:
            output_port = "AX1"
        elif self.opcode2 == 0x19:
            output_port = "AX2"
        elif self.opcode2 == 0x1A:
            output_port = "AX3"
        else:
            logging.error("ana2_output Wrong 2nd instruction used on this function (opcode2 = " + str(self.opcode2) + ")")
        
        self.insn[5].type = o_reg
        if (self.b3 & 0x80 > 0):
            self.insn[5].reg = self.get_register(output_port + "R")
        else:
            self.insn[5].reg = self.get_register(output_port + "L")
        
    def ana2_input(self):
        
        port = "1" if self.opcode2 == 0x0C else "2" #AR2 in case of 0x0D opcode2
        channel = "L" if self.b3 & 0x80 == 0 else "R"
        
        self.insn[4].type = o_reg
        self.insn[4].reg = self.get_register("AR" + port + channel)
        
        self.ana_dmem_addressing(5) #TODO investigation for C5, CD
    
    def ana2_extern(self):
        self.insn[4].type = o_reg
        self.insn[4].reg = self.get_register("XRD")
        
        #It is possible for this instruction to write to CMEM, but nothing uses it this way
        self.ana_dmem_addressing(5)
    
    def ana2_hir(self):
        if (self.b3 & 0x80 > 0):
            self.ana_cmem_addressing(4)
        else:
            self.ana_dmem_addressing(4)
        
        self.insn[5].type = o_reg
        self.insn[5].reg = self.get_register("HIR")
        
    def ana2_dereference(self):
        self.ana_cmem_addressing(4)
        
        mem = "C" if self.opcode2 == 0x9 else "D"
        
        self.insn[5].type = o_reg
        if (self.b3 & 0x40 > 0): #Store in CIR
            if (self.b3 & 0x80 > 0):
                self.insn[5].reg = self.get_register(mem + "IR2")
            else:
                self.insn[5].reg = self.get_register(mem + "IR1")
        else: #Store in CA
            if (self.b3 & 0x80 > 0):
                self.insn[5].reg = self.get_register(mem + "A2")
            else:
                self.insn[5].reg = self.get_register(mem + "A1")
        
    def ana2_macshift(self):
        self.insn[4].type = o_reg
        self.insn[4].reg = self.get_register("CR1.MOSM")
        
        self.insn[5].type = o_imm
        self.insn[5].specflag1 = 1 #Output sign for -8
        
        modes = [0, 2, 4, -8]
        modeselect = self.b3 >> 6
        if modeselect <= 3 and modeselect >= 0:
            self.insn[5].value = modes[modeselect]
        else:
            self.insn[5].value = 0
            logging.error("ana2_macshift modeselect out of array bounds")
        
    def ana2_OVclamp(self):
        arg = self.b3 >> 6
        
        self.insn[4].type = o_reg
        if arg <= 1: #Accumulator
            self.insn[4].reg = self.get_register("CR1.AOVM")
        else: #MACC
            self.insn[4].reg = self.get_register("CR1.MOVM")
            
        self.insn[5].type = o_imm
        self.insn[5].value = arg & 1 #1 = enabled
    
    def ana2_CR(self):
        CR_pos = 4
        cmem_pos = 5
        
        if self.opcode2 == 0x22: #If this is a write to CRx
            CR_pos = 5
            cmem_pos = 4
        
        CR = self.b3 >> 6
        self.insn[CR_pos].type = o_reg
        self.insn[CR_pos].reg = self.get_register("CR" + str(CR))
        
        self.ana_cmem_addressing(cmem_pos)
        
    def ana2_load_cmem(self, reg1, reg2, reg3, reg4):
        arg = self.b3 >> 6
        regs = [reg1, reg2, reg3, reg4]
        
        self.insn[5].type = o_reg
        self.insn[5].reg = self.get_register(regs[arg])
        
        self.ana_cmem_addressing(4)
        
    def ana2_2C(self):
        arg = self.b3 >> 6
        
        if arg >= 2:
            #002C8xxx - 002CFxxx
            #Unknown
            self.insn[4].type = o_reg
            self.insn[5].type = o_reg
            self.insn[4].reg = self.get_register("unkn")
            self.insn[5].reg = self.get_register("unkn")
        else:
            self.insn[4].type = o_imm
            self.insn[4].value = arg & 1
            
            self.insn[5].type = o_reg
            self.insn[5].reg = self.get_register("CR2.FREE")
    
    def ana2_circular_mode(self):
        arg = self.b3 >> 6
        
        value = arg & 1
        memory = arg & 2
        
        self.insn[4].type = o_reg
        if memory == 0:
            self.insn[4].reg = self.get_register("CR1.LDMEM")
        else:
            self.insn[4].reg = self.get_register("CR1.LCMEM")
        
        self.insn[5].type = o_imm
        self.insn[5].value = value
    
    def ana2(self):
        if self.opcode2 == 0x01:
            self.ana2_store("ACC1", "ACC2")
        elif self.opcode2 == 0x02:
            self.ana2_store("MACC1", "MACC2")
        elif self.opcode2 == 0x03:
            self.ana2_store("MACC1L", "MACC2L")
        elif self.opcode2 == 0x06:
            self.ana2_load_cmem("DA", "DIR", "CA", "CIR")
        elif self.opcode2 == 0x08 or self.opcode2 == 0x09:
            self.ana2_dereference()
        elif self.opcode2 == 0x0A:
            self.ana2_store_cmem("DA1", "DIR1", "DA2", "DIR2")
        elif self.opcode2 == 0x0B:
            self.ana2_store_cmem("CA1", "CIR1", "CA2", "CIR2")
        elif self.opcode2 == 0x0C or self.opcode2 == 0x0D:
            self.ana2_input()
        elif self.opcode2 >= 0x18 and self.opcode2 <= 0x1A:
            self.ana2_output()
        elif self.opcode2 == 0x20:
            self.ana2_extern()
        elif self.opcode2 == 0x22 or self.opcode2 == 0x23:
            self.ana2_CR()
        elif self.opcode2 == 0x26:
            self.ana2_hir()
        elif self.opcode2 == 0x29:
            self.ana2_macshift()
        elif self.opcode2 == 0x2C:
            self.ana2_2C()
        elif self.opcode2 == 0x2D:
            self.ana2_OVclamp()
        elif self.opcode2 == 0x2E:
            self.ana2_circular_mode()
        elif self.opcode2 != 0:
            #Unknown 2nd instruction
            self.insn[4].type = o_reg
            self.insn[5].type = o_reg
            self.insn[4].reg = self.get_register("unkn")
            self.insn[5].reg = self.get_register("unkn")
    
    def notify_ana(self, insn):
        logging.info("================= notify_ana =================")
        
        #1100207F
        wordstr = ida_bytes.get_bytes(insn.ea, 4)
        self.b4 = ord(wordstr[0]) #Still backwards, despite setting big endian
        self.b3 = ord(wordstr[1])
        self.b2 = ord(wordstr[2])
        self.b1 = ord(wordstr[3])
        instruction_word = self.b1 << 24 | self.b2 << 16 | self.b3 << 8 | self.b4
        logging.info("notify_ana instruction: " + hex(instruction_word, 8))
        
        self.insn = insn
        
        insn.size = 1 #fixed 4-byte (1 word) instruction size
        
        opcode1 = self.b1
        self.opcode2 = self.b2 & 0x3F #2 MSBs of byte 2 are args
        args1 = self.b2 >> 6 #Remove opcode2 bytes from self.b2
        
        logging.debug("notify_ana opcode1: " + hex(opcode1, 2))
        
        if opcode1 == 0x00:
            insn.itype = self.get_instruction("NOP")
        elif opcode1 >= 0x4 and opcode1 <= 0x7:
            self.ana_lacu()
        elif opcode1 >= 0x8 and opcode1 <= 0xB:
            self.ana_cpml()
        elif opcode1 >= 0x10 and opcode1 <= 0x13:
            self.ana_lac()
        elif opcode1 >= 0x14 and opcode1 <= 0x17:
            self.ana_inc()
        elif opcode1 >= 0x18 and opcode1 <= 0x1B:
            self.ana_dec()
        elif opcode1 == 0x1C:
            self.ana_shacc()
        elif opcode1 == 0x1D or opcode1 == 0x1F:
            self.ana_zacc()
        elif opcode1 >= 0x20 and opcode1 <= 0x23:
            self.ana_add()
        elif opcode1 >= 0x24 and opcode1 <= 0x27:
            self.ana_sub()
        elif opcode1 >= 0x28 and opcode1 <= 0x2B:
            self.ana_and()
        elif opcode1 >= 0x2C and opcode1 <= 0x2F:
            self.ana_or()
        elif opcode1 >= 0x30 and opcode1 <= 0x33:
            self.ana_xor()
        elif opcode1 >= 0x34 and opcode1 <= 0x37:
            self.ana_cmp()
        elif opcode1 == 0x39:
            self.ana_extern()
        elif opcode1 == 0x3C:
            self.ana_arith_dual("ADDD")
        elif opcode1 == 0x3D:
            if self.b2 & 0x80 == 0:
                self.ana_arith_dual("ANDD")
            else:
                self.ana_arith_dual("ORD")
        elif opcode1 == 0x3E:
            self.ana_arith_dual("XORD")
        elif opcode1 == 0x40 or opcode1 == 0x41:
            self.ana_mult_cmem("MPY(1)")
        elif opcode1 == 0x44 or opcode1 == 0x45:
            self.ana_mult_cmem("MPY(2)")
        elif opcode1 == 0x48 or opcode1 == 0x49:
            self.ana_mult_cmem("MPY(3)")
        elif opcode1 == 0x4C or opcode1 == 0x4D:
            self.ana_mult_cmem("MPY(4)")
        elif opcode1 == 0x42:
            self.ana_mult_dual("MPY(5)")
        elif opcode1 == 0x46:
            self.ana_mult_dual("MPY(6)")
        elif opcode1 == 0x4A:
            self.ana_mult_dual("MPY(7)")
        elif opcode1 == 0x4E:
            self.ana_mult_dual("MPY(8)")
        elif opcode1 == 0x43 or opcode1 == 0x47:
            self.ana_shm("SHM")
        elif opcode1 == 0x4B or opcode1 == 0x4F:
            self.ana_shm("SHMU")
        elif opcode1 == 0x50 or opcode1 == 0x51:
            self.ana_mult_cmem("MAC(1)")
        elif opcode1 == 0x52 or opcode1 == 0x53:
            self.ana_mult_dmem("MAC(2)")
        elif opcode1 == 0x5C or opcode1 == 0x5D:
            self.ana_mult_cmem("MAC(3)")
        elif opcode1 == 0x5E or opcode1 == 0x5F:
            self.ana_mult_dmem("MAC(4)")
        elif opcode1 == 0x6C:
            self.ana_mult_dual("MAC(5)")
        elif opcode1 == 0x6D:
            self.ana_mult_dual("MAC(6)")
        elif opcode1 == 0x6E:
            self.ana_mult_dual("MAC(7)")
        elif opcode1 == 0x6F:
            self.ana_mult_dual("MAC(8)")
        elif opcode1 == 0x72:
            self.ana_shmac()
        elif opcode1 == 0x73 and self.b2 < 0x80:
            self.ana_zmacc()
        elif opcode1 == 0x78 or opcode1 == 0x79:
            self.ana_loadmacc("LMH")
        elif opcode1 == 0x7A or opcode1 == 0x7B:
            self.ana_loadmacc("LMHC")
        elif opcode1 == 0x7C or opcode1 == 0x7D:
            self.ana_loadmacc("LML")
        elif opcode1 == 0xC1:
            self.ana_lri()
        elif opcode1 >= 0xC2 and opcode1 <= 0xC5:
            #load addressing registers with immediates
            self.ana_lri_dual()
        elif opcode1 >= 0xCA and opcode1 <= 0xCF:
            #Load register with immediate
            self.ana_lri_long()
        elif opcode1 == 0xE0 or opcode1 == 0xE4:
            self.ana_repeat()
        elif opcode1 == 0xEC:
            insn.itype = self.get_instruction("RET")
        elif opcode1 == 0xEE:
            insn.itype = self.get_instruction("RETI")
        elif opcode1 >= 0xF0:
            #jumps
            self.ana_jumps()
        else:
            logging.info("unknown instruction "  + hex(instruction_word, 8))
            #insn.size = 0
            #return 0
            insn.itype = self.get_instruction("UNKN")
            insn[0].type = o_imm
            insn[0].value = instruction_word
        
        #Analyse 2nd instruction
        if opcode1 >= 0 and opcode1 <= 0x7F: #Do not analyse single-instruction words
            self.ana2()
        
        # Return decoded instruction size or zero
        return insn.size
    
    def __init__(self):
        idaapi.processor_t.__init__(self)
        logging.info("module instantiated")
  
# ----------------------------------------------------------------------
# Every processor module script must provide this function.
# It should return a new instance of a class derived from idaapi.processor_t
def PROCESSOR_ENTRY():
    logging.info("PROCESSOR_ENTRY")
    return TMS57070_processor()