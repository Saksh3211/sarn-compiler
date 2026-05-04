#include "sarn/TypeChecker.h"
#include <sstream>
#include <cassert>

namespace sarn {
    std::string SarnType::to_string() const {
        switch (kind) {
            case TypeKind::INT:    return "int";
            case TypeKind::NUMBER: return "number";
            case TypeKind::STRING: return "string";
            case TypeKind::BOOL:   return "bool";
            case TypeKind::VOID:   return "void";
            case TypeKind::ANY:    return "any";
            case TypeKind::NULL_T: return "null";
            case TypeKind::ERROR:  return "<error>";
            case TypeKind::PTR: {
                std::string s = "ptr<";
                s += pointee ? pointee->to_string() : "?";
                return s + ">";
            }
            case TypeKind::RECORD: {
                std::string s = "{";
                for (size_t i = 0; i < fields.size(); i++) {
                    if (i) s += ", ";
                    s += fields[i].name + ": " + fields[i].type->to_string();
                }
                return s + "}";
            }
            case TypeKind::FUNC: {
                std::string s = "(";
                for (size_t i = 0; i < param_types.size(); i++) {
                    if (i) s += ", ";
                    s += param_types[i]->to_string();
                }
                s += ") -> ";
                s += return_type ? return_type->to_string() : "void";
                return s;
            }
            case TypeKind::GENERIC: {
                std::string s = name + "<";
                for (size_t i = 0; i < type_args.size(); i++) {
                    if (i) s += ", ";
                    s += type_args[i]->to_string();
                }
                return s + ">";
            }
            default: return "unknown";
        }
    }

    

    static SarnTypePtr make_prim(TypeKind k, const std::string& name = "") {
        auto t = std::make_shared<SarnType>();
        t->kind = k;
        t->name = name;
        return t;
    }

    SarnTypePtr make_int()    { return make_prim(TypeKind::INT,    "int");    }
    SarnTypePtr make_number() { return make_prim(TypeKind::NUMBER, "number"); }
    SarnTypePtr make_string() { return make_prim(TypeKind::STRING, "string"); }
    SarnTypePtr make_bool()   { return make_prim(TypeKind::BOOL,   "bool");   }
    SarnTypePtr make_void()   { return make_prim(TypeKind::VOID,   "void");   }
    SarnTypePtr make_any()    { return make_prim(TypeKind::ANY,    "any");    }
    SarnTypePtr make_null()   { return make_prim(TypeKind::NULL_T, "null");   }
    SarnTypePtr make_error()  { return make_prim(TypeKind::ERROR,  "<error>");}

    SarnTypePtr make_ptr(SarnTypePtr inner) {
        auto t = std::make_shared<SarnType>();
        t->kind    = TypeKind::PTR;
        t->name    = "ptr";
        t->pointee = std::move(inner);
        return t;
    }

    SarnTypePtr make_func(std::vector<SarnTypePtr> params, SarnTypePtr ret) {
        auto t = std::make_shared<SarnType>();
        t->kind        = TypeKind::FUNC;
        t->param_types = std::move(params);
        t->return_type = std::move(ret);
        return t;
    }

    void TypeChecker::push_env() {
        auto* e  = new TypeEnv();
        e->parent = env_;
        env_      = e;
    }

    void TypeChecker::pop_env() {
        assert(env_ && "pop_env on empty env");
        TypeEnv* old = env_;
        env_         = old->parent;
        delete old;
    }

    
    
    

    void TypeChecker::install_builtins() {
        
        env_->define("print",
            make_func({make_any()}, make_void()));

        
        env_->define("tostring",
            make_func({make_any()}, make_string()));

        
        env_->define("tonumber",
            make_func({make_any()}, make_number()));

        
        env_->define("read_line",
            make_func({}, make_any()));

        
        env_->define("assert",
            make_func({make_bool(), make_string()}, make_void()));

        
        env_->define("type",
            make_func({make_any()}, make_string()));

        
        
    }
    
