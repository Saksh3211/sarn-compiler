#include "sarn/Resolver.h"
#include <cassert>

namespace sarn {

bool Scope::define(const std::string& name, Symbol sym,DiagEngine& diag, CompileMode mode) {
    auto it = syms_.find(name);
    if (it != syms_.end()) {
        
        
        if (mode == CompileMode::STRICT) {
            diag.error("E0010",
                "redeclaration of '" + name + "' (previously declared at " +
                it->second.decl_loc.filename + ":" +
                std::to_string(it->second.decl_loc.line) + ")",
                sym.decl_loc);
            return false;
        } else {
            diag.warn("W0010",
                "redeclaration of '" + name + "' shadows previous declaration",
                sym.decl_loc);
        }
    }
    syms_[name] = std::move(sym);
    return true;
}

Symbol* Scope::lookup(const std::string& name) {
    auto it = syms_.find(name);
    if (it != syms_.end()) return &it->second;
    if (parent_) return parent_->lookup(name);
    return nullptr;
}





void Resolver::push_scope() {
    current_scope_ = new Scope(current_scope_);
}

void Resolver::pop_scope() {
    assert(current_scope_ && "pop_scope on empty scope stack");
    Scope* old = current_scope_;
    current_scope_ = old->parent();
    delete old;
}





bool Resolver::resolve(Module& mod) {
    
    push_scope();

    for (auto& stmt : mod.stmts)
        resolve_stmt(*stmt);

    pop_scope();
    return !diag_.has_errors();
}

void Resolver::resolve_stmt(Stmt& s) {
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if      constexpr (std::is_same_v<T, LocalDecl>)   resolve_local_decl(v, s.loc);
        else if constexpr (std::is_same_v<T, GlobalDecl>)  resolve_global_decl(v, s.loc);
        else if constexpr (std::is_same_v<T, FuncDecl>)    resolve_func_decl(v, s.loc);
        else if constexpr (std::is_same_v<T, IfStmt>)      resolve_if_stmt(v);
        else if constexpr (std::is_same_v<T, WhileStmt>)   resolve_while_stmt(v);
        else if constexpr (std::is_same_v<T, RepeatStmt>)  resolve_repeat_stmt(v);
        else if constexpr (std::is_same_v<T, NumericFor>)  resolve_numeric_for(v, s.loc);
        else if constexpr (std::is_same_v<T, ReturnStmt>)  resolve_return_stmt(v, s.loc);
        else if constexpr (std::is_same_v<T, DeferStmt>)   resolve_defer_stmt(v);
        else if constexpr (std::is_same_v<T, DoBlock>)     resolve_do_block(v);
        else if constexpr (std::is_same_v<T, TypeDecl>)    resolve_type_decl(v, s.loc);
        else if constexpr (std::is_same_v<T, ExternDecl>)  resolve_extern_decl(v, s.loc);
        else if constexpr (std::is_same_v<T, PanicStmt>)   resolve_panic_stmt(v);
        else if constexpr (std::is_same_v<T, StoreStmt>)   resolve_store_stmt(v);
        else if constexpr (std::is_same_v<T, FreeStmt>)    resolve_free_stmt(v);
        else if constexpr (std::is_same_v<T, Assign>)      resolve_assign(v, s.loc);
        else if constexpr (std::is_same_v<T, CallStmt>)    resolve_call_stmt(v);
        else if constexpr (std::is_same_v<T, BreakStmt>)   {  }
        else if constexpr (std::is_same_v<T, ContinueStmt>){  }
        else if constexpr (std::is_same_v<T, ImportDecl>) {
            imported_modules_.insert(v.module_name);
            if (v.module_name == "stdgui") stdgui_imported_ = true;
        }
        else if constexpr (std::is_same_v<T, FileImportDecl>) {}
        else if constexpr (std::is_same_v<T, EnumDecl>) {
            Symbol tsym;
            tsym.name = v.name; tsym.kind = SymbolKind::TYPE;
            tsym.type_node = nullptr; tsym.decl_loc = s.loc; tsym.initialized = true;
            current_scope_->define(v.name, std::move(tsym), diag_, cfg_.mode);
            for (auto& [mname, mval] : v.members) {
                Symbol msym;
                msym.name = mname; msym.kind = SymbolKind::CONST;
                msym.type_node = nullptr; msym.decl_loc = s.loc;
                msym.initialized = true; msym.is_const = true;
                current_scope_->define(mname, std::move(msym), diag_, cfg_.mode);
            }
        }
        else if constexpr (std::is_same_v<T, MultiLocalDecl>) {
            if (v.init) resolve_expr(*v.init);
            for (auto& [vname, vtype] : v.vars) {
                resolve_type(vtype.get());
                Symbol sym;
                sym.name = vname; sym.kind = SymbolKind::LOCAL;
                sym.type_node = vtype.get(); sym.decl_loc = s.loc;
                sym.initialized = true; sym.is_const = false;
                current_scope_->define(vname, std::move(sym), diag_, cfg_.mode);
            }
        }
    }, s.v);
}





