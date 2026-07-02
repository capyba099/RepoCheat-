#include "csdecomp/decompiler.hpp"

#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace csdecomp {

namespace {

std::string escape_csharp_string(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }
    escaped.push_back('"');
    return escaped;
}

std::string arg_name(uint32_t index, bool is_static, const std::vector<std::string>& param_names) {
    if (!is_static && index == 0) {
        return "this";
    }
    const uint32_t arg_index = is_static ? index : index - 1;
    if (arg_index < param_names.size()) {
        return param_names[arg_index];
    }
    return "arg" + std::to_string(arg_index);
}

std::string local_name(uint32_t index) { return "loc" + std::to_string(index); }

int32_t branch_target(int32_t operand, size_t insn_offset) {
    return static_cast<int32_t>(insn_offset) + operand;
}

}  // namespace

Decompiler::Decompiler(const PeReader& pe, const CliMetadata& metadata) : pe_(pe), metadata_(metadata) {}

void Decompiler::decompile_assembly(std::ostream& out, const DecompileOptions& options) const {
    out << "// Decompiled with csdecomp\n";
    out << "// https://github.com/RepoCheat\n\n";
    out << "using System;\n\n";

    for (size_t i = 0; i < metadata_.type_defs().size(); ++i) {
        bool is_nested = false;
        for (const auto& nested : metadata_.nested_classes()) {
            if (nested.nested_class_index == i + 1) {
                is_nested = true;
                break;
            }
        }
        if (!is_nested) {
            try {
                decompile_type(out, i, options);
                out << "\n";
            } catch (const std::exception& ex) {
                out << "// Failed to decompile type #" << i << ": " << ex.what() << "\n\n";
            }
        }
    }
}

void Decompiler::decompile_type(std::ostream& out, size_t type_def_index,
                                const DecompileOptions& options) const {
    const auto& type = metadata_.type_defs()[type_def_index];
    const std::string ns = metadata_.get_string(type.type_namespace_index);
    std::string name = metadata_.get_string(type.type_name_index);
    if (name.empty()) {
        name = "Type_" + std::to_string(type_def_index + 1);
    }
    const std::string base_type = metadata_.resolve_type_name(type.extends_index);

    if (!ns.empty()) {
        out << "namespace " << ns << "\n{\n";
    }

    const int indent_level = ns.empty() ? 0 : 1;
    out << indent(indent_level) << format_type_flags(type.flags) << "class " << name;
    if (!base_type.empty() && base_type != "object" && base_type != "System.Object") {
        out << " : " << base_type;
    }
    out << "\n" << indent(indent_level) << "{\n";

    for (const size_t field_index : metadata_.fields_for_type(type_def_index)) {
        if (field_index >= metadata_.fields().size()) {
            continue;
        }
        try {
            const auto& field = metadata_.fields()[field_index];
            const auto sig = metadata_.decode_field_signature(field.signature_index);
            out << indent(indent_level + 1) << format_field_flags(field.flags) << sig.type_name << " "
                << metadata_.get_string(field.name_index) << ";\n";
        } catch (const std::exception& ex) {
            out << indent(indent_level + 1) << "// Failed to decompile field: " << ex.what() << "\n";
        }
    }

    for (const size_t method_index : metadata_.methods_for_type(type_def_index)) {
        if (method_index >= metadata_.method_defs().size()) {
            continue;
        }
        try {
            out << decompile_method(method_index, type_def_index, options);
        } catch (const std::exception& ex) {
            out << indent(indent_level + 1) << "// Failed to decompile method: " << ex.what() << "\n\n";
        }
    }

    for (const size_t nested_index : metadata_.nested_types_for_type(type_def_index)) {
        if (nested_index >= metadata_.nested_classes().size()) {
            continue;
        }
        try {
            const uint32_t nested_type = metadata_.nested_classes()[nested_index].nested_class_index - 1;
            decompile_type(out, nested_type, options);
        } catch (const std::exception& ex) {
            out << indent(indent_level + 1) << "// Failed to decompile nested type: " << ex.what() << "\n";
        }
    }

    out << indent(indent_level) << "}\n";
    if (!ns.empty()) {
        out << "}\n";
    }
}

