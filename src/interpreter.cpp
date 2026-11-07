#include "interpreter.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

Value Value::ofInt(long long v) {
    Value result;
    result.i = v;
    return result;
}

Value Value::ofDouble(double v) {
    Value result;
    result.isDouble = true;
    result.d = v;
    return result;
}

double Value::asDouble() const {
    return isDouble ? d : static_cast<double>(i);
}

long long Value::asInt() const {
    return isDouble ? static_cast<long long>(d) : i;
}

bool Value::truthy() const {
    return isDouble ? d != 0.0 : i != 0;
}

Value& Variable::ref() {
    return alias ? *alias : scalar;
}

RuntimeError::RuntimeError(int line, const std::string& message) : std::runtime_error(message), line(line) {}

Interpreter::Interpreter(Node* program, std::ostream& out, long long maxSteps)
    : program(program), out(out), maxSteps(maxSteps) {}

Value Interpreter::convert(const Value& value, const std::string& type) {
    if (type == "double") {
        return Value::ofDouble(value.asDouble());
    }
    if (type == "int") {
        return Value::ofInt(value.asInt());
    }
    if (type == "char") {
        return Value::ofInt(static_cast<signed char>(value.asInt()));
    }
    if (type == "bool") {
        return Value::ofInt(value.truthy() ? 1 : 0);
    }
    return value;
}

Value Interpreter::zeroOf(const std::string& type) {
    return type == "double" ? Value::ofDouble(0.0) : Value::ofInt(0);
}

int Interpreter::run() {
    for (Node* node : program->kids) {
        if (node->kind == NodeKind::FuncDef) {
            functions[node->resolved] = node;
        }
    }
    for (Node* node : program->kids) {
        if (node->kind == NodeKind::DeclStmt) {
            for (Node* var : node->kids) {
                declare(var);
            }
        }
    }
    Node* mainFunction = functions.at("main");
    Value result = invoke(mainFunction, {}, mainFunction->line);
    out.flush();
    return static_cast<int>(result.asInt());
}

Interpreter::Scope& Interpreter::currentScope() {
    return frames.empty() ? globals : frames.back().back();
}

std::shared_ptr<Variable> Interpreter::find(const std::string& name, int line) {
    if (!frames.empty()) {
        auto& frame = frames.back();
        for (auto it = frame.rbegin(); it != frame.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return found->second;
            }
        }
    }
    auto found = globals.find(name);
    if (found != globals.end()) {
        return found->second;
    }
    throw RuntimeError(line, "unknown variable '" + name + "'");
}

void Interpreter::declare(Node* var) {
    auto variable = std::make_shared<Variable>();
    variable->type = var->dtype;
    variable->isArray = var->isArray;
    Node* init = var->init;
    if (var->isArray) {
        variable->elements =
            std::make_shared<std::vector<Value>>(static_cast<size_t>(var->arraySize), zeroOf(var->dtype));
        if (init) {
            for (size_t i = 0; i < init->kids.size(); ++i) {
                (*variable->elements)[i] = convert(eval(init->kids[i]), var->dtype);
            }
        }
    } else {
        variable->scalar = init ? convert(eval(init), var->dtype) : zeroOf(var->dtype);
    }
    currentScope()[var->text] = variable;
}

void Interpreter::tick(const Node* node) {
    if (maxSteps > 0 && ++steps > maxSteps) {
        throw RuntimeError(node->line, "step limit exceeded (possible infinite loop)");
    }
}

Interpreter::Flow Interpreter::execItems(const std::vector<Node*>& items) {
    for (Node* item : items) {
        Flow flow = exec(item);
        if (flow != Flow::Normal) {
            return flow;
        }
    }
    return Flow::Normal;
}

Interpreter::Flow Interpreter::execBlock(const std::vector<Node*>& items) {
    frames.back().emplace_back();
    Flow flow = execItems(items);
    frames.back().pop_back();
    return flow;
}

Interpreter::Flow Interpreter::execLoopBody(Node* body) {
    return body->kind == NodeKind::Block ? execBlock(body->kids) : exec(body);
}

Interpreter::Flow Interpreter::execRangeFor(Node* node) {
    Node* var = node->kids[0];
    Node* range = node->kids[1];
    Node* body = node->kids[2];
    std::shared_ptr<std::vector<Value>> array = find(range->text, range->line)->elements;
    for (size_t i = 0; i < array->size(); ++i) {
        frames.back().emplace_back();
        auto variable = std::make_shared<Variable>();
        variable->type = var->dtype;
        if (var->isRef) {
            variable->alias = &(*array)[i];
            variable->keepAlive = array;
        } else {
            variable->scalar = convert((*array)[i], var->dtype);
        }
        currentScope()[var->text] = variable;
        Flow flow = body->kind == NodeKind::Block ? execItems(body->kids) : exec(body);
        frames.back().pop_back();
        if (flow == Flow::Break) {
            break;
        }
        if (flow == Flow::Return) {
            return flow;
        }
        tick(node);
    }
    return Flow::Normal;
}

