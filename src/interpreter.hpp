#pragma once

#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.hpp"

struct Value {
    bool isDouble = false;
    long long i = 0;
    double d = 0.0;

    static Value ofInt(long long v);
    static Value ofDouble(double v);
    double asDouble() const;
    long long asInt() const;
    bool truthy() const;
};

struct Variable {
    std::string type;
    bool isArray = false;
    Value scalar;
    Value* alias = nullptr;
    std::shared_ptr<void> keepAlive;
    std::shared_ptr<std::vector<Value>> elements;

    Value& ref();
};

struct StreamState {
    int precision = 6;
    bool fixed = false;
    bool left = false;
    bool boolalpha = false;
    long long width = 0;
};

class RuntimeError : public std::runtime_error {
public:
    RuntimeError(int line, const std::string& message);
    int line;
};

class Interpreter {
public:
    Interpreter(Node* program, std::ostream& out, long long maxSteps = 0);
    int run();

private:
    enum class Flow { Normal, Break, Continue, Return };
    using Scope = std::unordered_map<std::string, std::shared_ptr<Variable>>;

    struct Argument {
        Value value;
        std::shared_ptr<std::vector<Value>> elements;
        Value* alias = nullptr;
        std::shared_ptr<void> keepAlive;
    };

    static constexpr int kMaxDepth = 3000;

    Node* program;
    std::ostream& out;
    long long maxSteps;
    long long steps = 0;
    int depth = 0;
    std::unordered_map<std::string, Node*> functions;
    Scope globals;
    std::vector<std::vector<Scope>> frames;
    Value returnValue;
    StreamState stream;

    static Value convert(const Value& value, const std::string& type);
    static Value zeroOf(const std::string& type);

    Scope& currentScope();
    std::shared_ptr<Variable> find(const std::string& name, int line);
    void declare(Node* var);
    void tick(const Node* node);

    Flow exec(Node* node);
    Flow execItems(const std::vector<Node*>& items);
    Flow execBlock(const std::vector<Node*>& items);
    Flow execLoopBody(Node* body);
    Flow execRangeFor(Node* node);

    Value eval(Node* node);
    Value& slot(Node* node, std::string& type);
    Value* element(Node* index, std::shared_ptr<std::vector<Value>>& owner);
    Value assign(Node* node);
    Value unary(Node* node);
    Value binary(Node* node);
    Value arithmetic(const std::string& op, const Value& left, const Value& right, int line);
    Value call(Node* node);
    Value invoke(Node* function, const std::vector<Argument>& args, int line);

    void print(Node* item);
    void put(const std::string& text);
    std::string formatDouble(double value) const;
};
