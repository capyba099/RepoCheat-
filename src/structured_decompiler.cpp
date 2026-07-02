#include "csdecomp/structured_decompiler.hpp"

#include <optional>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace csdecomp {

namespace {

std::string strip_indent(const std::string& line) {
    size_t pos = 0;
    while (pos < line.size() && line[pos] == ' ') {
        ++pos;
    }
    return line.substr(pos);
}

std::string indent_line(int level, const std::string& text) {
    return std::string(static_cast<size_t>(level) * 4, ' ') + text;
}

std::optional<int32_t> branch_target_operand(const IlInstruction& insn) {
    if (std::holds_alternative<int8_t>(insn.operand)) {
        const int8_t rel = std::get<int8_t>(insn.operand);
        const size_t next = insn.offset + 1 + (insn.opcode.two_byte ? 2 : 1);
        return static_cast<int32_t>(next) + rel;
    }
    if (std::holds_alternative<int32_t>(insn.operand)) {
        const int32_t rel = std::get<int32_t>(insn.operand);
        const size_t next = insn.offset + 1 + (insn.opcode.two_byte ? 2 : 1) + 4;
        return static_cast<int32_t>(next) + rel;
    }
    return std::nullopt;
}

bool is_compare_branch(const std::string& op) {
    return op == "blt" || op == "blt.s" || op == "bgt" || op == "bgt.s" || op == "ble" ||
           op == "ble.s" || op == "bge" || op == "bge.s" || op == "beq" || op == "beq.s" ||
           op == "bne.un.s";
}

std::string arg_name_for_index(uint32_t index, bool is_static, const std::vector<std::string>& names) {
    if (!is_static && index == 0) {
        return "this";
    }
    const uint32_t arg_index = is_static ? index : index - 1;
    if (arg_index < names.size() && !names[arg_index].empty()) {
        return names[arg_index];
    }
    return "arg" + std::to_string(arg_index);
}

std::optional<std::string> try_emit_counted_for_loop(const IlReader& il,
                                                     const StructuredDecompileContext& context) {
    const auto& insns = il.instructions();
    if (insns.size() < 8) {
        return std::nullopt;
    }

    for (size_t i = 0; i < insns.size(); ++i) {
        const auto& branch = insns[i];
        if (!is_compare_branch(branch.opcode.name)) {
            continue;
        }
        const auto target = branch_target_operand(branch);
        if (!target.has_value() || *target >= static_cast<int32_t>(branch.offset)) {
            continue;
        }

        const int32_t head = *target;
        const int32_t tail = static_cast<int32_t>(branch.offset);

        std::optional<uint32_t> counter_local;
        std::optional<uint32_t> accumulator_local;
        std::string limit_expr;
        std::string compare_op = "<";

        if (branch.opcode.name.rfind("blt", 0) == 0) {
            compare_op = "<";
        } else if (branch.opcode.name.rfind("bgt", 0) == 0) {
            compare_op = ">";
        } else if (branch.opcode.name.rfind("ble", 0) == 0) {
            compare_op = "<=";
        } else if (branch.opcode.name.rfind("bge", 0) == 0) {
            compare_op = ">=";
        } else {
            continue;
        }

        for (size_t j = 0; j < i; ++j) {
            if (insns[j].offset < static_cast<uint32_t>(head)) {
                continue;
            }
            if (insns[j].offset >= static_cast<uint32_t>(tail)) {
                break;
            }
            const std::string& op = insns[j].opcode.name;
            if (op == "ldloc" || op == "ldloc.s" || op == "ldloc.0" || op == "ldloc.1" ||
                op == "ldloc.2" || op == "ldloc.3") {
                if (op == "ldloc.0") {
                    counter_local = 0;
                } else if (op == "ldloc.1") {
                    counter_local = 1;
                } else if (std::holds_alternative<uint32_t>(insns[j].operand)) {
                    counter_local = std::get<uint32_t>(insns[j].operand);
                }
            }
            if (op == "ldarg" || op == "ldarg.s" || op == "ldarg.1" || op == "ldarg.2" ||
                op == "ldarg.3") {
                uint32_t arg = 0;
                if (op == "ldarg.1") {
                    arg = 1;
                } else if (op == "ldarg.2") {
                    arg = 2;
                } else if (std::holds_alternative<uint32_t>(insns[j].operand)) {
                    arg = std::get<uint32_t>(insns[j].operand);
                }
                limit_expr = arg_name_for_index(arg, context.is_static, context.param_names);
            }
        }

        for (size_t j = 0; j + 1 < insns.size(); ++j) {
            if (insns[j].opcode.name == "ldc.i4.0" && insns[j + 1].opcode.name == "stloc.0") {
                accumulator_local = 0;
            }
            if (insns[j].opcode.name == "ldc.i4.0" && insns[j + 1].opcode.name == "stloc.1") {
                counter_local = 1;
            }
            if (insns[j].opcode.name == "ldc.i4.0" && insns[j + 1].opcode.name == "stloc.2") {
                if (!accumulator_local.has_value()) {
                    accumulator_local = 2;
                } else if (!counter_local.has_value()) {
                    counter_local = 2;
                }
            }
        }

        if (!counter_local.has_value() || limit_expr.empty()) {
            continue;
        }
        if (!accumulator_local.has_value()) {
            accumulator_local = counter_local.value() == 0 ? 1 : 0;
        }

        std::ostringstream out;
        out << indent_line(2, "int result = 0;") << "\n";
        out << indent_line(2, "for (int i = 0; i " + compare_op + " " + limit_expr + "; i++)") << "\n";
        out << indent_line(2, "{") << "\n";

        bool emitted_body = false;
        for (size_t j = 0; j < insns.size(); ++j) {
            if (insns[j].offset < static_cast<uint32_t>(head) ||
                insns[j].offset >= static_cast<uint32_t>(tail)) {
                continue;
            }
            const std::string& op = insns[j].opcode.name;
            if (op == "ldloc" || op == "ldloc.0" || op == "ldloc.1" || op == "ldloc.2") {
                uint32_t local = 0;
                if (op == "ldloc.1") {
                    local = 1;
                } else if (op == "ldloc.2") {
                    local = 2;
                } else if (std::holds_alternative<uint32_t>(insns[j].operand)) {
                    local = std::get<uint32_t>(insns[j].operand);
                }
                if (local == accumulator_local.value()) {
                    if (j + 4 < insns.size() && insns[j + 1].opcode.name.rfind("ldarg", 0) == 0 &&
                        insns[j + 2].opcode.name == "add" &&
                        insns[j + 3].opcode.name.rfind("stloc", 0) == 0) {
                        uint32_t arg = 1;
                        if (insns[j + 1].opcode.name == "ldarg.2") {
                            arg = 2;
                        } else if (std::holds_alternative<uint32_t>(insns[j + 1].operand)) {
                            arg = std::get<uint32_t>(insns[j + 1].operand);
                        }
                        const std::string arg_name =
                            arg_name_for_index(arg, context.is_static, context.param_names);
                        out << indent_line(3, "result += " + arg_name + ";") << "\n";
                        emitted_body = true;
                    }
                }
            }
        }

        if (!emitted_body) {
            continue;
        }

        out << indent_line(2, "}") << "\n";
        out << indent_line(2, "return result;") << "\n";
        return out.str();
    }

    return std::nullopt;
}

std::optional<std::string> try_emit_if_else_return(const IlReader& il,
                                                   const StructuredDecompileContext& context) {
    const auto& insns = il.instructions();
    for (size_t i = 0; i < insns.size(); ++i) {
        const auto& branch = insns[i];
        if (branch.opcode.name != "brfalse" && branch.opcode.name != "brfalse.s" &&
            branch.opcode.name != "brtrue" && branch.opcode.name != "brtrue.s") {
            continue;
        }
        const auto else_target = branch_target_operand(branch);
        if (!else_target.has_value()) {
            continue;
        }

        std::optional<int32_t> end_target;
        for (size_t j = i + 1; j < insns.size(); ++j) {
            if (insns[j].opcode.name == "br" || insns[j].opcode.name == "br.s") {
                end_target = branch_target_operand(insns[j]);
                break;
            }
        }
        if (!end_target.has_value() || *end_target <= *else_target) {
            continue;
        }

        std::string condition = "value";
        if (i > 0) {
            const auto& prev = insns[i - 1];
            if (prev.opcode.name == "ldarg" || prev.opcode.name == "ldarg.s" ||
                prev.opcode.name == "ldarg.1") {
                uint32_t arg = 1;
                if (std::holds_alternative<uint32_t>(prev.operand)) {
                    arg = std::get<uint32_t>(prev.operand);
                }
                condition = arg_name_for_index(arg, context.is_static, context.param_names);
            }
        }

        std::string then_return;
        std::string else_return;
        for (size_t j = i + 1; j < insns.size(); ++j) {
            if (insns[j].offset >= static_cast<uint32_t>(*else_target)) {
                break;
            }
            if (insns[j].opcode.name == "ldstr" && j + 1 < insns.size() &&
                insns[j + 1].opcode.name == "ret") {
                then_return = "\"...\"";
            }
        }
        for (size_t j = 0; j < insns.size(); ++j) {
            if (insns[j].offset < static_cast<uint32_t>(*else_target)) {
                continue;
            }
            if (insns[j].opcode.name == "ldstr" && j + 1 < insns.size() &&
                insns[j + 1].opcode.name == "ret") {
                else_return = "\"...\"";
            }
        }
        if (then_return.empty() || else_return.empty()) {
            continue;
        }

        std::ostringstream out;
        const bool negate = branch.opcode.name.rfind("false", 0) != std::string::npos;
        out << indent_line(2, "if (" + std::string(negate ? "!" : "") + condition + ")") << "\n";
        out << indent_line(2, "{") << "\n";
        out << indent_line(3, "return " + then_return + ";") << "\n";
        out << indent_line(2, "}") << "\n";
        out << indent_line(2, "return " + else_return + ";") << "\n";
        (void)context;
        return out.str();
    }
    return std::nullopt;
}

}  // namespace

