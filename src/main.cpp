#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "ast.hpp"
#include "codegen_js.hpp"
#include "interpreter.hpp"
#include "parser.tab.hpp"
#include "semantic.hpp"

extern FILE* yyin;
extern char* yytext;
extern int yylineno;
extern int yycolumn;
extern Node* g_program;
int yylex();
void yyrestart(FILE* file);
std::string tokenName(int token);

namespace {

struct Options {
    std::string input;
    bool tokens = false;
    bool trace = false;
    bool ast = false;
    bool run = false;
    std::string jsFile;
    std::string jsonFile;
    long long maxSteps = 0;
};

void usage(const char* program) {
    std::cerr << "Usage: " << program << " <file.cpp> [options]\n"
              << "  --tokens          print the token stream (lexical analysis)\n"
              << "  --trace           print the LALR parser trace (shift/reduce steps)\n"
              << "  --ast             print the abstract syntax tree\n"
              << "  --run             interpret the program\n"
              << "  --js <file|->     translate the program to JavaScript\n"
              << "  --json <file|->   write AST, diagnostics, JS and output as JSON (for the web UI)\n"
              << "  --max-steps <n>   stop the interpreter after n steps\n"
              << "Without options: --ast --run\n";
}

bool parseArgs(int argc, char** argv, Options& options) {
    bool anyAction = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](std::string& target) {
            if (i + 1 >= argc) {
                return false;
            }
            target = argv[++i];
            return true;
        };
        if (arg == "--tokens") {
            options.tokens = anyAction = true;
        } else if (arg == "--trace") {
            options.trace = anyAction = true;
        } else if (arg == "--ast") {
            options.ast = anyAction = true;
        } else if (arg == "--run") {
            options.run = anyAction = true;
        } else if (arg == "--js") {
            if (!next(options.jsFile)) return false;
            anyAction = true;
        } else if (arg == "--json") {
            if (!next(options.jsonFile)) return false;
            anyAction = true;
        } else if (arg == "--max-steps") {
            std::string value;
            if (!next(value)) return false;
            options.maxSteps = std::stoll(value);
        } else if (arg == "-h" || arg == "--help") {
            return false;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "unknown option " << arg << "\n";
            return false;
        } else if (options.input.empty()) {
            options.input = arg;
        } else {
            return false;
        }
    }
    if (!anyAction) {
        options.ast = options.run = true;
    }
    return !options.input.empty();
}

void dumpTokens(std::ostream& out) {
    out << std::left << std::setw(10) << "LINE:COL" << std::setw(22) << "TOKEN" << "LEXEME\n";
    int token;
    while ((token = yylex()) != 0) {
        std::string position = std::to_string(yylloc.first_line) + ":" + std::to_string(yylloc.first_column);
        out << std::setw(10) << position << std::setw(22) << tokenName(token) << yytext << "\n";
    }
}

void printDiagnostics(const std::string& file) {
    for (const Diagnostic& d : g_diagnostics) {
        std::cerr << file << ":" << d.line << ":" << d.col << ": [" << d.stage << "] " << d.message << "\n";
    }
}

std::string diagnosticsJson() {
    std::string result = "[";
    for (size_t i = 0; i < g_diagnostics.size(); ++i) {
        const Diagnostic& d = g_diagnostics[i];
        if (i > 0) {
            result += ",";
        }
        result += "{\"stage\":\"" + jsonEscape(d.stage) + "\",\"line\":" + std::to_string(d.line) +
                  ",\"col\":" + std::to_string(d.col) + ",\"message\":\"" + jsonEscape(d.message) + "\"}";
    }
    return result + "]";
}

bool writeOutput(const std::string& target, const std::string& content) {
    if (target == "-") {
        std::cout << content;
        return true;
    }
    std::ofstream file(target);
    if (!file) {
        std::cerr << "cannot write " << target << "\n";
        return false;
    }
    file << content;
    return true;
}

}

int main(int argc, char** argv) {
    Options options;
    if (!parseArgs(argc, argv, options)) {
        usage(argv[0]);
        return 64;
    }
    yyin = std::fopen(options.input.c_str(), "r");
    if (!yyin) {
        std::perror(options.input.c_str());
        return 66;
    }

    bool jsonMode = !options.jsonFile.empty();
    bool sections = !jsonMode && (options.tokens || options.ast || options.jsFile == "-");

    if (options.tokens) {
        if (sections) std::cout << "==== TOKENS ====\n";
        dumpTokens(std::cout);
        g_diagnostics.clear();
        std::rewind(yyin);
        yyrestart(yyin);
        yylineno = 1;
        yycolumn = 1;
    }

    yydebug = options.trace ? 1 : 0;
    yyparse();
    std::fclose(yyin);

    bool syntaxOk = g_diagnostics.empty() && g_program != nullptr;
    bool semanticOk = false;
    if (syntaxOk) {
        SemanticAnalyzer analyzer;
        semanticOk = analyzer.analyze(g_program);
    }

    if (!jsonMode) {
        printDiagnostics(options.input);
    }

    if (options.ast && g_program && !jsonMode) {
        if (sections) std::cout << "==== AST ====\n";
        printTree(g_program, std::cout);
    }

    std::string js;
    if (semanticOk && (!options.jsFile.empty() || jsonMode)) {
        JsGenerator generator;
        js = generator.generate(g_program);
        if (!options.jsFile.empty()) {
            if (options.jsFile == "-" && sections) std::cout << "==== JAVASCRIPT ====\n";
            writeOutput(options.jsFile, js);
        }
    }

    int exitCode = !syntaxOk ? 1 : !semanticOk ? 2 : 0;
    std::string programOutput;
    std::string runtimeJson = "null";
    bool ran = false;
    if (semanticOk && options.run) {
        std::ostringstream captured;
        std::ostream& target = jsonMode ? static_cast<std::ostream&>(captured) : std::cout;
        if (sections) std::cout << "==== PROGRAM OUTPUT ====\n";
        std::cout.flush();
        Interpreter interpreter(g_program, target, options.maxSteps);
        try {
            exitCode = interpreter.run();
            ran = true;
        } catch (const RuntimeError& e) {
            target.flush();
            if (jsonMode) {
                runtimeJson = "{\"line\":" + std::to_string(e.line) + ",\"message\":\"" + jsonEscape(e.what()) + "\"}";
            } else {
                std::cout.flush();
                std::cerr << options.input << ":" << e.line << ": runtime error: " << e.what() << "\n";
            }
            exitCode = 3;
        }
        programOutput = captured.str();
        if (sections && ran) std::cout << "==== EXIT CODE " << exitCode << " ====\n";
    }

    if (jsonMode) {
        std::ifstream sourceFile(options.input);
        std::stringstream sourceText;
        sourceText << sourceFile.rdbuf();
        std::ostringstream json;
        json << "{\"ok\":" << (semanticOk ? "true" : "false")
             << ",\"source\":\"" << jsonEscape(sourceText.str()) << "\""
             << ",\"stage\":\"" << (!syntaxOk ? "syntax" : !semanticOk ? "semantic" : "done") << "\""
             << ",\"diagnostics\":" << diagnosticsJson()
             << ",\"ast\":" << (g_program ? toJson(g_program) : "null")
             << ",\"js\":\"" << jsonEscape(js) << "\""
             << ",\"ran\":" << (ran ? "true" : "false")
             << ",\"output\":\"" << jsonEscape(programOutput) << "\""
             << ",\"exitCode\":" << exitCode
             << ",\"runtimeError\":" << runtimeJson << "}\n";
        writeOutput(options.jsonFile, json.str());
    }
    return exitCode;
}
