#include "semantic.hpp"

#include <algorithm>
#include <set>

namespace {

const std::set<std::string> kStreams = {"cout"};
const std::set<std::string> kManipulators = {"endl", "fixed", "left", "right", "boolalpha"};

bool isNumeric(const std::string& type) {
    return type == "int" || type == "char" || type == "bool" || type == "double";
}

bool isIntegral(const std::string& type) {
    return type == "int" || type == "char" || type == "bool";
}

bool isArrayType(const std::string& type) {
    return type.size() > 2 && type.compare(type.size() - 2, 2, "[]") == 0;
}

std::string elementType(const std::string& type) {
    return type.substr(0, type.size() - 2);
}

std::string arithmeticType(const std::string& a, const std::string& b) {
    return (a == "double" || b == "double") ? "double" : "int";
}

bool isComparison(const std::string& op) {
    return op == "<" || op == ">" || op == "<=" || op == ">=" || op == "==" || op == "!=";
}

bool isLogical(const std::string& op) {
    return op == "&&" || op == "||";
}

bool isIntegerOnly(const std::string& op) {
    return op == "%" || op == "<<" || op == ">>" || op == "&" || op == "|" || op == "^";
}

std::string paramText(const ParamType& param) {
    std::string text = (param.isConst && (param.isRef || param.isArray)) ? "const " : "";
    text += param.type;
    if (param.isRef) {
        text += "&";
    }
    if (param.isArray) {
        text += "[]";
    }
    return text;
}

std::string paramCode(const ParamType& param) {
    return param.type + (param.isArray ? "Arr" : "") + (param.isRef ? "Ref" : "");
}

bool sameParams(const std::vector<ParamType>& a, const std::vector<ParamType>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        const ParamType& x = a[i];
        const ParamType& y = b[i];
        if (x.type != y.type || x.isArray != y.isArray || x.isRef != y.isRef) {
            return false;
        }
        if ((x.isArray || x.isRef) && x.isConst != y.isConst) {
            return false;
        }
    }
    return true;
}

std::string joinTypes(const std::vector<std::string>& types) {
    std::string result;
    for (size_t i = 0; i < types.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += types[i];
    }
    return result;
}

}

void SemanticAnalyzer::error(const Node* node, const std::string& message) {
    addDiagnostic("semantic", node ? node->line : 0, node ? node->col : 0, message);
    ++errors;
}

std::string SemanticAnalyzer::signature(const Overload& overload) {
    std::string text = overload.name + "(";
    for (size_t i = 0; i < overload.params.size(); ++i) {
        if (i > 0) {
            text += ", ";
        }
        text += paramText(overload.params[i]);
    }
    return text + ")";
}

bool SemanticAnalyzer::analyze(Node* program) {
    errors = 0;
    loopDepth = 0;
    usingStd = false;
    scopes.clear();
    functions.clear();
    functionNodes.clear();
    callTargets.clear();
    scopes.emplace_back();
    declareBuiltins();

    for (Node* node : program->kids) {
        switch (node->kind) {
            case NodeKind::Using:
                visitUsing(node);
                break;
            case NodeKind::DeclStmt:
                visitDecl(node, true);
                break;
            case NodeKind::FuncDef:
            case NodeKind::FuncDecl:
                visitFunction(node);
                break;
            default:
                break;
        }
    }

    bool hasMain = false;
    auto mainIt = functions.find("main");
    if (mainIt != functions.end()) {
        for (const Overload& overload : mainIt->second) {
            hasMain = hasMain || (!overload.builtin && overload.defined);
        }
    }
    if (!hasMain) {
        error(program, "function 'main' is not defined");
    }
    for (auto& [name, list] : functions) {
        for (Overload& overload : list) {
            if (overload.called && !overload.defined) {
                error(overload.firstCall, "undefined reference to '" + signature(overload) + "'");
            }
        }
    }
    assignResolvedNames();
    return errors == 0;
}

