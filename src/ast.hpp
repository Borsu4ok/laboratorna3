#pragma once

#include <ostream>
#include <string>
#include <vector>

enum class NodeKind {
    Program,
    Using,
    FuncDef,
    FuncDecl,
    Params,
    Param,
    DeclStmt,
    VarDecl,
    InitList,
    TypeSpec,
    Block,
    ExprStmt,
    Empty,
    If,
    While,
    DoWhile,
    For,
    RangeFor,
    Return,
    Break,
    Continue,
    Assign,
    Binary,
    Unary,
    Postfix,
    Ternary,
    Call,
    Index,
    Cast,
    Comma,
    Ident,
    IntLit,
    FloatLit,
    CharLit,
    BoolLit,
    StringLit,
    List
};

struct Node {
    NodeKind kind = NodeKind::Empty;
    std::string text;
    std::vector<Node*> kids;
    int line = 0;
    int col = 0;
    std::string dtype;
    bool isArray = false;
    bool isRef = false;
    bool isConst = false;
    bool isAuto = false;
    bool viaRef = false;
    long long arraySize = -1;
    Node* sizeExpr = nullptr;
    Node* init = nullptr;
    long long ival = 0;
    double dval = 0.0;
    std::string sval;
    std::string qualifier;
    std::string resolved;
    std::string type;
};

struct Diagnostic {
    std::string stage;
    int line;
    int col;
    std::string message;
};

extern std::vector<Diagnostic> g_diagnostics;

void addDiagnostic(const std::string& stage, int line, int col, const std::string& message);

Node* makeNode(NodeKind kind, int line, int col, const std::string& text = "", std::vector<Node*> kids = {});

const char* kindName(NodeKind kind);
std::string typeText(const Node* node);
std::string nodeLabel(const Node* node);
std::string childRole(const Node* parent, size_t index);
void printTree(const Node* node, std::ostream& out);
std::string toJson(const Node* node, const std::string& role = "");
std::string jsonEscape(const std::string& text);
std::string unescapeC(const std::string& body);