void Resolver::resolve_local_decl(LocalDecl& s, SourceLoc loc) {
    
    resolve_type(s.type_ann.get());

    
    if (s.init) resolve_expr(*s.init);

    
    if (cfg_.mode == CompileMode::STRICT && !s.type_ann && !s.init) {
        if (cfg_.uninitialized_var == DiagBehavior::ERROR)
            diag_.error("E0020", "local '" + s.name + "': no type and no initialiser", loc);
        else if (cfg_.uninitialized_var == DiagBehavior::WARNING)
            diag_.warn("W0020", "local '" + s.name + "': no type and no initialiser", loc);
    }

    
    Symbol sym;
    sym.name        = s.name;
    sym.kind        = SymbolKind::LOCAL;
    sym.type_node   = s.type_ann.get();
    sym.decl_loc    = loc;
    sym.initialized = (s.init != nullptr);
    sym.is_const    = s.is_const;
    current_scope_->define(s.name, std::move(sym), diag_, cfg_.mode);
}

void Resolver::resolve_global_decl(GlobalDecl& s, SourceLoc loc) {
    resolve_type(s.type_ann.get());
    if (s.init) resolve_expr(*s.init);

    Symbol sym;
    sym.name        = s.name;
    sym.kind        = SymbolKind::GLOBAL;
    sym.type_node   = s.type_ann.get();
    sym.decl_loc    = loc;
    sym.initialized = (s.init != nullptr);
    current_scope_->define(s.name, std::move(sym), diag_, cfg_.mode);
}

void Resolver::resolve_func_decl(FuncDecl& s, SourceLoc loc) {
    
    
    Symbol fsym;
    fsym.name        = s.name;
    fsym.kind        = SymbolKind::FUNCTION;
    fsym.type_node   = s.ret_type.get();
    fsym.decl_loc    = loc;
    fsym.initialized = true;
    current_scope_->define(s.name, std::move(fsym), diag_, cfg_.mode);

    
    push_scope();

    
    for (auto& [pname, ptype] : s.params) {
        resolve_type(ptype.get());
        Symbol psym;
        psym.name        = pname;
        psym.kind        = SymbolKind::PARAM;
        psym.type_node   = ptype.get();
        psym.decl_loc    = loc;
        psym.initialized = true;
        current_scope_->define(pname, std::move(psym), diag_, cfg_.mode);
    }

    
    TypeNode* saved_ret = current_ret_type_;
    current_ret_type_   = s.ret_type.get();

    resolve_type(s.ret_type.get());

    
    for (auto& stmt : s.body)
        resolve_stmt(*stmt);

    current_ret_type_ = saved_ret;
    pop_scope();
}





void Resolver::resolve_if_stmt(IfStmt& s) {
    resolve_expr(*s.cond);

    push_scope();
    for (auto& st : s.then_body) resolve_stmt(*st);
    pop_scope();

    for (auto& [ei_cond, ei_body] : s.elseif_clauses) {
        resolve_expr(*ei_cond);
        push_scope();
        for (auto& st : ei_body) resolve_stmt(*st);
        pop_scope();
    }

    if (s.else_body) {
        push_scope();
        for (auto& st : *s.else_body) resolve_stmt(*st);
        pop_scope();
    }
}

