#ifdef SLUA_HAS_LLVM

#include "slua/IREmitter.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ToolOutputFile.h>
#include <cassert>
#include <sstream>

namespace slua {

IREmitter::IREmitter(DiagEngine& diag, SemanticConfig cfg,
                     const std::string& module_name)
    : diag_(diag), cfg_(cfg),
      mod_(std::make_unique<llvm::Module>(module_name, ctx_)),
      builder_(ctx_) {
    declare_runtime();
}

void IREmitter::push_env() {
    auto* e  = new VarEnv();
    e->parent = env_;
    env_      = e;
}

void IREmitter::pop_env() {
    assert(env_);
    VarEnv* old = env_;
    env_ = old->parent;
    delete old;
}

void IREmitter::push_defer_scope() {
    defer_stack_.push_back({});
}

void IREmitter::pop_defer_scope() {
    if (defer_stack_.empty()) return;
    auto& scope = defer_stack_.back();

    for (int i = (int)scope.size() - 1; i >= 0; i--)
        scope[i].emit_fn();
    defer_stack_.pop_back();
}

void IREmitter::add_defer(std::function<void()> fn) {
    if (!defer_stack_.empty())
        defer_stack_.back().push_back({std::move(fn)});
}

llvm::AllocaInst* IREmitter::create_alloca(llvm::Type* ty,
                                            const std::string& name) {
    assert(cur_func_ && "create_alloca outside function");
    llvm::IRBuilder<> tmp(&cur_func_->getEntryBlock(),
                           cur_func_->getEntryBlock().begin());
    return tmp.CreateAlloca(ty, nullptr, name);
}

llvm::Type* IREmitter::llvm_type_named(const std::string& name) {
    if (name == "int"   || name == "int64") return llvm::Type::getInt64Ty(ctx_);
    if (name == "int32" || name == "int16") return llvm::Type::getInt32Ty(ctx_);
    if (name == "int8"  || name == "char" || name == "byte" ||
        name == "uint8")                    return llvm::Type::getInt8Ty(ctx_);
    if (name == "uint16")                   return llvm::Type::getInt16Ty(ctx_);
    if (name == "uint32")                   return llvm::Type::getInt32Ty(ctx_);
    if (name == "uint64")                   return llvm::Type::getInt64Ty(ctx_);
    if (name == "number" || name == "float" ||
        name == "double")                   return llvm::Type::getDoubleTy(ctx_);
    if (name == "bool")                     return llvm::Type::getInt1Ty(ctx_);
    if (name == "void")                     return llvm::Type::getVoidTy(ctx_);
    if (name == "string")                   return llvm::PointerType::getUnqual(
                                                llvm::Type::getInt8Ty(ctx_));
    if (name == "any")                      return tagvalue_type();

    auto it = struct_types_.find(name);
    if (it != struct_types_.end()) return it->second;

    return llvm::Type::getInt64Ty(ctx_);
}

llvm::Type* IREmitter::llvm_type(const TypeNode* t) {
    if (!t) return llvm::Type::getInt64Ty(ctx_);

    return std::visit([&](auto&& v) -> llvm::Type* {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, PrimitiveType>)
            return llvm_type_named(v.name);

        else if constexpr (std::is_same_v<T, PtrType>)
            return llvm::PointerType::getUnqual(llvm_type(v.pointee.get()));

        else if constexpr (std::is_same_v<T, GenericType>) {
            if (v.name == "ptr" && !v.args.empty())
                return llvm::PointerType::getUnqual(
                    llvm_type(v.args[0].get()));

            return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
        }

        else if constexpr (std::is_same_v<T, RecordType>) {

            std::vector<llvm::Type*> fields;
            for (auto& [fn, ft] : v.fields)
                fields.push_back(llvm_type(ft.get()));
            return llvm::StructType::get(ctx_, fields);
        }

        else if constexpr (std::is_same_v<T, FuncType>) {
            std::vector<llvm::Type*> params;
            for (auto& p : v.params)
                params.push_back(llvm_type(p.get()));
            llvm::Type* ret = v.ret
                ? llvm_type(v.ret.get())
                : llvm::Type::getVoidTy(ctx_);
            return llvm::FunctionType::get(ret, params, false)
                        ->getPointerTo();
        }

        else if constexpr (std::is_same_v<T, OptionalType>)
            return llvm_type(v.inner.get());

        else if constexpr (std::is_same_v<T, UnionType>)
            return tagvalue_type();

        return llvm::Type::getInt64Ty(ctx_);
    }, t->v);
}

llvm::Type* IREmitter::tagvalue_type() {
    static llvm::StructType* tv = nullptr;
    if (!tv) {
        tv = llvm::StructType::create(ctx_, "SluaValue");
        tv->setBody({
            llvm::Type::getInt8Ty(ctx_),
            llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx_), 7),
            llvm::Type::getInt64Ty(ctx_)
        });
    }
    return tv;
}

void IREmitter::declare_runtime() {
    auto* i8p  = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
    auto* i64  = llvm::Type::getInt64Ty(ctx_);
    auto* i32  = llvm::Type::getInt32Ty(ctx_);
    auto* f64  = llvm::Type::getDoubleTy(ctx_);
    auto* voidT= llvm::Type::getVoidTy(ctx_);

    auto declare = [&](const std::string& name,
                       llvm::Type* ret,
                       std::vector<llvm::Type*> params,
                       bool vararg = false) {
        auto* ft = llvm::FunctionType::get(ret, params, vararg);
        auto* fn = llvm::Function::Create(
            ft, llvm::Function::ExternalLinkage, name, *mod_);
        functions_[name] = fn;
    };

    declare("slua_alloc",       i8p,   {i64});
    declare("slua_free",        voidT, {i8p});
    declare("slua_panic",       voidT, {i8p, i8p, i32});
    declare("slua_print_str",   voidT, {i8p});
    declare("slua_print_int",   voidT, {i64});
    declare("slua_print_float", voidT, {f64});
    declare("slua_print_bool",  voidT, {i32});
    declare("slua_print_null",  voidT, {});
    declare("slua_time_ns",     i64,   {});
    declare("slua_exit",        voidT, {i32});

    declare("slua_read_line",        i8p,   {});
    declare("slua_read_char",        i32,   {});
    declare("slua_io_clear",         voidT, {});
    declare("slua_io_set_color",     voidT, {i8p});
    declare("slua_io_reset_color",   voidT, {});
    declare("slua_io_print_color",   voidT, {i8p, i8p});
    declare("slua_print_str_no_newline", voidT, {i8p});
    declare("slua_flush",            voidT, {});

    declare("slua_str_concat",  i8p,  {i8p, i8p});
    declare("slua_int_to_str",  i8p,  {i64});
    declare("slua_float_to_str",i8p,  {f64});

    declare("slua_sqrt",  f64, {f64});
    declare("slua_pow",   f64, {f64, f64});
    declare("slua_sin",   f64, {f64});
    declare("slua_cos",   f64, {f64});
    declare("slua_tan",   f64, {f64});
    declare("slua_log",   f64, {f64});
    declare("slua_log2",  f64, {f64});
    declare("slua_exp",   f64, {f64});
    declare("slua_inf",   f64, {});
    declare("slua_nan",   f64, {});

    declare("slua_tbl_new",      i8p,   {});
    declare("slua_tbl_iset_i64", voidT, {i8p, i64, i64});
    declare("slua_tbl_iset_f64", voidT, {i8p, i64, f64});
    declare("slua_tbl_iset_str", voidT, {i8p, i64, i8p});
    declare("slua_tbl_iset_bool",voidT, {i8p, i64, i32});
    declare("slua_tbl_sset_i64", voidT, {i8p, i8p, i64});
    declare("slua_tbl_sset_f64", voidT, {i8p, i8p, f64});
    declare("slua_tbl_sset_str", voidT, {i8p, i8p, i8p});
    declare("slua_tbl_sset_bool",voidT, {i8p, i8p, i32});
    declare("slua_tbl_iget_i64", i64,   {i8p, i64});
    declare("slua_tbl_iget_f64", f64,   {i8p, i64});
    declare("slua_tbl_iget_str", i8p,   {i8p, i64});
    declare("slua_tbl_iget_bool",i32,   {i8p, i64});
    declare("slua_tbl_sget_i64", i64,   {i8p, i8p});
    declare("slua_tbl_sget_f64", f64,   {i8p, i8p});
    declare("slua_tbl_sget_str", i8p,   {i8p, i8p});
    declare("slua_tbl_sget_bool",i32,   {i8p, i8p});
}

