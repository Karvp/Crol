#include <iostream>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "ast.h"
#include "object.h"
#include "natives.h"
#include "state.h"

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

bool compileAST(Node* root, ValuePool* pool, NativeBase* base, Chunk*& chunk, ExecutionState* state) {
    Compiler cp(root, pool);

    Compiler::name_array predefined_names = base->getNativeIDs();
    ExecutionState::global_mapping_type* globals_ref_map = NULL;
    if (state) {
        state->loadNames(&cp, predefined_names);
        globals_ref_map = &state->get_ref_map();
    }

    bool success = cp.start(predefined_names, globals_ref_map);

    cleanAST(root);
    chunk = cp.getChunk();

    if (state) {
        state->cleanGlobalsMap(base);
    }

    return success;
}

void optimizeBytecode(Chunk* chunk) {
    Code code(*chunk);
    code.optimizeBytecode();
    cout << "\n<Main>\n";
    code.chunk.print();
}

void run(string& source) {

    Lexer lex(source);
    if (!lex.start())
        return;

    Parser parser(lex.getTokens());
    Node* root = parser.parse();

    if (!root) {
        return;
    }

    ValuePool pool;
    NativeBase* base = NativeBase::generate(&pool);

    if (!optimizeAST(root, &pool)) {
        delete base;
        return;
    }

    Chunk* chunk = NULL;

    if (!compileAST(root, &pool, base, chunk, NULL)) {
        delete base;
        return;
    }

    optimizeBytecode(chunk);

    FunctionObject* mainfunc = new FunctionObject("(main)", chunk, 0);
    ClosureObject* main = new ClosureObject(mainfunc);

    VM vm(main, base);
    vm.associate(&pool);

    pool.registerObject(mainfunc);
    pool.registerObject(main);

    vm.run();

    delete base;
}

void runCMDLine() {

    ExecutionState state;

    string source;
    cout << ">>> ";
    getline(cin, source);

    while (!source.empty()) {

        Lexer lex(source);
        if (!lex.start())
            continue;

        Parser parser(lex.getTokens());
        Node* root = parser.parse();

        if (!root) {
            cout << ">>> ";
            getline(cin, source);
            continue;
        }

        ValuePool pool;
        NativeBase* base = NativeBase::generate(&pool);

        if (!optimizeAST(root, &pool)) {
            delete base;
            cout << ">>> ";
            getline(cin, source);
            continue;
        }

        Chunk* chunk = NULL;

        if (!compileAST(root, &pool, base, chunk, &state)) {
            delete base;
            cout << ">>> ";
            getline(cin, source);
            continue;
        }

        //optimizeBytecode(chunk);
        //chunk->print();

        FunctionObject* mainfunc = new FunctionObject("(main)", chunk, 0);
        ClosureObject* main = new ClosureObject(mainfunc);

        VM vm(main, base);
        vm.associate(&pool);

        pool.registerObject(mainfunc);
        pool.registerObject(main);

        state.loadSnapshot(&vm);
        vm.run();
        state.snapshot(&vm);

        delete base;
        cout << ">>> ";
        getline(cin, source);
    }
}

int main()
{
    runCMDLine();
    return 0;
}