std::string Decompiler::decompile_method(size_t method_def_index, size_t type_def_index,
                                         const DecompileOptions& options) const {
    const auto& method = metadata_.method_defs()[method_def_index];
    const auto signature = metadata_.decode_method_signature(method.signature_index);
    const auto param_indices = metadata_.params_for_method(method_def_index);

    std::vector<std::string> param_names;
    param_names.reserve(signature.param_types.size());
    for (size_t i = 0; i < signature.param_types.size(); ++i) {
        std::string param_name = "p" + std::to_string(i);
        if (i < param_indices.size()) {
            const auto& param = metadata_.params()[param_indices[i]];
            const std::string explicit_name = metadata_.get_string(param.name_index);
            if (!explicit_name.empty()) {
                param_name = explicit_name;
            }
        }
        param_names.push_back(param_name);
    }

    std::ostringstream out;
    out << indent(1) << format_method_flags(method.flags) << signature.return_type << " "
        << metadata_.get_string(method.name_index) << "(";

    for (size_t i = 0; i < signature.param_types.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << signature.param_types[i] << " " << param_names[i];
    }
    out << ")\n" << indent(1) << "{\n";
    out << decompile_method_body(method_def_index, type_def_index, options, param_names);
    out << indent(1) << "}\n\n";
    return out.str();
}