    SarnTypePtr TypeChecker::resolve_type_node(const TypeNode* t) {
        if (!t) return make_any();

        return std::visit([&](auto&& v) -> SarnTypePtr {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, PrimitiveType>) {
                const std::string& n = v.name;
                if (n == "int"   || n == "int8"  || n == "int16" ||
                    n == "int32" || n == "int64" ||
                    n == "uint8" || n == "uint16"|| n == "uint32"||
                    n == "uint64"|| n == "char"  || n == "byte")
                    return make_int();
                if (n == "number" || n == "float" || n == "double")
                    return make_number();
                if (n == "string")  return make_string();
                if (n == "bool")    return make_bool();
                if (n == "void")    return make_void();
                if (n == "any")     return make_any();
                if (n == "null")    return make_null();

                
                SarnTypePtr found = env_ ? env_->lookup(n) : nullptr;
                if (found) return found;

                
                if (cfg_.mode == CompileMode::STRICT) {
                    
                    return make_error();
                }
                return make_any();
            }

            else if constexpr (std::is_same_v<T, OptionalType>) {
                return make_any();
            }

            else if constexpr (std::is_same_v<T, UnionType>) {
                return make_any(); 
            }

            else if constexpr (std::is_same_v<T, PtrType>) {
                return make_ptr(resolve_type_node(v.pointee.get()));
            }

            else if constexpr (std::is_same_v<T, GenericType>) {
                if (v.name == "ptr" && !v.args.empty())
                    return make_ptr(resolve_type_node(v.args[0].get()));

                
                auto gt = std::make_shared<SarnType>();
                gt->kind = TypeKind::GENERIC;
                gt->name = v.name;
                for (auto& a : v.args)
                    gt->type_args.push_back(resolve_type_node(a.get()));

                
                SarnTypePtr base = env_ ? env_->lookup(v.name) : nullptr;
                if (base && base->kind == TypeKind::RECORD) {
                    
                    
                    return base;
                }
                return gt;
            }

            else if constexpr (std::is_same_v<T, RecordType>) {
                auto rt = std::make_shared<SarnType>();
                rt->kind = TypeKind::RECORD;
                for (auto& [fname, ftype] : v.fields)
                    rt->fields.push_back({fname, resolve_type_node(ftype.get())});
                return rt;
            }

            else if constexpr (std::is_same_v<T, FuncType>) {
                std::vector<SarnTypePtr> params;
                for (auto& p : v.params)
                    params.push_back(resolve_type_node(p.get()));
                return make_func(std::move(params), resolve_type_node(v.ret.get()));
            }

            else if constexpr (std::is_same_v<T, TupleType>) {
                return make_any();
            }
            return make_any();
        }, t->v);
    }

    
    
    

    bool TypeChecker::check(Module& mod) {
        push_env();
        install_builtins();

        for (auto& s : mod.stmts)
            check_stmt(*s);

        pop_env();
        return !diag_.has_errors();
    }

    void TypeChecker::check_stmt(Stmt& s) {
        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if      constexpr (std::is_same_v<T, LocalDecl>)   check_local_decl(v, s.loc);
            else if constexpr (std::is_same_v<T, GlobalDecl>)  check_global_decl(v, s.loc);
            else if constexpr (std::is_same_v<T, FuncDecl>)    check_func_decl(v, s.loc);
            else if constexpr (std::is_same_v<T, IfStmt>)      check_if_stmt(v, s.loc);
            else if constexpr (std::is_same_v<T, WhileStmt>)   check_while_stmt(v);
            else if constexpr (std::is_same_v<T, RepeatStmt>)  check_repeat_stmt(v);
            else if constexpr (std::is_same_v<T, NumericFor>)  check_numeric_for(v, s.loc);
            else if constexpr (std::is_same_v<T, ReturnStmt>)  check_return_stmt(v, s.loc);
            else if constexpr (std::is_same_v<T, Assign>)      check_assign(v, s.loc);
            else if constexpr (std::is_same_v<T, CallStmt>)    check_call_stmt(v);
            else if constexpr (std::is_same_v<T, DeferStmt>)   check_defer_stmt(v);
            else if constexpr (std::is_same_v<T, DoBlock>)     check_do_block(v);
            else if constexpr (std::is_same_v<T, StoreStmt>)   check_store_stmt(v, s.loc);
            else if constexpr (std::is_same_v<T, FreeStmt>)    check_free_stmt(v, s.loc);
            else if constexpr (std::is_same_v<T, PanicStmt>)   check_panic_stmt(v);
            else if constexpr (std::is_same_v<T, TypeDecl>)    check_type_decl(v, s.loc);
            else if constexpr (std::is_same_v<T, ExternDecl>)  check_extern_decl(v, s.loc);
            else if constexpr (std::is_same_v<T, BreakStmt>)   {}
            else if constexpr (std::is_same_v<T, ContinueStmt>){}

            else if constexpr (std::is_same_v<T, EnumDecl>) {
                env_->define(v.name, make_int());
                for (auto& [mname, mval] : v.members)
                    env_->define(mname, make_int());
            }
            else if constexpr (std::is_same_v<T, MultiLocalDecl>) {
                if (v.init) check_expr(*v.init);
                for (auto& [vname, vtype] : v.vars) {
                    SarnTypePtr t = vtype ? resolve_type_node(vtype.get()) : make_any();
                    env_->define(vname, t);
                }
            }
            else if constexpr (std::is_same_v<T, ImportDecl>) {
                if (v.module_name == "stdgui") {
                    stdgui_imported_ = true;
                    env_->define("window", make_any());
                    env_->define("draw", make_any());
                    env_->define("input", make_any());
                    env_->define("ui",make_any());
                    env_->define("font",make_any());
                } else if (v.module_name == "math")   { env_->define("math",   make_any()); }
                else if (v.module_name == "os") { env_->define("os",     make_any()); }
                else if (v.module_name == "io") { env_->define("io",     make_any()); }
                else if (v.module_name == "string") { env_->define("string", make_any()); }
                else if (v.module_name == "stdata") { env_->define("stdata", make_any()); }
                else if (v.module_name == "fs")       { env_->define("fs",       make_any()); }
                else if (v.module_name == "random")   { env_->define("random",   make_any()); }
                else if (v.module_name == "datetime") { env_->define("datetime", make_any()); }
                else if (v.module_name == "path")     { env_->define("path",     make_any()); }
                else if (v.module_name == "process")  { env_->define("process",  make_any()); }
                else if (v.module_name == "json")     { env_->define("json",     make_any()); }
                else if (v.module_name == "net")      { env_->define("net",      make_any()); }
                else if (v.module_name == "sync")     { env_->define("sync",     make_any()); }
                else if (v.module_name == "regex")    { env_->define("regex",    make_any()); }
                else if (v.module_name == "crypto")  { env_->define("crypto",  make_any()); }
                else if (v.module_name == "buf")     { env_->define("buf",     make_any()); }
                else if (v.module_name == "thread")  { env_->define("thread",  make_any()); }
                else if (v.module_name == "vec")     { env_->define("vec",     make_any()); }
                else if (v.module_name == "scene")   { env_->define("scene",   make_any()); }
                else if (v.module_name == "http")    { env_->define("http",    make_any()); }
                else if (v.module_name == "table")   { env_->define("table",   make_any()); }
                else { env_->define(v.module_name, make_any()); }
            }
        else if constexpr (std::is_same_v<T, FileImportDecl>) {}
        else if constexpr (std::is_same_v<T, ImportDecl>) { env_->define(v.module_name, make_any()); }
        }, s.v);
    }

    void TypeChecker::check_local_decl(LocalDecl& s, SourceLoc loc) {
        SarnTypePtr ann_type = s.type_ann
            ? resolve_type_node(s.type_ann.get())
            : nullptr;

        SarnTypePtr init_type;
        if (s.init) init_type = check_expr(*s.init);

        SarnTypePtr final_type;

        if (ann_type && init_type) {
            
            if (!is_assignable(init_type, ann_type, loc,
                    "initialiser of '" + s.name + "'"))
                final_type = ann_type; 
            else
                final_type = ann_type;
        } else if (ann_type) {
            final_type = ann_type;
        } else if (init_type) {
            final_type = init_type; 
        } else {
            
            final_type = (cfg_.mode == CompileMode::STRICT) ? make_error() : make_any();
        }

        env_->define(s.name, final_type);
    }

    void TypeChecker::check_global_decl(GlobalDecl& s, SourceLoc loc) {
        SarnTypePtr ann_type = s.type_ann
            ? resolve_type_node(s.type_ann.get())
            : nullptr;

        SarnTypePtr init_type;
        if (s.init) init_type = check_expr(*s.init);

        SarnTypePtr final_type = ann_type ? ann_type
                            : init_type ? init_type
                            : make_any();

        if (ann_type && init_type)
            is_assignable(init_type, ann_type, loc,
                "initialiser of global '" + s.name + "'");

        env_->define(s.name, final_type);
    }

    void TypeChecker::check_func_decl(FuncDecl& s, SourceLoc loc) {
        
        std::vector<SarnTypePtr> param_types;
        for (auto& [pname, ptype] : s.params)
            param_types.push_back(resolve_type_node(ptype.get()));

        SarnTypePtr ret = s.ret_type
            ? resolve_type_node(s.ret_type.get())
            : make_void();

        auto fn_type = make_func(param_types, ret);
        env_->define(s.name, fn_type);
        auto dot = s.name.find('.');
        if (dot != std::string::npos) {
            env_->define(s.name, fn_type);
        }
        
        push_env();
        
        for (size_t i = 0; i < s.params.size(); i++) {
            auto& [pname, ptype] = s.params[i];
            env_->define(pname, param_types[i]);
        }

        
        SarnTypePtr saved_ret = ret_type_;
        ret_type_ = ret;

        for (auto& stmt : s.body)
            check_stmt(*stmt);

        ret_type_ = saved_ret;
        pop_env();
    }

    
    
    

    void TypeChecker::check_if_stmt(IfStmt& s, SourceLoc loc) {
        SarnTypePtr cond_t = check_expr(*s.cond);

        
        if (cfg_.mode == CompileMode::STRICT &&
            cond_t && cond_t->kind != TypeKind::BOOL &&
            cond_t->kind != TypeKind::ANY  &&
            cond_t->kind != TypeKind::NULL_T &&
            !cond_t->is_error()) {
            diag_.warn("W0040",
                "if condition is type '" + cond_t->to_string() +
                "', expected bool", loc);
        }

        push_env();
        for (auto& st : s.then_body) check_stmt(*st);
        pop_env();

        for (auto& [ei_cond, ei_body] : s.elseif_clauses) {
            check_expr(*ei_cond);
            push_env();
            for (auto& st : ei_body) check_stmt(*st);
            pop_env();
        }

        if (s.else_body) {
            push_env();
            for (auto& st : *s.else_body) check_stmt(*st);
            pop_env();
        }
    }

    void TypeChecker::check_while_stmt(WhileStmt& s) {
        check_expr(*s.cond);
        push_env();
        for (auto& st : s.body) check_stmt(*st);
        pop_env();
    }

    void TypeChecker::check_repeat_stmt(RepeatStmt& s) {
        push_env();
        for (auto& st : s.body) check_stmt(*st);
        check_expr(*s.until_cond);
        pop_env();
    }

    void TypeChecker::check_numeric_for(NumericFor& s, SourceLoc loc) {
        SarnTypePtr start_t = check_expr(*s.start);
        SarnTypePtr stop_t  = check_expr(*s.stop);
        SarnTypePtr step_t  = s.step ? check_expr(*s.step) : make_int();

        
        if (cfg_.mode == CompileMode::STRICT) {
            if (start_t && !start_t->is_numeric() && start_t->kind != TypeKind::ANY)
                diag_.error("E0041", "for start must be numeric, got '" +
                    start_t->to_string() + "'", loc);
            if (stop_t && !stop_t->is_numeric() && stop_t->kind != TypeKind::ANY)
                diag_.error("E0041", "for stop must be numeric, got '" +
                    stop_t->to_string() + "'", loc);
        }

        
        SarnTypePtr var_type = (start_t && start_t->kind == TypeKind::NUMBER)
            ? make_number() : make_int();

        push_env();
        env_->define(s.var, var_type);
        for (auto& st : s.body) check_stmt(*st);
        pop_env();
    }

    void TypeChecker::check_return_stmt(ReturnStmt& s, SourceLoc loc) {
        if (s.values.empty()) {
            
            if (cfg_.mode == CompileMode::STRICT && ret_type_ &&
                ret_type_->kind != TypeKind::VOID &&
                ret_type_->kind != TypeKind::ANY  &&
                !ret_type_->is_error()) {
                diag_.error("E0031",
                    "missing return value, function returns '" +
                    ret_type_->to_string() + "'", loc);
            }
            return;
        }

        SarnTypePtr val_type = check_expr(*s.values[0]);

        if (ret_type_ && val_type &&
            !ret_type_->is_error() && !val_type->is_error()) {
            is_assignable(val_type, ret_type_, loc, "return value");
        }
    }

    void TypeChecker::check_assign(Assign& s, SourceLoc loc) {
        SarnTypePtr rhs_type = check_expr(*s.value);
        SarnTypePtr lhs_type = check_expr(*s.target);

        if (lhs_type && rhs_type &&
            !lhs_type->is_error() && !rhs_type->is_error()) {
            
            if (cfg_.mode == CompileMode::STRICT)
                is_assignable(rhs_type, lhs_type, loc, "assignment");
        }
    }

    void TypeChecker::check_call_stmt(CallStmt& s) {
        check_expr(*s.call);
    }

    void TypeChecker::check_defer_stmt(DeferStmt& s) {
        if (s.action) check_stmt(*s.action);
    }

    void TypeChecker::check_do_block(DoBlock& s) {
        push_env();
        for (auto& st : s.body) check_stmt(*st);
        pop_env();
    }

    void TypeChecker::check_store_stmt(StoreStmt& s, SourceLoc loc) {
        SarnTypePtr ptr_t = check_expr(*s.ptr);
        SarnTypePtr val_t = check_expr(*s.val);

        if (cfg_.mode == CompileMode::STRICT && ptr_t &&
            ptr_t->kind != TypeKind::PTR &&
            ptr_t->kind != TypeKind::ANY  &&
            !ptr_t->is_error()) {
            diag_.error("E0050",
                "store target must be a pointer, got '" +
                ptr_t->to_string() + "'", loc);
        }
    }

    void TypeChecker::check_free_stmt(FreeStmt& s, SourceLoc loc) {
        SarnTypePtr t = check_expr(*s.ptr);
        if (cfg_.mode == CompileMode::STRICT && t &&
            t->kind != TypeKind::PTR &&
            t->kind != TypeKind::ANY &&
            !t->is_error()) {
            diag_.error("E0051",
                "free requires a pointer, got '" + t->to_string() + "'", loc);
        }
    }

    void TypeChecker::check_panic_stmt(PanicStmt& s) {
        check_expr(*s.msg);
    }

    void TypeChecker::check_type_decl(TypeDecl& s, SourceLoc loc) {
        SarnTypePtr t = resolve_type_node(s.def.get());
        t->name = s.name;
        env_->define(s.name, t);
    }

    void TypeChecker::check_extern_decl(ExternDecl& s, SourceLoc loc) {
        SarnTypePtr ft = resolve_type_node(s.func_type.get());
        env_->define(s.name, ft);
    }

    
    
    

    SarnTypePtr TypeChecker::check_expr(Expr& e) {
        SarnTypePtr result = std::visit([&](auto&& v) -> SarnTypePtr {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, NullLit>)    return make_null();
            else if constexpr (std::is_same_v<T, BoolLit>)  return make_bool();
            else if constexpr (std::is_same_v<T, IntLit>)   return make_int();
            else if constexpr (std::is_same_v<T, FloatLit>) return make_number();
            else if constexpr (std::is_same_v<T, StrLit>)   return make_string();

            else if constexpr (std::is_same_v<T, Ident>) {
                SarnTypePtr t = env_ ? env_->lookup(v.name) : nullptr;
                if (!t) return make_any(); 
                return t;
            }

            else if constexpr (std::is_same_v<T, Binop>)
                return check_binop(v, e.loc);
            else if constexpr (std::is_same_v<T, Unop>)
                return check_unop(v, e.loc);
            else if constexpr (std::is_same_v<T, Call>)
                return check_call_expr(v, e.loc);
            else if constexpr (std::is_same_v<T, MethodCall>)
                return check_method_call(v, e.loc);
            else if constexpr (std::is_same_v<T, Field>)
                return check_field(v, e.loc);
            else if constexpr (std::is_same_v<T, Index>)
                return check_index(v, e.loc);
            else if constexpr (std::is_same_v<T, TableCtor>)
                return check_table_ctor(v, e.loc);
            else if constexpr (std::is_same_v<T, FuncExpr>)
                return check_func_expr(v, e.loc);
            else if constexpr (std::is_same_v<T, AllocExpr>)
                return check_alloc_expr(v, e.loc);

            else if constexpr (std::is_same_v<T, DerefExpr>) {
                SarnTypePtr pt = check_expr(*v.ptr);
                if (pt && pt->kind == TypeKind::PTR && pt->pointee)
                    return pt->pointee;
                if (pt && pt->kind == TypeKind::ANY) return make_any();
                if (cfg_.mode == CompileMode::STRICT && pt && !pt->is_error())
                    diag_.error("E0052",
                        "deref requires pointer, got '" + pt->to_string() + "'",
                        e.loc);
                return make_any();
            }

            else if constexpr (std::is_same_v<T, AddrExpr>) {
                SarnTypePtr inner = check_expr(*v.target);
                return make_ptr(inner);
            }

            else if constexpr (std::is_same_v<T, CastExpr>) {
                check_expr(*v.expr);
                return resolve_type_node(v.to.get());
            }

            else if constexpr (std::is_same_v<T, TypeofExpr>) {
                check_expr(*v.expr);
                return make_string(); 
            }

            else if constexpr (std::is_same_v<T, SizeofExpr>) {
                return make_int(); 
            }

            return make_any();
        }, e.v);

        
        
        e.inferred_type = nullptr; 

        return result ? result : make_any();
    }

    
    
    

    SarnTypePtr TypeChecker::check_binop(Binop& e, SourceLoc loc) {
        SarnTypePtr lhs = check_expr(*e.lhs);
        SarnTypePtr rhs = check_expr(*e.rhs);

        if (!lhs || !rhs) return make_any();
        if (lhs->kind == TypeKind::ANY || rhs->kind == TypeKind::ANY)
            return make_any(); 

        const std::string& op = e.op;

        
        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
        
        if (lhs->kind == TypeKind::PTR && (rhs->kind == TypeKind::INT || rhs->kind == TypeKind::NUMBER)) return lhs;
        if ((lhs->kind == TypeKind::INT || lhs->kind == TypeKind::NUMBER) && rhs->kind == TypeKind::PTR) return rhs;
            
            if (lhs->kind == TypeKind::NULL_T || rhs->kind == TypeKind::NULL_T) {
                if (cfg_.mode == CompileMode::STRICT)
                    diag_.error("E0060",
                        "arithmetic on null in strict mode", loc);
                return make_null();
            }
            if (!lhs->is_numeric() && !lhs->is_error()) {
                if (cfg_.mode == CompileMode::STRICT)
                    diag_.error("E0061",
                        "arithmetic operand must be numeric, got '" +
                        lhs->to_string() + "'", loc);
                return make_any();
            }
            if (!rhs->is_numeric() && !rhs->is_error()) {
                if (cfg_.mode == CompileMode::STRICT)
                    diag_.error("E0061",
                        "arithmetic operand must be numeric, got '" +
                        rhs->to_string() + "'", loc);
                return make_any();
            }
            
            if (lhs->kind == TypeKind::NUMBER || rhs->kind == TypeKind::NUMBER)
                return make_number();
            return make_int();
        }

        
        if (op == "..") {
            if (cfg_.mode == CompileMode::STRICT) {
                bool lhs_ok = (lhs->kind == TypeKind::STRING ||
                            lhs->is_numeric() || lhs->kind == TypeKind::ANY);
                bool rhs_ok = (rhs->kind == TypeKind::STRING ||
                            rhs->is_numeric() || rhs->kind == TypeKind::ANY);
                if (!lhs_ok && !lhs->is_error())
                    diag_.error("E0062",
                        "concat operand must be string or number, got '" +
                        lhs->to_string() + "'", loc);
                if (!rhs_ok && !rhs->is_error())
                    diag_.error("E0062",
                        "concat operand must be string or number, got '" +
                        rhs->to_string() + "'", loc);
            }
            return make_string();
        }

        
        if (op == "==" || op == "~=" ||
            op == "<"  || op == ">"  ||
            op == "<=" || op == ">=") {
            if (cfg_.mode == CompileMode::STRICT &&
                op != "==" && op != "~=" &&
                !lhs->is_numeric() && !lhs->is_error() &&
                lhs->kind != TypeKind::STRING) {
                diag_.error("E0063",
                    "comparison operand must be numeric or string, got '" +
                    lhs->to_string() + "'", loc);
            }
            return make_bool();
        }

        
        if (op == "and") return rhs; 
        if (op == "or")  return lhs; 

        
        if (op == "&" || op == "|" || op == "~" ||
            op == "<<" || op == ">>") {
            if (cfg_.mode == CompileMode::STRICT) {
                if (lhs->kind != TypeKind::INT && !lhs->is_error())
                    diag_.error("E0064",
                        "bitwise operand must be int, got '" +
                        lhs->to_string() + "'", loc);
                if (rhs->kind != TypeKind::INT && !rhs->is_error())
                    diag_.error("E0064",
                        "bitwise operand must be int, got '" +
                        rhs->to_string() + "'", loc);
            }
            return make_int();
        }

        return make_any();
    }

    
    
    

    SarnTypePtr TypeChecker::check_unop(Unop& e, SourceLoc loc) {
        SarnTypePtr operand = check_expr(*e.operand);
        if (!operand || operand->kind == TypeKind::ANY) return make_any();

        if (e.op == "-") {
            if (cfg_.mode == CompileMode::STRICT &&
                !operand->is_numeric() && !operand->is_error())
                diag_.error("E0065",
                    "unary minus requires numeric, got '" +
                    operand->to_string() + "'", loc);
            return operand;
        }
        if (e.op == "not") return make_bool();
        if (e.op == "#") {
            if (cfg_.mode == CompileMode::STRICT &&
                operand->kind != TypeKind::STRING &&
                operand->kind != TypeKind::RECORD &&
                operand->kind != TypeKind::GENERIC &&
                !operand->is_error())
                diag_.error("E0066",
                    "# requires string or table, got '" +
                    operand->to_string() + "'", loc);
            return make_int();
        }
        return make_any();
    }

    SarnTypePtr TypeChecker::check_call_expr(Call& e, SourceLoc loc) {
        if (auto* fi = std::get_if<Field>(&e.callee->v)) {
        if (auto* id = std::get_if<Ident>(&fi->table->v)) {
            std::string key = id->name + "." + fi->name;
            SarnTypePtr fn_t = env_ ? env_->lookup(key) : nullptr;
            if (fn_t && fn_t->kind == TypeKind::FUNC) {
                for (auto& arg : e.args) check_expr(*arg);
                return fn_t->return_type ? fn_t->return_type : make_void();
            }
        }
        }
        SarnTypePtr callee_t = check_expr(*e.callee);

        std::vector<SarnTypePtr> arg_types;
        for (auto& arg : e.args)
            arg_types.push_back(check_expr(*arg));

        if (!callee_t || callee_t->kind == TypeKind::ANY)
            return make_any(); 

        if (callee_t->kind != TypeKind::FUNC) {
            if (cfg_.mode == CompileMode::STRICT && !callee_t->is_error())
                diag_.error("E0070",
                    "attempt to call a non-function value of type '" +
                    callee_t->to_string() + "'", loc);
            return make_any();
        }

        
        if (cfg_.mode == CompileMode::STRICT &&
            arg_types.size() != callee_t->param_types.size()) {
            diag_.error("E0071",
                "wrong number of arguments: expected " +
                std::to_string(callee_t->param_types.size()) +
                ", got " + std::to_string(arg_types.size()), loc);
        }

        
        size_t n = std::min(arg_types.size(), callee_t->param_types.size());
        for (size_t i = 0; i < n; i++) {
            is_assignable(arg_types[i], callee_t->param_types[i], loc,
                "argument " + std::to_string(i+1));
        }

        return callee_t->return_type ? callee_t->return_type : make_void();
    }

    SarnTypePtr TypeChecker::check_method_call(MethodCall& e, SourceLoc loc) {
        check_expr(*e.obj);
        for (auto& arg : e.args) check_expr(*arg);
        return make_any(); 
    }

    SarnTypePtr TypeChecker::check_field(Field& e, SourceLoc loc) {
        if (auto* id = std::get_if<Ident>(&e.table->v)) {
            std::string method_key = id->name + "." + e.name;
            SarnTypePtr mfn = env_ ? env_->lookup(method_key) : nullptr;
            if (mfn) {
                if (mfn->kind == TypeKind::FUNC)
                    return mfn->return_type ? mfn->return_type : make_void();
                return make_any();
            }
            
            SarnTypePtr sym = env_ ? env_->lookup(id->name) : nullptr;
            if (sym && (sym->kind == TypeKind::RECORD ||sym->kind == TypeKind::GENERIC ||sym->kind == TypeKind::ANY)) {
                return make_any();
            }
        }
        
        SarnTypePtr obj_t = check_expr(*e.table);
        if (!obj_t || obj_t->kind == TypeKind::ANY) return make_any();

        if (obj_t->kind == TypeKind::RECORD) {
            
            for (auto& rf : obj_t->fields)
                if (rf.name == e.name) return rf.type;
            
            if (!obj_t->name.empty()) {
                std::string mk = obj_t->name + "." + e.name;
                SarnTypePtr mfn2 = env_ ? env_->lookup(mk) : nullptr;
                if (mfn2) {
                    if (mfn2->kind == TypeKind::FUNC)
                        return mfn2->return_type ? mfn2->return_type : make_void();
                    return make_any();
                }
            }
            
            if (cfg_.mode == CompileMode::STRICT)
                diag_.warn("W0080",
                    "record type ''" + obj_t->to_string() +
                    "'' has no field ''" + e.name + "''", loc);
            return make_any();
        }
        return make_any();
    }

    SarnTypePtr TypeChecker::check_index(Index& e, SourceLoc loc) {
        SarnTypePtr obj_t = check_expr(*e.table);
        SarnTypePtr key_t = check_expr(*e.key);

        if (obj_t && obj_t->kind == TypeKind::PTR && obj_t->pointee)
            return obj_t->pointee; 

        return make_any();
    }

    
    
    

    SarnTypePtr TypeChecker::check_table_ctor(TableCtor& e, SourceLoc loc) {
        
        bool all_named = true;
        for (auto& entry : e.entries) {
            if (entry.key) check_expr(**entry.key);
            check_expr(*entry.val);
            if (!entry.key) all_named = false;
            else {
                auto* sl = std::get_if<StrLit>(&(*entry.key)->v);
                if (!sl) all_named = false;
            }
        }

        if (all_named && !e.entries.empty()) {
            auto rt = std::make_shared<SarnType>();
            rt->kind = TypeKind::RECORD;
            for (auto& entry : e.entries) {
                if (!entry.key) continue;
                auto* sl = std::get_if<StrLit>(&(*entry.key)->v);
                if (sl) {
                    SarnTypePtr ft = check_expr(*entry.val);
                    RecordField rf;
                    rf.name = sl->val;
                    rf.type = ft;
                    rt->fields.push_back(std::move(rf));
                }
            }
            return rt;
        }

        return make_any(); 
    }

    
    
    

    SarnTypePtr TypeChecker::check_func_expr(FuncExpr& e, SourceLoc loc) {
        std::vector<SarnTypePtr> param_types;
        for (auto& [pname, ptype] : e.params)
            param_types.push_back(resolve_type_node(ptype.get()));

        SarnTypePtr ret = e.ret_type
            ? resolve_type_node(e.ret_type.get())
            : make_void();

        push_env();
        for (size_t i = 0; i < e.params.size(); i++)
            env_->define(e.params[i].first, param_types[i]);

        SarnTypePtr saved_ret = ret_type_;
        ret_type_ = ret;
        for (auto& stmt : e.body) check_stmt(*stmt);
        ret_type_ = saved_ret;
        pop_env();

        return make_func(std::move(param_types), ret);
    }

    
    
    

    SarnTypePtr TypeChecker::check_alloc_expr(AllocExpr& e, SourceLoc loc) {
        SarnTypePtr elem = resolve_type_node(e.elem_type.get());
        SarnTypePtr count_t = check_expr(*e.count);

        if (cfg_.mode == CompileMode::STRICT && count_t &&
            !count_t->is_numeric() && count_t->kind != TypeKind::ANY &&
            !count_t->is_error()) {
            diag_.error("E0053",
                "alloc count must be numeric, got '" +
                count_t->to_string() + "'", loc);
        }
        return make_ptr(elem);
    }

    
    
    

    bool TypeChecker::types_equal(SarnTypePtr a, SarnTypePtr b) {
        if (!a || !b) return false;
        if (a->kind != b->kind) return false;
        switch (a->kind) {
            case TypeKind::INT:
            case TypeKind::NUMBER:
            case TypeKind::STRING:
            case TypeKind::BOOL:
            case TypeKind::VOID:
            case TypeKind::ANY:
            case TypeKind::NULL_T:
                return true;
            case TypeKind::PTR:
                return types_equal(a->pointee, b->pointee);
            case TypeKind::RECORD:
                if (a->fields.size() != b->fields.size()) return false;
                for (size_t i = 0; i < a->fields.size(); i++) {
                    if (a->fields[i].name != b->fields[i].name) return false;
                    if (!types_equal(a->fields[i].type, b->fields[i].type))
                        return false;
                }
                return true;
            case TypeKind::GENERIC:
                if (a->name != b->name) return false;
                if (a->type_args.size() != b->type_args.size()) return false;
                for (size_t i = 0; i < a->type_args.size(); i++)
                    if (!types_equal(a->type_args[i], b->type_args[i]))
                        return false;
                return true;
            default:
                return a->name == b->name;
        }
    }

    bool TypeChecker::is_nullable(SarnTypePtr t) {
        if (!t) return true;
        return t->kind == TypeKind::NULL_T || t->kind == TypeKind::ANY;
    }

    bool TypeChecker::is_assignable(SarnTypePtr from, SarnTypePtr to,
                                    SourceLoc loc, const std::string& ctx) {
        if (!from || !to) return true;
        if (from->is_error() || to->is_error()) return true; 

        
        if (to->kind == TypeKind::ANY || from->kind == TypeKind::ANY) return true;

        
        if (from->kind == TypeKind::NULL_T) {
            if (to->kind == TypeKind::PTR) return true;
            if (cfg_.mode == CompileMode::NONSTRICT) return true;
            if (cfg_.mode == CompileMode::STRICT) {
                diag_.error("E0090",
                    ctx + ": cannot assign null to non-nullable type '" +
                    to->to_string() + "'", loc);
                return false;
            }
        }

        
        if (from->kind == TypeKind::INT && to->kind == TypeKind::NUMBER) return true;

        
        if (types_equal(from, to)) return true;

        
        if (from->kind == TypeKind::RECORD && to->kind == TypeKind::RECORD) {
            
            for (auto& tf : to->fields) {
                bool found = false;
                for (auto& ff : from->fields) {
                    if (ff.name == tf.name && (types_equal(ff.type, tf.type) || ff.type->kind == TypeKind::ANY || tf.type->kind == TypeKind::ANY)) {
                        found = true; break;
                    }
                }
                if (!found) {
                    if (cfg_.mode == CompileMode::STRICT) {
                        diag_.error("E0091",
                            ctx + ": record missing field '" + tf.name +
                            "' of type '" + tf.type->to_string() + "'", loc);
                        return false;
                    }
                }
            }
            return true;
        }

        
        if (cfg_.mode == CompileMode::STRICT) {
            diag_.error("E0092",
                ctx + ": type mismatch cannot assign '" + from->to_string() +
                "' to '" + to->to_string() + "'", loc);
            return false;
        } else {
            if (cfg_.type_annotation_viol == DiagBehavior::WARNING)
                diag_.warn("W0092",
                    ctx + ": type mismatch " + from->to_string() +
                    "' assigned to '" + to->to_string() + "'", loc);
            return true;
        }
    }

} 

