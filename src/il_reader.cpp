#include "csdecomp/il_reader.hpp"

#include <cstring>

namespace csdecomp {

IlReader::IlReader(const std::vector<uint8_t>& il) : il_(il) { parse(); }

void IlReader::parse() {
    if (il_.empty()) {
        return;
    }

    size_t offset = 0;
    const uint8_t first = il_[0];
    if ((first & 0x3) == 0x2) {
        offset = 1;
        max_stack_ = 8;
    } else {
        if (il_.size() < 12) {
            throw std::runtime_error("invalid fat method header");
        }
        uint16_t flags{};
        std::memcpy(&flags, il_.data(), 2);
        const uint16_t header_size = static_cast<uint16_t>((flags >> 12) * 4);
        max_stack_ = static_cast<uint32_t>(il_[2] | (il_[3] << 8));
        init_locals_ = (flags & 0x0400) != 0;
        uint32_t local_sig{};
        std::memcpy(&local_sig, il_.data() + 8, 4);
        local_var_sig_token_ = local_sig;
        offset = header_size;
    }

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
