#pragma once

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "ast.hpp"

struct ParamType {
    std::string type;
    bool isArray = false;
    bool isRef = false;
    bool isConst = false;
};

struct Overload {
    std::string name;
    std::string returnType;
    std::vector<ParamType> params;
    bool defined = false;
    bool builtin = false;
    bool stdOnly = false;
    bool called = false;
    Node* firstCall = nullptr;
    std::string resolved;
};

struct Symbol {
    std::string type;
    bool isArray = false;
    bool isConst = false;
    bool isRef = false;
    std::optional<long long> constValue;
};

class SemanticAnalyzer {
public:
    bool analyze(Node* program);

private:
    std::vector<std::map<std::string, Symbol>> scopes;
    std::map<std::string, std::deque<Overload>> functions;
    std::map<Node*, Overload*> functionNodes;
    std::map<Node*, Overload*> callTargets;
    bool usingStd = false;
    std::string currentReturnType;
    int loopDepth = 0;
    int errors = 0;

    void error(const Node* node, const std::string& message);
    void declareBuiltins();
    void assignResolvedNames();
    Symbol* lookup(const std::string& name);
    void declare(const Node* at, const std::string& name, const Symbol& symbol);

    void visitUsing(Node* node);
    void visitFunction(Node* node);
    void visitDecl(Node* node, bool global);
    void visitStmt(Node* node);
    void visitLoopBody(Node* body);
    void visitRangeFor(Node* node);
    void visitCondition(Node* node);
    std::string visitExpr(Node* node);
    std::string computeType(Node* node);
    std::string identType(Node* node);
    std::string binaryType(Node* node);
    std::string incrementType(Node* node);
    std::string checkCall(Node* node);
    int conversionCost(Node* arg, const std::string& argType, const ParamType& param, std::string& reason);
    bool requireNumeric(const Node* node, const std::string& type);
    void checkAssignable(const Node* target, const std::string& what);
    std::optional<long long> evalConst(const Node* node);
    static std::string signature(const Overload& overload);
};
