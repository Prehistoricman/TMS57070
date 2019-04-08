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
class TMS57070(idaapi.processor_t):
    
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
        "name": "??? assembler",
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
        "HIR", #Host interface register
        "CIR", #Incrementing registers
        "CIR1",
        "CIR2",
        "DIR",
        "DIR1",
        "DIR2",
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
        {'name': 'ZACC',   'feature': CF_USE1,           'cmt': "Zero ACC"}, #1D
        {'name': 'ADD',    'feature': CF_USE1 | CF_USE2, 'cmt': "Add MEM to ACC or MACC, store result in ACC"}, #20-23
        {'name': 'SUB',    'feature': CF_USE1 | CF_USE2, 'cmt': "Subtract ACC or MACC from MEM, store result in ACC"}, #24-27
        #{'name': 'ZACC',   'feature': CF_USE1 | CF_USE2, 'cmt': "Zero accumulator if CMEM less than accumulator"}, #2A
        {'name': 'AND',    'feature': CF_USE1 | CF_USE2, 'cmt': "Bitwise logical AND accumulator with MACC"}, #2B
        {'name': 'XOR',    'feature': CF_USE1 | CF_USE2, 'cmt': "Bitwise logical XOR accumulator with CMEM"}, #32
        {'name': 'RDE',    'feature': CF_USE1,           'cmt': "Read from external memory at address from CMEM"}, #39
        {'name': 'WRE',    'feature': CF_USE1,           'cmt': "Write to external memory at address from CMEM and data from DMEM"}, #39
        {'name': 'MPY(1)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM with ACC"}, #40, 41
        {'name': 'MPYU(1)','feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by ACC"}, #44, 45
        {'name': 'MPYU(2)','feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM by unsigned ACC"}, #48, 49
        {'name': 'MPYU(3)','feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by unsigned ACC"}, #4C, 4D
        {'name': 'MPY(2)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM with DMEM"}, #42
        {'name': 'MPYU(4)','feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by DMEM"}, #46
        {'name': 'MPYU(5)','feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM by unsigned DMEM"}, #4A
        {'name': 'MPYU(6)','feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by unsigned DMEM"}, #4E
        {'name': 'SHM(1)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Load DMEM into MACC and divide by 2"}, #43, 47
        {'name': 'SHM(2)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Load unsigned DMEM into MACC and divide by 2"}, #4B, 4F
        {'name': 'MAC(1)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM by ACC, accumulate into MACC"}, #50, 51
        {'name': 'MAC(2)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply DMEM by ACC, accumulate into MACC"}, #52, 53
        {'name': 'MACU(1)','feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by ACC, accumulate into MACC"}, #5C, 5D
        {'name': 'MACU(2)','feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned DMEM by ACC, accumulate into MACC"}, #5E, 5F
        {'name': 'MAC(3)', 'feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM by DMEM, accumulate into MACC"}, #6C
        {'name': 'MACU(3)','feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by DMEM, accumulate into MACC"}, #6D
        {'name': 'MACU(4)','feature': CF_USE1 | CF_USE2, 'cmt': "Multiply CMEM by unsigned DMEM, accumulate into MACC"}, #6E
        {'name': 'MACU(5)','feature': CF_USE1 | CF_USE2, 'cmt': "Multiply unsigned CMEM by unsigned DMEM, accumulate into MACC"}, #6F
        {'name': 'SHMAC',  'feature': CF_USE1 | CF_SHFT, 'cmt': "Shift MACC once left or right"}, #72
        {'name': 'ZMACC',  'feature': CF_USE1 | CF_USE2, 'cmt': "Zero both MACCs"}, #74
        {'name': 'LMH',    'feature': CF_USE1 | CF_USE2, 'cmt': "Load high MACC with MEM or ACC"}, #78, 79
        {'name': 'LMHC',   'feature': CF_USE1 | CF_USE2, 'cmt': "Load high MACC with MEM or ACC and clear low MACC"}, #7A, 7B
        {'name': 'LML',    'feature': CF_USE1 | CF_USE2, 'cmt': "Load low MACC with MEM or ACC"}, #7C, 7D
        {'name': 'LRI',    'feature': CF_USE1 | CF_USE2, 'cmt': "Load immediate into register"}, #Cx
        {'name': 'LRIAE',  'feature': CF_USE1 | CF_USE2, 'cmt': "Load immediate into register if accumulator greater than or equal to zero"}, #C14
        {'name': 'RPTK',   'feature': CF_USE1 | CF_JUMP, 'cmt': "Repeat next instruction"}, #E0
        {'name': 'RPTB',   'feature': CF_USE1 | CF_JUMP, 'cmt': "Repeat next block"}, #E4
        {'name': 'RET',    'feature': CF_STOP,           'cmt': "Return"}, #EC
        {'name': 'RETI',   'feature': CF_STOP,           'cmt': "Return from interrupt"}, #EE
        {'name': 'JMP',    'feature': CF_JUMP,           'cmt': "Jump unconditionally"}, #F0
        {'name': 'JZ',     'feature': CF_JUMP,           'cmt': "Jump if zero"}, #F10
        {'name': 'JNZ',    'feature': CF_JUMP,           'cmt': "Jump if not zero"}, #F18
        {'name': 'JGZ',    'feature': CF_JUMP,           'cmt': "Jump if greater than zero"}, #F20
        {'name': 'JLZ',    'feature': CF_JUMP,           'cmt': "Jump if less than zero"}, #F28
        {'name': 'JOV',    'feature': CF_JUMP,           'cmt': "Jump if accumulator overflow"}, #F30
        {'name': 'CALL',   'feature': CF_CALL,           'cmt': "Call unconditionally"}, #F8
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
        logging.debug("notify_init big endian: " + str(ida_ida.cvar.inf.is_wide_high_byte_first()))
        return True
    
    def notify_newfile(self, filename):
        #Create PMEM, CMEM and DMEM segments
        
        PMEM = segment_t()
        PMEM.startEA = 0
        PMEM.endEA = 0x200
        PMEM.bitness = 0 #16-bit addressing
        PMEM.type = SEG_CODE
        #PMEM.sel = find_free_selector() #???
        
        idaapi.add_segm_ex(PMEM, "PMEM", "CODE", ADDSEG_OR_DIE)
        
        CMEM = segment_t()
        CMEM.startEA = 0x200
        CMEM.endEA = 0x400
        CMEM.bitness = 0 #16-bit addressing
        CMEM.type = SEG_DATA | SEG_XTRN
        CMEM.sel = allocate_selector(CMEM.startEA >> 4) #???
        
        idaapi.add_segm_ex(CMEM, "CMEM", "DATA", ADDSEG_OR_DIE)
        
        DMEM = segment_t()
        DMEM.startEA = 0x400
        DMEM.endEA = 0x600
        DMEM.bitness = 0 #16-bit addressing
        DMEM.type = SEG_DATA | SEG_XTRN
        DMEM.sel = allocate_selector(DMEM.startEA >> 4) #???
        
        idaapi.add_segm_ex(DMEM, "DMEM", "DATA", ADDSEG_OR_DIE)
    
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
        jmps = ["JMP", "JZ", "JNZ", "JGZ", "JLZ"]
        if mnemonic in jmps:
            if insn[0].addr != 0x0:
                add_cref(insn.ea, insn[0].addr, fl_JN)
        
        #is it a call?
        calls = ["CALL"]
        if mnemonic in calls:
            if insn[0].addr != 0x0:
                add_cref(insn.ea, insn[0].addr, fl_CN)
        
        #is it a repeat?
        reps = ["RPTB", "RPTK"]
        if mnemonic in reps:
            if insn[1].addr != 0x0:
                #Add flow from end of loop to next instruction
                add_cref(insn[1].addr, insn.ea + insn.size, fl_JN)
        
        #Does the processor go to the next instruction?
        flow = (feature & CF_STOP == 0) and not uncond_jmp
        if flow:
            add_cref(insn.ea, insn.ea + insn.size, fl_F)
        
        #add data xref
        if insn[0].specflag2 == 1:
            pass#ua_add_dref(0, insn[0].addr, dr_W)
        if insn[1].specflag2 == 1:
            pass#ua_add_dref(0, insn[1].addr, dr_W)
        
        #Add data reference
        #TODO read/write flag
        for i in range(3):
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
            if (ctx.insn.get_canon_mnem() in ["SHACC", "SHMAC"]):
                ctx.out_register(self.regNames[op.reg])
                ctx.out_char(" ")
                if (op.specval == 1):
                    ctx.out_symbol("<")
                    ctx.out_symbol("<")
                elif (op.specval == 2):
                    ctx.out_symbol(">")
                    ctx.out_symbol(">")
                else:
                    logger.error("notify_out_operand No specval for SHACC")
                ctx.out_char(" ")
                ctx.out_symbol("1")
                
            #All other instructions
            else:
                if (op.specflag1 == 1):
                    ctx.out_symbol("*") #pointer symbol
                if (op.specflag3 == 1):
                    ctx.out_symbol("-") #minus sign
                ctx.out_register(self.regNames[op.reg])
                if (op.specflag2 == 1):
                    ctx.out_symbol("+") #post-increment symbol
            
        elif optype == o_imm:
            ctx.out_symbol("#")
            ctx.out_value(op, OOFW_IMM) #Hack: OOFW_IMM does not work
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
            if (op.specval == 1):
                ctx.out_printf("DMEM")
            elif (op.specval == 2):
                ctx.out_printf("CMEM")
            else:
                print("Wrong specval for memory region in operand")
            
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
            print("op type " + optype + " failed in outop")
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
        for i in xrange(1, 3):
            if insn[i].type == o_void:
                break
            ctx.out_symbol(",")
            ctx.out_char(" ")
            ctx.out_one_operand(i)
        
        #Check for 2nd instruction
        #output MOV and the rest of operands
        
        #ctx.set_gen_cmt() #Does nothing??
        ctx.flush_outbuf()
    
    def notify_get_autocmt(self, insn):
        pass
    
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
        return -1
    
    
    def ana_jumps(self):
        logging.info("ana_jumps")
        self.insn[0].type = o_near #All jumps are intra-segment
        self.insn[0].addr = self.b4 + (self.b3 << 8) & 0x1FF #immediate jump location
        
        logging.debug("ana_jumps address: 0x%0.3X" % self.insn[0].addr)
        
        #condition = lower 4 bits of self.b1 and upper 4 of self.b2
        condition = (self.b1 & 0xF) << 4 | self.b2 >> 4
        
        if condition == 0x00: #unconditional
            self.insn.itype = self.get_instruction("JMP")
        elif condition == 0x10: #jump if zero
            self.insn.itype = self.get_instruction("JZ")
        elif condition == 0x18: #jump if not zero
            self.insn.itype = self.get_instruction("JNZ")
        elif condition == 0x20: #jump if greater than zero
            self.insn.itype = self.get_instruction("JGZ")
        elif condition == 0x28: #jump if less than zero
            self.insn.itype = self.get_instruction("JLZ")
        elif condition == 0x30: #jump if acc overflow
            self.insn.itype = self.get_instruction("JOV")
        elif condition == 0x80: #call
            self.insn.itype = self.get_instruction("CALL")
            
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
            logging.error("ana_lri wrong operand to LADI: " + hex(self.b2))
            
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
            
            #post-increment flag
            self.insn[operand].specflag2 = self.b3 & 4 > 0 and 1 or 0
        elif (arg == 0x10):
            self.insn[operand].type = o_mem
            self.insn[operand].addr = 0x000 + (self.b3 & 1 | self.b4) #0x400
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
            
            #post-increment flag
            self.insn[operand].specflag2 = self.b4 & 0x40 > 0 and 1 or 0
        elif (arg == 0x10):
            self.insn[operand].type = o_reg
            self.insn[operand].specflag1 = 1 #Output *
            if (self.b3 & 8 > 0):
                self.insn[operand].reg = self.get_register("CA2")
            else:
                self.insn[operand].reg = self.get_register("CA1")
            
            #post-increment flag
            self.insn[operand].specflag2 = self.b3 & 2 > 0 and 1 or 0
        elif (arg == 0x20):
            self.insn[operand].type = o_mem
            self.insn[operand].addr = 0x000 + (self.b3 & 1 | self.b4) #0x200
            self.insn[operand].specval = 2 #CMEM
            
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
            self.insn[1].addr = self.b3 & 1 | self.b4
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
        if (self.b2 & 0x40 > 0):
            self.insn[0].reg = self.get_register("ACC2")
        else:
            self.insn[0].reg = self.get_register("ACC1")
    
    def ana_zmacc(self):
        logging.info("ana_zmacc")
        self.insn.itype = self.get_instruction("ZMACC")
        
        self.insn[0].type = o_reg
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
    
    def ana_add(self):
        logging.info("ana_add")
        self.insn.itype = self.get_instruction("ADD")
        self.ana_arith()
    
    def ana_sub(self):
        logging.info("ana_sub")
        self.insn.itype = self.get_instruction("SUB")
        self.ana_arith()
    
    def ana_mult_single(self):
        self.insn[1].type = o_reg
        if (self.b1 & 0x01 > 0):
            self.insn[1].reg = self.get_register("ACC2")
        else:
            self.insn[1].reg = self.get_register("ACC1")
        if (self.b2 & 0x80 > 0):
            self.insn[1].specflag3 = 1 #Disply minus sign
        
        self.insn[2].type = o_reg
        if (self.b2 & 0x40 > 0):
            self.insn[2].reg = self.get_register("MACC2")
        else:
            self.insn[2].reg = self.get_register("MACC1")
    
    def ana_mult_cmem(self, opcode):
        logging.info("ana_mult_cmem")
        self.insn.itype = self.get_instruction(opcode)
        
        self.ana_cmem_addressing(0)
        self.ana_mult_single()
        
    def ana_mult_dmem(self, opcode):
        logging.info("ana_mult_dmem")
        self.insn.itype = self.get_instruction(opcode)
        
        self.ana_dmem_addressing(0)
        self.ana_mult_single()
    
    def ana_mult_dual(self, opcode):
        logging.info("ana_mult_dual")
        self.insn.itype = self.get_instruction(opcode)
        
        #self.ana_dual_addressing(0, 1)
        self.ana_cmem_addressing(0)
        self.ana_dmem_addressing(1)
        
        self.insn[2].type = o_reg
        if (self.b2 & 0x40 > 0):
            self.insn[2].reg = self.get_register("MACC2")
        else:
            self.insn[2].reg = self.get_register("MACC1")
    
    def ana_shm(self, opcode):
        logging.info("ana_shm")
        self.insn.itype = self.get_instruction(opcode)
        
        self.ana_dmem_addressing(0)
        
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
        else:
            self.insn.itype = self.get_instruction("RDE")
            self.ana_cmem_addressing(0)
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
        opcode2 = self.b2 & 0x3F #2 MSBs of byte 2 are args
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
        elif opcode1 == 0x1D:
            self.ana_zacc()
        elif opcode1 >= 0x20 and opcode1 <= 0x23:
            self.ana_add()
        elif opcode1 >= 0x24 and opcode1 <= 0x27:
            self.ana_sub()
        #elif opcode1 == 0x2A:
        #    
        #elif opcode1 == 0x2B:
        #    
        #elif opcode1 == 0x32:
        #    
        elif opcode1 == 0x39:
            self.ana_extern()
        elif opcode1 == 0x40 or opcode1 == 0x41:
            self.ana_mult_cmem("MPY(1)")
        elif opcode1 == 0x44 or opcode1 == 0x45:
            self.ana_mult_cmem("MPYU(1)")
        elif opcode1 == 0x48 or opcode1 == 0x49:
            self.ana_mult_cmem("MPYU(2)")
        elif opcode1 == 0x4C or opcode1 == 0x4D:
            self.ana_mult_cmem("MPYU(3)")
        elif opcode1 == 0x42:
            self.ana_mult_dual("MPY(2)")
        elif opcode1 == 0x46:
            self.ana_mult_dual("MPYU(4)")
        elif opcode1 == 0x4A:
            self.ana_mult_dual("MPYU(5)")
        elif opcode1 == 0x4E:
            self.ana_mult_dual("MPYU(6)")
        elif opcode1 == 0x43 or opcode1 == 0x47:
            self.ana_shm("SHM(1)")
        elif opcode1 == 0x4B or opcode1 == 0x4F:
            self.ana_shm("SHM(2)")
        elif opcode1 == 0x50 or opcode1 == 0x51:
            self.ana_mult_cmem("MAC(1)")
        elif opcode1 == 0x52 or opcode1 == 0x53:
            self.ana_mult_dmem("MAC(2)")
        elif opcode1 == 0x5C or opcode1 == 0x5D:
            self.ana_mult_cmem("MACU(1)")
        elif opcode1 == 0x5E or opcode1 == 0x5F:
            self.ana_mult_dmem("MACU(2)")
        elif opcode1 == 0x6C:
            self.ana_mult_dual("MAC(3)")
        elif opcode1 == 0x6D:
            self.ana_mult_dual("MACU(3)")
        elif opcode1 == 0x6E:
            self.ana_mult_dual("MACU(4)")
        elif opcode1 == 0x6F:
            self.ana_mult_dual("MACU(5)")
        elif opcode1 == 0x72:
            self.ana_shmac()
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
            #logging.info("unknown instruction")
            #insn.size = 0
            #return 0
            insn.itype = self.get_instruction("UNKN")
            insn[0].type = o_imm
            insn[0].value = instruction_word
        
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
    return TMS57070()