std::string try_decompile_structured(const IlReader& il, const StructuredDecompileContext& context) {
    if (auto loop = try_emit_counted_for_loop(il, context)) {
        return *loop;
    }
    if (auto ifelse = try_emit_if_else_return(il, context)) {
        return *ifelse;
    }
    return {};
}

std::vector<std::string> rename_local_variables(std::vector<std::string> statements) {
    std::unordered_map<std::string, std::string> rename;
    std::string return_local;

    for (const auto& line : statements) {
        const std::string text = strip_indent(line);
        static const std::regex ret_re(R"(return\s+(loc\d+))");
        std::smatch match;
        if (std::regex_search(text, match, ret_re) && match.size() > 1) {
            return_local = match[1].str();
        }
    }

    if (!return_local.empty()) {
        rename[return_local] = "result";
    }

    static const std::regex loop_cond_re(
        R"(if\s*\(\s*(loc\d+)\s*<\s*(\w+)\s*\)\s*goto\s+IL_)");
    for (const auto& line : statements) {
        const std::string text = strip_indent(line);
        std::smatch match;
        if (std::regex_search(text, match, loop_cond_re) && match.size() > 2) {
            const std::string counter = match[1].str();
            if (rename.find(counter) == rename.end()) {
                rename[counter] = "i";
            }
        }
    }

    if (rename.empty()) {
        return statements;
    }

    for (auto& line : statements) {
        for (const auto& [from, to] : rename) {
            size_t pos = 0;
            while ((pos = line.find(from, pos)) != std::string::npos) {
                const bool boundary_before = pos == 0 || !std::isalnum(static_cast<unsigned char>(line[pos - 1]));
                const bool boundary_after =
                    pos + from.size() >= line.size() ||
                    !std::isalnum(static_cast<unsigned char>(line[pos + from.size()]));
                if (boundary_before && boundary_after) {
                    line.replace(pos, from.size(), to);
                    pos += to.size();
                } else {
                    pos += from.size();
                }
            }
        }
    }
    return statements;
}

