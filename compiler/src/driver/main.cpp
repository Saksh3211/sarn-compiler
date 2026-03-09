#include "slua/Lexer.h"
#include "slua/AST.h"
#include "slua/Diagnostics.h"
#include "slua/SemanticConfig.h"
#include "slua/Parser.h"
#include "slua/Resolver.h"
#include "slua/TypeChecker.h"
#ifdef SLUA_HAS_LLVM
#include "slua/IREmitter.h"
#endif
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { fprintf(stderr, "sluac: cannot open '%s'\n", path.c_str()); exit(1); }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

static void print_type(const slua::TypeNode* t, int indent);
static void print_expr(const slua::Expr* e, int indent);
static void print_stmt(const slua::Stmt* s, int indent);
static std::string ind(int n) { return std::string((size_t)n * 2, ' '); }

static void print_type(const slua::TypeNode* t, int indent) {
    if (!t) { printf("%s<null-type>\n", ind(indent).c_str()); return; }
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, slua::PrimitiveType>)
            printf("%sPrimitive(%s)\n", ind(indent).c_str(), v.name.c_str());
        else if constexpr (std::is_same_v<T, slua::OptionalType>) {
            printf("%sOptional\n", ind(indent).c_str());
            print_type(v.inner.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::UnionType>) {
            printf("%sUnion\n", ind(indent).c_str());
            for (auto& m : v.members) print_type(m.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::GenericType>) {
            printf("%sGeneric(%s)\n", ind(indent).c_str(), v.name.c_str());
            for (auto& a : v.args) print_type(a.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::PtrType>) {
            printf("%sPtr\n", ind(indent).c_str());
            print_type(v.pointee.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::FuncType>) {
            printf("%sFuncType\n", ind(indent).c_str());
            for (auto& p : v.params) print_type(p.get(), indent+1);
            printf("%s-> ", ind(indent).c_str());
            print_type(v.ret.get(), 0);
        }
        else if constexpr (std::is_same_v<T, slua::RecordType>) {
            printf("%sRecord\n", ind(indent).c_str());
            for (auto& [n,tp] : v.fields) {
                printf("%s  .%s: ", ind(indent).c_str(), n.c_str());
                print_type(tp.get(), 0);
            }
        }
    }, t->v);
}

static void print_expr(const slua::Expr* e, int indent) {
    if (!e) { printf("%s<null-expr>\n", ind(indent).c_str()); return; }
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, slua::NullLit>)
            printf("%sNull\n", ind(indent).c_str());
        else if constexpr (std::is_same_v<T, slua::BoolLit>)
            printf("%sBool(%s)\n", ind(indent).c_str(), v.val?"true":"false");
        else if constexpr (std::is_same_v<T, slua::IntLit>)
            printf("%sInt(%lld)\n", ind(indent).c_str(), (long long)v.val);
        else if constexpr (std::is_same_v<T, slua::FloatLit>)
            printf("%sFloat(%g)\n", ind(indent).c_str(), v.val);
        else if constexpr (std::is_same_v<T, slua::StrLit>)
            printf("%sStr(\"%s\")\n", ind(indent).c_str(), v.val.c_str());
        else if constexpr (std::is_same_v<T, slua::Ident>)
            printf("%sIdent(%s)\n", ind(indent).c_str(), v.name.c_str());
        else if constexpr (std::is_same_v<T, slua::Binop>) {
            printf("%sBinop(%s)\n", ind(indent).c_str(), v.op.c_str());
            print_expr(v.lhs.get(), indent+1);
            print_expr(v.rhs.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::Unop>) {
            printf("%sUnop(%s)\n", ind(indent).c_str(), v.op.c_str());
            print_expr(v.operand.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::Call>) {
            printf("%sCall\n", ind(indent).c_str());
            print_expr(v.callee.get(), indent+1);
            for (auto& a : v.args) print_expr(a.get(), indent+2);
        }
        else if constexpr (std::is_same_v<T, slua::MethodCall>) {
            printf("%sMethodCall(:%s)\n", ind(indent).c_str(), v.method.c_str());
            print_expr(v.obj.get(), indent+1);
            for (auto& a : v.args) print_expr(a.get(), indent+2);
        }
        else if constexpr (std::is_same_v<T, slua::Field>) {
            printf("%sField(.%s)\n", ind(indent).c_str(), v.name.c_str());
            print_expr(v.table.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::Index>) {
            printf("%sIndex\n", ind(indent).c_str());
            print_expr(v.table.get(), indent+1);
            print_expr(v.key.get(),   indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::TableCtor>)
            printf("%sTableCtor(%zu entries)\n", ind(indent).c_str(), v.entries.size());
        else if constexpr (std::is_same_v<T, slua::FuncExpr>)
            printf("%sFuncExpr(%zu params)\n", ind(indent).c_str(), v.params.size());
        else if constexpr (std::is_same_v<T, slua::AllocExpr>) {
            printf("%sAlloc\n", ind(indent).c_str());
            print_type(v.elem_type.get(), indent+1);
            print_expr(v.count.get(),     indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::DerefExpr>) {
            printf("%sDeref\n", ind(indent).c_str());
            print_expr(v.ptr.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::AddrExpr>) {
            printf("%sAddr\n", ind(indent).c_str());
            print_expr(v.target.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::CastExpr>) {
            printf("%sCast\n", ind(indent).c_str());
            print_type(v.to.get(),   indent+1);
            print_expr(v.expr.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::TypeofExpr>) {
            printf("%sTypeof\n", ind(indent).c_str());
            print_expr(v.expr.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::SizeofExpr>) {
            printf("%sSizeof\n", ind(indent).c_str());
            print_type(v.type.get(), indent+1);
        }
    }, e->v);
}

static void print_stmt(const slua::Stmt* s, int indent) {
    if (!s) return;
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, slua::LocalDecl>) {
            printf("%sLocal(%s%s)\n", ind(indent).c_str(),v.is_const ? "const " : "", v.name.c_str());
            if (v.type_ann) print_type(v.type_ann.get(), indent+1);
            if (v.init)     print_expr(v.init.get(),     indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::GlobalDecl>) {
            printf("%sGlobal(%s)\n", ind(indent).c_str(), v.name.c_str());
            if (v.type_ann) print_type(v.type_ann.get(), indent+1);
            if (v.init)     print_expr(v.init.get(),     indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::Assign>) {
            printf("%sAssign\n", ind(indent).c_str());
            print_expr(v.target.get(), indent+1);
            print_expr(v.value.get(),  indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::CallStmt>) {
            printf("%sCallStmt\n", ind(indent).c_str());
            print_expr(v.call.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::FuncDecl>) {
            printf("%sFunc(%s%s, %zu params)\n",ind(indent).c_str(),
                   v.exported ? "export " : "",v.name.c_str(), v.params.size());
            if (v.ret_type) print_type(v.ret_type.get(), indent+1);
            for (auto& st : v.body) print_stmt(st.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::IfStmt>) {
            printf("%sIf\n", ind(indent).c_str());
            print_expr(v.cond.get(), indent+1);
            printf("%s  then:\n", ind(indent).c_str());
            for (auto& st : v.then_body) print_stmt(st.get(), indent+2);
            if (v.else_body) {
                printf("%s  else:\n", ind(indent).c_str());
                for (auto& st : *v.else_body) print_stmt(st.get(), indent+2);
            }
        }
        else if constexpr (std::is_same_v<T, slua::WhileStmt>) {
            printf("%sWhile\n", ind(indent).c_str());
            print_expr(v.cond.get(), indent+1);
            for (auto& st : v.body) print_stmt(st.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::NumericFor>) {
            printf("%sFor(%s)\n", ind(indent).c_str(), v.var.c_str());
            print_expr(v.start.get(), indent+1);
            print_expr(v.stop.get(),  indent+1);
            if (v.step) print_expr(v.step.get(), indent+1);
            for (auto& st : v.body) print_stmt(st.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::ReturnStmt>) {
            printf("%sReturn(%zu values)\n", ind(indent).c_str(), v.values.size());
            for (auto& ex : v.values) print_expr(ex.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::DeferStmt>) {
            printf("%sDefer\n", ind(indent).c_str());
            print_stmt(v.action.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::TypeDecl>) {
            printf("%sTypeDecl(%s)\n", ind(indent).c_str(), v.name.c_str());
            print_type(v.def.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::ImportDecl>)
            printf("%sImport(%s)\n", ind(indent).c_str(), v.module_name.c_str());
        else if constexpr (std::is_same_v<T, slua::BreakStmt>)
            printf("%sBreak\n", ind(indent).c_str());
        else if constexpr (std::is_same_v<T, slua::ContinueStmt>)
            printf("%sContinue\n", ind(indent).c_str());
        else if constexpr (std::is_same_v<T, slua::PanicStmt>) {
            printf("%sPanic\n", ind(indent).c_str());
            print_expr(v.msg.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::StoreStmt>) {
            printf("%sStore\n", ind(indent).c_str());
            print_expr(v.ptr.get(), indent+1);
            print_expr(v.val.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::FreeStmt>) {
            printf("%sFree\n", ind(indent).c_str());
            print_expr(v.ptr.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::ExternDecl>)
            printf("%sExtern(%s)\n", ind(indent).c_str(), v.name.c_str());
        else if constexpr (std::is_same_v<T, slua::DoBlock>) {
            printf("%sDo\n", ind(indent).c_str());
            for (auto& st : v.body) print_stmt(st.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, slua::RepeatStmt>) {
            printf("%sRepeat\n", ind(indent).c_str());
            for (auto& st : v.body) print_stmt(st.get(), indent+1);
            printf("%sUntil\n", ind(indent).c_str());
            print_expr(v.until_cond.get(), indent+1);
        }
    }, s->v);
}

static void print_usage() {
    fprintf(stderr,
        "Usage: sluac [options] <file.slua>\n"
        "  --emit-tokens   Dump token stream\n"
        "  --emit-ast      Dump AST\n"
        "  --strict        Force strict mode\n"
        "  --nonstrict     Force nonstrict mode\n"
        "  --version       Print version\n"
    );
}

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(); return 1; }

    std::string input_file, output_file = "a.out";
    bool emit_tokens = false, emit_ast = false;
    bool override_strict = false, override_nonstrict = false;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-o") == 0 && i+1 < argc) output_file = argv[++i];
        else if (strcmp(argv[i], "--emit-tokens") == 0)     emit_tokens        = true;
        else if (strcmp(argv[i], "--emit-ast")    == 0)     emit_ast           = true;
        else if (strcmp(argv[i], "--strict")      == 0)     override_strict    = true;
        else if (strcmp(argv[i], "--nonstrict")   == 0)     override_nonstrict = true;
        else if (strcmp(argv[i], "--version")     == 0) { printf("sluac 0.3\n"); return 0; }
        else if (argv[i][0] != '-') input_file = argv[i];
        else { fprintf(stderr, "sluac: unknown option '%s'\n", argv[i]); return 1; }
    }
    if (input_file.empty()) { print_usage(); return 1; }

    std::string source = read_file(input_file);

    // Stage 1 mode detection
    slua::Directives directives = slua::detect_directives(source, input_file);
    slua::CompileMode mode = directives.type;
    if (override_strict)     mode = slua::CompileMode::STRICT;
    if (override_nonstrict)  mode = slua::CompileMode::NONSTRICT;
    fprintf(stderr, "sluac: mode = %s\n",
            mode == slua::CompileMode::STRICT ? "strict" : "nonstrict");

    // Stage 2 diag + config
    slua::DiagEngine     diag(mode);
    slua::SemanticConfig cfg = slua::SemanticConfig::for_mode(mode);
    cfg.mem_mode = directives.mem;

    // Stage 3 Ã¯Â¿Â½ token dump
    if (emit_tokens) {
        slua::Lexer lexer(source, input_file, mode);
        while (!lexer.at_eof()) {
            slua::Token tok = lexer.next();
            printf("  [%3d:%3d] kind=%-5d  '%s'\n",
                   tok.loc.line, tok.loc.col, (int)tok.kind, tok.text.c_str());
        }
        return 0;
    }

    // Stage 4 parse
    slua::Lexer  lexer(source, input_file, mode);
    slua::Parser parser(lexer, diag, mode);
    auto mod = parser.parse_module(input_file);

    if (diag.has_errors()) { diag.dump_all(); return 1; }

    // Stage 5 name resolution
    {
        slua::Resolver resolver(diag, cfg);
        resolver.resolve(*mod);
    }

    if (diag.has_errors()) { diag.dump_all(); return 1; }

    // Stage 6 type checking
    {
        slua::TypeChecker tc(diag, cfg);
        tc.check(*mod);
    }

    if (diag.has_errors()) { diag.dump_all(); return 1; }

    // Stage 7 AST dump
    if (emit_ast) {
        printf("Module: %s  mode=%s  stmts=%zu\n",
               mod->filename.c_str(),
               mod->mode == slua::CompileMode::STRICT ? "strict" : "nonstrict",
               mod->stmts.size());
        for (auto& s : mod->stmts) print_stmt(s.get(), 1);
        return 0;
    }

#ifdef SLUA_HAS_LLVM
    // Stage 7 IR emission
    {
        fprintf(stderr, "sluac: creating emitter\n"); fflush(stderr);
        slua::IREmitter emitter(diag, cfg, input_file);
        fprintf(stderr, "DBG: calling emit\n"); fflush(stderr);

        if (!emitter.emit(*mod)) { diag.dump_all(); return 1; }



        if (output_file == "a.out") output_file = "output.ll";
        fprintf(stderr, "sluac: writing ll\n"); fflush(stderr);
        if (!emitter.write_ll(output_file)) return 1;
        fprintf(stderr, "sluac: wrote IR to %s\n", output_file.c_str());
    }

    return 0;
#else
    fprintf(stderr, "sluac: LLVM not available\n");
    return 1;
#endif
}
