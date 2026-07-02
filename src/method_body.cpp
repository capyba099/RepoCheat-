#include "csdecomp/method_body.hpp"

#include <cstring>

namespace csdecomp {

namespace {

[[noreturn]] void throw_invalid_header(const std::string& details) {
    throw std::runtime_error("invalid method header: " + details);
}

}  // namespace

uint32_t method_body_total_size(const MethodHeader& header) {
    return header.header_size + header.code_size;
}

MethodHeader parse_method_header(const uint8_t* data, size_t available, size_t max_total_size) {
    if (available < 1) {
        throw_invalid_header("empty body");
    }

    const uint8_t first = data[0];
    const uint8_t format = first & 0x3;

    if (format == 0x2) {
        const uint32_t code_size = first >> 2;
        if (code_size > 63) {
            throw_invalid_header("tiny header code size exceeds 63 bytes");
        }
        const uint32_t total = 1 + code_size;
        if (total > max_total_size) {
            throw_invalid_header("tiny method body exceeds section bounds");
        }
        MethodHeader header{};
        header.tiny = true;
        header.header_size = 1;
        header.code_size = code_size;
        header.max_stack = 8;
        return header;
    }

    if (format != 0x3) {
        throw_invalid_header("unknown header format (expected tiny=0x2 or fat=0x3 in low bits)");
    }

    if (available < 12) {
        throw_invalid_header("fat header is truncated");
    }

    uint16_t flags{};
    std::memcpy(&flags, data, 2);
    if ((flags & 0x3) != 0x3) {
        throw_invalid_header("fat header flags mismatch");
    }

    const uint32_t header_size = static_cast<uint32_t>((flags >> 12) * 4);
    if (header_size < 12 || (header_size % 4) != 0) {
        throw_invalid_header("invalid fat header size");
    }
    if (available < header_size) {
        throw_invalid_header("fat header extends beyond available bytes");
    }

    uint32_t code_size{};
    std::memcpy(&code_size, data + 4, 4);
    const uint32_t total = header_size + code_size;
    constexpr uint32_t kMaxMethodBody = 64U * 1024U * 1024U;
    if (code_size > kMaxMethodBody || total > kMaxMethodBody || total > max_total_size ||
        total < header_size) {
        throw_invalid_header("method body size is out of bounds");
    }

    MethodHeader header{};
    header.tiny = false;
    header.header_size = header_size;
    header.code_size = code_size;
    header.max_stack = static_cast<uint32_t>(data[2] | (data[3] << 8));
    header.init_locals = (flags & 0x0400) != 0;
    std::memcpy(&header.local_var_sig_token, data + 8, 4);
    return header;
}

MethodHeader parse_method_header(const std::vector<uint8_t>& data, size_t max_total_size) {
    return parse_method_header(data.data(), data.size(), max_total_size);
}

}  // namespace csdecomp