Interpreter::Flow Interpreter::exec(Node* node) {
    tick(node);
    switch (node->kind) {
        case NodeKind::Block:
            return execBlock(node->kids);
        case NodeKind::DeclStmt:
            for (Node* var : node->kids) {
                declare(var);
            }
            return Flow::Normal;
        case NodeKind::ExprStmt:
            eval(node->kids[0]);
            return Flow::Normal;
        case NodeKind::Empty:
            return Flow::Normal;
        case NodeKind::If:
            if (eval(node->kids[0]).truthy()) {
                return exec(node->kids[1]);
            }
            if (node->kids.size() > 2) {
                return exec(node->kids[2]);
            }
            return Flow::Normal;
        case NodeKind::While:
            while (eval(node->kids[0]).truthy()) {
                Flow flow = exec(node->kids[1]);
                if (flow == Flow::Break) {
                    break;
                }
                if (flow == Flow::Return) {
                    return flow;
                }
                tick(node);
            }
            return Flow::Normal;
        case NodeKind::DoWhile:
            do {
                Flow flow = exec(node->kids[0]);
                if (flow == Flow::Break) {
                    break;
                }
                if (flow == Flow::Return) {
                    return flow;
                }
                tick(node);
            } while (eval(node->kids[1]).truthy());
            return Flow::Normal;
        case NodeKind::For: {
            frames.back().emplace_back();
            Flow result = Flow::Normal;
            Node* init = node->kids[0];
            Node* cond = node->kids[1];
            Node* step = node->kids[2];
            if (init->kind == NodeKind::DeclStmt) {
                exec(init);
            } else if (init->kind != NodeKind::Empty) {
                eval(init);
            }
            while (cond->kind == NodeKind::Empty || eval(cond).truthy()) {
                Flow flow = execLoopBody(node->kids[3]);
                if (flow == Flow::Break) {
                    break;
                }
                if (flow == Flow::Return) {
                    result = flow;
                    break;
                }
                if (step->kind != NodeKind::Empty) {
                    eval(step);
                }
                tick(node);
            }
            frames.back().pop_back();
            return result;
        }
        case NodeKind::RangeFor:
            return execRangeFor(node);
        case NodeKind::Return:
            returnValue = node->kids.empty() ? Value::ofInt(0) : eval(node->kids[0]);
            return Flow::Return;
        case NodeKind::Break:
            return Flow::Break;
        case NodeKind::Continue:
            return Flow::Continue;
        default:
            eval(node);
            return Flow::Normal;
    }
}

Value Interpreter::eval(Node* node) {
    switch (node->kind) {
        case NodeKind::IntLit:
        case NodeKind::CharLit:
        case NodeKind::BoolLit:
            return Value::ofInt(node->ival);
        case NodeKind::FloatLit:
            return Value::ofDouble(node->dval);
        case NodeKind::StringLit:
            throw RuntimeError(node->line, "string literal used as a value");
        case NodeKind::Ident: {
            if (node->type == "ostream" || node->type == "manip") {
                return Value::ofInt(0);
            }
            auto variable = find(node->text, node->line);
            if (variable->isArray) {
                throw RuntimeError(node->line, "array '" + node->text + "' used as a value");
            }
            return variable->ref();
        }
        case NodeKind::Index: {
            std::string type;
            return slot(node, type);
        }
        case NodeKind::Call:
            return call(node);
        case NodeKind::Assign:
            return assign(node);
        case NodeKind::Binary:
            return binary(node);
        case NodeKind::Unary:
            return unary(node);
        case NodeKind::Postfix: {
            std::string type;
            Value& target = slot(node->kids[0], type);
            Value old = target;
            Value delta = Value::ofInt(node->text == "++" ? 1 : -1);
            target = convert(arithmetic("+", old, delta, node->line), type);
            return old;
        }
        case NodeKind::Ternary: {
            Value chosen = eval(node->kids[0]).truthy() ? eval(node->kids[1]) : eval(node->kids[2]);
            return convert(chosen, node->type);
        }
        case NodeKind::Cast:
            return convert(eval(node->kids[0]), node->dtype);
        case NodeKind::Comma:
            eval(node->kids[0]);
            return eval(node->kids[1]);
        default:
            throw RuntimeError(node->line, std::string("cannot evaluate ") + kindName(node->kind));
    }
}

