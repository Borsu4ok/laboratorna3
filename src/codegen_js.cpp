#include "codegen_js.hpp"

#include <set>

namespace {

const char* kPrelude = R"JS("use strict";

function __write(text) {
    if (typeof globalThis.__sink === "function") globalThis.__sink(text);
    else process.stdout.write(text);
}

function __array(size, values) {
    const result = new Array(size).fill(0);
    for (let i = 0; i < values.length; i++) result[i] = values[i];
    return result;
}

function __ref(get, set) {
    return { get v() { return get(); }, set v(value) { set(value); } };
}

function __refAt(array, index) {
    if (index < 0 || index >= array.length) {
        throw new RangeError("array index " + index + " is out of bounds (size " + array.length + ")");
    }
    return { get v() { return array[index]; }, set v(value) { array[index] = value; } };
}

function __toChar(value) {
    return (Math.trunc(value) << 24) >> 24;
}

function __fixed(a, digits) {
    if (a >= 1e21) return a.toFixed(digits);
    const wide = a.toFixed(Math.min(100, digits + 30));
    const dot = wide.indexOf(".");
    const keep = digits === 0 ? wide.slice(0, dot) : wide.slice(0, dot + 1 + digits);
    const rest = wide.slice(dot + 1 + digits);
    if (/^50*$/.test(rest) && Number(keep.slice(-1)) % 2 === 0) return keep;
    return a.toFixed(digits);
}

function __formatDouble(value, precision, fixed) {
    if (Number.isNaN(value)) return "nan";
    if (!Number.isFinite(value)) return value < 0 ? "-inf" : "inf";
    const sign = value < 0 || Object.is(value, -0) ? "-" : "";
    const a = Math.abs(value);
    if (fixed) return sign + __fixed(a, precision);
    const p = Math.max(precision, 1);
    const exponent = a === 0 ? 0 : Number(a.toExponential(p - 1).split("e")[1]);
    let text;
    if (exponent < -4 || exponent >= p) {
        text = a.toExponential(p - 1).replace(/e([+-])(\d)$/, "e$10$2").replace(/\.?0+e/, "e");
    } else {
        text = __fixed(a, p - 1 - exponent);
        if (text.includes(".")) text = text.replace(/0+$/, "").replace(/\.$/, "");
    }
    return sign + text;
}

const __cout = {
    precision: 6,
    isFixed: false,
    alignLeft: false,
    alpha: false,
    width: 0,
    put(text) {
        if (text.length < this.width) {
            const padding = " ".repeat(this.width - text.length);
            text = this.alignLeft ? text + padding : padding + text;
        }
        this.width = 0;
        __write(text);
        return this;
    },
    text(value) { return this.put(value); },
    char(code) { return this.put(String.fromCharCode(code & 255)); },
    int(value) { return this.put(String(value)); },
    bool(value) { return this.put(this.alpha ? (value ? "true" : "false") : (value ? "1" : "0")); },
    double(value) { return this.put(__formatDouble(value, this.precision, this.isFixed)); },
    endl() { __write("\n"); return this; },
    fixed() { this.isFixed = true; return this; },
    left() { this.alignLeft = true; return this; },
    right() { this.alignLeft = false; return this; },
    boolalpha() { this.alpha = true; return this; },
    setw(n) { this.width = n; return this; },
    setprecision(n) { this.precision = n; return this; }
};
)JS";

const std::set<std::string> kReserved = {
    "arguments", "await", "case", "catch", "class", "debugger", "default", "delete", "enum", "eval",
    "export", "extends", "false", "finally", "function", "implements", "import", "in", "instanceof",
    "interface", "let", "new", "null", "package", "private", "protected", "public", "static", "super",
    "switch", "this", "throw", "true", "try", "typeof", "var", "with", "yield", "undefined", "NaN",
    "Infinity", "Math", "Array", "Object", "String", "Number", "Boolean", "console", "process",
    "globalThis", "module", "require", "window", "document", "Symbol", "Error", "JSON", "of", "RangeError"};

bool isComparison(const std::string& op) {
    return op == "<" || op == ">" || op == "<=" || op == ">=" || op == "==" || op == "!=";
}

std::string jsOperator(const std::string& op) {
    if (op == "==") return "===";
    if (op == "!=") return "!==";
    return op;
}

bool isIntegral(const std::string& type) {
    return type == "int" || type == "char" || type == "bool";
}

}

std::string JsGenerator::name(const std::string& identifier) {
    if (kReserved.count(identifier)) {
        return identifier + "_";
    }
    return identifier;
}

