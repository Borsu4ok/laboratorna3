#include "ast.hpp"

#include <cstdio>
#include <memory>
#include <sstream>

std::vector<Diagnostic> g_diagnostics;

static std::vector<std::unique_ptr<Node>> g_arena;

void addDiagnostic(const std::string& stage, int line, int col, const std::string& message) {
    g_diagnostics.push_back({stage, line, col, message});
}

Node* makeNode(NodeKind kind, int line, int col, const std::string& text, std::vector<Node*> kids) {
    g_arena.push_back(std::make_unique<Node>());
    Node* node = g_arena.back().get();
    node->kind = kind;
    node->line = line;
    node->col = col;
    node->text = text;
    node->kids = std::move(kids);
    return node;
}

const char* kindName(NodeKind kind) {
    switch (kind) {
        case NodeKind::Program: return "Program";
        case NodeKind::Using: return "Using";
        case NodeKind::FuncDef: return "FuncDef";
        case NodeKind::FuncDecl: return "FuncDecl";
        case NodeKind::Params: return "Params";
        case NodeKind::Param: return "Param";
        case NodeKind::DeclStmt: return "DeclStmt";
        case NodeKind::VarDecl: return "VarDecl";
        case NodeKind::InitList: return "InitList";
        case NodeKind::TypeSpec: return "TypeSpec";
        case NodeKind::Block: return "Block";
        case NodeKind::ExprStmt: return "ExprStmt";
        case NodeKind::Empty: return "Empty";
        case NodeKind::If: return "If";
        case NodeKind::While: return "While";
        case NodeKind::DoWhile: return "DoWhile";
        case NodeKind::For: return "For";
        case NodeKind::RangeFor: return "RangeFor";
        case NodeKind::Return: return "Return";
        case NodeKind::Break: return "Break";
        case NodeKind::Continue: return "Continue";
        case NodeKind::Assign: return "Assign";
        case NodeKind::Binary: return "Binary";
        case NodeKind::Unary: return "Unary";
        case NodeKind::Postfix: return "Postfix";
        case NodeKind::Ternary: return "Ternary";
        case NodeKind::Call: return "Call";
        case NodeKind::Index: return "Index";
        case NodeKind::Cast: return "Cast";
        case NodeKind::Comma: return "Comma";
        case NodeKind::Ident: return "Ident";
        case NodeKind::IntLit: return "IntLit";
        case NodeKind::FloatLit: return "FloatLit";
        case NodeKind::CharLit: return "CharLit";
        case NodeKind::BoolLit: return "BoolLit";
        case NodeKind::StringLit: return "StringLit";
        case NodeKind::List: return "List";
    }
    return "?";
}

std::string typeText(const Node* node) {
    std::string text = node->isConst ? "const " : "";
    text += node->isAuto && node->kind == NodeKind::VarDecl ? "auto" : node->dtype;
    if (node->isRef) {
        text += "&";
    }
    return text;
}

static std::string signatureText(const Node* function) {
    std::string text = function->text + "(";
    const auto& params = function->kids[0]->kids;
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            text += ", ";
        }
        text += typeText(params[i]) + (params[i]->isArray ? "[]" : "");
    }
    return text + ")";
}

std::string nodeLabel(const Node* node) {
    std::string name = kindName(node->kind);
    switch (node->kind) {
        case NodeKind::Using:
            return name + " namespace " + node->text;
        case NodeKind::FuncDef:
        case NodeKind::FuncDecl:
            return name + " " + node->dtype + " " + signatureText(node);
        case NodeKind::Param:
        case NodeKind::VarDecl: {
            std::string label = name + " " + typeText(node) + " " + node->text;
            if (node->isArray) {
                label += "[" + (node->arraySize > 0 ? std::to_string(node->arraySize) : std::string()) + "]";
            }
            return label;
        }
        case NodeKind::DeclStmt:
            return name + " " + node->text;
        case NodeKind::Assign:
        case NodeKind::Binary:
        case NodeKind::Unary:
        case NodeKind::Postfix:
            return name + " " + node->text;
        case NodeKind::Call:
            return name + " " + (node->sval.empty() ? node->text + "()" : node->sval);
        case NodeKind::Index:
            return name + " " + node->text + "[]";
        case NodeKind::Cast:
            return node->text == "static_cast" ? name + " static_cast<" + node->dtype + ">"
                                               : name + " (" + node->dtype + ")";
        case NodeKind::Ident:
            return name + " " + (node->qualifier.empty() ? "" : node->qualifier + "::") + node->text;
        case NodeKind::IntLit:
        case NodeKind::FloatLit:
        case NodeKind::CharLit:
        case NodeKind::BoolLit:
        case NodeKind::StringLit:
            return name + " " + node->text;
        default:
            return name;
    }
}