Value* Interpreter::element(Node* index, std::shared_ptr<std::vector<Value>>& owner) {
    auto variable = find(index->text, index->line);
    owner = variable->elements;
    long long position = eval(index->kids[0]).asInt();
    long long size = static_cast<long long>(owner->size());
    if (position < 0 || position >= size) {
        throw RuntimeError(index->line, "array index " + std::to_string(position) + " is out of bounds for '" +
                                            index->text + "' (size " + std::to_string(size) + ")");
    }
    return &(*owner)[static_cast<size_t>(position)];
}

Value& Interpreter::slot(Node* node, std::string& type) {
    auto variable = find(node->text, node->line);
    type = variable->type;
    if (node->kind == NodeKind::Ident) {
        return variable->ref();
    }
    std::shared_ptr<std::vector<Value>> owner;
    return *element(node, owner);
}

Value Interpreter::assign(Node* node) {
    Value right = eval(node->kids[1]);
    std::string type;
    Value& target = slot(node->kids[0], type);
    const std::string& op = node->text;
    Value result = op == "=" ? right : arithmetic(op.substr(0, op.size() - 1), target, right, node->line);
    target = convert(result, type);
    return target;
}

Value Interpreter::unary(Node* node) {
    const std::string& op = node->text;
    if (op == "++" || op == "--") {
        std::string type;
        Value& target = slot(node->kids[0], type);
        Value delta = Value::ofInt(op == "++" ? 1 : -1);
        target = convert(arithmetic("+", target, delta, node->line), type);
        return target;
    }
    Value value = eval(node->kids[0]);
    if (op == "-") {
        return value.isDouble ? Value::ofDouble(-value.d) : Value::ofInt(-value.i);
    }
    if (op == "!") {
        return Value::ofInt(!value.truthy());
    }
    if (op == "~") {
        return Value::ofInt(~value.i);
    }
    return value;
}

Value Interpreter::binary(Node* node) {
    const std::string& op = node->text;
    if (node->type == "ostream") {
        eval(node->kids[0]);
        print(node->kids[1]);
        return Value::ofInt(0);
    }
    if (op == "&&") {
        if (!eval(node->kids[0]).truthy()) {
            return Value::ofInt(0);
        }
        return Value::ofInt(eval(node->kids[1]).truthy());
    }
    if (op == "||") {
        if (eval(node->kids[0]).truthy()) {
            return Value::ofInt(1);
        }
        return Value::ofInt(eval(node->kids[1]).truthy());
    }
    Value left = eval(node->kids[0]);
    Value right = eval(node->kids[1]);
    return arithmetic(op, left, right, node->line);
}

Value Interpreter::arithmetic(const std::string& op, const Value& left, const Value& right, int line) {
    if (left.isDouble || right.isDouble) {
        double a = left.asDouble();
        double b = right.asDouble();
        if (op == "+") return Value::ofDouble(a + b);
        if (op == "-") return Value::ofDouble(a - b);
        if (op == "*") return Value::ofDouble(a * b);
        if (op == "/") return Value::ofDouble(a / b);
        if (op == "<") return Value::ofInt(a < b);
        if (op == ">") return Value::ofInt(a > b);
        if (op == "<=") return Value::ofInt(a <= b);
        if (op == ">=") return Value::ofInt(a >= b);
        if (op == "==") return Value::ofInt(a == b);
        if (op == "!=") return Value::ofInt(a != b);
    } else {
        long long a = left.i;
        long long b = right.i;
        if (op == "+") return Value::ofInt(a + b);
        if (op == "-") return Value::ofInt(a - b);
        if (op == "*") return Value::ofInt(a * b);
        if (op == "/" || op == "%") {
            if (b == 0) {
                throw RuntimeError(line, "division by zero");
            }
            return Value::ofInt(op == "/" ? a / b : a % b);
        }
        if (op == "<") return Value::ofInt(a < b);
        if (op == ">") return Value::ofInt(a > b);
        if (op == "<=") return Value::ofInt(a <= b);
        if (op == ">=") return Value::ofInt(a >= b);
        if (op == "==") return Value::ofInt(a == b);
        if (op == "!=") return Value::ofInt(a != b);
        if (op == "<<") return Value::ofInt(a << b);
        if (op == ">>") return Value::ofInt(a >> b);
        if (op == "&") return Value::ofInt(a & b);
        if (op == "|") return Value::ofInt(a | b);
        if (op == "^") return Value::ofInt(a ^ b);
    }
    throw RuntimeError(line, "unsupported operator '" + op + "'");
}