std::string JsGenerator::stripParens(const std::string& code) {
    if (code.size() < 2 || code.front() != '(' || code.back() != ')') {
        return code;
    }
    int level = 0;
    bool inString = false;
    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];
        if (inString) {
            if (c == '\\') {
                ++i;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == '(') {
            ++level;
        } else if (c == ')') {
            --level;
            if (level == 0 && i + 1 != code.size()) {
                return code;
            }
        }
    }
    return code.substr(1, code.size() - 2);
}

void JsGenerator::line(const std::string& text) {
    out << std::string(static_cast<size_t>(indent) * 4, ' ') << text << "\n";
}

std::string JsGenerator::generate(Node* program) {
    out.str("");
    out.clear();
    indent = 0;
    loopCounter = 0;
    functions.clear();
    for (Node* node : program->kids) {
        if (node->kind == NodeKind::FuncDef) {
            functions[node->resolved] = node;
        }
    }

    out << kPrelude << "\n";
    for (Node* node : program->kids) {
        if (node->kind == NodeKind::DeclStmt) {
            line(declaration(node) + ";");
            out << "\n";
        } else if (node->kind == NodeKind::FuncDef) {
            emitFunction(node);
            out << "\n";
        }
    }
    line("const __result = main();");
    line("const __exit_code = __result === undefined ? 0 : __result;");
    line("if (typeof window === \"undefined\" && typeof process !== \"undefined\") process.exitCode = __exit_code;");
    return out.str();
}

void JsGenerator::emitFunction(Node* function) {
    std::string params;
    for (Node* param : function->kids[0]->kids) {
        if (!params.empty()) {
            params += ", ";
        }
        params += name(param->text);
    }
    currentReturnType = function->dtype;
    line("function " + name(function->resolved) + "(" + params + ") {");
    ++indent;
    emitItems(function->kids[1]->kids);
    --indent;
    line("}");
}

std::string JsGenerator::declaration(Node* decl) {
    bool allConst = !decl->kids.empty();
    for (Node* var : decl->kids) {
        allConst = allConst && var->isConst;
    }
    std::string result = allConst ? "const " : "let ";
    for (size_t i = 0; i < decl->kids.size(); ++i) {
        Node* var = decl->kids[i];
        if (i > 0) {
            result += ", ";
        }
        result += name(var->text) + " = ";
        Node* init = var->init;
        if (var->isArray) {
            std::string values;
            if (init) {
                for (size_t k = 0; k < init->kids.size(); ++k) {
                    if (k > 0) {
                        values += ", ";
                    }
                    values += stripParens(convert(expr(init->kids[k]), init->kids[k]->type, var->dtype));
                }
            }
            result += "__array(" + std::to_string(var->arraySize) + ", [" + values + "])";
        } else if (init) {
            result += stripParens(convert(expr(init), init->type, var->dtype));
        } else {
            result += "0";
        }
    }
    return result;
}

void JsGenerator::emitItems(const std::vector<Node*>& items) {
    for (Node* item : items) {
        emitStatement(item);
    }
}

void JsGenerator::emitBody(Node* node) {
    ++indent;
    if (node->kind == NodeKind::Block) {
        emitItems(node->kids);
    } else {
        emitStatement(node);
    }
    --indent;
}

void JsGenerator::emitIf(Node* node, bool chained) {
    std::string head = "if (" + stripParens(cond(node->kids[0])) + ") {";
    line(chained ? "} else " + head : head);
    emitBody(node->kids[1]);
    if (node->kids.size() > 2) {
        Node* elsePart = node->kids[2];
        if (elsePart->kind == NodeKind::If) {
            emitIf(elsePart, true);
            return;
        }
        line("} else {");
        emitBody(elsePart);
    }
    line("}");
}

void JsGenerator::emitRangeFor(Node* node) {
    Node* var = node->kids[0];
    Node* range = node->kids[1];
    std::string array = expr(range);
    std::string variable = name(var->text);
    std::string element = range->type.substr(0, range->type.size() - 2);
    if (var->isRef) {
        std::string index = "__i" + std::to_string(++loopCounter);
        line("for (let " + index + " = 0; " + index + " < " + array + ".length; " + index + "++) {");
        ++indent;
        line("const " + variable + " = __refAt(" + array + ", " + index + ");");
        --indent;
    } else {
        std::string converted = convert("__v", element, var->dtype);
        if (converted == "__v") {
            line(std::string("for (") + (var->isConst ? "const " : "let ") + variable + " of " + array + ") {");
        } else {
            line("for (const __v of " + array + ") {");
            ++indent;
            line(std::string(var->isConst ? "const " : "let ") + variable + " = " + converted + ";");
            --indent;
        }
    }
    emitBody(node->kids[2]);
    line("}");
}

