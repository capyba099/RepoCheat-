#pragma once

#include "csdecomp/cli_metadata.hpp"
#include "csdecomp/il_opcode.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace csdecomp {

struct IlInstruction {
    size_t offset{0};
    OpcodeInfo opcode{};
    std::variant<std::monostate, int8_t, int16_t, int32_t, int64_t, float, double, uint32_t,
                 std::vector<int32_t>>
        operand;
};

class IlReader {
public:
    explicit IlReader(const std::vector<uint8_t>& il);

    [[nodiscard]] const std::vector<IlInstruction>& instructions() const { return instructions_; }
    [[nodiscard]] uint32_t max_stack() const { return max_stack_; }
    [[nodiscard]] uint32_t local_var_sig_token() const { return local_var_sig_token_; }
    [[nodiscard]] bool init_locals() const { return init_locals_; }

private:
    void parse();

    std::vector<uint8_t> il_;
    std::vector<IlInstruction> instructions_;
    uint32_t max_stack_{0};
    uint32_t local_var_sig_token_{0};
    bool init_locals_{false};
};

}  // namespace csdecomp
