#include "csdecomp/il_reader.hpp"

#include "csdecomp/method_body.hpp"

#include <cstring>
#include <stdexcept>

namespace csdecomp {

namespace {

constexpr size_t kMaxIlInstructions = 200000;
constexpr size_t kMaxSwitchTargets = 10000;

}  // namespace

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

    const auto ensure = [&](size_t count) {
        if (offset + count > il_.size()) {
            throw std::runtime_error("IL stream ended unexpectedly");
        }
    };

    while (offset < il_.size() && instructions_.size() < kMaxIlInstructions) {
        IlInstruction insn{};
        insn.offset = static_cast<uint32_t>(offset);
        ensure(1);
        uint8_t b1 = il_[offset++];
        uint8_t b2 = 0;
        if (b1 == 0xFE) {
            ensure(1);
            b2 = il_[offset++];
        }
        insn.opcode = lookup_opcode(b1, b2);
        if (insn.opcode.name == "unknown") {
            break;
        }

        switch (insn.opcode.operand) {
            case OperandKind::None:
                break;
            case OperandKind::Int8: {
                ensure(1);
                int8_t value = static_cast<int8_t>(il_[offset++]);
                insn.operand = value;
                break;
            }
            case OperandKind::Int16: {
                ensure(2);
                int16_t value{};
                std::memcpy(&value, il_.data() + offset, 2);
                offset += 2;
                insn.operand = value;
                break;
            }
            case OperandKind::Int32: {
                ensure(4);
                int32_t value{};
                std::memcpy(&value, il_.data() + offset, 4);
                offset += 4;
                insn.operand = value;
                break;
            }
            case OperandKind::Int64: {
                ensure(8);
                int64_t value{};
                std::memcpy(&value, il_.data() + offset, 8);
                offset += 8;
                insn.operand = value;
                break;
            }
            case OperandKind::Float32: {
                ensure(4);
                float value{};
                std::memcpy(&value, il_.data() + offset, 4);
                offset += 4;
                insn.operand = value;
                break;
            }
            case OperandKind::Float64: {
                ensure(8);
                double value{};
                std::memcpy(&value, il_.data() + offset, 8);
                offset += 8;
                insn.operand = value;
                break;
            }
            case OperandKind::ShortVar: {
                ensure(1);
                uint32_t value = il_[offset++];
                insn.operand = value;
                break;
            }
            case OperandKind::Var:
            case OperandKind::String:
            case OperandKind::Method:
            case OperandKind::Field:
            case OperandKind::Type:
            case OperandKind::Token: {
                ensure(4);
                uint32_t value{};
                std::memcpy(&value, il_.data() + offset, 4);
                offset += 4;
                insn.operand = value;
                break;
            }
            case OperandKind::Switch: {
                ensure(4);
                uint32_t count{};
                std::memcpy(&count, il_.data() + offset, 4);
                offset += 4;
                if (count > kMaxSwitchTargets) {
                    throw std::runtime_error("switch instruction has too many targets");
                }
                ensure(static_cast<size_t>(count) * 4);
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