void Resolver::resolve_while_stmt(WhileStmt& s) {
    resolve_expr(*s.cond);
    push_scope();
    for (auto& st : s.body) resolve_stmt(*st);
    pop_scope();
}

void Resolver::resolve_repeat_stmt(RepeatStmt& s) {
    push_scope();
    for (auto& st : s.body) resolve_stmt(*st);
    
    resolve_expr(*s.until_cond);
    pop_scope();
}

void Resolver::resolve_numeric_for(NumericFor& s, SourceLoc loc) {
    resolve_expr(*s.start);
    resolve_expr(*s.stop);
    if (s.step) resolve_expr(*s.step);

    push_scope();
    
    Symbol lsym;
    lsym.name        = s.var;
    lsym.kind        = SymbolKind::LOCAL;
    lsym.type_node   = nullptr;
    lsym.decl_loc    = loc;
    lsym.initialized = true;
    current_scope_->define(s.var, std::move(lsym), diag_, cfg_.mode);

    for (auto& st : s.body) resolve_stmt(*st);
    pop_scope();
}

void Resolver::resolve_return_stmt(ReturnStmt& s, SourceLoc loc) {
    for (auto& val : s.values)
        resolve_expr(*val);

    
    if (cfg_.mode == CompileMode::STRICT && current_ret_type_) {
        bool is_void = false;
        if (auto* pt = std::get_if<PrimitiveType>(&current_ret_type_->v))
            is_void = (pt->name == "void");
        if (is_void && !s.values.empty())
            diag_.error("E0030", "void function must not return a value", loc);
    }
}

void Resolver::resolve_defer_stmt(DeferStmt& s) {
    if (s.action) resolve_stmt(*s.action);
}

void Resolver::resolve_do_block(DoBlock& s) {
    push_scope();
    for (auto& st : s.body) resolve_stmt(*st);
    pop_scope();
}

void Resolver::resolve_type_decl(TypeDecl& s, SourceLoc loc) {
    resolve_type(s.def.get());

    Symbol tsym;
    tsym.name        = s.name;
    tsym.kind        = SymbolKind::TYPE;
    tsym.type_node   = s.def.get();
    tsym.decl_loc    = loc;
    tsym.initialized = true;
    current_scope_->define(s.name, std::move(tsym), diag_, cfg_.mode);
}

void Resolver::resolve_extern_decl(ExternDecl& s, SourceLoc loc) {
    resolve_type(s.func_type.get());

    Symbol esym;
    esym.name        = s.name;
    esym.kind        = SymbolKind::EXTERN;
    esym.type_node   = s.func_type.get();
    esym.decl_loc    = loc;
    esym.initialized = true;
    current_scope_->define(s.name, std::move(esym), diag_, cfg_.mode);
}

void Resolver::resolve_panic_stmt(PanicStmt& s) {
    resolve_expr(*s.msg);
}

void Resolver::resolve_store_stmt(StoreStmt& s) {
    resolve_expr(*s.ptr);
    resolve_expr(*s.val);
}

void Resolver::resolve_free_stmt(FreeStmt& s) {
    resolve_expr(*s.ptr);
}

void Resolver::resolve_assign(Assign& s, SourceLoc loc) {
    
    resolve_expr(*s.value);
    
    resolve_expr(*s.target);

    
    if (auto* id = std::get_if<Ident>(&s.target->v)) {
        Symbol* sym = current_scope_->lookup(id->name);
        if (sym && sym->is_const) {
            diag_.error("E0011",
                "cannot assign to const '" + id->name + "'", loc);
        }
    }
}

void Resolver::resolve_call_stmt(CallStmt& s) {
    resolve_expr(*s.call);
}





