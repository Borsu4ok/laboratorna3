#pragma once

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "ast.hpp"

class JsGenerator {
public:
    std::string generate(Node* program);

private:
    std::map<std::string, Node*> functions;
    std::ostringstream out;
    int indent = 0;
    int loopCounter = 0;
    std::string currentReturnType;

    void line(const std::string& text);
    void emitFunction(Node* function);
    void emitStatement(Node* node);
    void emitItems(const std::vector<Node*>& items);
    void emitBody(Node* node);
    void emitIf(Node* node, bool chained);
    void emitRangeFor(Node* node);
    std::string declaration(Node* decl);

    std::string expr(Node* node);
    std::string cond(Node* node, bool strict = false);
    std::string topLevel(Node* node);
    std::string streamItem(Node* item);
    std::string callExpr(Node* node);
    std::string convert(const std::string& code, const std::string& from, const std::string& to);
    std::string literal(Node* node);

    static std::string name(const std::string& identifier);
    static std::string stripParens(const std::string& code);
};
