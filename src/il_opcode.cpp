#include "csdecomp/il_opcode.hpp"

#include <unordered_map>

namespace csdecomp {

namespace {

struct OpcodeKey {
    uint8_t b1;
    uint8_t b2;

    bool operator==(const OpcodeKey& other) const { return b1 == other.b1 && b2 == other.b2; }
};

struct OpcodeKeyHash {
    size_t operator()(const OpcodeKey& key) const {
        return (static_cast<size_t>(key.b1) << 8) | key.b2;
    }
};

const std::unordered_map<OpcodeKey, OpcodeInfo, OpcodeKeyHash>& opcode_table() {
    static const std::unordered_map<OpcodeKey, OpcodeInfo, OpcodeKeyHash> table = [] {
        std::unordered_map<OpcodeKey, OpcodeInfo, OpcodeKeyHash> map;
        auto add = [&](uint8_t b1, uint8_t b2, const char* name, OperandKind op, int8_t push,
                        int8_t pop) {
            map[{b1, b2}] = OpcodeInfo{b1, b2, b2 != 0, name, op, push, pop};
        };

        add(0x00, 0, "nop", OperandKind::None, 0, 0);
        add(0x01, 0, "break", OperandKind::None, 0, 0);
        add(0x02, 0, "ldarg.0", OperandKind::None, 1, 0);
        add(0x03, 0, "ldarg.1", OperandKind::None, 1, 0);
        add(0x04, 0, "ldarg.2", OperandKind::None, 1, 0);
        add(0x05, 0, "ldarg.3", OperandKind::None, 1, 0);
        add(0x06, 0, "ldloc.0", OperandKind::None, 1, 0);
        add(0x07, 0, "ldloc.1", OperandKind::None, 1, 0);
        add(0x08, 0, "ldloc.2", OperandKind::None, 1, 0);
        add(0x09, 0, "ldloc.3", OperandKind::None, 1, 0);
        add(0x0A, 0, "stloc.0", OperandKind::None, 0, 1);
        add(0x0B, 0, "stloc.1", OperandKind::None, 0, 1);
        add(0x0C, 0, "stloc.2", OperandKind::None, 0, 1);
        add(0x0D, 0, "stloc.3", OperandKind::None, 0, 1);
        add(0x0E, 0, "ldarg.s", OperandKind::ShortVar, 1, 0);
        add(0x0F, 0, "ldarga.s", OperandKind::ShortVar, 1, 0);
        add(0x10, 0, "starg.s", OperandKind::ShortVar, 0, 1);
        add(0x11, 0, "ldloc.s", OperandKind::ShortVar, 1, 0);
        add(0x12, 0, "ldloca.s", OperandKind::ShortVar, 1, 0);
        add(0x13, 0, "stloc.s", OperandKind::ShortVar, 0, 1);
        add(0x14, 0, "ldnull", OperandKind::None, 1, 0);
        add(0x15, 0, "ldc.i4.m1", OperandKind::None, 1, 0);
        add(0x16, 0, "ldc.i4.0", OperandKind::None, 1, 0);
        add(0x17, 0, "ldc.i4.1", OperandKind::None, 1, 0);
        add(0x18, 0, "ldc.i4.2", OperandKind::None, 1, 0);
        add(0x19, 0, "ldc.i4.3", OperandKind::None, 1, 0);
        add(0x1A, 0, "ldc.i4.4", OperandKind::None, 1, 0);
        add(0x1B, 0, "ldc.i4.5", OperandKind::None, 1, 0);
        add(0x1C, 0, "ldc.i4.6", OperandKind::None, 1, 0);
        add(0x1D, 0, "ldc.i4.7", OperandKind::None, 1, 0);
        add(0x1E, 0, "ldc.i4.8", OperandKind::None, 1, 0);
        add(0x1F, 0, "ldc.i4.s", OperandKind::Int8, 1, 0);
        add(0x20, 0, "ldc.i4", OperandKind::Int32, 1, 0);
        add(0x21, 0, "ldc.i8", OperandKind::Int64, 1, 0);
        add(0x22, 0, "ldc.r4", OperandKind::Float32, 1, 0);
        add(0x23, 0, "ldc.r8", OperandKind::Float64, 1, 0);
        add(0x25, 0, "dup", OperandKind::None, 1, 0);
        add(0x26, 0, "pop", OperandKind::None, 0, 1);
        add(0x28, 0, "call", OperandKind::Method, 1, -1);
        add(0x29, 0, "calli", OperandKind::Token, 1, -1);
        add(0x2A, 0, "ret", OperandKind::None, 0, -1);
        add(0x2B, 0, "br.s", OperandKind::Int8, 0, 0);
        add(0x2C, 0, "brfalse.s", OperandKind::Int8, 0, 1);
        add(0x2D, 0, "brtrue.s", OperandKind::Int8, 0, 1);
        add(0x2E, 0, "beq.s", OperandKind::Int8, 0, 2);
        add(0x2F, 0, "bge.s", OperandKind::Int8, 0, 2);
        add(0x30, 0, "bgt.s", OperandKind::Int8, 0, 2);
        add(0x31, 0, "ble.s", OperandKind::Int8, 0, 2);
        add(0x32, 0, "blt.s", OperandKind::Int8, 0, 2);
        add(0x33, 0, "bne.un.s", OperandKind::Int8, 0, 2);
        add(0x38, 0, "br", OperandKind::Int32, 0, 0);
        add(0x39, 0, "brfalse", OperandKind::Int32, 0, 1);
        add(0x3A, 0, "brtrue", OperandKind::Int32, 0, 1);
        add(0x3B, 0, "beq", OperandKind::Int32, 0, 2);
        add(0x46, 0, "ldind.i1", OperandKind::None, 1, 1);
        add(0x47, 0, "ldind.u1", OperandKind::None, 1, 1);
        add(0x58, 0, "add", OperandKind::None, 1, 2);
        add(0x59, 0, "sub", OperandKind::None, 1, 2);
        add(0x5A, 0, "mul", OperandKind::None, 1, 2);
        add(0x5B, 0, "div", OperandKind::None, 1, 2);
        add(0x5C, 0, "div.un", OperandKind::None, 1, 2);
        add(0x5D, 0, "rem", OperandKind::None, 1, 2);
        add(0x5E, 0, "rem.un", OperandKind::None, 1, 2);
        add(0x5F, 0, "and", OperandKind::None, 1, 2);
        add(0x60, 0, "or", OperandKind::None, 1, 2);
        add(0x61, 0, "xor", OperandKind::None, 1, 2);
        add(0x62, 0, "shl", OperandKind::None, 1, 2);
        add(0x63, 0, "shr", OperandKind::None, 1, 2);
        add(0x64, 0, "shr.un", OperandKind::None, 1, 2);
        add(0x65, 0, "neg", OperandKind::None, 1, 1);
        add(0x66, 0, "not", OperandKind::None, 1, 1);
        add(0x67, 0, "conv.i1", OperandKind::None, 1, 1);
        add(0x68, 0, "conv.i2", OperandKind::None, 1, 1);
        add(0x69, 0, "conv.i4", OperandKind::None, 1, 1);
        add(0x6A, 0, "conv.i8", OperandKind::None, 1, 1);
        add(0x6B, 0, "conv.r4", OperandKind::None, 1, 1);
        add(0x6C, 0, "conv.r8", OperandKind::None, 1, 1);
        add(0x6D, 0, "conv.u4", OperandKind::None, 1, 1);
        add(0x6E, 0, "conv.u8", OperandKind::None, 1, 1);
        add(0x6F, 0, "callvirt", OperandKind::Method, 1, -1);
        add(0x70, 0, "cpobj", OperandKind::Type, 0, 2);
        add(0x71, 0, "ldobj", OperandKind::Type, 1, 1);
        add(0x72, 0, "ldstr", OperandKind::String, 1, 0);
        add(0x73, 0, "newobj", OperandKind::Method, 1, -1);
        add(0x74, 0, "castclass", OperandKind::Type, 1, 1);
        add(0x75, 0, "isinst", OperandKind::Type, 1, 1);
        add(0x76, 0, "conv.r.un", OperandKind::None, 1, 1);
        add(0x7B, 0, "ldfld", OperandKind::Field, 1, 1);
        add(0x7C, 0, "ldflda", OperandKind::Field, 1, 1);
        add(0x7D, 0, "stfld", OperandKind::Field, 0, 2);
        add(0x7E, 0, "ldsfld", OperandKind::Field, 1, 0);
        add(0x7F, 0, "ldsflda", OperandKind::Field, 1, 0);
        add(0x80, 0, "stsfld", OperandKind::Field, 0, 1);
        add(0x8C, 0, "box", OperandKind::Type, 1, 1);
        add(0x8D, 0, "newarr", OperandKind::Type, 1, 1);
        add(0x8E, 0, "ldlen", OperandKind::None, 1, 1);
        add(0x8F, 0, "ldelema", OperandKind::Type, 1, 2);
        add(0x9A, 0, "ldelem.i1", OperandKind::None, 1, 2);
        add(0x9B, 0, "ldelem.u1", OperandKind::None, 1, 2);
        add(0x9C, 0, "ldelem.i2", OperandKind::None, 1, 2);
        add(0x9D, 0, "ldelem.u2", OperandKind::None, 1, 2);
        add(0x9E, 0, "ldelem.i4", OperandKind::None, 1, 2);
        add(0x9F, 0, "ldelem.i8", OperandKind::None, 1, 2);
        add(0xA0, 0, "ldelem.i", OperandKind::None, 1, 2);
        add(0xA1, 0, "ldelem.r4", OperandKind::None, 1, 2);
        add(0xA2, 0, "ldelem.r8", OperandKind::None, 1, 2);
        add(0xA3, 0, "ldelem.ref", OperandKind::None, 1, 2);
        add(0xA4, 0, "stelem.i", OperandKind::None, 0, 3);
        add(0xA5, 0, "stelem.i1", OperandKind::None, 0, 3);
        add(0xB3, 0, "conv.ovf.i1", OperandKind::None, 1, 1);
        add(0xB7, 0, "conv.ovf.i4", OperandKind::None, 1, 1);
        add(0xC2, 0, "refanyval", OperandKind::Type, 1, 1);
        add(0xC3, 0, "ckfinite", OperandKind::None, 1, 1);
        add(0xD0, 0, "ldtoken", OperandKind::Token, 1, 0);
        add(0xD1, 0, "conv.u2", OperandKind::None, 1, 1);
        add(0xD2, 0, "conv.u1", OperandKind::None, 1, 1);
        add(0xD3, 0, "conv.i", OperandKind::None, 1, 1);
        add(0xD4, 0, "conv.ovf.i", OperandKind::None, 1, 1);
        add(0xFE, 0x00, "arglist", OperandKind::None, 1, 0);
        add(0xFE, 0x01, "ceq", OperandKind::None, 1, 2);
        add(0xFE, 0x02, "cgt", OperandKind::None, 1, 2);
        add(0xFE, 0x03, "cgt.un", OperandKind::None, 1, 2);
        add(0xFE, 0x04, "clt", OperandKind::None, 1, 2);
        add(0xFE, 0x05, "clt.un", OperandKind::None, 1, 2);
        add(0xFE, 0x06, "ldftn", OperandKind::Method, 1, 0);
        add(0xFE, 0x07, "ldvirtftn", OperandKind::Method, 1, 1);
        add(0xFE, 0x09, "ldarg", OperandKind::Var, 1, 0);
        add(0xFE, 0x0A, "ldarga", OperandKind::Var, 1, 0);
        add(0xFE, 0x0B, "starg", OperandKind::Var, 0, 1);
        add(0xFE, 0x0C, "ldloc", OperandKind::Var, 1, 0);
        add(0xFE, 0x0D, "ldloca", OperandKind::Var, 1, 0);
        add(0xFE, 0x0E, "stloc", OperandKind::Var, 0, 1);
        add(0xFE, 0x0F, "localloc", OperandKind::None, 1, 1);
        add(0xFE, 0x11, "endfilter", OperandKind::None, 0, 1);
        add(0xFE, 0x12, "unaligned.", OperandKind::Int8, 0, 0);
        add(0xFE, 0x13, "volatile.", OperandKind::None, 0, 0);
        add(0xFE, 0x14, "tail.", OperandKind::None, 0, 0);
        add(0xFE, 0x15, "initobj", OperandKind::Type, 0, 1);
        add(0xFE, 0x16, "constrained.", OperandKind::Type, 0, 0);
        add(0xFE, 0x17, "cpblk", OperandKind::None, 0, 3);
        add(0xFE, 0x18, "initblk", OperandKind::None, 0, 3);
        add(0xFE, 0x19, "no.", OperandKind::Int16, 0, 0);
        add(0xFE, 0x1A, "rethrow", OperandKind::None, 0, 0);
        add(0xFE, 0x1C, "sizeof", OperandKind::Type, 1, 0);
        add(0xFE, 0x1D, "refanytype", OperandKind::None, 1, 1);
        add(0xFE, 0x1E, "readonly.", OperandKind::None, 0, 0);

        return map;
    }();
    return table;
}

OpcodeInfo unknown_opcode(uint8_t b1, uint8_t b2) {
    return OpcodeInfo{b1, b2, b2 != 0, "unknown", OperandKind::None, 0, 0};
}

}  // namespace

const OpcodeInfo& lookup_opcode(uint8_t b1, uint8_t b2) {
    const auto& table = opcode_table();
    const auto it = table.find({b1, b2});
    if (it != table.end()) {
        return it->second;
    }
    static OpcodeInfo fallback = unknown_opcode(0, 0);
    fallback = unknown_opcode(b1, b2);
    return fallback;
}

}  // namespace csdecomp
