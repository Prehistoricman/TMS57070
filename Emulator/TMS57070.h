#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

#include "TMS57070_MAC.h"

#define TMSDEBUG 0
#if TMSDEBUG
#define tms_printf printf
#else
#define tms_printf(...)
#endif

constexpr uint32_t UINT24_MAX = 0xFFFFFF;
constexpr int32_t INT24_MAX = 0x7FFFFF;
constexpr int32_t INT24_MIN = -0x800000;

namespace TMS57070 {

    //Treat unknown instructions and behaviour as a fatal error
    //NDEBUG must not be defined for assert() to work
    constexpr bool UNKNOWN_STRICT = true;

    struct uint9_t {
        uint16_t value : 9;
    };

    struct uint12_t {
        uint16_t value : 12;
    };

    struct uint24_t {
        uint32_t value : 24;
    };

    struct int24_t {
        int32_t value : 24;
    };

    struct uint52_t {
        uint64_t value : 52;
    };

    union cr0_t {
        uint32_t value : 24;
        uint8_t bytes[3];
        struct {
            uint8_t byte0 : 8;
            uint8_t byte1 : 8;
            uint8_t byte2 : 8;
        };
    };

    union cr1_t {
        uint32_t value : 24;
        uint8_t bytes[3];
        struct {
            //byte 0
            uint8_t AOV : 1;
            uint8_t AOVL : 1;
            uint8_t ACCZ : 1;
            uint8_t ACCN : 1;
            uint8_t MOV : 1;
            uint8_t MOVL : 1;
            uint8_t MOVR : 1;
            uint8_t unk7 : 1;
            //byte 1
            uint8_t AOVM : 1;
            uint8_t unk3 : 1;
            uint8_t MACOV1 : 1;
            uint8_t unk4 : 1;
            uint8_t MOSM : 2; //5:4
            uint8_t MASM : 2; //7:6
            //byte 2
            uint8_t MOVM : 1;
            uint8_t unk0 : 1;
            uint8_t unk1 : 1;
            uint8_t EXTMEM : 1;
            uint8_t EXT : 1;
            uint8_t LDMEM : 1;
            uint8_t unk2 : 1;
            uint8_t LCMEM : 1;
        };
    };

    union cr2_t {
        uint32_t value : 24;
        uint8_t bytes[3];
        struct {
            //byte 0 - flags - interrupt is enabled if zero
            uint8_t ARI1_IF : 1;
            uint8_t ARI1A_IF : 1;
            uint8_t HIR_IF : 1;
            uint8_t INT1_IF : 1;
            uint8_t ARI2_IF : 1;
            uint8_t ARI2A_IF : 1;
            uint8_t INT2_IF : 1;
            uint8_t INT3_IF : 1;
            //byte 1 - enables
            uint8_t ARI1_IE : 1;
            uint8_t ARI1A_IE : 1;
            uint8_t HIR_IE : 1;
            uint8_t INT1_IE : 1;
            uint8_t ARI2_IE : 1;
            uint8_t ARI2A_IE : 1;
            uint8_t INT2_IE : 1;
            uint8_t INT3_IE : 1;
            //byte 2
            uint8_t unk0 : 1;
            uint8_t FREE : 1;
            uint8_t VOL : 1;
            uint8_t unk1 : 1;
            uint8_t XMEMsomething : 1;
            uint8_t unk2 : 1;
            uint8_t LVOL : 1;
            uint8_t unk3 : 1;
        };
    };

    union cr3_t {
        uint32_t value : 24;
        uint8_t bytes[3];
        struct {
            //byte 0
            uint8_t byte0 : 8;
            //byte 1
            uint8_t byte1 : 8;
            //byte 2
            uint8_t XWORD : 1;
            uint8_t XBUS : 2;
            uint8_t unk0 : 1;
            uint8_t unk1 : 1;
            uint8_t unk2 : 1;
            uint8_t unk3 : 1;
            uint8_t unk4 : 1;
            uint8_t unk5 : 1;
        };
    };

    //For CA, DIR, etc. registers
    union addr_reg_t {
        struct {
            uint12_t one;
            uint12_t two;
        };
    };

    enum class Channel {
        in_1L,
        in_1R,
        in_2L,
        in_2R,

        out_1L,
        out_1R,
        out_2L,
        out_2R,
        out_3L,
        out_3R,
    };

    using sample_out_callback_t = void(Channel channel, int32_t value);

    enum class ArithOperation {
        Add,
        Sub,
        And,
        Or,
        Xor,
        Cmp, //compare: subtract without changing value
    };

    struct interrupt_vector_t {
        uint9_t PC;
        uint8_t flag; //Flag of the specified interrupt
    };

    class Emulator {
    public:
        void reset();
        void step();
        void sample_in(Channel channel, int32_t value);
        void register_sample_out_callback(sample_out_callback_t cb);
        void ext_interrupt(uint8_t interrupt);
        void hir_interrupt(uint24_t input);
        uint32_t hir_out();
        std::string reportState();

    private:
        void exec1st();
        void exec2nd();
        void execJmp();
        void execPostIncrements();
        interrupt_vector_t int_vector_decode(uint8_t);
        uint32_t cmemAddressing();
        uint32_t dmemAddressing();
        uint32_t xmemAddressing(uint32_t addr);
        int24_t* loadACC();
        int24_t* arith(ArithOperation operation);
        int32_t processACCValue(int32_t acc);

    public:
        uint32_t PMEM[512];
        int24_t CMEM[512];
        int24_t DMEM[512];
        int24_t XMEM[0xFFFFFF]; //This is the largest possible XMEM configuration

        cr0_t CR0;
        cr1_t CR1;
        cr2_t CR2;
        cr3_t CR3;
        uint9_t PC;

    private: //Registers
        uint8_t SP;

        //9-bit
        uint9_t stack[4];
        uint9_t rep_start_PC;
        uint9_t rep_end_PC;
        uint8_t RPTC; //Number of repeats remaining

        //52-bit
        MAC MACC1{ this };
        MAC MACC2{ this };

        //24-bit
        int24_t ACC1;
        int24_t ACC2;
        uint24_t HIR;
        int24_t XRD;

        int24_t AR1L;
        int24_t AR1R;
        int24_t AR2L;
        int24_t AR2R;

        int24_t AX1L;
        int24_t AX1R;
        int24_t AX2L;
        int24_t AX2R;
        int24_t AX3L;
        int24_t AX3R;

        addr_reg_t CA;
        addr_reg_t DA;
        addr_reg_t CIR;
        addr_reg_t DIR;

        uint12_t COFF; //Current CMEM offset
        uint12_t CCIRC; //CMEM circular region end address

        uint12_t DOFF; //Current DMEM offset
        uint12_t DCIRC; //DMEM circular region end address

        uint16_t XOFF; //Current XMEM offset

    private: //Non-register variables
        uint32_t insn; //Current instruction
        sample_out_callback_t* sample_out_cb = nullptr;

        uint8_t opcode1;
        bool opcode1_flag4;
        bool opcode1_flag8;
        uint8_t opcode2;
        bool opcode2_flag4;
        bool opcode2_flag8;
        uint8_t opcode2_args;

        //MACC values delayed by 1 cycle
        MAC MACC1_delayed1{ this };
        MAC MACC2_delayed1{ this };
        //MACC values delayed by 2 cycles
        MAC MACC1_delayed2{ this };
        MAC MACC2_delayed2{ this };

        uint32_t XMEM_read_addr;
        uint32_t XMEM_read_cycles;
    };

}