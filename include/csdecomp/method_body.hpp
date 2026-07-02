#pragma once

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace csdecomp {

struct MethodHeader {
    bool tiny{false};
    uint32_t header_size{0};
    uint32_t code_size{0};
    uint32_t max_stack{8};
    uint32_t local_var_sig_token{0};
    bool init_locals{false};
};

uint32_t method_body_total_size(const MethodHeader& header);
MethodHeader parse_method_header(const uint8_t* data, size_t available, size_t max_total_size);
MethodHeader parse_method_header(const std::vector<uint8_t>& data, size_t max_total_size);

}  // namespace csdecomp
