//
// Created by andre on 01/01/2026.
//

#pragma once
#include "../common/types.h"
#include "../common/utils.h"

namespace cvsim {
    inline word_t genImm(inst_t inst, InstrType type) {
        using namespace utils;
        switch (type) {
            case InstrType::R_TYPE: return 0;
            case InstrType::I_TYPE:
                return sign_extend(get_field(inst, 31, 20), 12);
            case InstrType::S_TYPE:
                return sign_extend((get_field(inst, 31, 25) << 5) | get_field(inst, 11, 7), 12);
            case InstrType::B_TYPE:
                return sign_extend((get_bit(inst, 31) << 12) | (get_bit(inst, 7) << 11) |
                                   (get_field(inst, 30, 25) << 5) | (get_field(inst, 11, 8) << 1), 13);
            case InstrType::U_TYPE:
                return inst & 0xFFFFF000u;
            case InstrType::J_TYPE:
                return sign_extend((get_bit(inst, 31) << 20) | (get_field(inst, 19, 12) << 12) |
                                   (get_bit(inst, 20) << 11) | (get_field(inst, 30, 21) << 1), 21);
        }
        return 0;
    }
}