std::string Decompiler::decompile_method_body(size_t method_def_index, size_t /*type_def_index*/,
                                              const DecompileOptions& options,
                                              const std::vector<std::string>& param_names) const {
    const auto& method = metadata_.method_defs()[method_def_index];
    const bool is_static = (method.flags & 0x0010) != 0;

    std::vector<uint8_t> il_bytes;
    try {
        il_bytes = metadata_.get_method_il(method.rva);
    } catch (const std::exception& ex) {
        return indent(2) + "// Failed to read method body at RVA 0x" +
               std::to_string(method.rva) + ": " + ex.what() + "\n";
    }

    if (il_bytes.empty()) {
        return indent(2) + "// No IL body (abstract/extern/pinvoke)\n";
    }

    IlReader il(il_bytes);
    std::vector<std::string> stack;
    std::vector<std::string> statements;
    std::unordered_map<int32_t, std::string> labels;
    std::unordered_set<int32_t> label_targets;

    auto add_label = [&](int32_t offset) {
        label_targets.insert(offset);
        if (labels.find(offset) == labels.end()) {
            labels[offset] = "IL_" + std::to_string(offset);
        }
    };

    for (const auto& insn : il.instructions()) {
        const std::string op = insn.opcode.name;
        if (op == "br" || op == "br.s" || op == "brfalse" || op == "brfalse.s" || op == "brtrue" ||
            op == "brtrue.s" || op == "beq" || op == "beq.s" || op == "bge" || op == "bge.s" ||
            op == "bgt" || op == "bgt.s" || op == "ble" || op == "ble.s" || op == "blt" ||
            op == "blt.s" || op == "bne.un.s") {
            if (std::holds_alternative<int8_t>(insn.operand)) {
                add_label(branch_target(std::get<int8_t>(insn.operand),
                                        static_cast<size_t>(insn.offset) + 1 +
                                            (insn.opcode.two_byte ? 2 : 1)));
            } else if (std::holds_alternative<int32_t>(insn.operand)) {
                add_label(branch_target(std::get<int32_t>(insn.operand),
                                        static_cast<size_t>(insn.offset) + 1 +
                                            (insn.opcode.two_byte ? 2 : 1) + 4));
            }
        }
    }

    auto pop = [&]() -> std::string {
        if (stack.empty()) {
            return "/*missing*/";
        }
        std::string value = stack.back();
        stack.pop_back();
        return value;
    };

    auto push = [&](const std::string& value) { stack.push_back(value); };

    auto emit = [&](const std::string& line) { statements.push_back(indent(2) + line); };

    auto label_name = [&](int32_t target) -> std::string {
        const auto it = labels.find(target);
        if (it != labels.end()) {
            return it->second;
        }
        return "IL_" + std::to_string(target);
    };

    for (const auto& insn : il.instructions()) {
        if (label_targets.count(static_cast<int32_t>(insn.offset)) != 0) {
            emit(labels[static_cast<int32_t>(insn.offset)] + ":");
        }

        if (options.include_il_comments) {
            emit("// IL_" + std::to_string(insn.offset) + ": " + insn.opcode.name);
        }

        const std::string& op = insn.opcode.name;

        if (op == "nop") {
            continue;
        }
        if (op == "ldnull") {
            push("null");
            continue;
        }
        if (op == "ldc.i4.m1") {
            push("-1");
            continue;
        }
        if (op == "ldc.i4.0") {
            push("0");
            continue;
        }
        if (op == "ldc.i4.1") {
            push("1");
            continue;
        }
        if (op == "ldc.i4.2") {
            push("2");
            continue;
        }
        if (op == "ldc.i4.3") {
            push("3");
            continue;
        }
        if (op == "ldc.i4.4") {
            push("4");
            continue;
        }
        if (op == "ldc.i4.5") {
            push("5");
            continue;
        }
        if (op == "ldc.i4.6") {
            push("6");
            continue;
        }
        if (op == "ldc.i4.7") {
            push("7");
            continue;
        }
        if (op == "ldc.i4.8") {
            push("8");
            continue;
        }
        if (op == "ldc.i4.s" && std::holds_alternative<int8_t>(insn.operand)) {
            push(std::to_string(static_cast<int>(std::get<int8_t>(insn.operand))));
            continue;
        }
        if (op == "ldc.i4" && std::holds_alternative<int32_t>(insn.operand)) {
            push(std::to_string(std::get<int32_t>(insn.operand)));
            continue;
        }
        if (op == "ldc.i8" && std::holds_alternative<int64_t>(insn.operand)) {
            push(std::to_string(std::get<int64_t>(insn.operand)) + "L");
            continue;
        }
        if (op == "ldc.r4" && std::holds_alternative<float>(insn.operand)) {
            push(std::to_string(std::get<float>(insn.operand)) + "f");
            continue;
        }
        if (op == "ldc.r8" && std::holds_alternative<double>(insn.operand)) {
            push(std::to_string(std::get<double>(insn.operand)));
            continue;
        }
        if (op == "ldstr" && std::holds_alternative<uint32_t>(insn.operand)) {
            push(escape_csharp_string(
                metadata_.get_user_string(std::get<uint32_t>(insn.operand) & 0x00FFFFFF)));
            continue;
        }

        if (op == "ldarg.0") {
            push(arg_name(0, is_static, param_names));
            continue;
        }
        if (op == "ldarg.1") {
            push(arg_name(1, is_static, param_names));
            continue;
        }
        if (op == "ldarg.2") {
            push(arg_name(2, is_static, param_names));
            continue;
        }
        if (op == "ldarg.3") {
            push(arg_name(3, is_static, param_names));
            continue;
        }
        if ((op == "ldarg" || op == "ldarg.s") && std::holds_alternative<uint32_t>(insn.operand)) {
            push(arg_name(std::get<uint32_t>(insn.operand), is_static, param_names));
            continue;
        }

        if (op == "ldloc.0") {
            push(local_name(0));
            continue;
        }
        if (op == "ldloc.1") {
            push(local_name(1));
            continue;
        }
        if (op == "ldloc.2") {
            push(local_name(2));
            continue;
        }
        if (op == "ldloc.3") {
            push(local_name(3));
            continue;
        }
        if ((op == "ldloc" || op == "ldloc.s") && std::holds_alternative<uint32_t>(insn.operand)) {
            push(local_name(std::get<uint32_t>(insn.operand)));
            continue;
        }

        if (op == "stloc.0") {
            emit(local_name(0) + " = " + pop() + ";");
            continue;
        }
        if (op == "stloc.1") {
            emit(local_name(1) + " = " + pop() + ";");
            continue;
        }
        if (op == "stloc.2") {
            emit(local_name(2) + " = " + pop() + ";");
            continue;
        }
        if (op == "stloc.3") {
            emit(local_name(3) + " = " + pop() + ";");
            continue;
        }
        if ((op == "stloc" || op == "stloc.s") && std::holds_alternative<uint32_t>(insn.operand)) {
            emit(local_name(std::get<uint32_t>(insn.operand)) + " = " + pop() + ";");
            continue;
        }

        if (op == "add") {
            const auto right = pop();
            const auto left = pop();
            push("(" + left + " + " + right + ")");
            continue;
        }
        if (op == "sub") {
            const auto right = pop();
            const auto left = pop();
            push("(" + left + " - " + right + ")");
            continue;
        }
        if (op == "mul") {
            const auto right = pop();
            const auto left = pop();
            push("(" + left + " * " + right + ")");
            continue;
        }
        if (op == "div") {
            const auto right = pop();
            const auto left = pop();
            push("(" + left + " / " + right + ")");
            continue;
        }
        if (op == "rem") {
            const auto right = pop();
            const auto left = pop();
            push("(" + left + " % " + right + ")");
            continue;
        }
        if (op == "and") {
            const auto right = pop();
            const auto left = pop();
            push("(" + left + " & " + right + ")");
            continue;
        }
        if (op == "or") {
            const auto right = pop();
            const auto left = pop();
            push("(" + left + " | " + right + ")");
            continue;
        }
        if (op == "xor") {
            const auto right = pop();
            const auto left = pop();
            push("(" + left + " ^ " + right + ")");
            continue;
        }
        if (op == "neg") {
            push("(-" + pop() + ")");
            continue;
        }
        if (op == "not") {
            push("(~" + pop() + ")");
            continue;
        }
        if (op == "ceq") {
            const auto right = pop();
            const auto left = pop();
            push("(" + left + " == " + right + ")");
            continue;
        }
        if (op == "cgt") {
            const auto right = pop();
            const auto left = pop();
            push("(" + left + " > " + right + ")");
            continue;
        }
        if (op == "clt") {
            const auto right = pop();
            const auto left = pop();
            push("(" + left + " < " + right + ")");
            continue;
        }

        if ((op == "call" || op == "callvirt" || op == "newobj") &&
            std::holds_alternative<uint32_t>(insn.operand)) {
            const uint32_t token = std::get<uint32_t>(insn.operand);
            std::string callee = metadata_.resolve_method_token(token);

            std::vector<std::string> args;
            if (op == "callvirt" || op == "call") {
                const int pop_count = 8;
                for (int i = 0; i < pop_count && !stack.empty(); ++i) {
                    args.insert(args.begin(), pop());
                }
            } else if (op == "newobj") {
                const int pop_count = 4;
                for (int i = 0; i < pop_count && !stack.empty(); ++i) {
                    args.push_back(pop());
                }
            }

            std::ostringstream call;
            if (op == "newobj") {
                call << "new " << callee << "(";
            } else {
                call << callee << "(";
            }
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) {
                    call << ", ";
                }
                call << args[i];
            }
            call << ")";
            push(call.str());
            continue;
        }

        if (op == "ldfld" && std::holds_alternative<uint32_t>(insn.operand)) {
            const auto obj = pop();
            const uint32_t token = std::get<uint32_t>(insn.operand);
            const uint32_t index = token & 0x00FFFFFF;
            if (index > 0 && index <= metadata_.fields().size()) {
                push(obj + "." + metadata_.get_string(metadata_.fields()[index - 1].name_index));
            } else {
                push(obj + ".field?" + std::to_string(index));
            }
            continue;
        }
        if (op == "stfld" && std::holds_alternative<uint32_t>(insn.operand)) {
            const auto value = pop();
            const auto obj = pop();
            const uint32_t token = std::get<uint32_t>(insn.operand);
            const uint32_t index = token & 0x00FFFFFF;
            if (index > 0 && index <= metadata_.fields().size()) {
                emit(obj + "." + metadata_.get_string(metadata_.fields()[index - 1].name_index) +
                     " = " + value + ";");
            } else {
                emit(obj + ".field?" + std::to_string(index) + " = " + value + ";");
            }
            continue;
        }
        if (op == "ldsfld" && std::holds_alternative<uint32_t>(insn.operand)) {
            const uint32_t index = std::get<uint32_t>(insn.operand) & 0x00FFFFFF;
            if (index > 0 && index <= metadata_.fields().size()) {
                push(metadata_.get_string(metadata_.fields()[index - 1].name_index));
            } else {
                push("sfld?" + std::to_string(index));
            }
            continue;
        }
        if (op == "stsfld" && std::holds_alternative<uint32_t>(insn.operand)) {
            const auto value = pop();
            const uint32_t index = std::get<uint32_t>(insn.operand) & 0x00FFFFFF;
            if (index > 0 && index <= metadata_.fields().size()) {
                emit(metadata_.get_string(metadata_.fields()[index - 1].name_index) + " = " + value +
                     ";");
            }
            continue;
        }

        if (op == "box" && std::holds_alternative<uint32_t>(insn.operand)) {
            const auto value = pop();
            const std::string type = metadata_.resolve_type_token(std::get<uint32_t>(insn.operand));
            push("((" + type + ")" + value + ")");
            continue;
        }
        if (op == "castclass" && std::holds_alternative<uint32_t>(insn.operand)) {
            const auto value = pop();
            const std::string type = metadata_.resolve_type_token(std::get<uint32_t>(insn.operand));
            push("((" + type + ")" + value + ")");
            continue;
        }
        if (op == "isinst" && std::holds_alternative<uint32_t>(insn.operand)) {
            const auto value = pop();
            const std::string type = metadata_.resolve_type_token(std::get<uint32_t>(insn.operand));
            push("(" + value + " is " + type + ")");
            continue;
        }
        if (op == "newarr" && std::holds_alternative<uint32_t>(insn.operand)) {
            const auto length = pop();
            const std::string element = metadata_.resolve_type_token(std::get<uint32_t>(insn.operand));
            push("new " + element + "[" + length + "]");
            continue;
        }
        if (op == "ldlen") {
            push("(" + pop() + ").Length");
            continue;
        }
        if (op == "ldelem.ref" || op == "ldelem.i4") {
            const auto index = pop();
            const auto array = pop();
            push(array + "[" + index + "]");
            continue;
        }
        if (op == "stelem.ref" || op == "stelem.i4") {
            const auto value = pop();
            const auto index = pop();
            const auto array = pop();
            emit(array + "[" + index + "] = " + value + ";");
            continue;
        }

        if (op == "ret") {
            if (!stack.empty()) {
                emit("return " + pop() + ";");
            } else {
                emit("return;");
            }
            continue;
        }

        if (op == "pop") {
            pop();
            continue;
        }
        if (op == "dup") {
            if (!stack.empty()) {
                push(stack.back());
            }
            continue;
        }

        if (op == "blt" || op == "blt.s" || op == "bgt" || op == "bgt.s" || op == "bge" ||
            op == "bge.s" || op == "ble" || op == "ble.s") {
            const auto right = pop();
            const auto left = pop();
            int32_t target = 0;
            if (std::holds_alternative<int8_t>(insn.operand)) {
                target = branch_target(std::get<int8_t>(insn.operand), insn.offset + 2);
            } else if (std::holds_alternative<int32_t>(insn.operand)) {
                target = branch_target(std::get<int32_t>(insn.operand), insn.offset + 5);
            }
            std::string cmp;
            if (op.rfind("blt", 0) == 0) {
                cmp = left + " < " + right;
            } else if (op.rfind("bgt", 0) == 0) {
                cmp = left + " > " + right;
            } else if (op.rfind("bge", 0) == 0) {
                cmp = left + " >= " + right;
            } else {
                cmp = left + " <= " + right;
            }
            emit("if (" + cmp + ") goto " + label_name(target) + ";");
            continue;
        }

        if (op == "br" || op == "br.s") {
            int32_t target = 0;
            if (std::holds_alternative<int8_t>(insn.operand)) {
                target = branch_target(std::get<int8_t>(insn.operand), insn.offset + 2);
            } else if (std::holds_alternative<int32_t>(insn.operand)) {
                target = branch_target(std::get<int32_t>(insn.operand), insn.offset + 5);
            }
            emit("goto " + label_name(target) + ";");
            continue;
        }
        if (op == "brfalse" || op == "brfalse.s") {
            const auto cond = pop();
            int32_t target = 0;
            if (std::holds_alternative<int8_t>(insn.operand)) {
                target = branch_target(std::get<int8_t>(insn.operand), insn.offset + 2);
            } else if (std::holds_alternative<int32_t>(insn.operand)) {
                target = branch_target(std::get<int32_t>(insn.operand), insn.offset + 5);
            }
            emit("if (!(" + cond + ")) goto " + label_name(target) + ";");
            continue;
        }
        if (op == "brtrue" || op == "brtrue.s") {
            const auto cond = pop();
            int32_t target = 0;
            if (std::holds_alternative<int8_t>(insn.operand)) {
                target = branch_target(std::get<int8_t>(insn.operand), insn.offset + 2);
            } else if (std::holds_alternative<int32_t>(insn.operand)) {
                target = branch_target(std::get<int32_t>(insn.operand), insn.offset + 5);
            }
            emit("if (" + cond + ") goto " + label_name(target) + ";");
            continue;
        }
        if (op == "beq" || op == "beq.s") {
            const auto right = pop();
            const auto left = pop();
            int32_t target = 0;
            if (std::holds_alternative<int8_t>(insn.operand)) {
                target = branch_target(std::get<int8_t>(insn.operand), insn.offset + 2);
            } else if (std::holds_alternative<int32_t>(insn.operand)) {
                target = branch_target(std::get<int32_t>(insn.operand), insn.offset + 5);
            }
            emit("if (" + left + " == " + right + ") goto " + label_name(target) + ";");
            continue;
        }
        if (op == "bne.un.s") {
            const auto right = pop();
            const auto left = pop();
            int32_t target = 0;
            if (std::holds_alternative<int8_t>(insn.operand)) {
                target = branch_target(std::get<int8_t>(insn.operand), insn.offset + 2);
            }
            emit("if (" + left + " != " + right + ") goto " + label_name(target) + ";");
            continue;
        }

        emit("// unsupported: " + op);
    }

    std::ostringstream body;
    for (const auto& line : statements) {
        body << line << "\n";
    }
    return body.str();
}