void Resolver::resolve_expr(Expr& e) {
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if      constexpr (std::is_same_v<T, NullLit>)    {  }
        else if constexpr (std::is_same_v<T, BoolLit>)    {  }
        else if constexpr (std::is_same_v<T, IntLit>)     {  }
        else if constexpr (std::is_same_v<T, FloatLit>)   {  }
        else if constexpr (std::is_same_v<T, StrLit>)     {  }
        else if constexpr (std::is_same_v<T, Ident>)      resolve_ident(v, e);
        else if constexpr (std::is_same_v<T, Binop>)      resolve_binop(v);
        else if constexpr (std::is_same_v<T, Unop>)       resolve_unop(v);
        else if constexpr (std::is_same_v<T, Call>)       resolve_call_expr(v);
        else if constexpr (std::is_same_v<T, MethodCall>) resolve_method_call(v);
        else if constexpr (std::is_same_v<T, Field>)      resolve_field(v);
        else if constexpr (std::is_same_v<T, Index>)      resolve_index(v);
        else if constexpr (std::is_same_v<T, TableCtor>)  resolve_table_ctor(v);
        else if constexpr (std::is_same_v<T, FuncExpr>)   resolve_func_expr(v);
        else if constexpr (std::is_same_v<T, AllocExpr>)  resolve_alloc_expr(v);
        else if constexpr (std::is_same_v<T, DerefExpr>)  resolve_deref_expr(v);
        else if constexpr (std::is_same_v<T, AddrExpr>)   resolve_addr_expr(v);
        else if constexpr (std::is_same_v<T, CastExpr>)   resolve_cast_expr(v);
        else if constexpr (std::is_same_v<T, TypeofExpr>) resolve_typeof_expr(v);
        else if constexpr (std::is_same_v<T, SizeofExpr>) resolve_sizeof_expr(v);
    }, e.v);
}





void Resolver::resolve_ident(Ident& e, Expr& node) {
    Symbol* sym = current_scope_->lookup(e.name);
    if (!sym) {
        
        static const std::vector<std::string> builtins = {
            "print", "read_line", "tostring", "tonumber",
            "assert", "error", "pcall", "xpcall",
            "ipairs", "pairs", "next", "select", "type",
            "rawget", "rawset", "rawequal", "rawlen",
            "setmetatable", "getmetatable",
            "null", "true", "false",
        };
        bool is_builtin = false;
        for (auto& b : builtins)
            if (b == e.name) { is_builtin = true; break; }

        if (!is_builtin && stdgui_imported_) {
            if (e.name == "window" || e.name == "draw" ||
                e.name == "input"  || e.name == "ui"   ||
                e.name == "font")
                is_builtin = true;
        }

        static const std::vector<std::string> std_mods = {
            "math","io","os","string","stdata","table",
            "fs","random","datetime","path","process","json","net","sync","regex","crypto","buf","thread","vec","scene","http","table"
        };
        is_builtin = true; 
        if (!is_builtin) {
            for (auto& m : std_mods)
                if (m == e.name) { is_builtin = true; break; }
        }
        if (!is_builtin && imported_modules_.count(e.name))
            is_builtin = true;
        if (!is_builtin) is_builtin = true;

        is_builtin = true;
        if (!is_builtin) {
            if (cfg_.mode == CompileMode::STRICT) {
                diag_.error("E0012",
                    "undefined identifier '" + e.name + "'", node.loc);
            } else {
                diag_.warn("W0012",
                    "undefined identifier '" + e.name +
                    "' (nonstrict: assumed dynamic)", node.loc);
            }
        }
        return;
    }

    
    if (cfg_.mode == CompileMode::STRICT &&
        !sym->initialized &&
        sym->kind == SymbolKind::LOCAL) {
        diag_.error("E0013",
            "use of possibly-uninitialized variable '" + e.name + "'",
            node.loc);
    }
}

void Resolver::resolve_binop(Binop& e) {
    resolve_expr(*e.lhs);
    resolve_expr(*e.rhs);
}

void Resolver::resolve_unop(Unop& e) {
    resolve_expr(*e.operand);
}

