//
// Created by andre on 29/12/2025.
//

#pragma once
#include "../common/types.h"

namespace cvsim {
    struct IF_ID_Reg {
        addr_t pc = 0;
        inst_t instruction = 0;
        bool valid = false;
    };

    struct ID_EX_Reg {
        addr_t pc = 0;
        inst_t instruction = 0;
        word_t rs1_val = 0;
        word_t rs2_val = 0;
        word_t imm = 0;
        reg_addr_t rs1 = 0;
        reg_addr_t rs2 = 0;
        reg_addr_t rd = 0;
        Opcode opcode = Opcode::ALUI;
        InstrType instr_type = InstrType::I_TYPE;
        bool uses_rs1 = false;
        bool uses_rs2 = false;
        bool regWrite = false;
        bool memRead = false;
        bool memWrite = false;
        bool valid = false;
    };

    struct EX_MEM_Reg {
        addr_t pc = 0;
        inst_t instruction = 0;
        word_t alu_result = 0;
        word_t rs2_val = 0;
        reg_addr_t rd = 0;
        uint8_t funct3 = 0;
        bool regWrite = false;
        bool memRead = false;
        bool memWrite = false;
        bool valid = false;
    };

    struct MEM_WB_Reg {
        addr_t pc = 0;
        inst_t instruction = 0;
        word_t result = 0;
        reg_addr_t rd = 0;
        bool regWrite = false;
        bool valid = false;
    };
}