void JsGenerator::emitStatement(Node* node) {
    switch (node->kind) {
        case NodeKind::Block:
            line("{");
            emitBody(node);
            line("}");
            break;
        case NodeKind::DeclStmt:
            line(declaration(node) + ";");
            break;
        case NodeKind::ExprStmt:
            line(topLevel(node->kids[0]) + ";");
            break;
        case NodeKind::Empty:
            line(";");
            break;
        case NodeKind::If:
            emitIf(node, false);
            break;
        case NodeKind::While:
            line("while (" + stripParens(cond(node->kids[0])) + ") {");
            emitBody(node->kids[1]);
            line("}");
            break;
        case NodeKind::DoWhile:
            line("do {");
            emitBody(node->kids[0]);
            line("} while (" + stripParens(cond(node->kids[1])) + ");");
            break;
        case NodeKind::For: {
            Node* init = node->kids[0];
            std::string initCode;
            if (init->kind == NodeKind::DeclStmt) {
                initCode = declaration(init);
            } else if (init->kind != NodeKind::Empty) {
                initCode = topLevel(init);
            }
            std::string condCode = node->kids[1]->kind == NodeKind::Empty ? "" : stripParens(cond(node->kids[1]));
            std::string stepCode = node->kids[2]->kind == NodeKind::Empty ? "" : topLevel(node->kids[2]);
            line("for (" + initCode + "; " + condCode + "; " + stepCode + ") {");
            emitBody(node->kids[3]);
            line("}");
            break;
        }
        case NodeKind::RangeFor:
            emitRangeFor(node);
            break;
        case NodeKind::Return:
            if (node->kids.empty()) {
                line("return;");
            } else {
                Node* value = node->kids[0];
                line("return " + stripParens(convert(expr(value), value->type, currentReturnType)) + ";");
            }
            break;
        case NodeKind::Break:
            line("break;");
            break;
        case NodeKind::Continue:
            line("continue;");
            break;
        default:
            line(topLevel(node) + ";");
    }
}

std::string JsGenerator::topLevel(Node* node) {
    return stripParens(expr(node));
}

std::string JsGenerator::convert(const std::string& code, const std::string& from, const std::string& to) {
    if (from == to) {
        return code;
    }
    if (to == "bool") {
        return "(+!!" + code + ")";
    }
    if (to == "char") {
        return "__toChar(" + stripParens(code) + ")";
    }
    if (isIntegral(to) && from == "double") {
        return "Math.trunc(" + stripParens(code) + ")";
    }
    return code;
}

std::string JsGenerator::literal(Node* node) {
    std::string text = node->text;
    bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    std::string suffixes = hex ? "uUlL" : "uUlLfF";
    while (!text.empty() && suffixes.find(text.back()) != std::string::npos) {
        text.pop_back();
    }
    if (node->kind == NodeKind::IntLit) {
        return hex ? text : std::to_string(node->ival);
    }
    if (text.front() == '.') {
        text = "0" + text;
    }
    std::string::size_type dot = text.find('.');
    if (dot != std::string::npos && (dot + 1 == text.size() || text[dot + 1] == 'e' || text[dot + 1] == 'E')) {
        text.insert(dot + 1, "0");
    }
    return text;
}

std::string JsGenerator::cond(Node* node, bool strict) {
    if (node->kind == NodeKind::Binary) {
        const std::string& op = node->text;
        if (isComparison(op)) {
            return "(" + expr(node->kids[0]) + " " + jsOperator(op) + " " + expr(node->kids[1]) + ")";
        }
        if (op == "&&" || op == "||") {
            return "(" + cond(node->kids[0], strict) + " " + op + " " + cond(node->kids[1], strict) + ")";
        }
    }
    if (node->kind == NodeKind::Unary && node->text == "!") {
        return "(!" + cond(node->kids[0]) + ")";
    }
    return strict ? "!!" + expr(node) : expr(node);
}

std::string JsGenerator::streamItem(Node* item) {
    if (item->type == "manip") {
        if (item->kind == NodeKind::Call) {
            return "." + item->text + "(" + stripParens(expr(item->kids[0])) + ")";
        }
        return "." + item->text + "()";
    }
    if (item->kind == NodeKind::StringLit) {
        return ".text(\"" + jsonEscape(item->sval) + "\")";
    }
    if (item->type == "string") {
        return ".text(" + stripParens(expr(item)) + ")";
    }
    if (item->kind == NodeKind::CharLit) {
        return ".text(\"" + jsonEscape(std::string(1, static_cast<char>(item->ival))) + "\")";
    }
    std::string code = stripParens(expr(item));
    if (item->type == "char") return ".char(" + code + ")";
    if (item->type == "bool") return ".bool(" + code + ")";
    if (item->type == "double") return ".double(" + code + ")";
    return ".int(" + code + ")";
}