llvm::Function* IREmitter::get_runtime_fn(const std::string& name) {
    auto it = functions_.find(name);
    if (it != functions_.end()) return it->second;
    return nullptr;
}

bool IREmitter::emit(slua::Module& mod) {
    cur_mode_ = mod.mode;
    push_env();

    for (auto& s : mod.stmts) {
        if (auto* fd = std::get_if<FuncDecl>(&s->v)) {

            std::vector<llvm::Type*> param_tys;
            for (auto& [pn, pt] : fd->params)
                param_tys.push_back(llvm_type(pt.get()));

            llvm::Type* ret_ty = fd->ret_type
                ? llvm_type(fd->ret_type.get())
                : llvm::Type::getVoidTy(ctx_);

            auto* ft = llvm::FunctionType::get(ret_ty, param_tys, false);
            auto* fn = llvm::Function::Create(
                ft, llvm::Function::ExternalLinkage, fd->name, *mod_);

            size_t i = 0;
            for (auto& arg : fn->args())
                arg.setName(fd->params[i++].first);

            functions_[fd->name] = fn;
        }
        else if (auto* ed = std::get_if<ExternDecl>(&s->v)) {
            emit_extern_decl(*ed, s->loc);
        }
        else if (auto* td = std::get_if<TypeDecl>(&s->v)) {
            emit_type_decl(*td, s->loc);
        }
    }

    for (auto& s : mod.stmts)
        emit_stmt(*s);

    pop_env();

    std::string err;
    llvm::raw_string_ostream es(err);
    if (llvm::verifyModule(*mod_, &es)) {
        diag_.error("E0100", "LLVM IR verification failed: " + err, {});
        return false;
    }
    return true;
}

bool IREmitter::write_ll(const std::string& path) {
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_Text);
    if (ec) {
        diag_.error("E0101", "cannot open output file: " + ec.message(), {});
        return false;
    }
    mod_->print(out, nullptr);
    return true;
}

bool IREmitter::write_bc(const std::string& path) {
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        diag_.error("E0101", "cannot open output file: " + ec.message(), {});
        return false;
    }
    llvm::WriteBitcodeToFile(*mod_, out);
    return true;
}

void IREmitter::dump() {
    mod_->print(llvm::errs(), nullptr);
}

void IREmitter::emit_panic(const std::string& msg, SourceLoc loc) {
    auto* fn = get_runtime_fn("slua_panic");
    if (!fn) return;
    auto* msg_val  = builder_.CreateGlobalStringPtr(msg);
    auto* file_val = builder_.CreateGlobalStringPtr(loc.filename);
    auto* line_val = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(ctx_), loc.line);
    builder_.CreateCall(fn, {msg_val, file_val, line_val});
    builder_.CreateUnreachable();
}

void IREmitter::emit_stmt(Stmt& s) {

    if (cur_func_) {
        auto* bb = builder_.GetInsertBlock();
        if (bb && bb->getTerminator()) return;
    }

    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if      constexpr (std::is_same_v<T, LocalDecl>)    emit_local_decl(v, s.loc);
        else if constexpr (std::is_same_v<T, GlobalDecl>)   emit_global_decl(v, s.loc);
        else if constexpr (std::is_same_v<T, FuncDecl>)     emit_func_decl(v, s.loc);
        else if constexpr (std::is_same_v<T, IfStmt>)       emit_if_stmt(v);
        else if constexpr (std::is_same_v<T, WhileStmt>)    emit_while_stmt(v);
        else if constexpr (std::is_same_v<T, RepeatStmt>)   emit_repeat_stmt(v);
        else if constexpr (std::is_same_v<T, NumericFor>)   emit_numeric_for(v);
        else if constexpr (std::is_same_v<T, ReturnStmt>)   emit_return_stmt(v, s.loc);
        else if constexpr (std::is_same_v<T, Assign>)       emit_assign(v, s.loc);
        else if constexpr (std::is_same_v<T, CallStmt>)     emit_call_stmt(v);
        else if constexpr (std::is_same_v<T, DeferStmt>)    emit_defer_stmt(v);
        else if constexpr (std::is_same_v<T, DoBlock>)      emit_do_block(v);
        else if constexpr (std::is_same_v<T, StoreStmt>)    emit_store_stmt(v);
        else if constexpr (std::is_same_v<T, FreeStmt>)     emit_free_stmt(v);
        else if constexpr (std::is_same_v<T, PanicStmt>)    emit_panic_stmt(v);
        else if constexpr (std::is_same_v<T, TypeDecl>)     emit_type_decl(v, s.loc);
        else if constexpr (std::is_same_v<T, ExternDecl>)   emit_extern_decl(v, s.loc);
        else if constexpr (std::is_same_v<T, BreakStmt>)    {  }
        else if constexpr (std::is_same_v<T, ContinueStmt>) {  }
        else if constexpr (std::is_same_v<T, ImportDecl>)   {  }
    }, s.v);
}

void IREmitter::emit_local_decl(LocalDecl& s, SourceLoc loc) {
    llvm::Type* ty = s.type_ann
        ? llvm_type(s.type_ann.get())
        : llvm::Type::getInt64Ty(ctx_);

    llvm::AllocaInst* slot = create_alloca(ty, s.name);

    if (s.init) {
        llvm::Value* val = emit_expr(*s.init);
        if (val) {
            val = coerce(val, ty, loc);
            builder_.CreateStore(val, slot);
        }
    } else {

        builder_.CreateStore(llvm::Constant::getNullValue(ty), slot);
    }

    env_->define(s.name, slot);
}