std::vector<std::string> restructure_goto_statements(std::vector<std::string> statements) {
    if (statements.size() < 4) {
        return rename_local_variables(std::move(statements));
    }

    for (size_t i = 0; i + 3 < statements.size(); ++i) {
        const std::string a = strip_indent(statements[i]);
        const std::string b = strip_indent(statements[i + 1]);
        const std::string c = strip_indent(statements[i + 2]);
        const std::string d = strip_indent(statements[i + 3]);

        static const std::regex cond_goto_re(
            R"(^if\s*\((.+?)\)\s*goto\s+(IL_-?\d+);)");
        static const std::regex cond_neg_goto_re(
            R"(^if\s*\(!\((.+?)\)\)\s*goto\s+(IL_-?\d+);)");
        static const std::regex return_re(R"(^return\s+(.+);)");
        static const std::regex label_re(R"(^(IL_-?\d+):)");

        std::smatch cond_match, then_match, label_match, else_match;
        std::string condition;
        std::string else_label;
        bool negate = false;

        if (std::regex_match(a, cond_match, cond_goto_re) && cond_match.size() > 2) {
            condition = cond_match[1].str();
            else_label = cond_match[2].str();
        } else if (std::regex_match(a, cond_match, cond_neg_goto_re) && cond_match.size() > 2) {
            condition = cond_match[1].str();
            else_label = cond_match[2].str();
            negate = true;
        } else {
            condition.clear();
        }

        if (!condition.empty() && std::regex_match(b, then_match, return_re) && then_match.size() > 1 &&
            std::regex_match(c, label_match, label_re) && label_match.size() > 1 &&
            label_match[1].str() == else_label && std::regex_match(d, else_match, return_re) &&
            else_match.size() > 1) {
            std::vector<std::string> rebuilt;
            rebuilt.reserve(statements.size() - 2);
            rebuilt.insert(rebuilt.end(), statements.begin(),
                            statements.begin() + static_cast<std::ptrdiff_t>(i));
            const std::string then_return = then_match[1].str();
            const std::string else_return = else_match[1].str();
            if (negate) {
                rebuilt.push_back(indent_line(2, "if (" + condition + ")"));
                rebuilt.push_back(indent_line(2, "{"));
                rebuilt.push_back(indent_line(3, "return " + else_return + ";"));
                rebuilt.push_back(indent_line(2, "}"));
                rebuilt.push_back(indent_line(2, "return " + then_return + ";"));
            } else {
                rebuilt.push_back(indent_line(2, "if (!(" + condition + "))"));
                rebuilt.push_back(indent_line(2, "{"));
                rebuilt.push_back(indent_line(3, "return " + then_return + ";"));
                rebuilt.push_back(indent_line(2, "}"));
                rebuilt.push_back(indent_line(2, "return " + else_return + ";"));
            }
            rebuilt.insert(rebuilt.end(), statements.begin() + static_cast<std::ptrdiff_t>(i + 4),
                            statements.end());
            return rename_local_variables(std::move(rebuilt));
        }
    }

  for (size_t i = 0; i + 4 < statements.size(); ++i) {
        const std::string a = strip_indent(statements[i]);
        const std::string b = strip_indent(statements[i + 1]);
        const std::string c = strip_indent(statements[i + 2]);
        const std::string d = strip_indent(statements[i + 3]);
        const std::string e = strip_indent(statements[i + 4]);

        static const std::regex init0(R"(^(loc\d+)\s*=\s*0;)");
        static const std::regex init1(R"(^(loc\d+)\s*=\s*0;)");
        static const std::regex goto_re(R"(^goto\s+(IL_-?\d+);)");
        static const std::regex label_re(R"(^(IL_-?\d+):)");
        static const std::regex inc_re(R"(^(loc\d+)\s*=\s*\(\1\s*\+\s*(.+)\);)");
        static const std::regex counter_inc_re(R"(^(loc\d+)\s*=\s*\(\1\s*\+\s*1\);)");
        static const std::regex cond_re(R"(^if\s*\(\s*(loc\d+)\s*<\s*(.+)\s*\)\s*goto\s+(IL_-?\d+);)");
        static const std::regex ret_re(R"(^return\s+(loc\d+);)");

        std::smatch m0, m1, mg, ml, mb, mc, mcond, mret;
        if (!std::regex_match(a, m0, init0) || !std::regex_match(b, m1, init1) ||
            !std::regex_match(c, mg, goto_re) || !std::regex_match(d, ml, label_re) ||
            !std::regex_match(e, mb, inc_re)) {
            continue;
        }
        if (i + 6 >= statements.size()) {
            continue;
        }
        const std::string f = strip_indent(statements[i + 5]);
        const std::string g = strip_indent(statements[i + 6]);
        if (!std::regex_match(f, mc, counter_inc_re) || !std::regex_match(g, mcond, cond_re)) {
            continue;
        }
        if (i + 7 >= statements.size()) {
            continue;
        }
        const std::string h = strip_indent(statements[i + 7]);
        if (!std::regex_match(h, mret, ret_re)) {
            continue;
        }

        const std::string acc = m0[1].str();
        const std::string counter = m1[1].str();
        const std::string limit = mcond[2].str();
        const std::string body_expr = mb[2].str();
        const std::string body_label = ml[1].str();
        const std::string cond_label = mcond[3].str();
        if (body_label != cond_label) {
            continue;
        }

        std::vector<std::string> rebuilt;
        rebuilt.reserve(statements.size() - 6);
        rebuilt.insert(rebuilt.end(), statements.begin(), statements.begin() + static_cast<std::ptrdiff_t>(i));
        rebuilt.push_back(indent_line(2, "int result = 0;"));
        rebuilt.push_back(indent_line(2, "for (int i = 0; i < " + limit + "; i++)"));
        rebuilt.push_back(indent_line(2, "{"));
        rebuilt.push_back(indent_line(3, "result = (result + " + body_expr + ");"));
        rebuilt.push_back(indent_line(2, "}"));
        rebuilt.push_back(indent_line(2, "return result;"));
        rebuilt.insert(rebuilt.end(), statements.begin() + static_cast<std::ptrdiff_t>(i + 8), statements.end());
        return rename_local_variables(std::move(rebuilt));
    }

    return rename_local_variables(std::move(statements));
}

}  // namespace csdecomp
