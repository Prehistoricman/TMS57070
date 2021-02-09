#pragma once
#include <cstdint>
#include <cstdio>

#define TMSDEBUG 1
#if TMSDEBUG
#define tms_printf printf
#else
#define tms_printf(...)
#endif

namespace TMS57070 {

    typedef struct {
        uint16_t value : 9;
    } uint9_t;

    typedef struct {
        uint16_t value : 12;
    } uint12_t;

    typedef struct {
        uint32_t value : 24;
    } uint24_t;

    typedef struct {
        uint64_t value : 52;
    } uint52_t;

    typedef union {
        uint32_t value : 24;
        uint8_t bytes[3];
        struct {
            uint8_t byte0 : 8;
            uint8_t byte1 : 8;
            uint8_t byte2 : 8;
        };
    } cr0_t;

    typedef union {
        uint32_t value : 24;
        uint8_t bytes[3];
        struct {
            //byte 0
            uint8_t AOV : 1;
            uint8_t ACCsomething : 1;
            uint8_t ACCZ : 1;
            uint8_t MACZ : 1;
            uint8_t MACOV2 : 1;
            uint8_t MACOV3 : 1;
            uint8_t MACOV4 : 1;
            uint8_t unk7 : 1;
            //byte 1
            uint8_t AOVM : 1;
            uint8_t unk3 : 1;
            uint8_t MACOV1 : 1;
            uint8_t unk4 : 1;
            uint8_t MOSM : 2; //5:4
            uint8_t unk5 : 1;
            uint8_t unk6 : 1;
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
    } cr1_t;

    typedef union {
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
    } cr2_t;

    typedef union {
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
    } cr3_t;

    //For CA, DIR, etc. registers
    typedef union {
        uint32_t value : 24;
        struct {
            uint12_t one;
            uint12_t two;
        };
    } addr_reg_t;

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

    enum class MACBits {
        Upper,
        Lower,
    };

    enum class ArithOperation {
        Add,
        Sub,
        And,
        Or,
        Xor,
        Cmp, //compare: subtract without changing value
    };

    typedef struct {
        uint9_t PC;
        uint8_t flag; //Flag of the specified interrupt
    } interrupt_vector_t;

    class Emulator {
    public:
        void reset();
        void step();
        void sample_in(Channel channel, int32_t value);
        void register_sample_out_callback(sample_out_callback_t cb);
        void ext_interrupt(uint8_t interrupt);
        void hir_interrupt(uint24_t input);
        uint32_t hir_out();

    private:
        void exec1st();
        void exec2nd();
        interrupt_vector_t int_vector_decode(uint8_t);
        uint32_t getMAC(uint8_t mac, MACBits bits);
        uint32_t cmemAddressing();
        uint32_t dmemAddressing();
        uint24_t* loadACC();
        uint24_t* arith(ArithOperation operation);

    public:
        uint32_t PMEM[512];
        uint32_t CMEM[512];
        uint32_t DMEM[512];
        uint32_t XMEM[512]; //TODO: How big will this need to be?

    private: //Registers
        uint8_t SP;

        //9-bit
        uint9_t PC;
        uint9_t stack[4];
        uint9_t rep_start_PC;
        uint9_t rep_end_PC;
        uint8_t RPTC; //Number of repeats remaining

        //52-bit
        uint52_t MACC1;
        uint52_t MACC2;

        //24-bit
        uint24_t ACC1;
        uint24_t ACC2;
        uint24_t HIR;
        uint24_t XRD;

        uint24_t AR1L;
        uint24_t AR1R;
        uint24_t AR2L;
        uint24_t AR2R;

        uint24_t AX1L;
        uint24_t AX1R;
        uint24_t AX2L;
        uint24_t AX2R;
        uint24_t AX3L;
        uint24_t AX3R;

        cr0_t CR0;
        cr1_t CR1;
        cr2_t CR2;
        cr3_t CR3;

        addr_reg_t CA;
        addr_reg_t DA;
        addr_reg_t CIR;
        addr_reg_t DIR;

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
        uint52_t MACC1_delayed1;
        uint52_t MACC2_delayed1;
        //MACC values delayed by 2 cycles
        uint52_t MACC1_delayed2;
        uint52_t MACC2_delayed2;
    };

}