void IREmitter::emit_global_decl(GlobalDecl& s, SourceLoc loc) {
    llvm::Type* ty = s.type_ann
        ? llvm_type(s.type_ann.get())
        : llvm::Type::getInt64Ty(ctx_);

    llvm::Constant* init_val = llvm::Constant::getNullValue(ty);
    auto* gv = new llvm::GlobalVariable(
        *mod_, ty, false,
        llvm::GlobalValue::InternalLinkage,
        init_val, s.name);

    if (s.init && cur_func_) {

        llvm::Value* val = emit_expr(*s.init);
        if (val) builder_.CreateStore(coerce(val, ty, loc), gv);
    }

    env_->define(s.name, gv);
}

void IREmitter::emit_func_decl(FuncDecl& s, SourceLoc loc) {
    llvm::Function* fn = functions_[s.name];
    if (!fn) return;

    auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", fn);
    auto* exit_bb  = llvm::BasicBlock::Create(ctx_, "exit",  fn);

    builder_.SetInsertPoint(entry_bb);

    llvm::Function*   saved_func     = cur_func_;
    llvm::BasicBlock* saved_ret_bb   = cur_ret_bb_;
    llvm::AllocaInst* saved_ret_slot = cur_ret_slot_;

    cur_func_   = fn;
    cur_ret_bb_ = exit_bb;

    llvm::Type* ret_ty = fn->getReturnType();
    if (!ret_ty->isVoidTy()) {
        cur_ret_slot_ = create_alloca(ret_ty, "retval");
        builder_.CreateStore(llvm::Constant::getNullValue(ret_ty),
                             cur_ret_slot_);
    } else {
        cur_ret_slot_ = nullptr;
    }

    push_env();
    push_defer_scope();

    size_t i = 0;
    for (auto& arg : fn->args()) {
        auto& [pname, ptype] = s.params[i++];
        llvm::Type* pty = llvm_type(ptype.get());
        llvm::AllocaInst* slot = create_alloca(pty, pname);
        builder_.CreateStore(&arg, slot);
        env_->define(pname, slot);
    }

    for (auto& stmt : s.body)
        emit_stmt(*stmt);

    if (!builder_.GetInsertBlock()->getTerminator())
        builder_.CreateBr(exit_bb);

    builder_.SetInsertPoint(exit_bb);
    pop_defer_scope();

    if (cur_ret_slot_) {
        llvm::Value* ret = builder_.CreateLoad(ret_ty, cur_ret_slot_);
        builder_.CreateRet(ret);
    } else {
        builder_.CreateRetVoid();
    }

    pop_env();

    cur_func_     = saved_func;
    cur_ret_bb_   = saved_ret_bb;
    cur_ret_slot_ = saved_ret_slot;

    if (saved_func)
        builder_.SetInsertPoint(&saved_func->back());
}

void IREmitter::emit_if_stmt(IfStmt& s) {
    llvm::Value* cond = emit_expr(*s.cond);
    if (!cond) return;

    if (!cond->getType()->isIntegerTy(1)) {
        if (cond->getType()->isIntegerTy())
            cond = builder_.CreateICmpNE(
                cond, llvm::Constant::getNullValue(cond->getType()));
        else if (cond->getType()->isPointerTy())
            cond = builder_.CreateIsNotNull(cond);
        else
            cond = builder_.CreateFCmpUNE(
                cond, llvm::ConstantFP::get(cond->getType(), 0.0));
    }

    auto* then_bb = llvm::BasicBlock::Create(ctx_, "if.then", cur_func_);
    auto* else_bb = llvm::BasicBlock::Create(ctx_, "if.else", cur_func_);
    auto* end_bb  = llvm::BasicBlock::Create(ctx_, "if.end",  cur_func_);

    builder_.CreateCondBr(cond, then_bb, else_bb);

    builder_.SetInsertPoint(then_bb);
    push_env(); push_defer_scope();
    for (auto& st : s.then_body) emit_stmt(*st);
    pop_defer_scope(); pop_env();
    if (!builder_.GetInsertBlock()->getTerminator())
        builder_.CreateBr(end_bb);

    builder_.SetInsertPoint(else_bb);
    for (auto& [ei_cond, ei_body] : s.elseif_clauses) {
        llvm::Value* eic = emit_expr(*ei_cond);
        if (eic && !eic->getType()->isIntegerTy(1)) {
            if (eic->getType()->isIntegerTy())
                eic = builder_.CreateICmpNE(
                    eic, llvm::Constant::getNullValue(eic->getType()));
        }
        auto* ei_then = llvm::BasicBlock::Create(ctx_, "elif.then", cur_func_);
        auto* ei_else = llvm::BasicBlock::Create(ctx_, "elif.else", cur_func_);
        builder_.CreateCondBr(eic, ei_then, ei_else);
        builder_.SetInsertPoint(ei_then);
        push_env(); push_defer_scope();
        for (auto& st : ei_body) emit_stmt(*st);
        pop_defer_scope(); pop_env();
        if (!builder_.GetInsertBlock()->getTerminator())
            builder_.CreateBr(end_bb);
        builder_.SetInsertPoint(ei_else);
    }

    if (s.else_body) {
        push_env(); push_defer_scope();
        for (auto& st : *s.else_body) emit_stmt(*st);
        pop_defer_scope(); pop_env();
    }
    if (!builder_.GetInsertBlock()->getTerminator())
        builder_.CreateBr(end_bb);

    builder_.SetInsertPoint(end_bb);
}

void IREmitter::emit_while_stmt(WhileStmt& s) {
    auto* cond_bb = llvm::BasicBlock::Create(ctx_, "while.cond", cur_func_);
    auto* body_bb = llvm::BasicBlock::Create(ctx_, "while.body", cur_func_);
    auto* end_bb  = llvm::BasicBlock::Create(ctx_, "while.end",  cur_func_);

    builder_.CreateBr(cond_bb);
    builder_.SetInsertPoint(cond_bb);

    llvm::Value* cond = emit_expr(*s.cond);
    if (!cond) { builder_.CreateBr(end_bb); return; }
    if (!cond->getType()->isIntegerTy(1))
        cond = builder_.CreateICmpNE(
            cond, llvm::Constant::getNullValue(cond->getType()));
    builder_.CreateCondBr(cond, body_bb, end_bb);

    builder_.SetInsertPoint(body_bb);
    push_env(); push_defer_scope();
    for (auto& st : s.body) emit_stmt(*st);
    pop_defer_scope(); pop_env();
    if (!builder_.GetInsertBlock()->getTerminator())
        builder_.CreateBr(cond_bb);

    builder_.SetInsertPoint(end_bb);
}

void IREmitter::emit_repeat_stmt(RepeatStmt& s) {
    auto* body_bb = llvm::BasicBlock::Create(ctx_, "repeat.body", cur_func_);
    auto* end_bb  = llvm::BasicBlock::Create(ctx_, "repeat.end",  cur_func_);

    builder_.CreateBr(body_bb);
    builder_.SetInsertPoint(body_bb);

    push_env(); push_defer_scope();
    for (auto& st : s.body) emit_stmt(*st);

    llvm::Value* cond = emit_expr(*s.until_cond);
    pop_defer_scope(); pop_env();

    if (!builder_.GetInsertBlock()->getTerminator()) {
        if (cond && !cond->getType()->isIntegerTy(1))
            cond = builder_.CreateICmpNE(
                cond, llvm::Constant::getNullValue(cond->getType()));
        builder_.CreateCondBr(cond ? cond :
            llvm::ConstantInt::getFalse(ctx_), end_bb, body_bb);
    }
    builder_.SetInsertPoint(end_bb);
}