void SemanticAnalyzer::declareBuiltins() {
    auto add = [this](const std::string& name, const std::string& ret, const std::vector<std::string>& params,
                      bool stdOnly) {
        Overload overload;
        overload.name = name;
        overload.returnType = ret;
        for (const std::string& type : params) {
            ParamType param;
            param.type = type;
            overload.params.push_back(param);
        }
        overload.defined = true;
        overload.builtin = true;
        overload.stdOnly = stdOnly;
        functions[name].push_back(overload);
    };
    add("sqrt", "double", {"double"}, false);
    add("pow", "double", {"double", "double"}, false);
    add("fabs", "double", {"double"}, false);
    add("abs", "int", {"int"}, false);
    add("abs", "double", {"double"}, false);
    add("setw", "manip", {"int"}, true);
    add("setprecision", "manip", {"int"}, true);
}

void SemanticAnalyzer::assignResolvedNames() {
    for (auto& [name, list] : functions) {
        int userDefined = 0;
        for (const Overload& overload : list) {
            userDefined += overload.builtin ? 0 : 1;
        }
        for (Overload& overload : list) {
            if (overload.builtin) {
                overload.resolved = "builtin_" + name + (name == "abs" ? "_" + overload.params[0].type : "");
                continue;
            }
            if (userDefined <= 1) {
                overload.resolved = name;
                continue;
            }
            std::string mangled = name;
            for (const ParamType& param : overload.params) {
                mangled += "_" + paramCode(param);
            }
            if (overload.params.empty()) {
                mangled += "_void";
            }
            overload.resolved = mangled;
        }
    }
    for (auto& [node, overload] : functionNodes) {
        node->resolved = overload->resolved;
    }
    for (auto& [node, overload] : callTargets) {
        node->resolved = overload->resolved;
    }
}

