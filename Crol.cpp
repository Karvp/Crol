#include <iostream>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "ast.h"
#include "object.h"
#include "natives.h"

using namespace std;

void cleanAST(Node* root) {
    AST ast;
    ast.startScan(root);
    ast.freeNodes();
}

bool optimizeAST(Node*& root, ValuePool* pool) {
    ASTOptimizer astopt(root, pool);
    if (astopt.start()) {
        root = astopt.getRoot();
        return true;
    }
    else {
        cleanAST(root);
        return false;
    }
}

void optimizeBytecode(Chunk* chunk, bool print_bytecode = false) {
    BytecodeOptimizer bc(chunk);
    bc.optimize();
    if (print_bytecode) {
        cout << "\n<Main>\n";
        chunk->print();
    }
}

void run(string& source) {

    Lexer lex;
    if (!lex.start(source))
        return;

    Parser parser;
    bool parsed_as_expr;
    Node* root = parser.parse(lex.getTokens(), parsed_as_expr);

    if (!root) {
        return;
    }

    ValuePool pool;
    NativeBase* base = NativeBase::generate(&pool);

    if (!optimizeAST(root, &pool)) {
        delete base;
        return;
    }

    Compiler cp(root, &pool);
    bool success = cp.start({});

    cleanAST(root);
    if (!success) {
        delete base;
        return;
    }

    auto mainfunc = cp.produceFunction();
    ClosureObject* main = new ClosureObject(mainfunc);

    VM vm(main, base);
    vm.associate(&pool);

    pool.registerObject(mainfunc);
    pool.registerObject(main);

    vm.run();

    if (parsed_as_expr) {
        TValue::print(*vm.stack_end--);
    }

    delete base;
}

void runCMDLine() {

    Lexer lex;
    Parser parser;
    bool parsed_as_expr;

    ValuePool pool;
    NativeBase* base = NativeBase::generate(&pool);

    Compiler cp(&pool);

    VM vm(base);
    vm.associate(&pool);

    string source;
    cout << ">>> ";
    getline(cin, source);

    while (!source.empty()) {

        if (!lex.start(source)) {
            cout << ">>> ";
            getline(cin, source);
            continue;
        }

        Node* root = parser.parse(lex.getTokens(), parsed_as_expr);

        if (!root) {
            cout << ">>> ";
            getline(cin, source);
            continue;
        }

        if (!optimizeAST(root, &pool)) {
            cout << ">>> ";
            getline(cin, source);
            continue;
        }

        cp.initialize(root);
        bool success = cp.start({}, true);

        cleanAST(root);
        if (!success) {
            cout << ">>> ";
            getline(cin, source);
            continue;
        }

        auto mainfunc = cp.produceFunction();
        auto main = new ClosureObject(mainfunc);

#ifdef PRINT_CHUNK_AFTER_COMPILED
        mainfunc->print();
        printf("\n");
        mainfunc->chunk->print();
        printf("\n");
#endif

        vm.initialize(main);

        pool.silentRegisterObject(mainfunc);
        pool.silentRegisterObject(main);

        vm.run();

        if (parsed_as_expr && vm.execution_finished) {
            TValue::print(*vm.stack_end--);
        }

        printf("\n>>> " + !parsed_as_expr);
        getline(cin, source);
    }
}

int main()
{
    runCMDLine();
    
    reprValue(toValue(1.0)).as.num;
    return 0;
}