void IREmitter::emit_numeric_for(NumericFor& s) {

    auto* i64 = llvm::Type::getInt64Ty(ctx_);

    llvm::Value* start = emit_expr(*s.start);
    llvm::Value* stop  = emit_expr(*s.stop);
    llvm::Value* step  = s.step
        ? emit_expr(*s.step)
        : llvm::ConstantInt::get(i64, 1);

    start = coerce(start, i64, {});
    stop  = coerce(stop,  i64, {});
    step  = coerce(step,  i64, {});

    auto* loop_bb = llvm::BasicBlock::Create(ctx_, "for.loop", cur_func_);
    auto* body_bb = llvm::BasicBlock::Create(ctx_, "for.body", cur_func_);
    auto* end_bb  = llvm::BasicBlock::Create(ctx_, "for.end",  cur_func_);

    llvm::AllocaInst* i_slot = create_alloca(i64, s.var);
    builder_.CreateStore(start, i_slot);
    builder_.CreateBr(loop_bb);

    builder_.SetInsertPoint(loop_bb);
    llvm::Value* i_val = builder_.CreateLoad(i64, i_slot);

    llvm::Value* cond = builder_.CreateICmpSLE(i_val, stop, "for.cond");
    builder_.CreateCondBr(cond, body_bb, end_bb);

    builder_.SetInsertPoint(body_bb);
    push_env(); push_defer_scope();
    env_->define(s.var, i_slot);
    for (auto& st : s.body) emit_stmt(*st);
    pop_defer_scope(); pop_env();

    if (!builder_.GetInsertBlock()->getTerminator()) {

        llvm::Value* i_cur  = builder_.CreateLoad(i64, i_slot);
        llvm::Value* i_next = builder_.CreateAdd(i_cur, step);
        builder_.CreateStore(i_next, i_slot);
        builder_.CreateBr(loop_bb);
    }

    builder_.SetInsertPoint(end_bb);
}

void IREmitter::emit_return_stmt(ReturnStmt& s, SourceLoc loc) {
    if (!s.values.empty() && cur_ret_slot_) {
        llvm::Value* val = emit_expr(*s.values[0]);
        if (val) {
            llvm::Type* ret_ty = cur_ret_slot_->getAllocatedType();
            val = coerce(val, ret_ty, loc);
            builder_.CreateStore(val, cur_ret_slot_);
        }
    }

    for (int i = (int)defer_stack_.size() - 1; i >= 0; i--) {
        auto& scope = defer_stack_[i];
        for (int j = (int)scope.size() - 1; j >= 0; j--)
            scope[j].emit_fn();
    }
    builder_.CreateBr(cur_ret_bb_);
}

void IREmitter::emit_assign(Assign& s, SourceLoc loc) {
    if (auto* idx = std::get_if<Index>(&s.target->v)) {
        llvm::Value* tbl = emit_expr(*idx->table);
        llvm::Value* key = emit_expr(*idx->key);
        llvm::Value* val = emit_expr(*s.value);
        if (!tbl || !key || !val) return;

        auto* i64 = llvm::Type::getInt64Ty(ctx_);
        auto* i32 = llvm::Type::getInt32Ty(ctx_);

        bool key_is_str = key->getType()->isPointerTy();
        if (!key_is_str) key = builder_.CreateSExt(key, i64);
        std::string pfx = key_is_str ? "slua_tbl_sset" : "slua_tbl_iset";

        llvm::Type* vty = val->getType();
        if (vty->isDoubleTy()) {
            auto* fn = get_runtime_fn(pfx + "_f64");
            if (fn) builder_.CreateCall(fn, {tbl, key, val});
        } else if (vty->isIntegerTy(1)) {
            auto* fn = get_runtime_fn(pfx + "_bool");
            auto* ext = builder_.CreateZExt(val, i32);
            if (fn) builder_.CreateCall(fn, {tbl, key, ext});
        } else if (vty->isPointerTy()) {
            auto* fn = get_runtime_fn(pfx + "_str");
            if (fn) builder_.CreateCall(fn, {tbl, key, val});
        } else {
            auto* fn = get_runtime_fn(pfx + "_i64");
            auto* ext = builder_.CreateSExt(val, i64);
            if (fn) builder_.CreateCall(fn, {tbl, key, ext});
        }
        return;
    }

    llvm::Value* rhs = emit_expr(*s.value);
    llvm::Value* lhs = emit_lvalue(*s.target);
    if (!rhs || !lhs) return;

    if (auto* pt = llvm::dyn_cast<llvm::AllocaInst>(lhs))
            rhs = coerce(rhs, pt->getAllocatedType(), loc);
    else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(lhs))
        rhs = coerce(rhs, gv->getValueType(), loc);

    builder_.CreateStore(rhs, lhs);
}

void IREmitter::emit_call_stmt(CallStmt& s) {
    emit_expr(*s.call);
}

void IREmitter::emit_defer_stmt(DeferStmt& s) {

    Stmt* raw = s.action.get();
    add_defer([this, raw]() {
        if (raw) emit_stmt(*raw);
    });
}

void IREmitter::emit_do_block(DoBlock& s) {
    push_env(); push_defer_scope();
    for (auto& st : s.body) emit_stmt(*st);
    pop_defer_scope(); pop_env();
}

void IREmitter::emit_store_stmt(StoreStmt& s) {
    llvm::Value* ptr = emit_expr(*s.ptr);
    llvm::Value* val = emit_expr(*s.val);
    if (!ptr || !val) return;

    if (!ptr->getType()->isPointerTy()) return;

    llvm::Type* pointee = llvm::Type::getInt8Ty(ctx_);
    if (auto* pt = llvm::dyn_cast<llvm::PointerType>(ptr->getType()))
        (void)pt;

    val = coerce(val, llvm::Type::getInt8Ty(ctx_), {});
    builder_.CreateStore(val, ptr);
}

void IREmitter::emit_free_stmt(FreeStmt& s) {
    llvm::Value* ptr = emit_expr(*s.ptr);
    if (!ptr) return;

    auto* i8p = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
    if (!ptr->getType()->isPointerTy())
        ptr = builder_.CreateIntToPtr(ptr, i8p);
    else
        ptr = builder_.CreateBitCast(ptr, i8p);

    auto* free_fn = get_runtime_fn("slua_free");
    if (free_fn) builder_.CreateCall(free_fn, {ptr});
}

