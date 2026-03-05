#pragma once
#include "AST.h"
#include "Diagnostics.h"
#include "SemanticConfig.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace slua {

// ── Symbol kinds ──────────────────────────────────────────────────────────────
enum class SymbolKind {
    LOCAL,      // local x
    CONST,      // const x
    GLOBAL,     // global x
    PARAM,      // function parameter
    FUNCTION,   // function declaration
    TYPE,       // type alias
    EXTERN,     // extern function
};

struct Symbol {
    std::string  name;
    SymbolKind   kind;
    TypeNode*    type_node;   // may be null (untyped / not yet resolved)
    SourceLoc    decl_loc;
    bool         initialized = false;
    bool         is_const    = false;
};

// ── Scope ─────────────────────────────────────────────────────────────────────
class Scope {
public:
    explicit Scope(Scope* parent = nullptr) : parent_(parent) {}

    // Define a new symbol in this scope.
    // Returns false (and emits error) if already defined here.
    bool define(const std::string& name, Symbol sym,
                DiagEngine& diag, CompileMode mode);

    // Look up a symbol by name, walking parent chain.
    Symbol* lookup(const std::string& name);

    Scope* parent() const { return parent_; }

private:
    std::unordered_map<std::string, Symbol> syms_;
    Scope* parent_;
};

// ── Resolver ──────────────────────────────────────────────────────────────────
class Resolver {
public:
    Resolver(DiagEngine& diag, SemanticConfig cfg)
        : diag_(diag), cfg_(cfg) {}

    // Entry point — resolves all names in the module.
    // Returns false if any errors were emitted.
    bool resolve(Module& mod);

private:
    DiagEngine&    diag_;
    SemanticConfig cfg_;

    // Scope stack
    Scope*  current_scope_ = nullptr;

    // Current function return type (for return checking)
    TypeNode* current_ret_type_ = nullptr;

    // Scope management
    void push_scope();
    void pop_scope();

    // Visitors
    void resolve_stmt(Stmt& s);
    void resolve_expr(Expr& e);
    void resolve_type(TypeNode* t);

    // Statement helpers
    void resolve_local_decl   (LocalDecl&   s, SourceLoc loc);
    void resolve_global_decl  (GlobalDecl&  s, SourceLoc loc);
    void resolve_func_decl    (FuncDecl&    s, SourceLoc loc);
    void resolve_if_stmt      (IfStmt&      s);
    void resolve_while_stmt   (WhileStmt&   s);
    void resolve_repeat_stmt  (RepeatStmt&  s);
    void resolve_numeric_for  (NumericFor&  s, SourceLoc loc);
    void resolve_return_stmt  (ReturnStmt&  s, SourceLoc loc);
    void resolve_defer_stmt   (DeferStmt&   s);
    void resolve_do_block     (DoBlock&     s);
    void resolve_type_decl    (TypeDecl&    s, SourceLoc loc);
    void resolve_extern_decl  (ExternDecl&  s, SourceLoc loc);
    void resolve_panic_stmt   (PanicStmt&   s);
    void resolve_store_stmt   (StoreStmt&   s);
    void resolve_free_stmt    (FreeStmt&    s);
    void resolve_assign       (Assign&      s, SourceLoc loc);
    void resolve_call_stmt    (CallStmt&    s);

    // Expression helpers
    void resolve_ident        (Ident&       e, Expr& node);
    void resolve_binop        (Binop&       e);
    void resolve_unop         (Unop&        e);
    void resolve_call_expr    (Call&        e);
    void resolve_method_call  (MethodCall&  e);
    void resolve_field        (Field&       e);
    void resolve_index        (Index&       e);
    void resolve_table_ctor   (TableCtor&   e);
    void resolve_func_expr    (FuncExpr&    e);
    void resolve_alloc_expr   (AllocExpr&   e);
    void resolve_deref_expr   (DerefExpr&   e);
    void resolve_addr_expr    (AddrExpr&    e);
    void resolve_cast_expr    (CastExpr&    e);
    void resolve_typeof_expr  (TypeofExpr&  e);
    void resolve_sizeof_expr  (SizeofExpr&  e);
};

} // namespace slua