Value Interpreter::call(Node* node) {
    const std::string& name = node->resolved;
    if (name == "builtin_sqrt") {
        return Value::ofDouble(std::sqrt(eval(node->kids[0]).asDouble()));
    }
    if (name == "builtin_pow") {
        double base = eval(node->kids[0]).asDouble();
        return Value::ofDouble(std::pow(base, eval(node->kids[1]).asDouble()));
    }
    if (name == "builtin_fabs" || name == "builtin_abs_double") {
        return Value::ofDouble(std::fabs(eval(node->kids[0]).asDouble()));
    }
    if (name == "builtin_abs_int") {
        return Value::ofInt(std::llabs(eval(node->kids[0]).asInt()));
    }

    auto found = functions.find(name);
    if (found == functions.end()) {
        throw RuntimeError(node->line, "function '" + node->text + "' has no definition");
    }
    Node* function = found->second;
    const auto& params = function->kids[0]->kids;
    std::vector<Argument> args;
    for (size_t i = 0; i < node->kids.size(); ++i) {
        Node* param = params[i];
        Node* arg = node->kids[i];
        Argument argument;
        if (param->isArray) {
            argument.elements = find(arg->text, arg->line)->elements;
        } else if (param->isRef && !param->isConst) {
            if (arg->kind == NodeKind::Index) {
                std::shared_ptr<std::vector<Value>> owner;
                argument.alias = element(arg, owner);
                argument.keepAlive = owner;
            } else {
                auto variable = find(arg->text, arg->line);
                argument.alias = &variable->ref();
                argument.keepAlive = variable;
            }
        } else {
            argument.value = eval(arg);
        }
        args.push_back(argument);
    }
    return invoke(function, args, node->line);
}

Value Interpreter::invoke(Node* function, const std::vector<Argument>& args, int line) {
    if (++depth > kMaxDepth) {
        throw RuntimeError(line, "stack overflow: recursion deeper than " + std::to_string(kMaxDepth) + " calls");
    }
    std::vector<Scope> frame(1);
    const auto& params = function->kids[0]->kids;
    for (size_t i = 0; i < params.size(); ++i) {
        auto variable = std::make_shared<Variable>();
        variable->type = params[i]->dtype;
        variable->isArray = params[i]->isArray;
        if (variable->isArray) {
            variable->elements = args[i].elements;
        } else if (args[i].alias) {
            variable->alias = args[i].alias;
            variable->keepAlive = args[i].keepAlive;
        } else {
            variable->scalar = convert(args[i].value, variable->type);
        }
        frame[0][params[i]->text] = variable;
    }
    frames.push_back(std::move(frame));
    Flow flow = execItems(function->kids[1]->kids);
    frames.pop_back();
    --depth;
    if (function->dtype == "void") {
        return Value::ofInt(0);
    }
    return flow == Flow::Return ? convert(returnValue, function->dtype) : zeroOf(function->dtype);
}

std::string Interpreter::formatDouble(double value) const {
    const char* format = stream.fixed ? "%.*f" : "%.*g";
    int length = std::snprintf(nullptr, 0, format, stream.precision, value);
    std::string buffer(static_cast<size_t>(length) + 1, '\0');
    std::snprintf(&buffer[0], buffer.size(), format, stream.precision, value);
    buffer.resize(static_cast<size_t>(length));
    return buffer;
}

void Interpreter::put(const std::string& text) {
    std::string padded = text;
    if (stream.width > static_cast<long long>(text.size())) {
        std::string padding(static_cast<size_t>(stream.width) - text.size(), ' ');
        padded = stream.left ? text + padding : padding + text;
    }
    stream.width = 0;
    out << padded;
}

void Interpreter::print(Node* item) {
    if (item->type == "manip") {
        const std::string& name = item->text;
        if (item->kind == NodeKind::Call) {
            long long argument = eval(item->kids[0]).asInt();
            if (name == "setw") {
                stream.width = argument;
            } else {
                stream.precision = static_cast<int>(argument);
            }
        } else if (name == "endl") {
            out << '\n';
            out.flush();
        } else if (name == "fixed") {
            stream.fixed = true;
        } else if (name == "left") {
            stream.left = true;
        } else if (name == "right") {
            stream.left = false;
        } else if (name == "boolalpha") {
            stream.boolalpha = true;
        }
        return;
    }
    if (item->type == "string") {
        Node* text = item;
        while (text->kind == NodeKind::Ternary) {
            text = eval(text->kids[0]).truthy() ? text->kids[1] : text->kids[2];
        }
        put(text->sval);
        return;
    }
    Value value = eval(item);
    if (item->type == "char") {
        put(std::string(1, static_cast<char>(value.asInt())));
    } else if (item->type == "bool") {
        bool flag = value.truthy();
        put(stream.boolalpha ? (flag ? "true" : "false") : (flag ? "1" : "0"));
    } else if (item->type == "double") {
        put(formatDouble(value.asDouble()));
    } else {
        put(std::to_string(value.asInt()));
    }
}