void IREmitter::emit_panic_stmt(PanicStmt& s) {

    llvm::Value* msg = emit_expr(*s.msg);
    auto* fn = get_runtime_fn("slua_panic");
    if (!fn || !msg) return;

    auto* i8p = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
    if (!msg->getType()->isPointerTy())
        msg = builder_.CreateGlobalStringPtr("<panic>");
    auto* file_val = builder_.CreateGlobalStringPtr("?");
    auto* line_val = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(ctx_), 0);
    builder_.CreateCall(fn, {msg, file_val, line_val});
    builder_.CreateUnreachable();
}

void IREmitter::emit_type_decl(TypeDecl& s, SourceLoc loc) {

    if (auto* rt = std::get_if<RecordType>(&s.def->v)) {
        std::vector<llvm::Type*> fields;
        for (auto& [fn, ft] : rt->fields)
            fields.push_back(llvm_type(ft.get()));

        auto* st = llvm::StructType::create(ctx_, s.name);
        st->setBody(fields);
        struct_types_[s.name] = st;
    }
}

void IREmitter::emit_extern_decl(ExternDecl& s, SourceLoc loc) {
    if (!s.func_type) return;
    auto* ft_node = std::get_if<FuncType>(&s.func_type->v);
    if (!ft_node) return;

    std::vector<llvm::Type*> params;
    for (auto& p : ft_node->params)
        params.push_back(llvm_type(p.get()));
    llvm::Type* ret = ft_node->ret
        ? llvm_type(ft_node->ret.get())
        : llvm::Type::getVoidTy(ctx_);

    auto* fty = llvm::FunctionType::get(ret, params, false);
    auto* fn  = llvm::Function::Create(
        fty, llvm::Function::ExternalLinkage, s.name, *mod_);
    functions_[s.name] = fn;
}

llvm::Value* IREmitter::emit_expr(Expr& e) {
    return std::visit([&](auto&& v) -> llvm::Value* {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, NullLit>)
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);

        else if constexpr (std::is_same_v<T, BoolLit>)
            return llvm::ConstantInt::get(
                llvm::Type::getInt1Ty(ctx_), v.val ? 1 : 0);

        else if constexpr (std::is_same_v<T, IntLit>)
            return llvm::ConstantInt::get(
                llvm::Type::getInt64Ty(ctx_), (uint64_t)v.val, true);

        else if constexpr (std::is_same_v<T, FloatLit>)
            return llvm::ConstantFP::get(
                llvm::Type::getDoubleTy(ctx_), v.val);

        else if constexpr (std::is_same_v<T, StrLit>) {
            return builder_.CreateGlobalStringPtr(v.val);
        }

        else if constexpr (std::is_same_v<T, Ident>) {
            llvm::Value* slot = env_ ? env_->lookup(v.name) : nullptr;
            if (!slot) {

                auto fit = functions_.find(v.name);
                if (fit != functions_.end()) return fit->second;
                return llvm::ConstantInt::get(
                    llvm::Type::getInt64Ty(ctx_), 0);
            }

            llvm::Type* ty = nullptr;
            if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(slot))
                ty = ai->getAllocatedType();
            else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(slot))
                ty = gv->getValueType();
            if (ty) return builder_.CreateLoad(ty, slot, v.name);
            return slot;
        }

        else if constexpr (std::is_same_v<T, Binop>)
            return emit_binop(v, e.loc);
        else if constexpr (std::is_same_v<T, Unop>)
            return emit_unop(v, e.loc);
        else if constexpr (std::is_same_v<T, Call>)
            return emit_call_expr(v, e.loc);
        else if constexpr (std::is_same_v<T, MethodCall>)
            return emit_method_call(v, e.loc);
        else if constexpr (std::is_same_v<T, Field>)
            return emit_field(v, e.loc);
        else if constexpr (std::is_same_v<T, Index>)
            return emit_index(v, e.loc);
        else if constexpr (std::is_same_v<T, TableCtor>)
            return emit_table_ctor(v, e.loc);
        else if constexpr (std::is_same_v<T, AllocExpr>)
            return emit_alloc_expr(v, e.loc);
        else if constexpr (std::is_same_v<T, DerefExpr>)
            return emit_deref_expr(v, e.loc);
        else if constexpr (std::is_same_v<T, AddrExpr>)
            return emit_addr_expr(v, e.loc);
        else if constexpr (std::is_same_v<T, CastExpr>)
            return emit_cast_expr(v, e.loc);

        else if constexpr (std::is_same_v<T, SizeofExpr>) {
            llvm::Type* ty = llvm_type(v.type.get());
            auto* dl = &mod_->getDataLayout();
            uint64_t sz = dl->getTypeAllocSize(ty);
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), sz);
        }

        else if constexpr (std::is_same_v<T, TypeofExpr>) {
            emit_expr(*v.expr);
            return builder_.CreateGlobalStringPtr("any");
        }

        else if constexpr (std::is_same_v<T, FuncExpr>) {

            static int anon_id = 0;
            std::string name = "__anon_" + std::to_string(anon_id++);

            std::vector<llvm::Type*> param_tys;
            for (auto& [pn, pt] : v.params)
                param_tys.push_back(llvm_type(pt.get()));
            llvm::Type* ret_ty = v.ret_type
                ? llvm_type(v.ret_type.get())
                : llvm::Type::getVoidTy(ctx_);

            auto* fty = llvm::FunctionType::get(ret_ty, param_tys, false);
            auto* fn  = llvm::Function::Create(
                fty, llvm::Function::InternalLinkage, name, *mod_);
            functions_[name] = fn;

            auto* saved_func     = cur_func_;
            auto* saved_ret_bb   = cur_ret_bb_;
            auto* saved_ret_slot = cur_ret_slot_;

            auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", fn);
            auto* exit_bb  = llvm::BasicBlock::Create(ctx_, "exit",  fn);
            builder_.SetInsertPoint(entry_bb);
            cur_func_   = fn;
            cur_ret_bb_ = exit_bb;

            if (!ret_ty->isVoidTy()) {
                cur_ret_slot_ = create_alloca(ret_ty, "retval");
                builder_.CreateStore(
                    llvm::Constant::getNullValue(ret_ty), cur_ret_slot_);
            } else cur_ret_slot_ = nullptr;

            push_env(); push_defer_scope();
            size_t idx = 0;
            for (auto& arg : fn->args()) {
                auto& [pn, pt] = v.params[idx++];
                auto* sl = create_alloca(arg.getType(), pn);
                builder_.CreateStore(&arg, sl);
                env_->define(pn, sl);
            }
            for (auto& st : v.body) emit_stmt(*st);
            pop_defer_scope(); pop_env();

            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(exit_bb);
            builder_.SetInsertPoint(exit_bb);
            if (cur_ret_slot_) {
                auto* rv = builder_.CreateLoad(ret_ty, cur_ret_slot_);
                builder_.CreateRet(rv);
            } else builder_.CreateRetVoid();

            cur_func_     = saved_func;
            cur_ret_bb_   = saved_ret_bb;
            cur_ret_slot_ = saved_ret_slot;
            if (saved_func)
                builder_.SetInsertPoint(&saved_func->back());

            return fn;
        }

        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
    }, e.v);
}

