#include "csdecomp/il_reader.hpp"

#include "csdecomp/method_body.hpp"

#include <cstring>

namespace csdecomp {

IlReader::IlReader(const std::vector<uint8_t>& il) : il_(il) { parse(); }

void IlReader::parse() {
    if (il_.empty()) {
        return;
    }

    const MethodHeader header = parse_method_header(il_, il_.size());
    max_stack_ = header.max_stack;
    init_locals_ = header.init_locals;
    local_var_sig_token_ = header.local_var_sig_token;
    size_t offset = header.header_size;

    while (offset < il_.size()) {
        IlInstruction insn{};
        insn.offset = static_cast<uint32_t>(offset);
        uint8_t b1 = il_[offset++];
        uint8_t b2 = 0;
        if (b1 == 0xFE) {
            if (offset >= il_.size()) {
                break;
            }
            b2 = il_[offset++];
        }
        insn.opcode = lookup_opcode(b1, b2);

        switch (insn.opcode.operand) {
            case OperandKind::None:
                break;
            case OperandKind::Int8: {
                int8_t value = static_cast<int8_t>(il_[offset++]);
                insn.operand = value;
                break;
            }
            case OperandKind::Int16: {
                int16_t value{};
                std::memcpy(&value, il_.data() + offset, 2);
                offset += 2;
                insn.operand = value;
                break;
            }
            case OperandKind::Int32: {
                int32_t value{};
                std::memcpy(&value, il_.data() + offset, 4);
                offset += 4;
                insn.operand = value;
                break;
            }
            case OperandKind::Int64: {
                int64_t value{};
                std::memcpy(&value, il_.data() + offset, 8);
                offset += 8;
                insn.operand = value;
                break;
            }
            case OperandKind::Float32: {
                float value{};
                std::memcpy(&value, il_.data() + offset, 4);
                offset += 4;
                insn.operand = value;
                break;
            }
            case OperandKind::Float64: {
                double value{};
                std::memcpy(&value, il_.data() + offset, 8);
                offset += 8;
                insn.operand = value;
                break;
            }
            case OperandKind::ShortVar:
            case OperandKind::Var:
            case OperandKind::String:
            case OperandKind::Method:
            case OperandKind::Field:
            case OperandKind::Type:
            case OperandKind::Token: {
                uint32_t value{};
                std::memcpy(&value, il_.data() + offset, 4);
                offset += 4;
                insn.operand = value;
                break;
            }
            case OperandKind::Switch: {
                uint32_t count{};
                std::memcpy(&count, il_.data() + offset, 4);
                offset += 4;
                std::vector<int32_t> targets(count);
                for (uint32_t i = 0; i < count; ++i) {
                    int32_t target{};
                    std::memcpy(&target, il_.data() + offset, 4);
                    offset += 4;
                    targets[i] = target;
                }
                insn.operand = targets;
                break;
            }
        }
        instructions_.push_back(insn);
    }
}

}  // namespace csdecomp