std::string Decompiler::format_method_flags(uint16_t flags) const {
    std::string prefix;
    const uint16_t access = flags & 0x0007;
    if (access == 0x0006 || access == 0x0001) {
        prefix += "public ";
    } else if (access == 0x0004) {
        prefix += "private ";
    } else if (access == 0x0003) {
        prefix += "protected ";
    } else if (access == 0x0002) {
        prefix += "internal ";
    }
    if (flags & 0x0010) {
        prefix += "static ";
    }
    if (flags & 0x0020) {
        prefix += "sealed ";
    }
    if (flags & 0x0040) {
        prefix += "virtual ";
    }
    if (flags & 0x0400) {
        prefix += "abstract ";
    }
    return prefix;
}

std::string Decompiler::format_type_flags(uint32_t flags) const {
    std::string prefix;
    if (flags & 0x00000001) {
        prefix += "public ";
    } else {
        prefix += "internal ";
    }
    if (flags & 0x00000080) {
        prefix += "sealed ";
    }
    if (flags & 0x00000100) {
        prefix += "abstract ";
    }
    return prefix;
}

std::string Decompiler::format_field_flags(uint16_t flags) const {
    std::string prefix;
    if (flags & 0x0006) {
        prefix += "private ";
    } else {
        prefix += "public ";
    }
    if (flags & 0x0010) {
        prefix += "static ";
    }
    if (flags & 0x0040) {
        prefix += "readonly ";
    }
    return prefix;
}

std::string Decompiler::indent(int level) const {
    return std::string(static_cast<size_t>(level) * 4, ' ');
}

}  // namespace csdecomp