llvm::Value* IREmitter::emit_binop(Binop& e, SourceLoc loc) {

    if (e.op == "and" || e.op == "or") {
        llvm::Value* lhs = emit_expr(*e.lhs);
        if (!lhs) return nullptr;
        llvm::Value* cond;
        if (lhs->getType()->isIntegerTy(1))
            cond = lhs;
        else if (lhs->getType()->isIntegerTy())
            cond = builder_.CreateICmpNE(
                lhs, llvm::Constant::getNullValue(lhs->getType()));
        else
            cond = builder_.CreateIsNotNull(lhs);

        auto* rhs_bb  = llvm::BasicBlock::Create(ctx_, "logic.rhs", cur_func_);
        auto* end_bb  = llvm::BasicBlock::Create(ctx_, "logic.end", cur_func_);
        auto* cur_bb  = builder_.GetInsertBlock();

        if (e.op == "and")
            builder_.CreateCondBr(cond, rhs_bb, end_bb);
        else
            builder_.CreateCondBr(cond, end_bb, rhs_bb);

        builder_.SetInsertPoint(rhs_bb);
        llvm::Value* rhs = emit_expr(*e.rhs);
        auto* rhs_end_bb = builder_.GetInsertBlock();
        builder_.CreateBr(end_bb);

        builder_.SetInsertPoint(end_bb);
        if (lhs->getType() == (rhs ? rhs->getType() : lhs->getType())) {
            auto* phi = builder_.CreatePHI(lhs->getType(), 2);
            phi->addIncoming(lhs, cur_bb);
            if (rhs) phi->addIncoming(rhs, rhs_end_bb);
            return phi;
        }
        return lhs;
    }

    llvm::Value* lhs = emit_expr(*e.lhs);
    llvm::Value* rhs = emit_expr(*e.rhs);
    if (!lhs || !rhs) return nullptr;

    bool is_float = lhs->getType()->isDoubleTy() ||
                    rhs->getType()->isDoubleTy();

    if (is_float && !lhs->getType()->isPointerTy() && !rhs->getType()->isPointerTy()) {
        if (!lhs->getType()->isDoubleTy())
            lhs = builder_.CreateSIToFP(lhs, llvm::Type::getDoubleTy(ctx_));
        if (!rhs->getType()->isDoubleTy())
            rhs = builder_.CreateSIToFP(rhs, llvm::Type::getDoubleTy(ctx_));
    } else {

        if (lhs->getType() != rhs->getType()) {
            auto* i64 = llvm::Type::getInt64Ty(ctx_);
            if (lhs->getType()->isIntegerTy())
                lhs = builder_.CreateSExt(lhs, i64);
            if (rhs->getType()->isIntegerTy())
                rhs = builder_.CreateSExt(rhs, i64);
        }
    }

    if (e.op == "+") return is_float ? builder_.CreateFAdd(lhs, rhs)
                                      : builder_.CreateAdd(lhs, rhs);
    if (e.op == "-") return is_float ? builder_.CreateFSub(lhs, rhs)
                                      : builder_.CreateSub(lhs, rhs);
    if (e.op == "*") return is_float ? builder_.CreateFMul(lhs, rhs)
                                      : builder_.CreateMul(lhs, rhs);
    if (e.op == "/") {
        if (!is_float) {
            lhs = builder_.CreateSIToFP(lhs, llvm::Type::getDoubleTy(ctx_));
            rhs = builder_.CreateSIToFP(rhs, llvm::Type::getDoubleTy(ctx_));
        }
        return builder_.CreateFDiv(lhs, rhs);
    }
    if (e.op == "%") return is_float ? builder_.CreateFRem(lhs, rhs)
                                      : builder_.CreateSRem(lhs, rhs);

    if (e.op == "==" || e.op == "~=") {
        llvm::Value* cmp = is_float
            ? builder_.CreateFCmpOEQ(lhs, rhs)
            : builder_.CreateICmpEQ(lhs, rhs);
        return (e.op == "~=") ? builder_.CreateNot(cmp) : cmp;
    }
    if (e.op == "<")  return is_float ? builder_.CreateFCmpOLT(lhs, rhs)
                                       : builder_.CreateICmpSLT(lhs, rhs);
    if (e.op == ">")  return is_float ? builder_.CreateFCmpOGT(lhs, rhs)
                                       : builder_.CreateICmpSGT(lhs, rhs);
    if (e.op == "<=") return is_float ? builder_.CreateFCmpOLE(lhs, rhs)
                                       : builder_.CreateICmpSLE(lhs, rhs);
    if (e.op == ">=") return is_float ? builder_.CreateFCmpOGE(lhs, rhs)
                                       : builder_.CreateICmpSGE(lhs, rhs);

    if (e.op == "&")  return builder_.CreateAnd(lhs, rhs);
    if (e.op == "|")  return builder_.CreateOr(lhs, rhs);
    if (e.op == "~")  return builder_.CreateXor(lhs, rhs);
    if (e.op == "<<") return builder_.CreateShl(lhs, rhs);
    if (e.op == ">>") return builder_.CreateAShr(lhs, rhs);

    if (e.op == "..") {
        auto* fn = get_runtime_fn("slua_str_concat");
        if (!fn) return lhs;
        auto to_str = [&](llvm::Value* v) -> llvm::Value* {
            if (!v) return llvm::ConstantPointerNull::get(
                llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_)));
            if (v->getType()->isPointerTy()) return v;
            if (v->getType()->isDoubleTy()) {
                auto* cvt = get_runtime_fn("slua_float_to_str");
                if (cvt) return builder_.CreateCall(cvt, {v}, "fstr");
            }
            if (v->getType()->isIntegerTy()) {
                auto* i64t = llvm::Type::getInt64Ty(ctx_);
                auto* ext = builder_.CreateSExt(v, i64t);
                auto* cvt = get_runtime_fn("slua_int_to_str");
                if (cvt) return builder_.CreateCall(cvt, {ext}, "istr");
            }
            return v;
        };
        return builder_.CreateCall(fn, {to_str(lhs), to_str(rhs)}, "concat");
    }

    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
}

llvm::Value* IREmitter::emit_unop(Unop& e, SourceLoc loc) {
    llvm::Value* val = emit_expr(*e.operand);
    if (!val) return nullptr;

    if (e.op == "-") {
        if (val->getType()->isDoubleTy())
            return builder_.CreateFNeg(val);
        return builder_.CreateNeg(val);
    }
    if (e.op == "not") {
        if (!val->getType()->isIntegerTy(1))
            val = builder_.CreateICmpEQ(
                val, llvm::Constant::getNullValue(val->getType()));
        return builder_.CreateNot(val);
    }
    if (e.op == "#") {

        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
    }
    return val;
}