std::string JsGenerator::callExpr(Node* node) {
    const std::string& target = node->resolved;
    auto argument = [&](size_t i) { return stripParens(expr(node->kids[i])); };
    if (target == "builtin_sqrt") return "Math.sqrt(" + argument(0) + ")";
    if (target == "builtin_pow") return "Math.pow(" + argument(0) + ", " + argument(1) + ")";
    if (target == "builtin_fabs" || target == "builtin_abs_int" || target == "builtin_abs_double") {
        return "Math.abs(" + argument(0) + ")";
    }

    auto found = functions.find(target);
    std::vector<Node*> params;
    if (found != functions.end()) {
        params = found->second->kids[0]->kids;
    }
    std::string args;
    for (size_t i = 0; i < node->kids.size(); ++i) {
        Node* arg = node->kids[i];
        Node* param = i < params.size() ? params[i] : nullptr;
        if (i > 0) {
            args += ", ";
        }
        if (param && param->isRef && !param->isConst) {
            if (arg->kind == NodeKind::Index) {
                args += "__refAt(" + name(arg->text) + ", " + stripParens(expr(arg->kids[0])) + ")";
            } else if (arg->viaRef) {
                args += name(arg->text);
            } else {
                std::string variable = name(arg->text);
                args += "__ref(() => " + variable + ", (__v) => " + variable + " = __v)";
            }
        } else if (param && param->isArray) {
            args += name(arg->text);
        } else {
            args += stripParens(convert(expr(arg), arg->type, param ? param->dtype : arg->type));
        }
    }
    return name(target) + "(" + args + ")";
}

std::string JsGenerator::expr(Node* node) {
    const std::string& op = node->text;
    switch (node->kind) {
        case NodeKind::IntLit:
        case NodeKind::FloatLit:
            return literal(node);
        case NodeKind::CharLit:
        case NodeKind::BoolLit:
            return std::to_string(node->ival);
        case NodeKind::StringLit:
            return "\"" + jsonEscape(node->sval) + "\"";
        case NodeKind::Ident:
            if (node->type == "ostream") return "__cout";
            if (node->type == "manip") return "undefined";
            return name(node->text) + (node->viaRef ? ".v" : "");
        case NodeKind::Index:
            return name(node->text) + "[" + stripParens(expr(node->kids[0])) + "]";
        case NodeKind::Call:
            return callExpr(node);
        case NodeKind::Assign: {
            Node* target = node->kids[0];
            Node* value = node->kids[1];
            std::string left = expr(target);
            if (op == "=") {
                return "(" + left + " = " + stripParens(convert(expr(value), value->type, target->type)) + ")";
            }
            std::string base = op.substr(0, op.size() - 1);
            std::string combined = left + " " + base + " " + expr(value);
            if (target->type == "char" || target->type == "bool") {
                return "(" + left + " = " + convert("(" + combined + ")", "int", target->type) + ")";
            }
            if (isIntegral(target->type) && (base == "/" || value->type == "double")) {
                return "(" + left + " = Math.trunc(" + combined + "))";
            }
            return "(" + left + " " + op + " " + expr(value) + ")";
        }
        case NodeKind::Binary: {
            if (node->type == "ostream") {
                return expr(node->kids[0]) + streamItem(node->kids[1]);
            }
            if (isComparison(op) || op == "&&" || op == "||") {
                return "(+" + cond(node, true) + ")";
            }
            std::string left = expr(node->kids[0]);
            std::string right = expr(node->kids[1]);
            if (op == "/" && isIntegral(node->type)) {
                return "Math.trunc(" + left + " / " + right + ")";
            }
            return "(" + left + " " + op + " " + right + ")";
        }
        case NodeKind::Unary:
            if (op == "!") {
                return "(+" + cond(node, true) + ")";
            }
            return "(" + op + expr(node->kids[0]) + ")";
        case NodeKind::Postfix:
            return "(" + expr(node->kids[0]) + op + ")";
        case NodeKind::Ternary: {
            std::string a = convert(expr(node->kids[1]), node->kids[1]->type, node->type);
            std::string b = convert(expr(node->kids[2]), node->kids[2]->type, node->type);
            return "(" + cond(node->kids[0]) + " ? " + a + " : " + b + ")";
        }
        case NodeKind::Cast:
            return convert(expr(node->kids[0]), node->kids[0]->type, node->dtype);
        case NodeKind::Comma:
            return "(" + stripParens(expr(node->kids[0])) + ", " + stripParens(expr(node->kids[1])) + ")";
        default:
            return "undefined";
    }
}
