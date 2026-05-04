#include "sarn/Lexer.h"
#include "sarn/AST.h"
#include "sarn/Diagnostics.h"
#include "sarn/SemanticConfig.h"
#include "sarn/Parser.h"
#include "sarn/Resolver.h"
#include "sarn/TypeChecker.h"
#ifdef SARN_HAS_LLVM
#include "sarn/IREmitter.h"
#endif
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <unordered_set>
#include <functional>

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { fprintf(stderr, "sarnc: cannot open '%s'\n", path.c_str()); exit(1); }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

static void print_type(const sarn::TypeNode* t, int indent);
static void print_expr(const sarn::Expr* e, int indent);
static void print_stmt(const sarn::Stmt* s, int indent);
static std::string ind(int n) { return std::string((size_t)n * 2, ' '); }

static void print_type(const sarn::TypeNode* t, int indent) {
    if (!t) { printf("%s<null-type>\n", ind(indent).c_str()); return; }
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, sarn::PrimitiveType>)
            printf("%sPrimitive(%s)\n", ind(indent).c_str(), v.name.c_str());
        else if constexpr (std::is_same_v<T, sarn::OptionalType>) {
            printf("%sOptional\n", ind(indent).c_str());
            print_type(v.inner.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::UnionType>) {
            printf("%sUnion\n", ind(indent).c_str());
            for (auto& m : v.members) print_type(m.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::GenericType>) {
            printf("%sGeneric(%s)\n", ind(indent).c_str(), v.name.c_str());
            for (auto& a : v.args) print_type(a.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::PtrType>) {
            printf("%sPtr\n", ind(indent).c_str());
            print_type(v.pointee.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::FuncType>) {
            printf("%sFuncType\n", ind(indent).c_str());
            for (auto& p : v.params) print_type(p.get(), indent+1);
            printf("%s-> ", ind(indent).c_str());
            print_type(v.ret.get(), 0);
        }
        else if constexpr (std::is_same_v<T, sarn::TupleType>) {
            printf("%sTuple(%zu)\n", ind(indent).c_str(), v.members.size());
            for (auto& m : v.members) print_type(m.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::RecordType>) {
            printf("%sRecord\n", ind(indent).c_str());
            for (auto& [n,tp] : v.fields) {
                printf("%s  .%s: ", ind(indent).c_str(), n.c_str());
                print_type(tp.get(), 0);
            }
        }
    }, t->v);
}

static void print_expr(const sarn::Expr* e, int indent) {
    if (!e) { printf("%s<null-expr>\n", ind(indent).c_str()); return; }
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, sarn::NullLit>)
            printf("%sNull\n", ind(indent).c_str());
        else if constexpr (std::is_same_v<T, sarn::BoolLit>)
            printf("%sBool(%s)\n", ind(indent).c_str(), v.val?"true":"false");
        else if constexpr (std::is_same_v<T, sarn::IntLit>)
            printf("%sInt(%lld)\n", ind(indent).c_str(), (long long)v.val);
        else if constexpr (std::is_same_v<T, sarn::FloatLit>)
            printf("%sFloat(%g)\n", ind(indent).c_str(), v.val);
        else if constexpr (std::is_same_v<T, sarn::StrLit>)
            printf("%sStr(\"%s\")\n", ind(indent).c_str(), v.val.c_str());
        else if constexpr (std::is_same_v<T, sarn::Ident>)
            printf("%sIdent(%s)\n", ind(indent).c_str(), v.name.c_str());
        else if constexpr (std::is_same_v<T, sarn::Binop>) {
            printf("%sBinop(%s)\n", ind(indent).c_str(), v.op.c_str());
            print_expr(v.lhs.get(), indent+1);
            print_expr(v.rhs.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::Unop>) {
            printf("%sUnop(%s)\n", ind(indent).c_str(), v.op.c_str());
            print_expr(v.operand.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::Call>) {
            printf("%sCall\n", ind(indent).c_str());
            print_expr(v.callee.get(), indent+1);
            for (auto& a : v.args) print_expr(a.get(), indent+2);
        }
        else if constexpr (std::is_same_v<T, sarn::MethodCall>) {
            printf("%sMethodCall(:%s)\n", ind(indent).c_str(), v.method.c_str());
            print_expr(v.obj.get(), indent+1);
            for (auto& a : v.args) print_expr(a.get(), indent+2);
        }
        else if constexpr (std::is_same_v<T, sarn::Field>) {
            printf("%sField(.%s)\n", ind(indent).c_str(), v.name.c_str());
            print_expr(v.table.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::Index>) {
            printf("%sIndex\n", ind(indent).c_str());
            print_expr(v.table.get(), indent+1);
            print_expr(v.key.get(),   indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::TableCtor>)
            printf("%sTableCtor(%zu entries)\n", ind(indent).c_str(), v.entries.size());
        else if constexpr (std::is_same_v<T, sarn::FuncExpr>)
            printf("%sFuncExpr(%zu params)\n", ind(indent).c_str(), v.params.size());
        else if constexpr (std::is_same_v<T, sarn::AllocExpr>) {
            printf("%sAlloc\n", ind(indent).c_str());
            print_type(v.elem_type.get(), indent+1);
            print_expr(v.count.get(),     indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::DerefExpr>) {
            printf("%sDeref\n", ind(indent).c_str());
            print_expr(v.ptr.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::AddrExpr>) {
            printf("%sAddr\n", ind(indent).c_str());
            print_expr(v.target.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::CastExpr>) {
            printf("%sCast\n", ind(indent).c_str());
            print_type(v.to.get(),   indent+1);
            print_expr(v.expr.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::TypeofExpr>) {
            printf("%sTypeof\n", ind(indent).c_str());
            print_expr(v.expr.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::SizeofExpr>) {
            printf("%sSizeof\n", ind(indent).c_str());
            print_type(v.type.get(), indent+1);
        }
    }, e->v);
}

static void print_stmt(const sarn::Stmt* s, int indent) {
    if (!s) return;
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, sarn::LocalDecl>) {
            printf("%sLocal(%s%s)\n", ind(indent).c_str(),v.is_const ? "const " : "", v.name.c_str());
            if (v.type_ann) print_type(v.type_ann.get(), indent+1);
            if (v.init)     print_expr(v.init.get(),     indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::GlobalDecl>) {
            printf("%sGlobal(%s)\n", ind(indent).c_str(), v.name.c_str());
            if (v.type_ann) print_type(v.type_ann.get(), indent+1);
            if (v.init)     print_expr(v.init.get(),     indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::Assign>) {
            printf("%sAssign\n", ind(indent).c_str());
            print_expr(v.target.get(), indent+1);
            print_expr(v.value.get(),  indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::CallStmt>) {
            printf("%sCallStmt\n", ind(indent).c_str());
            print_expr(v.call.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::FuncDecl>) {
            printf("%sFunc(%s%s, %zu params)\n",ind(indent).c_str(),v.exported ? "export " : "",v.name.c_str(), v.params.size());
            if (v.ret_type) print_type(v.ret_type.get(), indent+1);
            for (auto& st : v.body) print_stmt(st.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::IfStmt>) {
            printf("%sIf\n", ind(indent).c_str());
            print_expr(v.cond.get(), indent+1);
            printf("%s  then:\n", ind(indent).c_str());
            for (auto& st : v.then_body) print_stmt(st.get(), indent+2);
            if (v.else_body) {
                printf("%s  else:\n", ind(indent).c_str());
                for (auto& st : *v.else_body) print_stmt(st.get(), indent+2);
            }
        }
        else if constexpr (std::is_same_v<T, sarn::WhileStmt>) {
            printf("%sWhile\n", ind(indent).c_str());
            print_expr(v.cond.get(), indent+1);
            for (auto& st : v.body) print_stmt(st.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::NumericFor>) {
            printf("%sFor(%s)\n", ind(indent).c_str(), v.var.c_str());
            print_expr(v.start.get(), indent+1);
            print_expr(v.stop.get(),  indent+1);
            if (v.step) print_expr(v.step.get(), indent+1);
            for (auto& st : v.body) print_stmt(st.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::ReturnStmt>) {
            printf("%sReturn(%zu values)\n", ind(indent).c_str(), v.values.size());
            for (auto& ex : v.values) print_expr(ex.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::DeferStmt>) {
            printf("%sDefer\n", ind(indent).c_str());
            print_stmt(v.action.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::TypeDecl>) {
            printf("%sTypeDecl(%s)\n", ind(indent).c_str(), v.name.c_str());
            print_type(v.def.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::ImportDecl>)
            printf("%sImport(%s)\n", ind(indent).c_str(), v.module_name.c_str());
        else if constexpr (std::is_same_v<T, sarn::BreakStmt>)
            printf("%sBreak\n", ind(indent).c_str());
        else if constexpr (std::is_same_v<T, sarn::ContinueStmt>)
            printf("%sContinue\n", ind(indent).c_str());
        else if constexpr (std::is_same_v<T, sarn::PanicStmt>) {
            printf("%sPanic\n", ind(indent).c_str());
            print_expr(v.msg.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::StoreStmt>) {
            printf("%sStore\n", ind(indent).c_str());
            print_expr(v.ptr.get(), indent+1);
            print_expr(v.val.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::FreeStmt>) {
            printf("%sFree\n", ind(indent).c_str());
            print_expr(v.ptr.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::ExternDecl>)
            printf("%sExtern(%s)\n", ind(indent).c_str(), v.name.c_str());
        else if constexpr (std::is_same_v<T, sarn::DoBlock>) {
            printf("%sDo\n", ind(indent).c_str());
            for (auto& st : v.body) print_stmt(st.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::RepeatStmt>) {
            printf("%sRepeat\n", ind(indent).c_str());
            for (auto& st : v.body) print_stmt(st.get(), indent+1);
            printf("%sUntil\n", ind(indent).c_str());
            print_expr(v.until_cond.get(), indent+1);
        }
        else if constexpr (std::is_same_v<T, sarn::EnumDecl>) {
            printf("%sEnum(%s, %zu members)\n", ind(indent).c_str(),v.name.c_str(), v.members.size());
            for (auto& [mn, mv] : v.members)
                printf("%s  %s = %lld\n", ind(indent).c_str(),
                       mn.c_str(), (long long)(mv.has_value() ? *mv : 0));
        }
        else if constexpr (std::is_same_v<T, sarn::MultiLocalDecl>) {
            printf("%sMultiLocal(%zu vars)\n", ind(indent).c_str(), v.vars.size());
            for (auto& [vn, vt] : v.vars) {
                printf("%s  %s\n", ind(indent).c_str(), vn.c_str());
                if (vt) print_type(vt.get(), indent+2);
            }
            if (v.init) print_expr(v.init.get(), indent+1);
        }
    }, s->v);
}

static void print_usage() {
    fprintf(stderr,
        "Usage: sarnc [options] <file.sarn>\n"
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
        else if (strcmp(argv[i], "--version")     == 0) { printf("sarnc 0.3\n"); return 0; }
        else if (argv[i][0] != '-') input_file = argv[i];
        else { fprintf(stderr, "sarnc: unknown option '%s'\n", argv[i]); return 1; }
    }
    if (input_file.empty()) { print_usage(); return 1; }
    std::string source = read_file(input_file);
    
    sarn::Directives directives = sarn::detect_directives(source, input_file);
    sarn::CompileMode mode = directives.type;
    if (override_strict)     mode = sarn::CompileMode::STRICT;
    if (override_nonstrict)  mode = sarn::CompileMode::NONSTRICT;
    fprintf(stderr, "sarnc: mode = %s\n",mode == sarn::CompileMode::STRICT ? "strict" : "nonstrict");
    
    sarn::DiagEngine     diag(mode);
    sarn::SemanticConfig cfg = sarn::SemanticConfig::for_mode(mode);
    cfg.mem_mode = directives.mem;
    
    if (emit_tokens) {
        sarn::Lexer lexer(source, input_file, mode);
        while (!lexer.at_eof()) {
            sarn::Token tok = lexer.next();
            printf("  [%3d:%3d] kind=%-5d  '%s'\n",tok.loc.line, tok.loc.col, (int)tok.kind, tok.text.c_str());
        }
        return 0;
    }
    
    sarn::Lexer  lexer(source, input_file, mode);
    sarn::Parser parser(lexer, diag, mode);
    auto mod = parser.parse_module(input_file);

    {
        std::function<void(sarn::Module&, const std::string&)> resolve_imports;
        resolve_imports = [&](sarn::Module& m, const std::string& base_file) {
            std::string base_dir = base_file;
            auto slash = base_dir.find_last_of("/\\");
            if (slash != std::string::npos) base_dir = base_dir.substr(0, slash + 1);
            else base_dir = "";
            std::vector<std::unique_ptr<sarn::Stmt>> expanded;
            static const std::unordered_set<std::string> slua_builtins = {
                "io","math","os","string","stdata","table","fs","random",
                "datetime","path","process","json","net","sync","regex",
                "crypto","buf","thread","vec","scene","http","stdgui"
            };
            const char* slua_root_env = getenv("SLUA_ROOT");
            std::string slua_root = slua_root_env ? slua_root_env : ".";
            for (auto& s : m.stmts) {
                if (auto* fi = std::get_if<sarn::FileImportDecl>(&s->v)) {
                    std::string fpath = base_dir + fi->path;
                    std::ifstream ff(fpath, std::ios::binary);
                    if (!ff) { fprintf(stderr, "sarnc: cannot open import '%s'\n", fpath.c_str()); exit(1); }
                    std::ostringstream ss; ss << ff.rdbuf();
                    sarn::Lexer  flex(ss.str(), fpath, mode);
                    sarn::Parser fpar(flex, diag, mode);
                    auto fmod = fpar.parse_module(fpath);
                    resolve_imports(*fmod, fpath);
                    for (auto& fs : fmod->stmts)
                        expanded.push_back(std::move(fs));
                } else if (auto* id = std::get_if<sarn::ImportDecl>(&s->v)) {
                    if (slua_builtins.count(id->module_name) == 0) {
                        std::vector<std::string> search = {
                            base_dir + ".packages/" + id->module_name + "/__init__.sarn",
                            slua_root + "/.packages/" + id->module_name + "/__init__.sarn"
                        };
                        bool pkg_found = false;
                        for (auto& fpath : search) {
                            std::ifstream ff(fpath, std::ios::binary);
                            if (ff) {
                                std::ostringstream ss; ss << ff.rdbuf();
                                sarn::Lexer  flex(ss.str(), fpath, mode);
                                sarn::Parser fpar(flex, diag, mode);
                                auto fmod = fpar.parse_module(fpath);
                                resolve_imports(*fmod, fpath);
                                for (auto& fs : fmod->stmts) {
                                    if (auto* fd = std::get_if<sarn::FuncDecl>(&fs->v)) {
                                        if (fd->exported) {
                                            fd->name = id->module_name + "." + fd->name;
                                        }
                                    }
                                    expanded.push_back(std::move(fs));
                                }
                                pkg_found = true;
                                break;
                            }
                        }
                        if (!pkg_found) expanded.push_back(std::move(s));
                    } else {
                        expanded.push_back(std::move(s));
                    }
                } else {
                    expanded.push_back(std::move(s));
                }
            }
            m.stmts = std::move(expanded);
        };
        resolve_imports(*mod, input_file);
    }


    if (diag.has_errors()) { diag.dump_all(); return 1; }
    
    {
        sarn::Resolver resolver(diag, cfg);
        resolver.resolve(*mod);
    }

    if (diag.has_errors()) { diag.dump_all(); return 1; }

    
    {
        sarn::TypeChecker tc(diag, cfg);
        tc.check(*mod);
    }

    if (diag.has_errors()) { diag.dump_all(); return 1; }

    
    if (emit_ast) {
        printf("Module: %s  mode=%s  stmts=%zu\n",mod->filename.c_str(),
            mod->mode == sarn::CompileMode::STRICT ? "strict" : "nonstrict",
            mod->stmts.size());
        for (auto& s : mod->stmts) print_stmt(s.get(), 1);
        return 0;
    }
#ifdef SARN_HAS_LLVM
    {
        fprintf(stderr, "sarnc: creating emitter\n"); fflush(stderr);
        sarn::IREmitter emitter(diag, cfg, input_file);
        fprintf(stderr, "DBG: calling emit\n"); fflush(stderr);
        if (!emitter.emit(*mod)) { diag.dump_all(); return 1; }

        if (output_file == "a.out") output_file = "output.ll";
        fprintf(stderr, "sarnc: writing ll\n"); fflush(stderr);
        if (!emitter.write_ll(output_file)) return 1;
        fprintf(stderr, "sarnc: wrote IR to %s\n", output_file.c_str());
    }
    return 0;
#else
    fprintf(stderr, "sarnc: LLVM not available\n");
    return 1;
#endif
}