llvm::Value* IREmitter::emit_call_expr(Call& e, SourceLoc loc) {
    if (auto* fi = std::get_if<Field>(&e.callee->v)) {
        if (auto* mod_id = std::get_if<Ident>(&fi->table->v)) {
            const std::string& mod  = mod_id->name;
            const std::string& meth = fi->name;

            if (mod == "io") {
                if (meth == "read_line") {
                    auto* fn = get_runtime_fn("slua_read_line");
                    if (fn) return builder_.CreateCall(fn, {}, "rl");
                    return llvm::ConstantPointerNull::get(
                        llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_)));
                }
                if (meth == "read_char") {
                    auto* fn = get_runtime_fn("slua_read_char");
                    if (fn) return builder_.CreateCall(fn, {}, "rc");
                    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
                }
                if (meth == "clear") {
                    auto* fn = get_runtime_fn("slua_io_clear");
                    if (fn) builder_.CreateCall(fn, {});
                    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
                }
                if (meth == "set_color" && !e.args.empty()) {
                    auto* fn  = get_runtime_fn("slua_io_set_color");
                    auto* arg = emit_expr(*e.args[0]);
                    if (fn && arg) builder_.CreateCall(fn, {arg});
                    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
                }
                if (meth == "reset_color") {
                    auto* fn = get_runtime_fn("slua_io_reset_color");
                    if (fn) builder_.CreateCall(fn, {});
                    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
                }
                if (meth == "print_color" && e.args.size() >= 2) {
                    auto* fn  = get_runtime_fn("slua_io_print_color");
                    auto* msg = emit_expr(*e.args[0]);
                    auto* col = emit_expr(*e.args[1]);
                    if (fn && msg && col) builder_.CreateCall(fn, {msg, col});
                    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
                }
                if (meth == "print" && !e.args.empty()) {
                    auto* arg = emit_expr(*e.args[0]);
                    if (arg && arg->getType()->isPointerTy()) {
                        auto* fn = get_runtime_fn("slua_print_str");
                        if (fn) builder_.CreateCall(fn, {arg});
                    }
                    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
                }
                if (meth == "flush") {
                    auto* fn = get_runtime_fn("slua_flush");
                    if (fn) builder_.CreateCall(fn, {});
                    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
                }
                return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
            }

            if (mod == "math") {
                auto* f64 = llvm::Type::getDoubleTy(ctx_);
                auto coerce_f64 = [&](llvm::Value* v) -> llvm::Value* {
                    if (!v) return llvm::ConstantFP::get(f64, 0.0);
                    if (v->getType()->isDoubleTy()) return v;
                    if (v->getType()->isIntegerTy()) return builder_.CreateSIToFP(v, f64);
                    return llvm::ConstantFP::get(f64, 0.0);
                };
                auto call1 = [&](const std::string& name) -> llvm::Value* {
                    if (e.args.empty()) return llvm::ConstantFP::get(f64, 0.0);
                    auto* fn = get_runtime_fn(name);
                    if (!fn) return llvm::ConstantFP::get(f64, 0.0);
                    return builder_.CreateCall(fn, {coerce_f64(emit_expr(*e.args[0]))}, name);
                };
                if (meth == "sqrt")  return call1("slua_sqrt");
                if (meth == "sin")   return call1("slua_sin");
                if (meth == "cos")   return call1("slua_cos");
                if (meth == "tan")   return call1("slua_tan");
                if (meth == "log")   return call1("slua_log");
                if (meth == "log2")  return call1("slua_log2");
                if (meth == "exp")   return call1("slua_exp");
                if (meth == "pow" && e.args.size() >= 2) {
                    auto* fn = get_runtime_fn("slua_pow");
                    if (!fn) return llvm::ConstantFP::get(f64, 0.0);
                    auto* a = coerce_f64(emit_expr(*e.args[0]));
                    auto* b = coerce_f64(emit_expr(*e.args[1]));
                    return builder_.CreateCall(fn, {a, b}, "pow");
                }
                if (meth == "inf") {
                    auto* fn = get_runtime_fn("slua_inf");
                    if (fn) return builder_.CreateCall(fn, {}, "inf");
                }
                if (meth == "nan") {
                    auto* fn = get_runtime_fn("slua_nan");
                    if (fn) return builder_.CreateCall(fn, {}, "nan");
                }
                return llvm::ConstantFP::get(f64, 0.0);
            }
        }
    }

    if (auto* id = std::get_if<Ident>(&e.callee->v)) {
        if (id->name == "print" && !e.args.empty()) {
            llvm::Value* arg = emit_expr(*e.args[0]);
            if (!arg) return nullptr;

            llvm::Type* ty = arg->getType();
            if (ty->isDoubleTy()) {
                auto* fn = get_runtime_fn("slua_print_float");
                if (fn) builder_.CreateCall(fn, {arg});
            } else if (ty->isIntegerTy(1)) {
                auto* fn = get_runtime_fn("slua_print_bool");
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                if (fn) builder_.CreateCall(fn,
                    {builder_.CreateZExt(arg, i32)});
            } else if (ty->isIntegerTy()) {
                auto* fn = get_runtime_fn("slua_print_int");
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                if (fn) builder_.CreateCall(fn,
                    {builder_.CreateSExt(arg, i64)});
            } else if (ty->isPointerTy()) {
                auto* fn = get_runtime_fn("slua_print_str");
                if (fn) builder_.CreateCall(fn, {arg});
            }
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
        }
    }

    llvm::Value* callee = emit_expr(*e.callee);
    if (!callee) return nullptr;

    llvm::Function* fn = llvm::dyn_cast<llvm::Function>(callee);
    if (!fn) return nullptr;

    std::vector<llvm::Value*> args;
    auto param_tys = fn->getFunctionType()->params().vec();
    for (size_t i = 0; i < e.args.size(); i++) {
        llvm::Value* arg = emit_expr(*e.args[i]);
        if (!arg) return nullptr;
        if (i < param_tys.size())
            arg = coerce(arg, param_tys[i], loc);
        args.push_back(arg);
    }

    llvm::Value* result = builder_.CreateCall(fn, args);
    return result;
}

llvm::Value* IREmitter::emit_method_call(MethodCall& e, SourceLoc loc) {

    emit_expr(*e.obj);
    for (auto& arg : e.args) emit_expr(*arg);
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
}

llvm::Value* IREmitter::emit_field(Field& e, SourceLoc loc) {
    llvm::Value* obj = emit_lvalue(*e.table);
    if (!obj) return nullptr;

    llvm::Type* struct_ty = nullptr;
    if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(obj))
        struct_ty = ai->getAllocatedType();
    else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(obj))
        struct_ty = gv->getValueType();

    if (!struct_ty || !struct_ty->isStructTy()) {

        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
    }

    auto* gep = builder_.CreateStructGEP(struct_ty, obj, 0, e.name);
    llvm::Type* field_ty = llvm::cast<llvm::StructType>(struct_ty)
        ->getElementType(0);
    return builder_.CreateLoad(field_ty, gep, e.name);
}