std::string childRole(const Node* parent, size_t index) {
    static const char* forRoles[] = {"init", "cond", "step", "body"};
    static const char* rangeRoles[] = {"var", "range", "body"};
    switch (parent->kind) {
        case NodeKind::If:
        case NodeKind::Ternary:
            return index == 0 ? "cond" : index == 1 ? "then" : "else";
        case NodeKind::While:
            return index == 0 ? "cond" : "body";
        case NodeKind::DoWhile:
            return index == 0 ? "body" : "cond";
        case NodeKind::For:
            return index < 4 ? forRoles[index] : "";
        case NodeKind::RangeFor:
            return index < 3 ? rangeRoles[index] : "";
        case NodeKind::FuncDef:
            return index == 0 ? "params" : "body";
        case NodeKind::Assign:
            return index == 0 ? "lhs" : "rhs";
        case NodeKind::Binary:
        case NodeKind::Comma:
            return index == 0 ? "left" : "right";
        case NodeKind::VarDecl:
            return parent->kids[index] == parent->sizeExpr ? "size" : "init";
        case NodeKind::Index:
            return "index";
        case NodeKind::Call:
            return "arg" + std::to_string(index + 1);
        case NodeKind::Return:
            return "value";
        case NodeKind::Unary:
        case NodeKind::Postfix:
        case NodeKind::Cast:
            return "operand";
        default:
            return "";
    }
}

static void printNode(const Node* node, std::ostream& out, const std::string& prefix, bool last, bool root,
                      const std::string& role) {
    out << prefix;
    if (!root) {
        out << (last ? "└── " : "├── ");
    }
    if (!role.empty()) {
        out << role << ": ";
    }
    out << nodeLabel(node);
    if (!node->type.empty()) {
        out << "  <" << node->type << ">";
    }
    if (node->line > 0) {
        out << "  @" << node->line;
    }
    out << "\n";
    std::string childPrefix = root ? "" : prefix + (last ? "    " : "│   ");
    for (size_t i = 0; i < node->kids.size(); ++i) {
        printNode(node->kids[i], out, childPrefix, i + 1 == node->kids.size(), false, childRole(node, i));
    }
}

void printTree(const Node* node, std::ostream& out) {
    printNode(node, out, "", true, true, "");
}

std::string jsonEscape(const std::string& text) {
    std::string result;
    for (unsigned char c : text) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            case '\r': result += "\\r"; break;
            default:
                if (c < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof buffer, "\\u%04x", c);
                    result += buffer;
                } else {
                    result += static_cast<char>(c);
                }
        }
    }
    return result;
}

std::string toJson(const Node* node, const std::string& role) {
    std::ostringstream out;
    out << "{\"kind\":\"" << kindName(node->kind) << "\""
        << ",\"label\":\"" << jsonEscape(nodeLabel(node)) << "\""
        << ",\"line\":" << node->line
        << ",\"col\":" << node->col
        << ",\"type\":\"" << jsonEscape(node->type) << "\""
        << ",\"role\":\"" << jsonEscape(role) << "\""
        << ",\"children\":[";
    for (size_t i = 0; i < node->kids.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << toJson(node->kids[i], childRole(node, i));
    }
    out << "]}";
    return out.str();
}

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string unescapeC(const std::string& body) {
    std::string result;
    for (size_t i = 0; i < body.size(); ++i) {
        char c = body[i];
        if (c != '\\' || i + 1 >= body.size()) {
            result += c;
            continue;
        }
        char e = body[++i];
        switch (e) {
            case 'n': result += '\n'; break;
            case 't': result += '\t'; break;
            case 'r': result += '\r'; break;
            case 'a': result += '\a'; break;
            case 'b': result += '\b'; break;
            case 'f': result += '\f'; break;
            case 'v': result += '\v'; break;
            case '\\': result += '\\'; break;
            case '\'': result += '\''; break;
            case '"': result += '"'; break;
            case '?': result += '?'; break;
            case 'x': {
                int value = 0;
                while (i + 1 < body.size() && hexValue(body[i + 1]) >= 0) {
                    value = value * 16 + hexValue(body[++i]);
                }
                result += static_cast<char>(value);
                break;
            }
            default:
                if (e >= '0' && e <= '7') {
                    int value = e - '0';
                    for (int k = 0; k < 2 && i + 1 < body.size() && body[i + 1] >= '0' && body[i + 1] <= '7'; ++k) {
                        value = value * 8 + (body[++i] - '0');
                    }
                    result += static_cast<char>(value);
                } else {
                    result += e;
                }
        }
    }
    return result;
}