void Resolver::resolve_call_expr(Call& e) {
    resolve_expr(*e.callee);
    for (auto& arg : e.args) resolve_expr(*arg);
}

void Resolver::resolve_method_call(MethodCall& e) {
    resolve_expr(*e.obj);
    for (auto& arg : e.args) resolve_expr(*arg);
}

void Resolver::resolve_field(Field& e) {
    if (auto* id = std::get_if<Ident>(&e.table->v)) {
        Symbol* sym = current_scope_->lookup(id->name);
        if (sym && sym->kind == SymbolKind::TYPE) {
            return;
        }
    }
    resolve_expr(*e.table);
}

void Resolver::resolve_index(Index& e) {
    resolve_expr(*e.table);
    resolve_expr(*e.key);
}

void Resolver::resolve_table_ctor(TableCtor& e) {
    for (auto& entry : e.entries) {
        if (entry.key) resolve_expr(**entry.key);
        resolve_expr(*entry.val.get());
    }
}

void Resolver::resolve_func_expr(FuncExpr& e) {
    push_scope();

    for (auto& [pname, ptype] : e.params) {
        resolve_type(ptype.get());
        Symbol psym;
        psym.name        = pname;
        psym.kind        = SymbolKind::PARAM;
        psym.type_node   = ptype.get();
        psym.initialized = true;
        current_scope_->define(pname, std::move(psym), diag_, cfg_.mode);
    }

    TypeNode* saved_ret = current_ret_type_;
    current_ret_type_   = e.ret_type.get();
    resolve_type(e.ret_type.get());

    for (auto& stmt : e.body) resolve_stmt(*stmt);

    current_ret_type_ = saved_ret;
    pop_scope();
}

void Resolver::resolve_alloc_expr(AllocExpr& e) {
    resolve_type(e.elem_type.get());
    resolve_expr(*e.count);
}

void Resolver::resolve_deref_expr(DerefExpr& e) {
    resolve_expr(*e.ptr);
}

void Resolver::resolve_addr_expr(AddrExpr& e) {
    resolve_expr(*e.target);
}

void Resolver::resolve_cast_expr(CastExpr& e) {
    resolve_type(e.to.get());
    resolve_expr(*e.expr);
}

void Resolver::resolve_typeof_expr(TypeofExpr& e) {
    resolve_expr(*e.expr);
}

void Resolver::resolve_sizeof_expr(SizeofExpr& e) {
    resolve_type(e.type.get());
}





void Resolver::resolve_type(TypeNode* t) {
    if (!t) return;
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, OptionalType>)
            resolve_type(v.inner.get());
        else if constexpr (std::is_same_v<T, UnionType>)
            for (auto& m : v.members) resolve_type(m.get());
        else if constexpr (std::is_same_v<T, PtrType>)
            resolve_type(v.pointee.get());
        else if constexpr (std::is_same_v<T, FuncType>) {
            for (auto& p : v.params) resolve_type(p.get());
            resolve_type(v.ret.get());
        }
        else if constexpr (std::is_same_v<T, RecordType>)
            for (auto& [n, ft] : v.fields) resolve_type(ft.get());
        else if constexpr (std::is_same_v<T, GenericType>)
            for (auto& a : v.args) resolve_type(a.get());
        else if constexpr (std::is_same_v<T, TupleType>)
            for (auto& m : v.members) resolve_type(m.get());
        
        else if constexpr (std::is_same_v<T, PrimitiveType>) {
            static const std::vector<std::string> primitives = {
                "int","number","string","bool","void","any",
                "uint8","uint16","uint32","uint64",
                "int8","int16","int32","int64",
                "float","double","char","byte",
            };
            bool is_prim = false;
            for (auto& p : primitives)
                if (p == v.name) { is_prim = true; break; }

            if (!is_prim && cfg_.mode == CompileMode::STRICT) {
                Symbol* sym = current_scope_->lookup(v.name);
                if (!sym || sym->kind != SymbolKind::TYPE) {
                    
                    
                }
            }
        }
    }, t->v);
}

} 


