#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace csdecomp {

enum class OperandKind {
    None,
    Int8,
    Int16,
    Int32,
    Int64,
    Float32,
    Float64,
    Var,
    ShortVar,
    String,
    Method,
    Field,
    Type,
    Token,
    Switch,
};

struct OpcodeInfo {
    uint8_t byte1{0};
    uint8_t byte2{0};
    bool two_byte{false};
    std::string name;
    OperandKind operand{OperandKind::None};
    int8_t stack_push{0};
    int8_t stack_pop{0};
};

const OpcodeInfo& lookup_opcode(uint8_t b1, uint8_t b2 = 0);

}  // namespace csdecomp