Symbol* SemanticAnalyzer::lookup(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

void SemanticAnalyzer::declare(const Node* at, const std::string& name, const Symbol& symbol) {
    auto& scope = scopes.back();
    if (scope.count(name)) {
        error(at, "redeclaration of '" + name + "'");
        return;
    }
    scope[name] = symbol;
}

void SemanticAnalyzer::visitUsing(Node* node) {
    if (node->text != "std") {
        error(node, "'" + node->text + "' is not a namespace-name");
        return;
    }
    usingStd = true;
}

void SemanticAnalyzer::visitFunction(Node* node) {
    const std::string& name = node->text;
    Overload candidate;
    candidate.name = name;
    candidate.returnType = node->dtype;
    if (node->dtype == "auto") {
        error(node, "functions with an 'auto' return type are not supported");
        candidate.returnType = "int";
    }
    for (Node* param : node->kids[0]->kids) {
        if (param->dtype == "void" || param->dtype == "auto") {
            error(param, "parameter '" + param->text + "' cannot have type '" + param->dtype + "'");
        }
        ParamType type;
        type.type = param->dtype;
        type.isArray = param->isArray;
        type.isRef = param->isRef;
        type.isConst = param->isConst;
        candidate.params.push_back(type);
    }
    if (name == "main") {
        if (candidate.returnType != "int") {
            error(node, "'::main' must return 'int'");
        }
        if (!candidate.params.empty()) {
            error(node, "'main' must not take parameters in this C++ subset");
        }
    }
    if (scopes.front().count(name)) {
        error(node, "'" + name + "' redeclared as a different kind of entity");
        return;
    }

    auto& list = functions[name];
    Overload* target = nullptr;
    for (Overload& existing : list) {
        if (!sameParams(existing.params, candidate.params)) {
            continue;
        }
        if (existing.builtin) {
            error(node, "redefinition of library function '" + signature(existing) + "'");
            return;
        }
        if (existing.returnType != candidate.returnType) {
            error(node, "functions that differ only in their return type cannot be overloaded: '" +
                            signature(candidate) + "'");
            return;
        }
        target = &existing;
    }
    if (!target && name == "main") {
        for (const Overload& existing : list) {
            if (!existing.builtin) {
                error(node, "'main' cannot be overloaded");
                return;
            }
        }
    }
    if (!target) {
        list.push_back(candidate);
        target = &list.back();
    }
    functionNodes[node] = target;
    if (node->kind == NodeKind::FuncDecl) {
        return;
    }
    if (target->defined) {
        error(node, "redefinition of '" + signature(*target) + "'");
    }
    target->defined = true;

    currentReturnType = candidate.returnType;
    scopes.emplace_back();
    for (Node* param : node->kids[0]->kids) {
        Symbol symbol;
        symbol.type = param->dtype;
        symbol.isArray = param->isArray;
        symbol.isConst = param->isConst;
        symbol.isRef = param->isRef && !param->isConst;
        declare(param, param->text, symbol);
    }
    for (Node* item : node->kids[1]->kids) {
        visitStmt(item);
    }
    scopes.pop_back();
}

void SemanticAnalyzer::visitDecl(Node* node, bool global) {
    for (Node* var : node->kids) {
        Node* init = var->init;
        bool initChecked = false;
        if (var->dtype == "void") {
            error(var, "variable '" + var->text + "' declared void");
            var->dtype = "int";
        }
        if (var->isAuto) {
            std::string deduced = "int";
            if (var->isArray) {
                error(var, "'" + var->text + "' declared as an array of 'auto'");
            } else if (!init) {
                error(var, "declaration of 'auto " + var->text + "' has no initializer");
            } else if (init->kind == NodeKind::InitList) {
                error(init, "cannot deduce 'auto' from a brace-enclosed list");
            } else {
                std::string type = visitExpr(init);
                initChecked = true;
                if (isNumeric(type)) {
                    deduced = type;
                } else {
                    requireNumeric(init, type);
                }
            }
            var->dtype = deduced;
        }

        if (var->isArray) {
            if (var->sizeExpr) {
                std::string sizeType = visitExpr(var->sizeExpr);
                std::optional<long long> size = evalConst(var->sizeExpr);
                if (!isIntegral(sizeType) || !size) {
                    error(var->sizeExpr, "size of array '" + var->text + "' is not an integral constant expression");
                } else if (*size <= 0) {
                    error(var->sizeExpr, "size of array '" + var->text + "' is not positive");
                } else {
                    var->arraySize = *size;
                }
            }
            if (init && init->kind != NodeKind::InitList) {
                error(init, "array '" + var->text + "' must be initialized with a brace-enclosed list");
            } else if (init) {
                long long count = static_cast<long long>(init->kids.size());
                if (!var->sizeExpr) {
                    var->arraySize = count;
                } else if (var->arraySize > 0 && count > var->arraySize) {
                    error(init, "too many initializers for array '" + var->text + "'");
                }
                for (Node* element : init->kids) {
                    std::string type = visitExpr(element);
                    if (requireNumeric(element, type) && type == "double" && isIntegral(var->dtype)) {
                        error(element, "narrowing conversion from 'double' to '" + var->dtype + "' inside { }");
                    }
                }
            } else if (!var->sizeExpr) {
                error(var, "array size missing in '" + var->text + "'");
            }
        } else if (init) {
            if (init->kind == NodeKind::InitList) {
                error(init, "scalar '" + var->text + "' initialized with a brace-enclosed list");
            } else if (!initChecked) {
                requireNumeric(init, visitExpr(init));
            }
        }
        if (var->isConst && !init) {
            error(var, "uninitialized const '" + var->text + "'");
        }

        Symbol symbol;
        symbol.type = var->dtype;
        symbol.isArray = var->isArray;
        symbol.isConst = var->isConst;
        if (var->isConst && !var->isArray && init && init->kind != NodeKind::InitList && isIntegral(var->dtype)) {
            std::optional<long long> value = evalConst(init);
            if (value) {
                if (var->dtype == "bool") {
                    symbol.constValue = *value != 0;
                } else if (var->dtype == "char") {
                    symbol.constValue = static_cast<signed char>(*value);
                } else {
                    symbol.constValue = *value;
                }
            }
        }
        if (global && functions.count(var->text)) {
            error(var, "'" + var->text + "' redeclared as a different kind of entity");
            continue;
        }
        declare(var, var->text, symbol);
    }
}

std::optional<long long> SemanticAnalyzer::evalConst(const Node* node) {
    switch (node->kind) {
        case NodeKind::IntLit:
        case NodeKind::CharLit:
        case NodeKind::BoolLit:
            return node->ival;
        case NodeKind::Ident: {
            if (!node->qualifier.empty()) {
                return std::nullopt;
            }
            Symbol* symbol = lookup(node->text);
            if (symbol && symbol->constValue) {
                return symbol->constValue;
            }
            return std::nullopt;
        }
        case NodeKind::Unary: {
            std::optional<long long> value = evalConst(node->kids[0]);
            if (!value) return std::nullopt;
            if (node->text == "-") return -*value;
            if (node->text == "+") return *value;
            if (node->text == "~") return ~*value;
            if (node->text == "!") return static_cast<long long>(!*value);
            return std::nullopt;
        }
        case NodeKind::Cast:
            if (!isIntegral(node->dtype)) {
                return std::nullopt;
            }
            return evalConst(node->kids[0]);
        case NodeKind::Ternary: {
            std::optional<long long> condition = evalConst(node->kids[0]);
            if (!condition) return std::nullopt;
            return evalConst(*condition ? node->kids[1] : node->kids[2]);
        }
        case NodeKind::Binary: {
            std::optional<long long> a = evalConst(node->kids[0]);
            std::optional<long long> b = evalConst(node->kids[1]);
            if (!a || !b) return std::nullopt;
            const std::string& op = node->text;
            if (op == "+") return *a + *b;
            if (op == "-") return *a - *b;
            if (op == "*") return *a * *b;
            if (op == "/") return *b == 0 ? std::nullopt : std::optional<long long>(*a / *b);
            if (op == "%") return *b == 0 ? std::nullopt : std::optional<long long>(*a % *b);
            if (op == "<<") return *a << *b;
            if (op == ">>") return *a >> *b;
            if (op == "&") return *a & *b;
            if (op == "|") return *a | *b;
            if (op == "^") return *a ^ *b;
            if (op == "<") return static_cast<long long>(*a < *b);
            if (op == ">") return static_cast<long long>(*a > *b);
            if (op == "<=") return static_cast<long long>(*a <= *b);
            if (op == ">=") return static_cast<long long>(*a >= *b);
            if (op == "==") return static_cast<long long>(*a == *b);
            if (op == "!=") return static_cast<long long>(*a != *b);
            if (op == "&&") return static_cast<long long>(*a && *b);
            if (op == "||") return static_cast<long long>(*a || *b);
            return std::nullopt;
        }
        default:
            return std::nullopt;
    }
}

void SemanticAnalyzer::visitCondition(Node* node) {
    requireNumeric(node, visitExpr(node));
}

void SemanticAnalyzer::visitLoopBody(Node* body) {
    if (body->kind == NodeKind::Block) {
        for (Node* item : body->kids) {
            visitStmt(item);
        }
    } else {
        visitStmt(body);
    }
}

void SemanticAnalyzer::visitRangeFor(Node* node) {
    scopes.emplace_back();
    Node* var = node->kids[0];
    Node* range = node->kids[1];
    std::string rangeType = visitExpr(range);
    std::string element = "int";
    Symbol* arraySymbol = nullptr;
    if (range->kind != NodeKind::Ident || !isArrayType(rangeType)) {
        error(range, "range-based 'for' needs an array, got '" + rangeType + "'");
    } else {
        element = elementType(rangeType);
        arraySymbol = lookup(range->text);
    }
    if (var->isAuto) {
        var->dtype = element;
    } else if (var->dtype == "void") {
        error(var, "variable '" + var->text + "' declared void");
        var->dtype = "int";
    }
    bool constArray = arraySymbol && arraySymbol->isConst;
    if (var->isRef) {
        if (var->dtype != element) {
            error(var, "cannot bind reference of type '" + var->dtype + "&' to an element of type '" + element + "'");
        }
        if (!var->isConst && constArray) {
            error(var, "binding reference of type '" + var->dtype + "&' to a const array discards qualifiers");
        }
    }
    Symbol symbol;
    symbol.type = var->dtype;
    symbol.isConst = var->isConst || (var->isRef && constArray);
    symbol.isRef = var->isRef;
    declare(var, var->text, symbol);
    ++loopDepth;
    visitLoopBody(node->kids[2]);
    --loopDepth;
    scopes.pop_back();
}

void SemanticAnalyzer::visitStmt(Node* node) {
    switch (node->kind) {
        case NodeKind::Block:
            scopes.emplace_back();
            for (Node* item : node->kids) {
                visitStmt(item);
            }
            scopes.pop_back();
            break;
        case NodeKind::DeclStmt:
            visitDecl(node, false);
            break;
        case NodeKind::ExprStmt:
            visitExpr(node->kids[0]);
            break;
        case NodeKind::Empty:
            break;
        case NodeKind::If:
            visitCondition(node->kids[0]);
            visitStmt(node->kids[1]);
            if (node->kids.size() > 2) {
                visitStmt(node->kids[2]);
            }
            break;
        case NodeKind::While:
            visitCondition(node->kids[0]);
            ++loopDepth;
            visitStmt(node->kids[1]);
            --loopDepth;
            break;
        case NodeKind::DoWhile:
            ++loopDepth;
            visitStmt(node->kids[0]);
            --loopDepth;
            visitCondition(node->kids[1]);
            break;
        case NodeKind::For: {
            scopes.emplace_back();
            Node* init = node->kids[0];
            if (init->kind == NodeKind::DeclStmt) {
                visitDecl(init, false);
            } else if (init->kind != NodeKind::Empty) {
                visitExpr(init);
            }
            if (node->kids[1]->kind != NodeKind::Empty) {
                visitCondition(node->kids[1]);
            }
            if (node->kids[2]->kind != NodeKind::Empty) {
                visitExpr(node->kids[2]);
            }
            ++loopDepth;
            visitLoopBody(node->kids[3]);
            --loopDepth;
            scopes.pop_back();
            break;
        }
        case NodeKind::RangeFor:
            visitRangeFor(node);
            break;
        case NodeKind::Return:
            if (node->kids.empty()) {
                if (currentReturnType != "void") {
                    error(node, "return-statement with no value, in function returning '" + currentReturnType + "'");
                }
            } else {
                std::string type = visitExpr(node->kids[0]);
                if (currentReturnType == "void") {
                    error(node, "return-statement with a value, in function returning 'void'");
                } else {
                    requireNumeric(node->kids[0], type);
                }
            }
            break;
        case NodeKind::Break:
            if (loopDepth == 0) {
                error(node, "break statement not within loop");
            }
            break;
        case NodeKind::Continue:
            if (loopDepth == 0) {
                error(node, "continue statement not within loop");
            }
            break;
        default:
            visitExpr(node);
    }
}

bool SemanticAnalyzer::requireNumeric(const Node* node, const std::string& type) {
    if (isNumeric(type)) {
        return true;
    }
    if (type == "string") {
        error(node, "a string literal can only be printed with 'cout <<'");
    } else if (isArrayType(type)) {
        error(node, "array used as a value");
    } else if (type == "void") {
        error(node, "void value not ignored as it ought to be");
    } else if (type == "ostream") {
        error(node, "'cout' can only stand on the left of '<<'");
    } else if (type == "manip") {
        error(node, "a stream manipulator can only be used after 'cout <<'");
    } else {
        error(node, "expression of type '" + type + "' is not a number");
    }
    return false;
}

void SemanticAnalyzer::checkAssignable(const Node* target, const std::string& what) {
    if (target->kind == NodeKind::Ident && target->qualifier.empty()) {
        Symbol* symbol = lookup(target->text);
        if (!symbol) {
            if (kStreams.count(target->text) || kManipulators.count(target->text) || functions.count(target->text)) {
                error(target, "lvalue required as left operand of " + what);
            }
            return;
        }
        if (symbol->isArray) {
            error(target, "invalid array " + what + " to '" + target->text + "'");
        } else if (symbol->isConst) {
            error(target, what + " of read-only variable '" + target->text + "'");
        }
        return;
    }
    if (target->kind == NodeKind::Index) {
        Symbol* symbol = lookup(target->text);
        if (symbol && symbol->isConst) {
            error(target, what + " of read-only location '" + target->text + "[...]'");
        }
        return;
    }
    error(target, "lvalue required as left operand of " + what);
}

std::string SemanticAnalyzer::visitExpr(Node* node) {
    std::string type = computeType(node);
    node->type = type;
    return type;
}

std::string SemanticAnalyzer::identType(Node* node) {
    const std::string& name = node->text;
    bool stream = kStreams.count(name) > 0;
    bool manipulator = kManipulators.count(name) > 0;
    if (!node->qualifier.empty()) {
        if (node->qualifier != "std") {
            error(node, "'" + node->qualifier + "' has not been declared");
            return "int";
        }
        if (stream) return "ostream";
        if (manipulator) return "manip";
        error(node, "'" + name + "' is not a member of 'std' in this C++ subset");
        return "int";
    }
    if (Symbol* symbol = lookup(name)) {
        node->viaRef = symbol->isRef;
        return symbol->isArray ? symbol->type + "[]" : symbol->type;
    }
    if (stream || manipulator) {
        if (!usingStd) {
            error(node, "'" + name + "' was not declared in this scope; did you mean 'std::" + name + "'?");
        }
        return stream ? "ostream" : "manip";
    }
    if (functions.count(name)) {
        error(node, "function '" + name + "' used as a variable");
        return "int";
    }
    error(node, "'" + name + "' was not declared in this scope");
    return "int";
}

std::string SemanticAnalyzer::binaryType(Node* node) {
    const std::string& op = node->text;
    std::string left = visitExpr(node->kids[0]);
    if (op == "<<" && left == "ostream") {
        std::string right = visitExpr(node->kids[1]);
        if (!(isNumeric(right) || right == "string" || right == "manip")) {
            if (isArrayType(right)) {
                error(node->kids[1], "cannot print a whole array with 'cout <<' (print its elements in a loop)");
            } else if (right == "void") {
                error(node->kids[1], "cannot print a void value");
            } else {
                error(node->kids[1], "invalid operand of type '" + right + "' for 'cout <<'");
            }
        }
        return "ostream";
    }
    std::string right = visitExpr(node->kids[1]);
    bool ok = requireNumeric(node->kids[0], left);
    ok = requireNumeric(node->kids[1], right) && ok;
    if (ok && isIntegerOnly(op) && (!isIntegral(left) || !isIntegral(right))) {
        error(node, "invalid operands of types '" + left + "' and '" + right + "' to binary 'operator" + op + "'");
    }
    if (isComparison(op) || isLogical(op)) {
        return "bool";
    }
    if (isIntegerOnly(op) || !ok) {
        return "int";
    }
    return arithmeticType(left, right);
}

std::string SemanticAnalyzer::incrementType(Node* node) {
    const std::string& op = node->text;
    std::string operand = visitExpr(node->kids[0]);
    checkAssignable(node->kids[0], op == "++" ? "increment" : "decrement");
    if (operand == "bool") {
        error(node, "use of an operand of type 'bool' in 'operator" + op + "' is forbidden in C++17");
    }
    return requireNumeric(node->kids[0], operand) ? operand : "int";
}

std::string SemanticAnalyzer::computeType(Node* node) {
    const std::string& op = node->text;
    switch (node->kind) {
        case NodeKind::IntLit:
            return "int";
        case NodeKind::FloatLit:
            return "double";
        case NodeKind::CharLit:
            return "char";
        case NodeKind::BoolLit:
            return "bool";
        case NodeKind::StringLit:
            return "string";
        case NodeKind::Ident:
            return identType(node);
        case NodeKind::Index: {
            std::string indexType = visitExpr(node->kids[0]);
            if (!isIntegral(indexType)) {
                error(node->kids[0], "array subscript is not an integer");
            }
            if (!node->qualifier.empty()) {
                error(node, "'" + node->qualifier + "::" + node->text + "' is not an array");
                return "int";
            }
            Symbol* symbol = lookup(node->text);
            if (!symbol) {
                error(node, "'" + node->text + "' was not declared in this scope");
                return "int";
            }
            if (!symbol->isArray) {
                error(node, "invalid types for array subscript: '" + node->text + "' is not an array");
                return symbol->type;
            }
            return symbol->type;
        }
        case NodeKind::Call:
            return checkCall(node);
        case NodeKind::Assign: {
            std::string left = visitExpr(node->kids[0]);
            std::string right = visitExpr(node->kids[1]);
            checkAssignable(node->kids[0], "assignment");
            bool ok = requireNumeric(node->kids[1], right);
            std::string base = op.substr(0, op.size() - 1);
            if (ok && isIntegerOnly(base) && (!isIntegral(left) || !isIntegral(right))) {
                error(node, "invalid operands of types '" + left + "' and '" + right + "' to 'operator" + op + "'");
            }
            return isNumeric(left) ? left : "int";
        }
        case NodeKind::Binary:
            return binaryType(node);
        case NodeKind::Unary: {
            if (op == "++" || op == "--") {
                return incrementType(node);
            }
            std::string operand = visitExpr(node->kids[0]);
            bool ok = requireNumeric(node->kids[0], operand);
            if (op == "!") {
                return "bool";
            }
            if (op == "~") {
                if (ok && !isIntegral(operand)) {
                    error(node, "wrong type argument to bit-complement");
                }
                return "int";
            }
            return operand == "double" ? "double" : "int";
        }
        case NodeKind::Postfix:
            return incrementType(node);
        case NodeKind::Ternary: {
            visitCondition(node->kids[0]);
            std::string a = visitExpr(node->kids[1]);
            std::string b = visitExpr(node->kids[2]);
            if (a == "string" && b == "string") {
                return "string";
            }
            bool ok = requireNumeric(node->kids[1], a);
            ok = requireNumeric(node->kids[2], b) && ok;
            if (!ok) {
                return "int";
            }
            return a == b ? a : arithmeticType(a, b);
        }
        case NodeKind::Cast: {
            std::string operand = visitExpr(node->kids[0]);
            requireNumeric(node->kids[0], operand);
            if (node->dtype == "void" || node->dtype == "auto") {
                error(node, "invalid cast to '" + node->dtype + "'");
                return "int";
            }
            return node->dtype;
        }
        case NodeKind::Comma:
            visitExpr(node->kids[0]);
            return visitExpr(node->kids[1]);
        default:
            return "int";
    }
}

int SemanticAnalyzer::conversionCost(Node* arg, const std::string& argType, const ParamType& param,
                                     std::string& reason) {
    if (param.isArray) {
        if (arg->kind != NodeKind::Ident || argType != param.type + "[]") {
            reason = "expected an array of '" + param.type + "', got '" + argType + "'";
            return -1;
        }
        Symbol* symbol = lookup(arg->text);
        if (!param.isConst && symbol && symbol->isConst) {
            reason = "passing const array '" + arg->text + "' discards qualifiers";
            return -1;
        }
        return 0;
    }
    if (param.isRef && !param.isConst) {
        bool lvalue = arg->kind == NodeKind::Index ||
                      (arg->kind == NodeKind::Ident && arg->qualifier.empty() && lookup(arg->text) && !isArrayType(argType));
        if (!lvalue) {
            reason = "cannot bind non-const lvalue reference of type '" + param.type + "&' to an rvalue";
            return -1;
        }
        if (argType != param.type) {
            reason = "cannot bind reference of type '" + param.type + "&' to a value of type '" + argType + "'";
            return -1;
        }
        Symbol* symbol = lookup(arg->text);
        if (symbol && symbol->isConst) {
            reason = "binding reference of type '" + param.type + "&' to const '" + arg->text + "' discards qualifiers";
            return -1;
        }
        return 0;
    }
    if (!isNumeric(argType)) {
        reason = "cannot convert '" + argType + "' to '" + param.type + "'";
        return -1;
    }
    if (argType == param.type) {
        return 0;
    }
    if (param.type == "int" && (argType == "char" || argType == "bool")) {
        return 1;
    }
    return 2;
}

std::string SemanticAnalyzer::checkCall(Node* node) {
    const std::string& name = node->text;
    std::vector<std::string> argTypes;
    for (Node* arg : node->kids) {
        argTypes.push_back(visitExpr(arg));
    }
    std::string callText = name + "(" + joinTypes(argTypes) + ")";

    std::vector<Overload*> candidates;
    auto found = functions.find(name);
    if (!node->qualifier.empty()) {
        if (node->qualifier != "std") {
            error(node, "'" + node->qualifier + "' has not been declared");
            return "int";
        }
        if (found != functions.end()) {
            for (Overload& overload : found->second) {
                if (overload.builtin) {
                    candidates.push_back(&overload);
                }
            }
        }
        if (candidates.empty()) {
            error(node, "'" + name + "' is not a member of 'std' in this C++ subset");
            return "int";
        }
    } else {
        if (lookup(name)) {
            error(node, "'" + name + "' cannot be used as a function");
            return "int";
        }
        bool hiddenStd = false;
        if (found != functions.end()) {
            for (Overload& overload : found->second) {
                if (overload.stdOnly && !usingStd) {
                    hiddenStd = true;
                } else {
                    candidates.push_back(&overload);
                }
            }
        }
        if (candidates.empty()) {
            error(node, "'" + name + "' was not declared in this scope" +
                            (hiddenStd ? "; did you mean 'std::" + name + "'?" : std::string()));
            return "int";
        }
    }

    struct Option {
        Overload* overload;
        int cost;
    };
    std::vector<Option> viable;
    std::string reason;
    for (Overload* overload : candidates) {
        if (overload->params.size() != node->kids.size()) {
            if (candidates.size() == 1) {
                reason = "function '" + signature(*overload) + "' expects " + std::to_string(overload->params.size()) +
                         " argument(s), got " + std::to_string(node->kids.size());
            }
            continue;
        }
        int total = 0;
        bool ok = true;
        for (size_t i = 0; i < node->kids.size(); ++i) {
            std::string why;
            int cost = conversionCost(node->kids[i], argTypes[i], overload->params[i], why);
            if (cost < 0) {
                ok = false;
                if (candidates.size() == 1) {
                    reason = "argument " + std::to_string(i + 1) + " of '" + signature(*overload) + "': " + why;
                }
                break;
            }
            total += cost;
        }
        if (ok) {
            viable.push_back({overload, total});
        }
    }
    if (viable.empty()) {
        error(node, reason.empty() ? "no matching function for call to '" + callText + "'" : reason);
        return candidates.front()->returnType;
    }
    int best = viable.front().cost;
    for (const Option& option : viable) {
        best = std::min(best, option.cost);
    }
    std::vector<Overload*> winners;
    for (const Option& option : viable) {
        if (option.cost == best) {
            winners.push_back(option.overload);
        }
    }
    if (winners.size() > 1) {
        error(node, "call of overloaded '" + callText + "' is ambiguous");
        return winners.front()->returnType;
    }
    Overload* chosen = winners.front();
    chosen->called = true;
    if (!chosen->firstCall) {
        chosen->firstCall = node;
    }
    callTargets[node] = chosen;
    node->sval = signature(*chosen);
    return chosen->returnType;
}
