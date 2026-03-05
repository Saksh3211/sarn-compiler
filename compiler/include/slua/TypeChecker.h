#pragma once
#include "AST.h"
#include "Diagnostics.h"
#include "SemanticConfig.h"
#include "Resolver.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace slua {

// =============================================================================
//  SluaType — the internal type representation used by the type checker
//  (separate from the AST TypeNode which is just parse-tree syntax)
// =============================================================================

struct SluaType;
using SluaTypePtr = std::shared_ptr<SluaType>;

enum class TypeKind {
    // Primitives
    INT, NUMBER, STRING, BOOL, VOID, ANY, NULL_T,
    // Pointer
    PTR,
    // Record / struct
    RECORD,
    // Function
    FUNC,
    // Generic instance e.g. Stack<int>
    GENERIC,
    // Unknown — used during error recovery
    ERROR,
};

struct RecordField {
    std::string name;
    SluaTypePtr type;
};

struct SluaType {
    TypeKind kind;

    // PTR: inner type
    SluaTypePtr pointee;

    // RECORD: field list
    std::vector<RecordField> fields;

    // FUNC: param types + return type
    std::vector<SluaTypePtr> param_types;
    SluaTypePtr              return_type;

    // GENERIC / named: base name
    std::string name;

    // GENERIC: type arguments
    std::vector<SluaTypePtr> type_args;

    // Helpers
    bool is_numeric() const {
        return kind == TypeKind::INT || kind == TypeKind::NUMBER;
    }
    bool is_error() const { return kind == TypeKind::ERROR; }

    std::string to_string() const;
};

// Singleton-style builders
SluaTypePtr make_int();
SluaTypePtr make_number();
SluaTypePtr make_string();
SluaTypePtr make_bool();
SluaTypePtr make_void();
SluaTypePtr make_any();
SluaTypePtr make_null();
SluaTypePtr make_error();
SluaTypePtr make_ptr(SluaTypePtr inner);
SluaTypePtr make_func(std::vector<SluaTypePtr> params, SluaTypePtr ret);

// =============================================================================
//  TypeChecker
// =============================================================================

class TypeChecker {
public:
    TypeChecker(DiagEngine& diag, SemanticConfig cfg)
        : diag_(diag), cfg_(cfg) {}

    bool check(Module& mod);

private:
    DiagEngine&    diag_;
    SemanticConfig cfg_;

    // Type environment: name → SluaType
    // Mirrors the Resolver's scope but carries full type info
    struct TypeEnv {
        std::unordered_map<std::string, SluaTypePtr> vars;
        TypeEnv* parent = nullptr;

        SluaTypePtr lookup(const std::string& name) const {
            auto it = vars.find(name);
            if (it != vars.end()) return it->second;
            if (parent) return parent->lookup(name);
            return nullptr;
        }
        void define(const std::string& name, SluaTypePtr t) {
            vars[name] = std::move(t);
        }
    };

    TypeEnv*    env_     = nullptr;
    SluaTypePtr ret_type_= nullptr;  // current function's return type

    void push_env();
    void pop_env();

    // Convert AST TypeNode → SluaType
    SluaTypePtr resolve_type_node(const TypeNode* t);

    // Statement checking
    void check_stmt(Stmt& s);
    void check_local_decl  (LocalDecl&  s, SourceLoc loc);
    void check_global_decl (GlobalDecl& s, SourceLoc loc);
    void check_func_decl   (FuncDecl&   s, SourceLoc loc);
    void check_if_stmt     (IfStmt&     s, SourceLoc loc);
    void check_while_stmt  (WhileStmt&  s);
    void check_repeat_stmt (RepeatStmt& s);
    void check_numeric_for (NumericFor& s, SourceLoc loc);
    void check_return_stmt (ReturnStmt& s, SourceLoc loc);
    void check_assign      (Assign&     s, SourceLoc loc);
    void check_call_stmt   (CallStmt&   s);
    void check_defer_stmt  (DeferStmt&  s);
    void check_do_block    (DoBlock&    s);
    void check_store_stmt  (StoreStmt&  s, SourceLoc loc);
    void check_free_stmt   (FreeStmt&   s, SourceLoc loc);
    void check_panic_stmt  (PanicStmt&  s);
    void check_type_decl   (TypeDecl&   s, SourceLoc loc);
    void check_extern_decl (ExternDecl& s, SourceLoc loc);

    // Expression checking — returns the inferred type
    SluaTypePtr check_expr(Expr& e);
    SluaTypePtr check_binop      (Binop&      e, SourceLoc loc);
    SluaTypePtr check_unop       (Unop&       e, SourceLoc loc);
    SluaTypePtr check_call_expr  (Call&       e, SourceLoc loc);
    SluaTypePtr check_method_call(MethodCall& e, SourceLoc loc);
    SluaTypePtr check_field      (Field&      e, SourceLoc loc);
    SluaTypePtr check_index      (Index&      e, SourceLoc loc);
    SluaTypePtr check_table_ctor (TableCtor&  e, SourceLoc loc);
    SluaTypePtr check_func_expr  (FuncExpr&   e, SourceLoc loc);
    SluaTypePtr check_alloc_expr (AllocExpr&  e, SourceLoc loc);

    // Type compatibility
    bool is_assignable(SluaTypePtr from, SluaTypePtr to, SourceLoc loc,
                       const std::string& context);
    bool types_equal(SluaTypePtr a, SluaTypePtr b);
    bool is_nullable(SluaTypePtr t);

    // Built-in type environment
    void install_builtins();
};

} // namespace slua