llvm::Value* IREmitter::emit_index(Index& e, SourceLoc loc) {
    llvm::Value* base = emit_expr(*e.table);
    llvm::Value* key  = emit_expr(*e.key);
    if (!base || !key) return nullptr;

    auto* i64 = llvm::Type::getInt64Ty(ctx_);
    auto* i8p = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));

    std::string inferred;
    if (e.table->inferred_type) {
        if (auto* p = std::get_if<PrimitiveType>(&e.table->inferred_type->v))
            inferred = p->name;
    }

    bool key_is_str = key->getType()->isPointerTy();
    std::string pfx = key_is_str ? "slua_tbl_sget" : "slua_tbl_iget";

    if (!key_is_str)
        key = builder_.CreateSExt(key, i64);

    std::string suffix = "_i64";
    if (inferred == "number") suffix = "_f64";
    else if (inferred == "string") suffix = "_str";
    else if (inferred == "bool") suffix = "_bool";

    auto* fn = get_runtime_fn(pfx + suffix);
    if (!fn) return llvm::ConstantInt::get(i64, 0);
    return builder_.CreateCall(fn, {base, key}, "tget");
}

llvm::Value* IREmitter::emit_table_ctor(TableCtor& e, SourceLoc loc) {
    auto* i8p = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
    auto* i64 = llvm::Type::getInt64Ty(ctx_);
    auto* i32 = llvm::Type::getInt32Ty(ctx_);
    auto* f64 = llvm::Type::getDoubleTy(ctx_);

    auto* new_fn = get_runtime_fn("slua_tbl_new");
    if (!new_fn) return llvm::ConstantPointerNull::get(i8p);
    llvm::Value* tbl = builder_.CreateCall(new_fn, {}, "tbl");

    int64_t auto_idx = 1;
    for (auto& entry : e.entries) {
        llvm::Value* val = emit_expr(*entry.val);
        if (!val) { auto_idx++; continue; }

        llvm::Value* key_val = nullptr;
        bool key_is_str = false;

        if (entry.key) {
            key_val = emit_expr(**entry.key);
            if (key_val && key_val->getType()->isPointerTy())
                key_is_str = true;
        } else {
            key_val = llvm::ConstantInt::get(i64, auto_idx++);
        }

        if (!key_val) continue;

        auto emit_set = [&](const std::string& pfx) {
            llvm::Type* vty = val->getType();
            if (vty->isDoubleTy()) {
                auto* fn = get_runtime_fn(pfx + "_f64");
                if (fn) builder_.CreateCall(fn, {tbl, key_val, val});
            } else if (vty->isIntegerTy(1)) {
                auto* fn = get_runtime_fn(pfx + "_bool");
                auto* ext = builder_.CreateZExt(val, i32);
                if (fn) builder_.CreateCall(fn, {tbl, key_val, ext});
            } else if (vty->isPointerTy()) {
                auto* fn = get_runtime_fn(pfx + "_str");
                if (fn) builder_.CreateCall(fn, {tbl, key_val, val});
            } else {
                auto* fn = get_runtime_fn(pfx + "_i64");
                auto* ext = builder_.CreateSExt(val, i64);
                if (fn) builder_.CreateCall(fn, {tbl, key_val, ext});
            }
        };

        if (key_is_str) emit_set("slua_tbl_sset");
        else            emit_set("slua_tbl_iset");
    }
    return tbl;
}

llvm::Value* IREmitter::emit_alloc_expr(AllocExpr& e, SourceLoc loc) {
    llvm::Value* count = emit_expr(*e.count);
    if (!count) return nullptr;

    llvm::Type* elem_ty = llvm_type(e.elem_type.get());
    auto* dl = &mod_->getDataLayout();
    uint64_t elem_sz = dl->getTypeAllocSize(elem_ty);

    auto* i64 = llvm::Type::getInt64Ty(ctx_);
    count = coerce(count, i64, loc);
    auto* sz = builder_.CreateMul(
        count, llvm::ConstantInt::get(i64, elem_sz), "alloc.sz");

    auto* fn = get_runtime_fn("slua_alloc");
    if (!fn) return nullptr;
    llvm::Value* raw = builder_.CreateCall(fn, {sz}, "raw_ptr");

    auto* dst_ty = llvm::PointerType::getUnqual(elem_ty);
    return builder_.CreateBitCast(raw, dst_ty, "typed_ptr");
}

llvm::Value* IREmitter::emit_deref_expr(DerefExpr& e, SourceLoc loc) {
    llvm::Value* ptr = emit_expr(*e.ptr);
    if (!ptr || !ptr->getType()->isPointerTy()) return nullptr;
    auto* elem_ty = llvm::Type::getInt8Ty(ctx_);
    return builder_.CreateLoad(elem_ty, ptr, "deref");
}

llvm::Value* IREmitter::emit_addr_expr(AddrExpr& e, SourceLoc loc) {
    return emit_lvalue(*e.target);
}

llvm::Value* IREmitter::emit_cast_expr(CastExpr& e, SourceLoc loc) {
    llvm::Value* val = emit_expr(*e.expr);
    llvm::Type*  dst = llvm_type(e.to.get());
    if (!val || !dst) return val;
    return coerce(val, dst, loc);
}

llvm::Value* IREmitter::emit_lvalue(Expr& e) {
    if (auto* id = std::get_if<Ident>(&e.v)) {
        return env_ ? env_->lookup(id->name) : nullptr;
    }
    if (auto* fi = std::get_if<Field>(&e.v)) {
        llvm::Value* obj = emit_lvalue(*fi->table);
        if (!obj) return nullptr;
        llvm::Type* sty = nullptr;
        if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(obj))
            sty = ai->getAllocatedType();
        if (!sty || !sty->isStructTy()) return obj;
        return builder_.CreateStructGEP(sty, obj, 0, fi->name);
    }
    if (auto* idx = std::get_if<Index>(&e.v)) {
        llvm::Value* base = emit_expr(*idx->table);
        llvm::Value* key  = emit_expr(*idx->key);
        if (!base || !key || !base->getType()->isPointerTy()) return nullptr;
        key = coerce(key, llvm::Type::getInt64Ty(ctx_), {});
        return builder_.CreateGEP(
            llvm::Type::getInt8Ty(ctx_), base, key);
    }
    if (auto* dr = std::get_if<DerefExpr>(&e.v))
        return emit_expr(*dr->ptr);

    return emit_expr(e);
}

llvm::Value* IREmitter::coerce(llvm::Value* v, llvm::Type* to,
                                SourceLoc ) {
    if (!v || !to || v->getType() == to) return v;

    llvm::Type* from = v->getType();

    if (from->isIntegerTy() && to->isDoubleTy())
        return builder_.CreateSIToFP(v, to);

    if (from->isDoubleTy() && to->isIntegerTy())
        return builder_.CreateFPToSI(v, to);

    if (from->isIntegerTy() && to->isIntegerTy()) {
        unsigned fb = from->getIntegerBitWidth();
        unsigned tb = to->getIntegerBitWidth();
        if (fb < tb) return builder_.CreateSExt(v, to);
        if (fb > tb) return builder_.CreateTrunc(v, to);
        return v;
    }

    if (from->isPointerTy() && to->isPointerTy())
        return builder_.CreateBitCast(v, to);

    if (from->isIntegerTy() && to->isPointerTy())
        return builder_.CreateIntToPtr(v, to);

    if (from->isPointerTy() && to->isIntegerTy())
        return builder_.CreatePtrToInt(v, to);

    if (from->isIntegerTy(1) && to->isIntegerTy())
        return builder_.CreateZExt(v, to);

    return v;
}

}

#endif