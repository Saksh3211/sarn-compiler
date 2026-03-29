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

IREmitter::IREmitter(DiagEngine& diag, SemanticConfig cfg,const std::string& module_name)
    : diag_(diag), cfg_(cfg),mod_(std::make_unique<llvm::Module>(module_name, ctx_)),
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

llvm::AllocaInst* IREmitter::create_alloca(llvm::Type* ty,const std::string& name) {
    assert(cur_func_ && "create_alloca outside function");
    llvm::IRBuilder<> tmp(&cur_func_->getEntryBlock(),cur_func_->getEntryBlock().begin());
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
    if (name == "string" || name == "table")
        return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
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
                return llvm::PointerType::getUnqual(llvm_type(v.args[0].get()));
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
            return llvm::FunctionType::get(ret, params, false)->getPointerTo();
        }

        else if constexpr (std::is_same_v<T, OptionalType>)
            return llvm_type(v.inner.get());

        else if constexpr (std::is_same_v<T, UnionType>)
            return tagvalue_type();
        
        else if constexpr (std::is_same_v<T, TupleType>) {
            std::vector<llvm::Type*> elems;
            for (auto& m : v.members)
                elems.push_back(llvm_type(m.get()));
            return llvm::StructType::get(ctx_, elems);
        }
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
// here i declare all the runtime funcs . -==-=-=-=-=-=-=--=-=-=-=-=--=-=--=-=-=-=-=-=-=-=-=-=-=-=-=
void IREmitter::declare_runtime() {
    auto* i8p   = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
    auto* i64   = llvm::Type::getInt64Ty(ctx_);
    auto* i32   = llvm::Type::getInt32Ty(ctx_);
    auto* f64   = llvm::Type::getDoubleTy(ctx_);
    auto* voidT = llvm::Type::getVoidTy(ctx_);

    auto declare = [&](const std::string& name,
        llvm::Type* ret,std::vector<llvm::Type*> params,
            bool vararg = false) {
        auto* ft = llvm::FunctionType::get(ret, params, vararg);
        auto* fn = llvm::Function::Create(
            ft, llvm::Function::ExternalLinkage, name, *mod_);
        functions_[name] = fn;
    };

    declare("slua_alloc",                i8p,   {i64});
    declare("slua_free",                 voidT, {i8p});
    declare("slua_panic",                voidT, {i8p, i8p, i32});
    declare("slua_print_str",            voidT, {i8p});
    declare("slua_print_int",            voidT, {i64});
    declare("slua_print_float",          voidT, {f64});
    declare("slua_print_bool",           voidT, {i32});
    declare("slua_print_null",           voidT, {});
    declare("slua_time_ns",              i64,   {});
    declare("slua_exit",                 voidT, {i32});

    declare("slua_read_line",            i8p,   {});
    declare("slua_read_char",            i32,   {});
    declare("slua_io_clear",             voidT, {});
    declare("slua_io_set_color",         voidT, {i8p});
    declare("slua_io_reset_color",       voidT, {});
    declare("slua_io_print_color",       voidT, {i8p, i8p});
    declare("slua_print_str_no_newline", voidT, {i8p});
    declare("slua_flush",                voidT, {});

    declare("slua_str_concat", i8p,   {i8p, i8p});
    declare("slua_int_to_str",  i8p,   {i64});
    declare("slua_float_to_str",i8p,   {f64});
    declare("slua_str_len",i32,   {i8p});
    declare("slua_str_byte",i32,   {i8p, i32});
    declare("slua_str_char",i8p,   {i32});
    declare("slua_str_sub", i8p,   {i8p, i32, i32});
    declare("slua_str_upper",i8p,   {i8p});
    declare("slua_str_lower", i8p,   {i8p});
    declare("slua_str_find", i32,   {i8p, i8p, i32});
    declare("slua_str_trim",i8p,   {i8p});
    declare("slua_str_to_int",i64,   {i8p});
    declare("slua_str_to_float",f64,   {i8p});

    declare("slua_os_time",i64,   {});
    declare("slua_os_sleep",voidT, {i64});
    declare("slua_os_getenv", i8p,   {i8p});
    declare("slua_os_system", voidT, {i8p});
    declare("slua_os_cwd",i8p,   {});
    declare("slua_os_sleepS", voidT, {i64});

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

    declare("slua_tbl_new",       i8p,   {});
    declare("slua_tbl_iset_i64",  voidT, {i8p, i64, i64});
    declare("slua_tbl_iset_f64",  voidT, {i8p, i64, f64});
    declare("slua_tbl_iset_str",  voidT, {i8p, i64, i8p});
    declare("slua_tbl_iset_bool", voidT, {i8p, i64, i32});
    declare("slua_tbl_sset_i64",  voidT, {i8p, i8p, i64});
    declare("slua_tbl_sset_f64",  voidT, {i8p, i8p, f64});
    declare("slua_tbl_sset_str",  voidT, {i8p, i8p, i8p});
    declare("slua_tbl_sset_bool", voidT, {i8p, i8p, i32});
    declare("slua_tbl_iget_i64",  i64,   {i8p, i64});
    declare("slua_tbl_iget_f64",  f64,   {i8p, i64});
    declare("slua_tbl_iget_str",  i8p,   {i8p, i64});
    declare("slua_tbl_iget_bool", i32,   {i8p, i64});
    declare("slua_tbl_sget_i64",  i64,   {i8p, i8p});
    declare("slua_tbl_sget_f64",  f64,   {i8p, i8p});
    declare("slua_tbl_sget_str",  i8p,   {i8p, i8p});
    declare("slua_tbl_sget_bool", i32,   {i8p, i8p});
    auto* f32 = llvm::Type::getFloatTy(ctx_);
    declare("slua_window_init",          voidT, {i32, i32, i8p});
    declare("slua_window_close",         voidT, {});
    declare("slua_window_should_close",  i32,   {});
    declare("slua_begin_drawing",        voidT, {});
    declare("slua_end_drawing",          voidT, {});
    declare("slua_clear_bg",             voidT, {i32, i32, i32, i32});
    declare("slua_set_target_fps",       voidT, {i32});
    declare("slua_get_fps",              i32,   {});
    declare("slua_get_frame_time",       f64,   {});
    declare("slua_screen_width",         i32,   {});
    declare("slua_screen_height",        i32,   {});
    declare("slua_draw_rect",            voidT, {i32, i32, i32, i32, i32, i32, i32, i32});
    declare("slua_draw_rect_outline",    voidT, {i32, i32, i32, i32, i32, i32, i32, i32, i32});
    declare("slua_draw_circle",          voidT, {i32, i32, f32, i32, i32, i32, i32});
    declare("slua_draw_circle_outline",  voidT, {i32, i32, f32, i32, i32, i32, i32});
    declare("slua_draw_line",            voidT, {i32, i32, i32, i32, i32, i32, i32, i32, i32});
    declare("slua_draw_triangle",        voidT, {i32, i32, i32, i32, i32, i32, i32, i32, i32, i32});
    declare("slua_draw_text",            voidT, {i8p, i32, i32, i32, i32, i32, i32, i32});
    declare("slua_measure_text",         i32,   {i8p, i32});
    declare("slua_is_key_down",          i32,   {i32});
    declare("slua_is_key_pressed",       i32,   {i32});
    declare("slua_is_key_released",      i32,   {i32});
    declare("slua_get_mouse_x",          i32,   {});
    declare("slua_get_mouse_y",          i32,   {});
    declare("slua_is_mouse_btn_pressed", i32,   {i32});
    declare("slua_is_mouse_btn_down",    i32,   {i32});
    declare("slua_get_mouse_wheel",      f64,   {});
    declare("slua_ui_button",            i32,   {i32, i32, i32, i32, i8p});
    declare("slua_ui_label",             voidT, {i32, i32, i32, i32, i8p});
    declare("slua_ui_checkbox",          i32,   {i32, i32, i32, i8p, i32});
    declare("slua_ui_slider",            f64,   {i32, i32, i32, i32, f64, f64, f64});
    declare("slua_ui_progress_bar",      voidT, {i32, i32, i32, i32, f64, f64});
    declare("slua_ui_panel",             voidT, {i32, i32, i32, i32, i8p});
    declare("slua_ui_text_input",        i32,   {i32, i32, i32, i32, i8p, i32, i32});
    declare("slua_ui_set_font_size",     voidT, {i32});
    declare("slua_ui_set_accent",        voidT, {i32, i32, i32});
    declare("slua_font_load",            i32,   {i8p, i32});
    declare("slua_font_unload",          voidT, {i32});
    declare("slua_draw_text_font",       voidT, {i32, i8p, i32, i32, i32, f32, i32, i32, i32, i32});
    declare("slua_fs_read_all",    i8p,   {i8p});
    declare("slua_fs_write",       i32,   {i8p, i8p});
    declare("slua_fs_append",      i32,   {i8p, i8p});
    declare("slua_fs_exists",      i32,   {i8p});
    declare("slua_fs_delete",      i32,   {i8p});
    declare("slua_fs_mkdir",       i32,   {i8p});
    declare("slua_fs_rename",      i32,   {i8p, i8p});
    declare("slua_fs_size",        i64,   {i8p});
    declare("slua_fs_listdir",     i8p,   {i8p});
    declare("slua_fs_open",        i64,   {i8p, i8p});
    declare("slua_fs_close",       i32,   {i64});
    declare("slua_fs_readline",    i8p,   {i64});
    declare("slua_fs_writeh",      i32,   {i64, i8p});
    declare("slua_fs_flush",       i32,   {i64});
    declare("slua_fs_copy",        i32,   {i8p, i8p});
    declare("slua_random_seed",    voidT, {i64});
    declare("slua_random_int",     i64,   {i64, i64});
    declare("slua_random_float",   f64,   {});
    declare("slua_random_range",   i64,   {i64, i64});
    declare("slua_random_gauss",   f64,   {f64, f64});
    declare("slua_datetime_now",        i64, {});
    declare("slua_datetime_format",     i8p, {i64, i8p});
    declare("slua_datetime_parse",      i64, {i8p, i8p});
    declare("slua_datetime_diff",       i64, {i64, i64});
    declare("slua_datetime_add",        i64, {i64, i64});
    declare("slua_datetime_now_str",    i8p, {i8p});
    declare("slua_datetime_year",       i32, {i64});
    declare("slua_datetime_month",      i32, {i64});
    declare("slua_datetime_day",        i32, {i64});
    declare("slua_datetime_hour",       i32, {i64});
    declare("slua_datetime_minute",     i32, {i64});
    declare("slua_datetime_second",     i32, {i64});
    declare("slua_path_join",      i8p,   {i8p, i8p});
    declare("slua_path_basename",  i8p,   {i8p});
    declare("slua_path_dirname",   i8p,   {i8p});
    declare("slua_path_extension", i8p,   {i8p});
    declare("slua_path_stem",      i8p,   {i8p});
    declare("slua_path_absolute",  i8p,   {i8p});
    declare("slua_path_normalize", i8p,   {i8p});
    declare("slua_path_exists",    i32,   {i8p});
    declare("slua_path_is_file",   i32,   {i8p});
    declare("slua_path_is_dir",    i32,   {i8p});
    declare("slua_process_run",    i64,   {i8p});
    declare("slua_process_output", i8p,   {i8p});
    declare("slua_process_spawn",  i64,   {i8p});
    declare("slua_process_wait",   i32,   {i64});
    declare("slua_process_kill",   i32,   {i64});
    declare("slua_process_alive",  i32,   {i64});
    declare("slua_json_encode_str",    i8p, {i8p});
    declare("slua_json_encode_int",    i8p, {i64});
    declare("slua_json_encode_float",  i8p, {f64});
    declare("slua_json_encode_bool",   i8p, {i32});
    declare("slua_json_encode_null",   i8p, {});
    declare("slua_json_get_str",       i8p, {i8p, i8p});
    declare("slua_json_get_int",       i64, {i8p, i8p});
    declare("slua_json_get_float",     f64, {i8p, i8p});
    declare("slua_json_get_bool",      i32, {i8p, i8p});
    declare("slua_json_has_key",       i32, {i8p, i8p});
    declare("slua_json_minify",        i8p, {i8p});
    declare("slua_json_get_array_item",i8p, {i8p, i8p, i32});
    declare("slua_json_get_nested_float", f64, {i8p, i8p, i8p});
    declare("slua_json_get_nested_int",   i64, {i8p, i8p, i8p});
    declare("slua_json_get_nested_str",   i8p, {i8p, i8p, i8p});
    declare("slua_net_init",        i32, {});
    declare("slua_net_connect",     i64, {i8p, i32});
    declare("slua_net_listen",      i64, {i32});
    declare("slua_net_accept",      i64, {i64});
    declare("slua_net_send",        i32, {i64, i8p});
    declare("slua_net_send_bytes",  i32, {i64, i8p, i32});
    declare("slua_net_recv",        i8p, {i64, i32});
    declare("slua_net_close",       i32, {i64});
    declare("slua_net_local_ip",    i8p, {});
    declare("slua_sync_mutex_new",     i64, {});
    declare("slua_sync_mutex_lock",    i32, {i64});
    declare("slua_sync_mutex_unlock",  i32, {i64});
    declare("slua_sync_mutex_trylock", i32, {i64});
    declare("slua_sync_mutex_free",    i32, {i64});
    declare("slua_regex_match",    i32, {i8p, i8p});
    declare("slua_regex_find",     i32, {i8p, i8p, i32});
    declare("slua_regex_replace",  i8p, {i8p, i8p, i8p});
    declare("slua_regex_groups",   i8p, {i8p, i8p});
    declare("slua_regex_count",    i32, {i8p, i8p});
    declare("slua_regex_find_all", i8p, {i8p, i8p});
    declare("slua_crypto_sha256",         i8p,   {i8p});
    declare("slua_crypto_md5",            i8p,   {i8p});
    declare("slua_crypto_base64_encode",  i8p,   {i8p, i32});
    declare("slua_crypto_base64_decode",  i8p,   {i8p});
    declare("slua_crypto_hex_encode",     i8p,   {i8p, i32});
    declare("slua_crypto_hex_decode",     i8p,   {i8p});
    declare("slua_crypto_hex_decode_len", i32,   {i8p});
    declare("slua_crypto_crc32",          i32,   {i8p, i32});
    declare("slua_crypto_hmac_sha256",    i8p,   {i8p, i8p});
    declare("slua_crypto_xor",            i8p,   {i8p, i32, i8p, i32});
    declare("slua_buf_new",          i64,   {i32});
    declare("slua_buf_from_str",     i64,   {i8p, i32});
    declare("slua_buf_free",         i32,   {i64});
    declare("slua_buf_size",         i32,   {i64});
    declare("slua_buf_write_u8",     i32,   {i64, i32, i32});
    declare("slua_buf_write_u16",    i32,   {i64, i32, i32});
    declare("slua_buf_write_u32",    i32,   {i64, i32, i32});
    declare("slua_buf_write_i64",    i32,   {i64, i32, i64});
    declare("slua_buf_write_f32",    i32,   {i64, i32, f64});
    declare("slua_buf_write_f64",    i32,   {i64, i32, f64});
    declare("slua_buf_read_u8",      i32,   {i64, i32});
    declare("slua_buf_read_u16",     i32,   {i64, i32});
    declare("slua_buf_read_u32_i",   i32,   {i64, i32});
    declare("slua_buf_read_i64",     i64,   {i64, i32});
    declare("slua_buf_read_f32",     f64,   {i64, i32});
    declare("slua_buf_read_f64",     f64,   {i64, i32});
    declare("slua_buf_to_str",       i8p,   {i64});
    declare("slua_buf_to_hex",       i8p,   {i64});
    declare("slua_buf_copy",         i32,   {i64, i32, i64, i32, i32});
    declare("slua_buf_fill",         i32,   {i64, i32, i32, i32});
    declare("slua_buf_write_str",    i32,   {i64, i32, i8p});
    declare("slua_thread_join",      i32,   {i64});
    declare("slua_thread_detach",    i32,   {i64});
    declare("slua_thread_alive",     i32,   {i64});
    declare("slua_thread_sleep_ms",  voidT, {i64});
    declare("slua_thread_self_id",   i64,   {});
    declare("slua_vec2_dot",     f64, {f64, f64, f64, f64});
    declare("slua_vec2_len",     f64, {f64, f64});
    declare("slua_vec2_dist",    f64, {f64, f64, f64, f64});
    declare("slua_vec2_norm_x",  f64, {f64, f64});
    declare("slua_vec2_norm_y",  f64, {f64, f64});
    declare("slua_vec3_dot",     f64, {f64, f64, f64, f64, f64, f64});
    declare("slua_vec3_len",     f64, {f64, f64, f64});
    declare("slua_vec3_dist",    f64, {f64, f64, f64, f64, f64, f64});
    declare("slua_vec3_norm_x",  f64, {f64, f64, f64});
    declare("slua_vec3_norm_y",  f64, {f64, f64, f64});
    declare("slua_vec3_norm_z",  f64, {f64, f64, f64});
    declare("slua_vec3_cross_x", f64, {f64, f64, f64, f64, f64, f64});
    declare("slua_vec3_cross_y", f64, {f64, f64, f64, f64, f64, f64});
    declare("slua_vec3_cross_z", f64, {f64, f64, f64, f64, f64, f64});
    declare("slua_math_clamp",   f64, {f64, f64, f64});
    declare("slua_math_lerp",    f64, {f64, f64, f64});
    declare("slua_math_abs",     f64, {f64});
    declare("slua_math_floor",   f64, {f64});
    declare("slua_math_ceil",    f64, {f64});
    declare("slua_math_round",   f64, {f64});
    declare("slua_math_min2",    f64, {f64, f64});
    declare("slua_math_max2",    f64, {f64, f64});
    declare("slua_math_sign",    f64, {f64});
    declare("slua_math_fract",   f64, {f64});
    declare("slua_math_mod",     f64, {f64, f64});
    declare("slua_camera3d_set",     voidT, {f64,f64,f64,f64,f64,f64,f64,f64,f64,f64,i32});
    declare("slua_camera3d_update",  voidT, {});
    declare("slua_begin_mode3d",     voidT, {});
    declare("slua_end_mode3d",       voidT, {});
    declare("slua_draw_cube",        voidT, {f32,f32,f32,f32,f32,f32,i32,i32,i32,i32});
    declare("slua_draw_cube_wires",  voidT, {f32,f32,f32,f32,f32,f32,i32,i32,i32,i32});
    declare("slua_draw_sphere",      voidT, {f32,f32,f32,f32,i32,i32,i32,i32});
    declare("slua_draw_sphere_wires",voidT, {f32,f32,f32,f32,i32,i32,i32,i32,i32,i32});
    declare("slua_draw_plane",       voidT, {f32,f32,f32,f32,f32,i32,i32,i32,i32});
    declare("slua_draw_cylinder",    voidT, {f32,f32,f32,f32,f32,f32,i32,i32,i32,i32,i32});
    declare("slua_draw_grid",        voidT, {i32, f32});
    declare("slua_draw_ray",         voidT, {f32,f32,f32,f32,f32,f32,i32,i32,i32,i32});
    declare("slua_draw_line3d",      voidT, {f32,f32,f32,f32,f32,f32,i32,i32,i32,i32});
    declare("slua_model_load",       i32,   {i8p});
    declare("slua_model_draw",       voidT, {i32,f32,f32,f32,f32,i32,i32,i32,i32});
    declare("slua_model_draw_ex",    voidT, {i32,f32,f32,f32,f32,f32,f32,f32,f32,f32,f32,i32,i32,i32,i32});
    declare("slua_model_unload",     voidT, {i32});
    declare("slua_texture_load",     i32,   {i8p});
    declare("slua_texture_draw",     voidT, {i32,i32,i32,i32,i32,i32,i32});
    declare("slua_texture_draw_ex",  voidT, {i32,f32,f32,f32,f32,i32,i32,i32,i32});
    declare("slua_texture_unload",   voidT, {i32});
    declare("slua_texture_width",    i32,   {i32});
    declare("slua_texture_height",   i32,   {i32});
    declare("slua_window_init_3d",   voidT, {i32,i32,i8p});
    declare("slua_get_time",         f64,   {});
    declare("slua_draw_fps_counter", voidT, {i32,i32});
    declare("slua_http_get",         i8p,   {i8p});
    declare("slua_http_post",        i8p,   {i8p, i8p, i8p});
    declare("slua_http_post_json",   i8p,   {i8p, i8p});
    declare("slua_http_status",      i32,   {i8p});
    declare("slua_tbl_len_rt",       i32,   {i8p});
    declare("slua_tbl_push",         voidT, {i8p, i64});
    declare("slua_tbl_push_f",       voidT, {i8p, f64});
    declare("slua_tbl_push_s",       voidT, {i8p, i8p});
    declare("slua_tbl_pop",          voidT, {i8p});
    declare("slua_tbl_contains_s",   i32,   {i8p, i8p});
    declare("slua_tbl_contains_i",   i32,   {i8p, i64});
    declare("slua_tbl_keys",         i8p,   {i8p});
    declare("slua_tbl_remove_at",    voidT, {i8p, i32});
    declare("slua_tbl_clear",        voidT, {i8p});
    declare("slua_tbl_merge",        i8p,   {i8p, i8p});
    declare("slua_tbl_slice",        i8p,   {i8p, i32, i32});
    declare("slua_tbl_reverse",      voidT, {i8p});
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
        else if (auto* ed = std::get_if<EnumDecl>(&s->v)) {
            auto* i64t = llvm::Type::getInt64Ty(ctx_);
            for (auto& [mname, mval] : ed->members) {
                int64_t val = mval.has_value() ? *mval : 0;
                auto* gv = new llvm::GlobalVariable(
                    *mod_, i64t, true,
                    llvm::GlobalValue::InternalLinkage,
                    llvm::ConstantInt::get(i64t, val), mname);
                env_->define(mname, gv);
            }
        }
    }

    for (auto& s : mod.stmts)
        emit_stmt(*s);

    pop_env();

    std::vector<llvm::Function*> dead;
    for (auto& fn : *mod_)
        if (fn.isDeclaration() && fn.use_empty())
            dead.push_back(&fn);
    for (auto* fn : dead) fn->eraseFromParent();

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
    auto* line_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), loc.line);
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
        else if constexpr (std::is_same_v<T, CStyleFor>)    emit_cstyle_for(v);
        else if constexpr (std::is_same_v<T, ReturnStmt>)   emit_return_stmt(v, s.loc);
        else if constexpr (std::is_same_v<T, Assign>)       emit_assign(v, s.loc);
        else if constexpr (std::is_same_v<T, CallStmt>)     emit_call_stmt(v);
        else if constexpr (std::is_same_v<T, DeferStmt>)    emit_defer_stmt(v);
        else if constexpr (std::is_same_v<T, DoBlock>)      emit_do_block(v);
        else if constexpr (std::is_same_v<T, StoreStmt>)    emit_store_stmt(v);
        else if constexpr (std::is_same_v<T, FreeStmt>)     emit_free_stmt(v);
        else if constexpr (std::is_same_v<T, PanicStmt>)    emit_panic_stmt(v);
        else if constexpr (std::is_same_v<T, TypeDecl>) {} 
        else if constexpr (std::is_same_v<T, ExternDecl>) {}
        else if constexpr (std::is_same_v<T, BreakStmt>) {
            if (!break_targets_.empty()) {
                builder_.CreateBr(break_targets_.back());
                auto* dead = llvm::BasicBlock::Create(ctx_, "break.dead", cur_func_);
                builder_.SetInsertPoint(dead);
            }
        }
        else if constexpr (std::is_same_v<T, ContinueStmt>) {
            if (!continue_targets_.empty()) {
                builder_.CreateBr(continue_targets_.back());
                auto* dead = llvm::BasicBlock::Create(ctx_, "cont.dead", cur_func_);
                builder_.SetInsertPoint(dead);
            }
        }
        else if constexpr (std::is_same_v<T, ImportDecl>) {}
        else if constexpr (std::is_same_v<T, EnumDecl>)       {}
        else if constexpr (std::is_same_v<T, MultiLocalDecl>) emit_multi_local_decl(v, s.loc);
    }, s.v);
}

void IREmitter::emit_local_decl(LocalDecl& s, SourceLoc loc) {
    llvm::Type* ty = s.type_ann
        ? llvm_type(s.type_ann.get())
        : llvm::Type::getInt64Ty(ctx_);

    llvm::AllocaInst* slot = create_alloca(ty, s.name);

    if (s.init) {
        llvm::Value* val = nullptr;
        if (auto* tc = std::get_if<TableCtor>(&s.init->v)) {
            if (s.type_ann && ty->isStructTy()) {
                auto* st = llvm::cast<llvm::StructType>(ty);
                std::string sname = st->getName().str();
                auto fit = struct_fields_.find(sname);
                if (fit != struct_fields_.end()) {
                    llvm::Value* agg = llvm::Constant::getNullValue(ty);
                    for (auto& entry : tc->entries) {
                        if (!entry.key) continue;
                        std::string fname;
                        if (auto* sl = std::get_if<StrLit>(&(*entry.key)->v))
                            fname = sl->val;
                        if (fname.empty()) continue;
                        auto& fnames = fit->second;
                        for (unsigned i = 0; i < fnames.size(); i++) {
                            if (fnames[i] == fname) {
                                llvm::Value* fval = emit_expr(*entry.val);
                                if (fval) {
                                    fval = coerce(fval, st->getElementType(i), loc);
                                    agg = builder_.CreateInsertValue(agg, fval, {i});
                                }
                                break;
                            }
                        }
                    }
                    val = agg;
                }
            }
            if (!val) val = emit_expr(*s.init);
        } else if (auto* idx = std::get_if<Index>(&s.init->v)) {
            val = emit_index(*idx, s.init->loc, s.type_ann.get());
        } else {
            val = emit_expr(*s.init);
        }
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
        builder_.CreateStore(llvm::Constant::getNullValue(ret_ty), cur_ret_slot_);
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
            cond = builder_.CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
        else if (cond->getType()->isPointerTy())
            cond = builder_.CreateIsNotNull(cond);
        else
            cond = builder_.CreateFCmpUNE(cond, llvm::ConstantFP::get(cond->getType(), 0.0));
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
                eic = builder_.CreateICmpNE(eic, llvm::Constant::getNullValue(eic->getType()));
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

    break_targets_.push_back(end_bb);
    continue_targets_.push_back(cond_bb);

    builder_.CreateBr(cond_bb);
    builder_.SetInsertPoint(cond_bb);

    llvm::Value* cond = emit_expr(*s.cond);
    if (!cond) { builder_.CreateBr(end_bb); return; }
    if (!cond->getType()->isIntegerTy(1))
        cond = builder_.CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
    builder_.CreateCondBr(cond, body_bb, end_bb);

    builder_.SetInsertPoint(body_bb);
    push_env(); push_defer_scope();
    for (auto& st : s.body) emit_stmt(*st);
    pop_defer_scope(); pop_env();
    if (!builder_.GetInsertBlock()->getTerminator())
        builder_.CreateBr(cond_bb);

    break_targets_.pop_back();
    continue_targets_.pop_back();

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
            cond = builder_.CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
        builder_.CreateCondBr(cond ? cond : llvm::ConstantInt::getFalse(ctx_), end_bb, body_bb);
    }
    builder_.SetInsertPoint(end_bb);
}

void IREmitter::emit_numeric_for(NumericFor& s) {
    auto* i64 = llvm::Type::getInt64Ty(ctx_);

    llvm::Value* start = emit_expr(*s.start);
    llvm::Value* stop  = emit_expr(*s.stop);
    llvm::Value* step  = s.step ? emit_expr(*s.step) : llvm::ConstantInt::get(i64, 1);

    start = coerce(start, i64, {});
    stop  = coerce(stop,  i64, {});
    step  = coerce(step,  i64, {});

    auto* loop_bb = llvm::BasicBlock::Create(ctx_, "for.loop", cur_func_);
    auto* body_bb = llvm::BasicBlock::Create(ctx_, "for.body", cur_func_);
    auto* end_bb  = llvm::BasicBlock::Create(ctx_, "for.end",  cur_func_);

    break_targets_.push_back(end_bb);
    continue_targets_.push_back(loop_bb);

    llvm::AllocaInst* i_slot = create_alloca(i64, s.var);
    builder_.CreateStore(start, i_slot);
    builder_.CreateBr(loop_bb);

    builder_.SetInsertPoint(loop_bb);
    llvm::Value* i_val = builder_.CreateLoad(i64, i_slot);
    llvm::Value* cond  = builder_.CreateICmpSLE(i_val, stop, "for.cond");
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

    break_targets_.pop_back();
    continue_targets_.pop_back();

    builder_.SetInsertPoint(end_bb);
}

void IREmitter::emit_cstyle_for(CStyleFor& s) {
    auto* i64 = llvm::Type::getInt64Ty(ctx_);

    push_env();
    llvm::AllocaInst* slot    = create_alloca(i64, s.var);
    llvm::Value*      initval = coerce(emit_expr(*s.init), i64, {});
    builder_.CreateStore(initval, slot);
    env_->define(s.var, slot);

    auto* cond_bb = llvm::BasicBlock::Create(ctx_, "cfor.cond", cur_func_);
    auto* body_bb = llvm::BasicBlock::Create(ctx_, "cfor.body", cur_func_);
    auto* step_bb = llvm::BasicBlock::Create(ctx_, "cfor.step", cur_func_);
    auto* end_bb  = llvm::BasicBlock::Create(ctx_, "cfor.end",  cur_func_);

    break_targets_.push_back(end_bb);
    continue_targets_.push_back(step_bb);

    builder_.CreateBr(cond_bb);
    builder_.SetInsertPoint(cond_bb);
    llvm::Value* cv = emit_expr(*s.cond);
    if (cv->getType() != llvm::Type::getInt1Ty(ctx_))
        cv = builder_.CreateICmpNE(cv, llvm::ConstantInt::get(cv->getType(), 0));
    builder_.CreateCondBr(cv, body_bb, end_bb);

    builder_.SetInsertPoint(body_bb);
    push_defer_scope();
    for (auto& st : s.body) emit_stmt(*st);
    pop_defer_scope();
    if (!builder_.GetInsertBlock()->getTerminator())
        builder_.CreateBr(step_bb);

    builder_.SetInsertPoint(step_bb);
    builder_.CreateStore(coerce(emit_expr(*s.step), i64, {}), slot);
    builder_.CreateBr(cond_bb);

    break_targets_.pop_back();
    continue_targets_.pop_back();
    pop_env();

    builder_.SetInsertPoint(end_bb);
}

void IREmitter::emit_return_stmt(ReturnStmt& s, SourceLoc loc) {
    if (cur_ret_slot_) {
        llvm::Type* ret_ty = cur_ret_slot_->getAllocatedType();
        if (s.values.size() == 1) {
            if (auto* tc = std::get_if<TableCtor>(&s.values[0]->v)) {
                if (ret_ty->isStructTy()) {
                    auto* st = llvm::cast<llvm::StructType>(ret_ty);
                    std::string sname = st->getName().str();
                    auto fit = struct_fields_.find(sname);
                    if (fit != struct_fields_.end()) {
                        llvm::Value* agg = llvm::Constant::getNullValue(ret_ty);
                        for (auto& entry : tc->entries) {
                            if (!entry.key) continue;
                            std::string fname;
                            if (auto* sl = std::get_if<StrLit>(&(*entry.key)->v))
                                fname = sl->val;
                            if (fname.empty()) continue;
                            for (unsigned i = 0; i < fit->second.size(); i++) {
                                if (fit->second[i] == fname) {
                                    llvm::Value* fval = emit_expr(*entry.val);
                                    if (fval) {
                                        fval = coerce(fval, st->getElementType(i), loc);
                                        agg = builder_.CreateInsertValue(agg, fval, {i});
                                    }
                                    break;
                                }
                            }
                        }
                        builder_.CreateStore(agg, cur_ret_slot_);
                        for (int i = (int)defer_stack_.size()-1; i >= 0; i--)
                            for (int j = (int)defer_stack_[i].size()-1; j >= 0; j--)
                                defer_stack_[i][j].emit_fn();
                        builder_.CreateBr(cur_ret_bb_);
                        return;
                    }
                }
            }
            llvm::Value* val = emit_expr(*s.values[0]);
            if (val) {
                val = coerce(val, ret_ty, loc);
                builder_.CreateStore(val, cur_ret_slot_);
            }
        } else if (s.values.size() > 1) {
            if (auto* st = llvm::dyn_cast<llvm::StructType>(ret_ty)) {
                llvm::Value* agg = llvm::Constant::getNullValue(st);
                for (size_t i = 0; i < s.values.size() && i < st->getNumElements(); i++) {
                    llvm::Value* v = emit_expr(*s.values[i]);
                    if (v) {
                        v = coerce(v, st->getElementType(i), loc);
                        agg = builder_.CreateInsertValue(agg, v, {(unsigned)i});
                    }
                }
                builder_.CreateStore(agg, cur_ret_slot_);
            }
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
            auto* fn  = get_runtime_fn(pfx + "_bool");
            auto* ext = builder_.CreateZExt(val, i32);
            if (fn) builder_.CreateCall(fn, {tbl, key, ext});
        } else if (vty->isPointerTy()) {
            auto* fn = get_runtime_fn(pfx + "_str");
            if (fn) builder_.CreateCall(fn, {tbl, key, val});
        } else {
            auto* fn  = get_runtime_fn(pfx + "_i64");
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

    auto* fn = get_runtime_fn("slua_free");
    if (fn) builder_.CreateCall(fn, {ptr});
}

void IREmitter::emit_panic_stmt(PanicStmt& s) {
    llvm::Value* msg = emit_expr(*s.msg);
    auto* fn = get_runtime_fn("slua_panic");
    if (!fn || !msg) return;

    if (!msg->getType()->isPointerTy())
        msg = builder_.CreateGlobalStringPtr("<panic>");
    auto* file_val = builder_.CreateGlobalStringPtr("?");
    auto* line_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0);
    builder_.CreateCall(fn, {msg, file_val, line_val});
    builder_.CreateUnreachable();
}

void IREmitter::emit_type_decl(TypeDecl& s, SourceLoc loc) {
    if (auto* rt = std::get_if<RecordType>(&s.def->v)) {
        std::vector<llvm::Type*> fields;
        std::vector<std::string> names;
        for (auto& [fn, ft] : rt->fields) {
            fields.push_back(llvm_type(ft.get()));
            names.push_back(fn);
        }
        auto* st = llvm::StructType::create(ctx_, s.name);
        st->setBody(fields);
        struct_types_[s.name] = st;
        struct_fields_[s.name] = names;
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
    auto* fn  = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, s.name, *mod_);
    functions_[s.name] = fn;
}

llvm::Value* IREmitter::emit_expr(Expr& e) {
    return std::visit([&](auto&& v) -> llvm::Value* {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, NullLit>)
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);

        else if constexpr (std::is_same_v<T, BoolLit>)
            return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx_), v.val ? 1 : 0);

        else if constexpr (std::is_same_v<T, IntLit>)
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), (uint64_t)v.val, true);

        else if constexpr (std::is_same_v<T, FloatLit>)
            return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), v.val);

        else if constexpr (std::is_same_v<T, StrLit>)
            return builder_.CreateGlobalStringPtr(v.val);

        else if constexpr (std::is_same_v<T, Ident>) {
            llvm::Value* slot = env_ ? env_->lookup(v.name) : nullptr;
            if (!slot) {
                auto fit = functions_.find(v.name);
                if (fit != functions_.end()) return fit->second;
                return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
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
            return emit_index(v, e.loc, e.inferred_type.get());
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
            auto*    dl = &mod_->getDataLayout();
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
            auto* fn  = llvm::Function::Create(fty, llvm::Function::InternalLinkage, name, *mod_);
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
                builder_.CreateStore(llvm::Constant::getNullValue(ret_ty), cur_ret_slot_);
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
            cond = builder_.CreateICmpNE(lhs, llvm::Constant::getNullValue(lhs->getType()));
        else
            cond = builder_.CreateIsNotNull(lhs);

        auto* rhs_bb = llvm::BasicBlock::Create(ctx_, "logic.rhs", cur_func_);
        auto* end_bb = llvm::BasicBlock::Create(ctx_, "logic.end", cur_func_);
        auto* cur_bb = builder_.GetInsertBlock();

        if (e.op == "and") builder_.CreateCondBr(cond, rhs_bb, end_bb);
        else               builder_.CreateCondBr(cond, end_bb, rhs_bb);

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

    bool is_float = lhs->getType()->isDoubleTy() || rhs->getType()->isDoubleTy();

    if (is_float && !lhs->getType()->isPointerTy() && !rhs->getType()->isPointerTy()) {
        if (!lhs->getType()->isDoubleTy())
            lhs = builder_.CreateSIToFP(lhs, llvm::Type::getDoubleTy(ctx_));
        if (!rhs->getType()->isDoubleTy())
            rhs = builder_.CreateSIToFP(rhs, llvm::Type::getDoubleTy(ctx_));
    } else {
        if (lhs->getType() != rhs->getType()) {
            auto* i64 = llvm::Type::getInt64Ty(ctx_);
            if (lhs->getType()->isIntegerTy()) lhs = builder_.CreateSExt(lhs, i64);
            if (rhs->getType()->isIntegerTy()) rhs = builder_.CreateSExt(rhs, i64);
        }
    }

    if (e.op == "+")  return is_float ? builder_.CreateFAdd(lhs, rhs) : builder_.CreateAdd(lhs, rhs);
    if (e.op == "-")  return is_float ? builder_.CreateFSub(lhs, rhs) : builder_.CreateSub(lhs, rhs);
    if (e.op == "*")  return is_float ? builder_.CreateFMul(lhs, rhs) : builder_.CreateMul(lhs, rhs);
    if (e.op == "/") {
        if (!is_float) {
            lhs = builder_.CreateSIToFP(lhs, llvm::Type::getDoubleTy(ctx_));
            rhs = builder_.CreateSIToFP(rhs, llvm::Type::getDoubleTy(ctx_));
        }
        return builder_.CreateFDiv(lhs, rhs);
    }
    if (e.op == "%")  return is_float ? builder_.CreateFRem(lhs, rhs) : builder_.CreateSRem(lhs, rhs);

    if (e.op == "==" || e.op == "~=") {
        llvm::Value* cmp = is_float ? builder_.CreateFCmpOEQ(lhs, rhs) : builder_.CreateICmpEQ(lhs, rhs);
        return (e.op == "~=") ? builder_.CreateNot(cmp) : cmp;
    }
    if (e.op == "<")  return is_float ? builder_.CreateFCmpOLT(lhs, rhs) : builder_.CreateICmpSLT(lhs, rhs);
    if (e.op == ">")  return is_float ? builder_.CreateFCmpOGT(lhs, rhs) : builder_.CreateICmpSGT(lhs, rhs);
    if (e.op == "<=") return is_float ? builder_.CreateFCmpOLE(lhs, rhs) : builder_.CreateICmpSLE(lhs, rhs);
    if (e.op == ">=") return is_float ? builder_.CreateFCmpOGE(lhs, rhs) : builder_.CreateICmpSGE(lhs, rhs);

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
                auto* ext = builder_.CreateSExt(v, llvm::Type::getInt64Ty(ctx_));
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
        if (val->getType()->isDoubleTy()) return builder_.CreateFNeg(val);
        return builder_.CreateNeg(val);
    }
    if (e.op == "not") {
        if (!val->getType()->isIntegerTy(1))
            val = builder_.CreateICmpEQ(val, llvm::Constant::getNullValue(val->getType()));
        return builder_.CreateNot(val);
    }
    if (e.op == "#") {
        auto* i64 = llvm::Type::getInt64Ty(ctx_);
        if (val->getType()->isPointerTy()) {
            auto* fn = get_runtime_fn("slua_tbl_len_rt");
            if (fn) {
                auto* r = builder_.CreateCall(fn, {val}, "tlen");
                return builder_.CreateSExt(r, i64);
            }
        }
        if (val->getType()->isIntegerTy()) {
            auto* fn = get_runtime_fn("slua_str_len");
            if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {val}, "slen"), i64);
        }
        return llvm::ConstantInt::get(i64, 0);
    }
    return val;
}
// here the umm, all the libs -=-========-=-=-=-=-=-=-=-=-=-=-=-=-=-=-========-=-=-=-=-=-=-=-=-=-=-=
llvm::Value* IREmitter::emit_call_expr(Call& e, SourceLoc loc) {
    if (auto* fld = std::get_if<Field>(&e.callee->v)) {
        if (auto* base_id = std::get_if<Ident>(&fld->table->v)) {
            std::string full_name = base_id->name + "." + fld->name;
            auto fit = functions_.find(full_name);
            if (fit != functions_.end()) {
                llvm::Function* fn = fit->second;
                auto* ftype = fn->getFunctionType();
                std::vector<llvm::Value*> args;
                for (size_t i = 0; i < e.args.size(); i++) {
                    llvm::Value* av = emit_expr(*e.args[i]);
                    if (i < ftype->getNumParams()) {
                        llvm::Type* pt = ftype->getParamType((unsigned)i);
                        if (av && av->getType() != pt) {
                            if (pt->isDoubleTy() && av->getType()->isIntegerTy())
                                av = builder_.CreateSIToFP(av, pt, "coerce_f");
                            else if (pt->isIntegerTy() && av->getType()->isDoubleTy())
                                av = builder_.CreateFPToSI(av, pt, "coerce_i");
                            else if (pt->isIntegerTy() && av->getType()->isIntegerTy() && pt->getIntegerBitWidth() != av->getType()->getIntegerBitWidth())
                                av = builder_.CreateSExtOrTrunc(av, pt, "coerce_sz");
                            else if (pt->isPointerTy() && !av->getType()->isPointerTy())
                                av = builder_.CreateIntToPtr(av, pt, "coerce_p");
                            else if (!pt->isPointerTy() && av->getType()->isPointerTy())
                                av = builder_.CreatePtrToInt(av, pt, "coerce_pi");
                        }
                        args.push_back(av);
                    } else {
                        args.push_back(av);
                    }
                }
                return builder_.CreateCall(fn, args, "oop_ret");
            }
        }
    }

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

            if (mod == "string") {
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto arg0 = [&]() -> llvm::Value* { return e.args.size() > 0 ? emit_expr(*e.args[0]) : nullptr; };
                auto arg1 = [&]() -> llvm::Value* { return e.args.size() > 1 ? emit_expr(*e.args[1]) : nullptr; };
                auto arg2 = [&]() -> llvm::Value* { return e.args.size() > 2 ? emit_expr(*e.args[2]) : nullptr; };
                auto ci32 = [&](llvm::Value* v) -> llvm::Value* {
                    if (!v) return llvm::ConstantInt::get(i32, 0);
                    if (v->getType()->isIntegerTy(32)) return v;
                    return builder_.CreateTrunc(v, i32);
                };
                if (meth == "len" && e.args.size() >= 1) {
                    auto* fn = get_runtime_fn("slua_str_len");
                    auto* a  = arg0();
                    if (fn && a) return builder_.CreateSExt(builder_.CreateCall(fn, {a}, "slen"), i64);
                }
                if (meth == "byte" && e.args.size() >= 2) {
                    auto* fn = get_runtime_fn("slua_str_byte");
                    auto* a  = arg0(); auto* b = ci32(arg1());
                    if (fn && a) return builder_.CreateSExt(builder_.CreateCall(fn, {a, b}, "sbyte"), i64);
                }
                if (meth == "char" && e.args.size() >= 1) {
                    auto* fn = get_runtime_fn("slua_str_char");
                    auto* a  = ci32(arg0());
                    if (fn) return builder_.CreateCall(fn, {a}, "schar");
                }
                if (meth == "sub" && e.args.size() >= 3) {
                    auto* fn = get_runtime_fn("slua_str_sub");
                    auto* a  = arg0(); auto* b = ci32(arg1()); auto* c = ci32(arg2());
                    if (fn && a) return builder_.CreateCall(fn, {a, b, c}, "ssub");
                }
                if (meth == "upper" && e.args.size() >= 1) {
                    auto* fn = get_runtime_fn("slua_str_upper");
                    auto* a  = arg0();
                    if (fn && a) return builder_.CreateCall(fn, {a}, "supper");
                }
                if (meth == "lower" && e.args.size() >= 1) {
                    auto* fn = get_runtime_fn("slua_str_lower");
                    auto* a  = arg0();
                    if (fn && a) return builder_.CreateCall(fn, {a}, "slower");
                }
                if (meth == "find" && e.args.size() >= 2) {
                    auto* fn   = get_runtime_fn("slua_str_find");
                    auto* a    = arg0(); auto* b = arg1();
                    auto* from = e.args.size() >= 3 ? ci32(arg2()) : llvm::ConstantInt::get(i32, 0);
                    if (fn && a && b) return builder_.CreateSExt(builder_.CreateCall(fn, {a, b, from}, "sfind"), i64);
                }
                if (meth == "trim" && e.args.size() >= 1) {
                    auto* fn = get_runtime_fn("slua_str_trim");
                    auto* a  = arg0();
                    if (fn && a) return builder_.CreateCall(fn, {a}, "strim");
                }
                if (meth == "to_int" && e.args.size() >= 1) {
                    auto* fn = get_runtime_fn("slua_str_to_int");
                    auto* a  = arg0();
                    if (fn && a) return builder_.CreateCall(fn, {a}, "stoi");
                }
                if (meth == "to_float" && e.args.size() >= 1) {
                    auto* fn = get_runtime_fn("slua_str_to_float");
                    auto* a  = arg0();
                    if (fn && a) return builder_.CreateCall(fn, {a}, "stof");
                }
                if (meth == "concat" && e.args.size() >= 2) {
                    auto* fn = get_runtime_fn("slua_str_concat");
                    auto* a  = arg0(); auto* b = arg1();
                    if (fn && a && b) return builder_.CreateCall(fn, {a, b}, "scat");
                }
                return llvm::ConstantInt::get(i64, 0);
            }

            if (mod == "stdata") {
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* i1  = llvm::Type::getInt1Ty(ctx_);
                auto* f64 = llvm::Type::getDoubleTy(ctx_);
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                auto getarg = [&](size_t n) -> llvm::Value* {
                    return e.args.size() > n ? emit_expr(*e.args[n]) : nullptr;
                };
                if (meth == "typeof" && e.args.size() >= 1) {
                    llvm::Value* v = getarg(0);
                    if (!v)                          return builder_.CreateGlobalStringPtr("null");
                    if (v->getType()->isDoubleTy())  return builder_.CreateGlobalStringPtr("number");
                    if (v->getType()->isIntegerTy(1))return builder_.CreateGlobalStringPtr("bool");
                    if (v->getType()->isPointerTy()) return builder_.CreateGlobalStringPtr("string");
                    return builder_.CreateGlobalStringPtr("int");
                }
                if (meth == "tostring" && e.args.size() >= 1) {
                    llvm::Value* v = getarg(0);
                    if (!v) return builder_.CreateGlobalStringPtr("null");
                    if (v->getType()->isPointerTy()) return v;
                    if (v->getType()->isDoubleTy()) {
                        auto* fn = get_runtime_fn("slua_float_to_str");
                        if (fn) return builder_.CreateCall(fn, {v}, "fts");
                    }
                    auto* fn = get_runtime_fn("slua_int_to_str");
                    if (fn) return builder_.CreateCall(fn, {builder_.CreateSExt(v, i64)}, "its");
                    return builder_.CreateGlobalStringPtr("?");
                }
                if (meth == "tointeger" && e.args.size() >= 1) {
                    llvm::Value* v = getarg(0);
                    if (!v) return llvm::ConstantInt::get(i64, 0);
                    return coerce(v, i64, loc);
                }
                if (meth == "tofloat" && e.args.size() >= 1) {
                    llvm::Value* v = getarg(0);
                    if (!v) return llvm::ConstantFP::get(f64, 0.0);
                    return coerce(v, f64, loc);
                }
                if (meth == "tobool" && e.args.size() >= 1) {
                    llvm::Value* v = getarg(0);
                    if (!v) return llvm::ConstantInt::get(i1, 0);
                    if (v->getType()->isIntegerTy(1)) return v;
                    if (v->getType()->isPointerTy())  return builder_.CreateIsNotNull(v);
                    return builder_.CreateICmpNE(v, llvm::Constant::getNullValue(v->getType()));
                }
                if (meth == "isnull" && e.args.size() >= 1) {
                    llvm::Value* v = getarg(0);
                    if (!v) return llvm::ConstantInt::get(i1, 1);
                    if (v->getType()->isPointerTy()) return builder_.CreateIsNull(v);
                    return builder_.CreateICmpEQ(v, llvm::Constant::getNullValue(v->getType()));
                }
                if (meth == "assert" && e.args.size() >= 1) {
                    llvm::Value* cond = getarg(0);
                    llvm::Value* msg  = e.args.size() >= 2 ? getarg(1) : builder_.CreateGlobalStringPtr("assertion failed");
                    if (!cond) return llvm::ConstantInt::get(i64, 0);
                    if (!cond->getType()->isIntegerTy(1))
                        cond = builder_.CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
                    auto* pass_bb = llvm::BasicBlock::Create(ctx_, "assert.pass", cur_func_);
                    auto* fail_bb = llvm::BasicBlock::Create(ctx_, "assert.fail", cur_func_);
                    builder_.CreateCondBr(cond, pass_bb, fail_bb);
                    builder_.SetInsertPoint(fail_bb);
                    auto* pfn = get_runtime_fn("slua_panic");
                    if (pfn) {
                        if (!msg || !msg->getType()->isPointerTy())
                            msg = builder_.CreateGlobalStringPtr("assertion failed");
                        builder_.CreateCall(pfn, {msg,
                            builder_.CreateGlobalStringPtr("?"),
                            llvm::ConstantInt::get(i32, 0)});
                    }
                    builder_.CreateUnreachable();
                    builder_.SetInsertPoint(pass_bb);
                    return llvm::ConstantInt::get(i64, 0);
                }
                return llvm::ConstantInt::get(i64, 0);
            }

            if (mod == "os") {
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                if (meth == "time") {
                    auto* fn = get_runtime_fn("slua_os_time");
                    if (fn) return builder_.CreateCall(fn, {}, "ostime");
                }
                if (meth == "sleep" && e.args.size() >= 1) {
                    auto* fn = get_runtime_fn("slua_os_sleep");
                    auto* a  = coerce(emit_expr(*e.args[0]), i64, loc);
                    if (fn) builder_.CreateCall(fn, {a});
                    return llvm::ConstantInt::get(i64, 0);
                }
                if (meth == "sleepS" && e.args.size() >= 1) {
                    auto* fn = get_runtime_fn("slua_os_sleepS");
                    auto* a  = coerce(emit_expr(*e.args[0]), i64, loc);
                    if (fn) builder_.CreateCall(fn, {a});
                    return llvm::ConstantInt::get(i64, 0);
                }
                if (meth == "getenv" && e.args.size() >= 1) {
                    auto* fn = get_runtime_fn("slua_os_getenv");
                    auto* a  = emit_expr(*e.args[0]);
                    if (fn && a) return builder_.CreateCall(fn, {a}, "osenv");
                }
                if (meth == "system" && e.args.size() >= 1) {
                    auto* fn = get_runtime_fn("slua_os_system");
                    auto* a  = emit_expr(*e.args[0]);
                    if (fn && a) builder_.CreateCall(fn, {a});
                    return llvm::ConstantInt::get(i64, 0);
                }
                if (meth == "cwd") {
                    auto* fn = get_runtime_fn("slua_os_cwd");
                    if (fn) return builder_.CreateCall(fn, {}, "oscwd");
                }
                return llvm::ConstantInt::get(i64, 0);
            }

            if (mod == "window") {
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto ci32 = [&](llvm::Value* v) -> llvm::Value* {
                    if (!v) return llvm::ConstantInt::get(i32, 0);
                    if (v->getType()->isIntegerTy(32)) return v;
                    if (v->getType()->isIntegerTy()) return builder_.CreateTrunc(v, i32);
                    if (v->getType()->isDoubleTy())  return builder_.CreateFPToSI(v, i32);
                    return v;
                };
                auto ga = [&](size_t n) -> llvm::Value* { return e.args.size() > n ? emit_expr(*e.args[n]) : nullptr; };
                if (meth == "init" && e.args.size() >= 3) {
                    auto* fn = get_runtime_fn("slua_window_init");
                    if (fn) builder_.CreateCall(fn, {ci32(ga(0)), ci32(ga(1)), ga(2)});
                    return llvm::ConstantInt::get(i64, 0);
                }
                if (meth == "close") { auto* fn = get_runtime_fn("slua_window_close"); if (fn) builder_.CreateCall(fn, {}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "should_close") { auto* fn = get_runtime_fn("slua_window_should_close"); if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {}, "wsc"), i64); }
                if (meth == "begin_drawing") { auto* fn = get_runtime_fn("slua_begin_drawing"); if (fn) builder_.CreateCall(fn, {}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "end_drawing")   { auto* fn = get_runtime_fn("slua_end_drawing");   if (fn) builder_.CreateCall(fn, {}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "clear" && e.args.size() >= 4) { auto* fn = get_runtime_fn("slua_clear_bg"); if (fn) builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3))}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "set_fps" && e.args.size() >= 1) { auto* fn = get_runtime_fn("slua_set_target_fps"); if (fn) builder_.CreateCall(fn, {ci32(ga(0))}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "get_fps") { auto* fn = get_runtime_fn("slua_get_fps"); if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {}, "fps"), i64); }
                if (meth == "frame_time") { auto* fn = get_runtime_fn("slua_get_frame_time"); if (fn) return builder_.CreateCall(fn, {}, "ft"); }
                if (meth == "width")  { auto* fn = get_runtime_fn("slua_screen_width");  if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {}, "sw"), i64); }
                if (meth == "height") { auto* fn = get_runtime_fn("slua_screen_height"); if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {}, "sh"), i64); }
                return llvm::ConstantInt::get(i64, 0);
            }

            if (mod == "draw") {
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* f32 = llvm::Type::getFloatTy(ctx_);
                auto ci32 = [&](llvm::Value* v) -> llvm::Value* {
                    if (!v) return llvm::ConstantInt::get(i32, 0);
                    if (v->getType()->isIntegerTy(32)) return v;
                    if (v->getType()->isIntegerTy()) return builder_.CreateTrunc(v, i32);
                    if (v->getType()->isDoubleTy())  return builder_.CreateFPToSI(v, i32);
                    return v;
                };
                auto cf32 = [&](llvm::Value* v) -> llvm::Value* {
                    if (!v) return llvm::ConstantFP::get(f32, 0.0);
                    if (v->getType()->isFloatTy())   return v;
                    if (v->getType()->isDoubleTy())  return builder_.CreateFPTrunc(v, f32);
                    if (v->getType()->isIntegerTy()) return builder_.CreateSIToFP(v, f32);
                    return v;
                };
                auto ga = [&](size_t n) -> llvm::Value* { return e.args.size() > n ? emit_expr(*e.args[n]) : nullptr; };
                if (meth == "rect" && e.args.size() >= 8) { auto* fn = get_runtime_fn("slua_draw_rect"); if (fn) builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3)),ci32(ga(4)),ci32(ga(5)),ci32(ga(6)),ci32(ga(7))}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "rect_outline" && e.args.size() >= 9) { auto* fn = get_runtime_fn("slua_draw_rect_outline"); if (fn) builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3)),ci32(ga(4)),ci32(ga(5)),ci32(ga(6)),ci32(ga(7)),ci32(ga(8))}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "circle" && e.args.size() >= 7) { auto* fn = get_runtime_fn("slua_draw_circle"); if (fn) builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),cf32(ga(2)),ci32(ga(3)),ci32(ga(4)),ci32(ga(5)),ci32(ga(6))}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "circle_outline" && e.args.size() >= 7) { auto* fn = get_runtime_fn("slua_draw_circle_outline"); if (fn) builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),cf32(ga(2)),ci32(ga(3)),ci32(ga(4)),ci32(ga(5)),ci32(ga(6))}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "line" && e.args.size() >= 9) { auto* fn = get_runtime_fn("slua_draw_line"); if (fn) builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3)),ci32(ga(4)),ci32(ga(5)),ci32(ga(6)),ci32(ga(7)),ci32(ga(8))}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "triangle" && e.args.size() >= 10) { auto* fn = get_runtime_fn("slua_draw_triangle"); if (fn) builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3)),ci32(ga(4)),ci32(ga(5)),ci32(ga(6)),ci32(ga(7)),ci32(ga(8)),ci32(ga(9))}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "text" && e.args.size() >= 8) { auto* fn = get_runtime_fn("slua_draw_text"); if (fn) builder_.CreateCall(fn, {ga(0),ci32(ga(1)),ci32(ga(2)),ci32(ga(3)),ci32(ga(4)),ci32(ga(5)),ci32(ga(6)),ci32(ga(7))}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "measure_text" && e.args.size() >= 2) { auto* fn = get_runtime_fn("slua_measure_text"); if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {ga(0), ci32(ga(1))}, "mtw"), i64); }
                if (meth == "text_font" && e.args.size() >= 10) { auto* fn = get_runtime_fn("slua_draw_text_font"); if (fn) builder_.CreateCall(fn, {ci32(ga(0)),ga(1),ci32(ga(2)),ci32(ga(3)),ci32(ga(4)),cf32(ga(5)),ci32(ga(6)),ci32(ga(7)),ci32(ga(8)),ci32(ga(9))}); return llvm::ConstantInt::get(i64, 0); }
                
                return llvm::ConstantInt::get(i64, 0);
            }
            
            if (mod == "input") {
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto ci32 = [&](llvm::Value* v) -> llvm::Value* {
                    if (!v) return llvm::ConstantInt::get(i32, 0);
                    if (v->getType()->isIntegerTy(32)) return v;
                    if (v->getType()->isIntegerTy()) return builder_.CreateTrunc(v, i32);
                    return v;
                };
                auto ga = [&](size_t n) -> llvm::Value* { return e.args.size() > n ? emit_expr(*e.args[n]) : nullptr; };
                if (meth == "key_down"     && e.args.size() >= 1) { auto* fn = get_runtime_fn("slua_is_key_down");if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {ci32(ga(0))}, "kd"), i64); }
                if (meth == "key_pressed"  && e.args.size() >= 1) { auto* fn = get_runtime_fn("slua_is_key_pressed"); if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {ci32(ga(0))}, "kp"), i64); }
                if (meth == "key_released" && e.args.size() >= 1) { auto* fn = get_runtime_fn("slua_is_key_released");       if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {ci32(ga(0))}, "kr"), i64); }
                if (meth == "mouse_x")     { auto* fn = get_runtime_fn("slua_get_mouse_x");  if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {}, "mx"), i64); }
                if (meth == "mouse_y")     { auto* fn = get_runtime_fn("slua_get_mouse_y"); if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {}, "my"), i64); }
                if (meth == "mouse_pressed" && e.args.size() >= 1) { auto* fn = get_runtime_fn("slua_is_mouse_btn_pressed"); if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {ci32(ga(0))}, "mbp"), i64); }
                if (meth == "mouse_down"    && e.args.size() >= 1) { auto* fn = get_runtime_fn("slua_is_mouse_btn_down");    if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {ci32(ga(0))}, "mbd"), i64); }
                if (meth == "mouse_wheel")  { auto* fn = get_runtime_fn("slua_get_mouse_wheel"); if (fn) return builder_.CreateCall(fn, {}, "mw"); }
                return llvm::ConstantInt::get(i64, 0);
            }
            
            if (mod == "font") {
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto ci32 = [&](llvm::Value* v) -> llvm::Value* {
                    if (!v) return llvm::ConstantInt::get(i32, 0);
                    if (v->getType()->isIntegerTy(32)) return v;
                    if (v->getType()->isIntegerTy()) return builder_.CreateTrunc(v, i32);
                    if (v->getType()->isDoubleTy())  return builder_.CreateFPToSI(v, i32);
                    return v;
                };
                auto ga = [&](size_t n) -> llvm::Value* {
                    return e.args.size() > n ? emit_expr(*e.args[n]) : nullptr;
                };
                if (meth == "load" && e.args.size() >= 2) {
                    auto* fn = get_runtime_fn("slua_font_load");
                    if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {ga(0), ci32(ga(1))}, "fload"), i64);
                }
                if (meth == "unload" && e.args.size() >= 1) {
                    auto* fn = get_runtime_fn("slua_font_unload");
                    if (fn) builder_.CreateCall(fn, {ci32(ga(0))});
                    return llvm::ConstantInt::get(i64, 0);
                }
                return llvm::ConstantInt::get(i64, 0);
            }
            if (mod == "ui") {
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* f64 = llvm::Type::getDoubleTy(ctx_);
                auto ci32 = [&](llvm::Value* v) -> llvm::Value* {
                    if (!v) return llvm::ConstantInt::get(i32, 0);
                    if (v->getType()->isIntegerTy(32)) return v;
                    if (v->getType()->isIntegerTy()) return builder_.CreateTrunc(v, i32);
                    if (v->getType()->isDoubleTy())  return builder_.CreateFPToSI(v, i32);
                    return v;
                };
                auto cf64 = [&](llvm::Value* v) -> llvm::Value* {
                    if (!v) return llvm::ConstantFP::get(f64, 0.0);
                    if (v->getType()->isDoubleTy()) return v;
                    if (v->getType()->isIntegerTy()) return builder_.CreateSIToFP(v, f64);
                    return v;
                };
                auto ga = [&](size_t n) -> llvm::Value* { return e.args.size() > n ? emit_expr(*e.args[n]) : nullptr; };
                if (meth == "button"   && e.args.size() >= 5) { auto* fn = get_runtime_fn("slua_ui_button");   if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3)),ga(4)}, "ubtn"), i64); }
                if (meth == "label"    && e.args.size() >= 5) { auto* fn = get_runtime_fn("slua_ui_label");    if (fn) builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3)),ga(4)}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "checkbox" && e.args.size() >= 5) { auto* fn = get_runtime_fn("slua_ui_checkbox"); if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ga(3),ci32(ga(4))}, "uchk"), i64); }
                if (meth == "slider"   && e.args.size() >= 7) { auto* fn = get_runtime_fn("slua_ui_slider");   if (fn) return builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3)),cf64(ga(4)),cf64(ga(5)),cf64(ga(6))}, "usldr"); }
                if (meth == "progress_bar" && e.args.size() >= 6) { auto* fn = get_runtime_fn("slua_ui_progress_bar"); if (fn) builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3)),cf64(ga(4)),cf64(ga(5))}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "panel"    && e.args.size() >= 5) { auto* fn = get_runtime_fn("slua_ui_panel");    if (fn) builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3)),ga(4)}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "text_input" && e.args.size() >= 7) { auto* fn = get_runtime_fn("slua_ui_text_input"); if (fn) return builder_.CreateSExt(builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3)),ga(4),ci32(ga(5)),ci32(ga(6))}, "uti"), i64); }
                if (meth == "set_font_size" && e.args.size() >= 1) { auto* fn = get_runtime_fn("slua_ui_set_font_size"); if (fn) builder_.CreateCall(fn, {ci32(ga(0))}); return llvm::ConstantInt::get(i64, 0); }
                if (meth == "set_accent" && e.args.size() >= 3) { auto* fn = get_runtime_fn("slua_ui_set_accent"); if (fn) builder_.CreateCall(fn, {ci32(ga(0)),ci32(ga(1)),ci32(ga(2))}); return llvm::ConstantInt::get(i64, 0); }
                return llvm::ConstantInt::get(i64, 0);
            }
            if (mod == "fs") {
            auto* i8p = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                (void)i8p; (void)i32;
                auto ga  = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto c64 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i64,0);
                    if(v->getType()->isIntegerTy(64)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSExt(v,i64);
                    if(v->getType()->isDoubleTy()) return builder_.CreateFPToSI(v,i64);
                    return v;
                };
                auto sxi = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="read_all" && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_fs_read_all"); if(fn) return builder_.CreateCall(fn,{ga(0)},"fsra"); }
                if (meth=="listdir"  && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_fs_listdir");  if(fn) return builder_.CreateCall(fn,{ga(0)},"fsld"); }
                if (meth=="exists"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_fs_exists");   if(fn) return sxi(builder_.CreateCall(fn,{ga(0)},"fse")); }
                if (meth=="delete"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_fs_delete");   if(fn) return sxi(builder_.CreateCall(fn,{ga(0)},"fsd")); }
                if (meth=="mkdir"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_fs_mkdir");    if(fn) return sxi(builder_.CreateCall(fn,{ga(0)},"fsmd")); }
                if (meth=="size"     && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_fs_size");     if(fn) return builder_.CreateCall(fn,{ga(0)},"fssz"); }
                if (meth=="write"    && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_fs_write");    if(fn) return sxi(builder_.CreateCall(fn,{ga(0),ga(1)},"fsw")); }
                if (meth=="append"   && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_fs_append");   if(fn) return sxi(builder_.CreateCall(fn,{ga(0),ga(1)},"fsa")); }
                if (meth=="rename"   && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_fs_rename");   if(fn) return sxi(builder_.CreateCall(fn,{ga(0),ga(1)},"fsrn")); }
                if (meth=="copy"     && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_fs_copy");     if(fn) return sxi(builder_.CreateCall(fn,{ga(0),ga(1)},"fscp")); }
                if (meth=="open"     && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_fs_open");     if(fn) return builder_.CreateCall(fn,{ga(0),ga(1)},"fsop"); }
                if (meth=="close"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_fs_close");    if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"fscl")); }
                if (meth=="readline" && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_fs_readline"); if(fn) return builder_.CreateCall(fn,{c64(ga(0))},"fsrl"); }
                if (meth=="writeh"   && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_fs_writeh");   if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ga(1)},"fswh")); }
                if (meth=="flush"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_fs_flush");    if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"fsfl")); }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "random") {
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* f64 = llvm::Type::getDoubleTy(ctx_);
                auto ga   = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto c64  = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i64,0);
                    if(v->getType()->isIntegerTy(64)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSExt(v,i64);
                    return v;
                };
                auto cf64 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantFP::get(f64,0.0);
                    if(v->getType()->isDoubleTy()) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSIToFP(v,f64);
                    return v;
                };
                if (meth=="seed"  && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_random_seed");  if(fn){ builder_.CreateCall(fn,{c64(ga(0))}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="int"   && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_random_int");   if(fn) return builder_.CreateCall(fn,{c64(ga(0)),c64(ga(1))},"rni"); }
                if (meth=="range" && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_random_range"); if(fn) return builder_.CreateCall(fn,{c64(ga(0)),c64(ga(1))},"rnr"); }
                if (meth=="float")                     { auto* fn=get_runtime_fn("slua_random_float"); if(fn) return builder_.CreateCall(fn,{},"rnf"); }
                if (meth=="gauss" && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_random_gauss"); if(fn) return builder_.CreateCall(fn,{cf64(ga(0)),cf64(ga(1))},"rng"); }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "datetime") {
                auto* i8p = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                (void)i8p;
                auto ga  = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto c64 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i64,0);
                    if(v->getType()->isIntegerTy(64)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSExt(v,i64);
                    return v;
                };
                auto sxi = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="now")                         { auto* fn=get_runtime_fn("slua_datetime_now");     if(fn) return builder_.CreateCall(fn,{},"dtnow"); }
                if (meth=="now_str" && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_datetime_now_str");if(fn) return builder_.CreateCall(fn,{ga(0)},"dtns"); }
                if (meth=="format"  && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_datetime_format"); if(fn) return builder_.CreateCall(fn,{c64(ga(0)),ga(1)},"dtfmt"); }
                if (meth=="parse"   && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_datetime_parse");  if(fn) return builder_.CreateCall(fn,{ga(0),ga(1)},"dtprs"); }
                if (meth=="diff"    && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_datetime_diff");   if(fn) return builder_.CreateCall(fn,{c64(ga(0)),c64(ga(1))},"dtdif"); }
                if (meth=="add"     && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_datetime_add");    if(fn) return builder_.CreateCall(fn,{c64(ga(0)),c64(ga(1))},"dtadd"); }
                if (meth=="year"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_datetime_year");   if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"dty")); }
                if (meth=="month"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_datetime_month");  if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"dtmo")); }
                if (meth=="day"     && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_datetime_day");    if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"dtd")); }
                if (meth=="hour"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_datetime_hour");   if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"dth")); }
                if (meth=="minute"  && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_datetime_minute"); if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"dtmi")); }
                if (meth=="second"  && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_datetime_second"); if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"dts")); }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "path") {
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto ga  = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto sxi = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="join"      && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_path_join");      if(fn) return builder_.CreateCall(fn,{ga(0),ga(1)},"pjn"); }
                if (meth=="basename"  && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_path_basename");  if(fn) return builder_.CreateCall(fn,{ga(0)},"pbn"); }
                if (meth=="dirname"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_path_dirname");   if(fn) return builder_.CreateCall(fn,{ga(0)},"pdn"); }
                if (meth=="extension" && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_path_extension"); if(fn) return builder_.CreateCall(fn,{ga(0)},"pex"); }
                if (meth=="stem"      && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_path_stem");      if(fn) return builder_.CreateCall(fn,{ga(0)},"pst"); }
                if (meth=="absolute"  && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_path_absolute");  if(fn) return builder_.CreateCall(fn,{ga(0)},"pab"); }
                if (meth=="normalize" && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_path_normalize"); if(fn) return builder_.CreateCall(fn,{ga(0)},"pnm"); }
                if (meth=="exists"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_path_exists");    if(fn) return sxi(builder_.CreateCall(fn,{ga(0)},"pe")); }
                if (meth=="is_file"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_path_is_file");   if(fn) return sxi(builder_.CreateCall(fn,{ga(0)},"pif")); }
                if (meth=="is_dir"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_path_is_dir");    if(fn) return sxi(builder_.CreateCall(fn,{ga(0)},"pid")); }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "process") {
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto ga  = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto c64 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i64,0);
                    if(v->getType()->isIntegerTy(64)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSExt(v,i64);
                    return v;
                };
                auto sxi = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="run"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_process_run");    if(fn) return builder_.CreateCall(fn,{ga(0)},"prun"); }
                if (meth=="output" && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_process_output"); if(fn) return builder_.CreateCall(fn,{ga(0)},"pout"); }
                if (meth=="spawn"  && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_process_spawn");  if(fn) return builder_.CreateCall(fn,{ga(0)},"pspn"); }
                if (meth=="wait"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_process_wait");   if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"pwt")); }
                if (meth=="kill"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_process_kill");   if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"pkl")); }
                if (meth=="alive"  && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_process_alive");  if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"pal")); }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "json") {
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* f64 = llvm::Type::getDoubleTy(ctx_);
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                auto ga   = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto c64  = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i64,0);
                    if(v->getType()->isIntegerTy(64)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSExt(v,i64);
                    return v;
                };
                auto cf64 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantFP::get(f64,0.0);
                    if(v->getType()->isDoubleTy()) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSIToFP(v,f64);
                    return v;
                };
                auto ci32 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i32,0);
                    if(v->getType()->isIntegerTy(32)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateTrunc(v,i32);
                    return v;
                };
                auto sxi  = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="encode_str"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_json_encode_str");     if(fn) return builder_.CreateCall(fn,{ga(0)},"jes"); }
                if (meth=="encode_int"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_json_encode_int");     if(fn) return builder_.CreateCall(fn,{c64(ga(0))},"jei"); }
                if (meth=="encode_float"  && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_json_encode_float");   if(fn) return builder_.CreateCall(fn,{cf64(ga(0))},"jef"); }
                if (meth=="encode_bool"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_json_encode_bool");    if(fn) return builder_.CreateCall(fn,{ci32(ga(0))},"jeb"); }
                if (meth=="encode_null")                        { auto* fn=get_runtime_fn("slua_json_encode_null");    if(fn) return builder_.CreateCall(fn,{},"jen"); }
                if (meth=="get_str"       && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_json_get_str");        if(fn) return builder_.CreateCall(fn,{ga(0),ga(1)},"jgs"); }
                if (meth=="get_int"       && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_json_get_int");        if(fn) return builder_.CreateCall(fn,{ga(0),ga(1)},"jgi"); }
                if (meth=="get_float"     && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_json_get_float");      if(fn) return builder_.CreateCall(fn,{ga(0),ga(1)},"jgf"); }
                if (meth=="get_bool"      && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_json_get_bool");       if(fn) return sxi(builder_.CreateCall(fn,{ga(0),ga(1)},"jgb")); }
                if (meth=="has_key"       && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_json_has_key");        if(fn) return sxi(builder_.CreateCall(fn,{ga(0),ga(1)},"jhk")); }
                if (meth=="minify"        && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_json_minify");         if(fn) return builder_.CreateCall(fn,{ga(0)},"jmn"); }
                if (meth=="get_array_item"&& e.args.size()>=3) { auto* fn=get_runtime_fn("slua_json_get_array_item"); if(fn) return builder_.CreateCall(fn,{ga(0),ga(1),ci32(ga(2))},"jai"); }
                if (meth=="get_nested_float"&& e.args.size()>=3) { auto* fn=get_runtime_fn("slua_json_get_nested_float"); if(fn) return builder_.CreateCall(fn,{ga(0),ga(1),ga(2)},"jgnf"); }
                if (meth=="get_nested_int"  && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_json_get_nested_int");   if(fn) return builder_.CreateCall(fn,{ga(0),ga(1),ga(2)},"jgni"); }
                if (meth=="get_nested_str"  && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_json_get_nested_str");   if(fn) return builder_.CreateCall(fn,{ga(0),ga(1),ga(2)},"jgns"); }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "net") {
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                auto ga   = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto c64  = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i64,0);
                    if(v->getType()->isIntegerTy(64)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSExt(v,i64);
                    return v;
                };
                auto ci32 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i32,0);
                    if(v->getType()->isIntegerTy(32)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateTrunc(v,i32);
                    return v;
                };
                auto sxi  = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="init")                         { auto* fn=get_runtime_fn("slua_net_init");        if(fn) return sxi(builder_.CreateCall(fn,{},"nini")); }
                if (meth=="local_ip")                     { auto* fn=get_runtime_fn("slua_net_local_ip");    if(fn) return builder_.CreateCall(fn,{},"nip"); }
                if (meth=="listen"    && e.args.size()>=1){ auto* fn=get_runtime_fn("slua_net_listen");      if(fn) return builder_.CreateCall(fn,{ci32(ga(0))},"nls"); }
                if (meth=="accept"    && e.args.size()>=1){ auto* fn=get_runtime_fn("slua_net_accept");      if(fn) return builder_.CreateCall(fn,{c64(ga(0))},"nac"); }
                if (meth=="connect"   && e.args.size()>=2){ auto* fn=get_runtime_fn("slua_net_connect");     if(fn) return builder_.CreateCall(fn,{ga(0),ci32(ga(1))},"nco"); }
                if (meth=="send"      && e.args.size()>=2){ auto* fn=get_runtime_fn("slua_net_send");        if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ga(1)},"nsnd")); }
                if (meth=="send_bytes"&& e.args.size()>=3){ auto* fn=get_runtime_fn("slua_net_send_bytes");  if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ga(1),ci32(ga(2))},"nsb")); }
                if (meth=="recv"      && e.args.size()>=2){ auto* fn=get_runtime_fn("slua_net_recv");        if(fn) return builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1))},"nrcv"); }
                if (meth=="close"     && e.args.size()>=1){ auto* fn=get_runtime_fn("slua_net_close");       if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"ncl")); }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "sync") {
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto ga  = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto c64 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i64,0);
                    if(v->getType()->isIntegerTy(64)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSExt(v,i64);
                    return v;
                };
                auto sxi = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="mutex_new")                     { auto* fn=get_runtime_fn("slua_sync_mutex_new");     if(fn) return builder_.CreateCall(fn,{},"smn"); }
                if (meth=="lock"      && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_sync_mutex_lock");    if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"sml")); }
                if (meth=="unlock"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_sync_mutex_unlock");  if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"smu")); }
                if (meth=="trylock"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_sync_mutex_trylock"); if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"smt")); }
                if (meth=="free"      && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_sync_mutex_free");    if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"smf")); }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "http") {
                auto* i8p = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                (void)i8p;
                auto ga  = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto sxi = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="get"       && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_http_get");       if(fn) return builder_.CreateCall(fn,{ga(0)},"hget"); }
                if (meth=="post"      && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_http_post");      if(fn) return builder_.CreateCall(fn,{ga(0),ga(1),ga(2)},"hpost"); }
                if (meth=="post_json" && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_http_post_json"); if(fn) return builder_.CreateCall(fn,{ga(0),ga(1)},"hpj"); }
                if (meth=="status"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_http_status");    if(fn) return sxi(builder_.CreateCall(fn,{ga(0)},"hst")); }
                return llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_)));
            }
            if (mod == "table") {
                auto* i8p = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* f64 = llvm::Type::getDoubleTy(ctx_);
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                (void)i8p;
                auto ga   = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto c64  = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i64,0);
                    if(v->getType()->isIntegerTy(64)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSExt(v,i64);
                    return v;
                };
                auto ci32 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i32,0);
                    if(v->getType()->isIntegerTy(32)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateTrunc(v,i32);
                    if(v->getType()->isDoubleTy()) return builder_.CreateFPToSI(v,i32);
                    return v;
                };
                auto cf64 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantFP::get(f64,0.0);
                    if(v->getType()->isDoubleTy()) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSIToFP(v,f64);
                    return v;
                };
                auto sxi  = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="len"        && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_tbl_len_rt");    if(fn) return sxi(builder_.CreateCall(fn,{ga(0)},"tlen")); }
                if (meth=="push"       && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_tbl_push");      if(fn){ builder_.CreateCall(fn,{ga(0),c64(ga(1))}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="push_float" && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_tbl_push_f");    if(fn){ builder_.CreateCall(fn,{ga(0),cf64(ga(1))}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="push_str"   && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_tbl_push_s");    if(fn){ builder_.CreateCall(fn,{ga(0),ga(1)}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="pop"        && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_tbl_pop");       if(fn){ builder_.CreateCall(fn,{ga(0)}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="contains_str"&&e.args.size()>=2) { auto* fn=get_runtime_fn("slua_tbl_contains_s");if(fn) return sxi(builder_.CreateCall(fn,{ga(0),ga(1)},"tcs")); }
                if (meth=="contains_int"&&e.args.size()>=2) { auto* fn=get_runtime_fn("slua_tbl_contains_i");if(fn) return sxi(builder_.CreateCall(fn,{ga(0),c64(ga(1))},"tci")); }
                if (meth=="keys"       && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_tbl_keys");      if(fn) return builder_.CreateCall(fn,{ga(0)},"tkeys"); }
                if (meth=="remove_at"  && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_tbl_remove_at"); if(fn){ builder_.CreateCall(fn,{ga(0),ci32(ga(1))}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="clear"      && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_tbl_clear");     if(fn){ builder_.CreateCall(fn,{ga(0)}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="merge"      && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_tbl_merge");     if(fn) return builder_.CreateCall(fn,{ga(0),ga(1)},"tmerge"); }
                if (meth=="slice"      && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_tbl_slice");     if(fn) return builder_.CreateCall(fn,{ga(0),ci32(ga(1)),ci32(ga(2))},"tslice"); }
                if (meth=="reverse"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_tbl_reverse");   if(fn){ builder_.CreateCall(fn,{ga(0)}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="new")                             { auto* fn=get_runtime_fn("slua_tbl_new");       if(fn) return builder_.CreateCall(fn,{},"tnew"); }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "crypto") {
                auto* i8p = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                (void)i8p;
                auto ga   = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto ci32 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i32,0);
                    if(v->getType()->isIntegerTy(32)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateTrunc(v,i32);
                    return v;
                };
                auto sxi  = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="sha256"        && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_crypto_sha256");        if(fn) return builder_.CreateCall(fn,{ga(0)},"csha"); }
                if (meth=="md5"           && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_crypto_md5");           if(fn) return builder_.CreateCall(fn,{ga(0)},"cmd5"); }
                if (meth=="base64_encode" && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_crypto_base64_encode"); if(fn) return builder_.CreateCall(fn,{ga(0),ci32(ga(1))},"cb64e"); }
                if (meth=="base64_decode" && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_crypto_base64_decode"); if(fn) return builder_.CreateCall(fn,{ga(0)},"cb64d"); }
                if (meth=="hex_encode"    && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_crypto_hex_encode");    if(fn) return builder_.CreateCall(fn,{ga(0),ci32(ga(1))},"chxe"); }
                if (meth=="hex_decode"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_crypto_hex_decode");    if(fn) return builder_.CreateCall(fn,{ga(0)},"chxd"); }
                if (meth=="hex_decode_len"&& e.args.size()>=1) { auto* fn=get_runtime_fn("slua_crypto_hex_decode_len");if(fn) return sxi(builder_.CreateCall(fn,{ga(0)},"chxdl")); }
                if (meth=="crc32"         && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_crypto_crc32");         if(fn) return sxi(builder_.CreateCall(fn,{ga(0),ci32(ga(1))},"ccrc")); }
                if (meth=="hmac_sha256"   && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_crypto_hmac_sha256");   if(fn) return builder_.CreateCall(fn,{ga(0),ga(1)},"chmac"); }
                if (meth=="xor"           && e.args.size()>=4) { auto* fn=get_runtime_fn("slua_crypto_xor");           if(fn) return builder_.CreateCall(fn,{ga(0),ci32(ga(1)),ga(2),ci32(ga(3))},"cxor"); }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "buf") {
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* f64 = llvm::Type::getDoubleTy(ctx_);
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                auto ga   = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto c64  = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i64,0);
                    if(v->getType()->isIntegerTy(64)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSExt(v,i64);
                    return v;
                };
                auto ci32 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i32,0);
                    if(v->getType()->isIntegerTy(32)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateTrunc(v,i32);
                    return v;
                };
                auto cf64 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantFP::get(f64,0.0);
                    if(v->getType()->isDoubleTy()) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSIToFP(v,f64);
                    return v;
                };
                auto sxi  = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="new"       && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_buf_new");       if(fn) return builder_.CreateCall(fn,{ci32(ga(0))},"bfn"); }
                if (meth=="from_str"  && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_buf_from_str");  if(fn) return builder_.CreateCall(fn,{ga(0),ci32(ga(1))},"bffs"); }
                if (meth=="free"      && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_buf_free");      if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"bff")); }
                if (meth=="size"      && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_buf_size");      if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"bfsz")); }
                if (meth=="to_str"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_buf_to_str");    if(fn) return builder_.CreateCall(fn,{c64(ga(0))},"bfts"); }
                if (meth=="to_hex"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_buf_to_hex");    if(fn) return builder_.CreateCall(fn,{c64(ga(0))},"bfth"); }
                if (meth=="fill"      && e.args.size()>=4) { auto* fn=get_runtime_fn("slua_buf_fill");      if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3))},"bffl")); }
                if (meth=="write_str" && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_buf_write_str"); if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1)),ga(2)},"bfws")); }
                if (meth=="write_u8"  && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_buf_write_u8");  if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1)),ci32(ga(2))},"bfw8")); }
                if (meth=="write_u16" && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_buf_write_u16"); if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1)),ci32(ga(2))},"bfw16")); }
                if (meth=="write_u32" && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_buf_write_u32"); if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1)),ci32(ga(2))},"bfw32")); }
                if (meth=="write_i64" && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_buf_write_i64"); if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1)),c64(ga(2))},"bfw64")); }
                if (meth=="write_f32" && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_buf_write_f32"); if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1)),cf64(ga(2))},"bfwf32")); }
                if (meth=="write_f64" && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_buf_write_f64"); if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1)),cf64(ga(2))},"bfwf64")); }
                if (meth=="read_u8"   && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_buf_read_u8");   if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1))},"bfr8")); }
                if (meth=="read_u16"  && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_buf_read_u16");  if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1))},"bfr16")); }
                if (meth=="read_u32"  && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_buf_read_u32_i");if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1))},"bfr32")); }
                if (meth=="read_i64"  && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_buf_read_i64");  if(fn) return builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1))},"bfr64"); }
                if (meth=="read_f32"  && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_buf_read_f32");  if(fn) return builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1))},"bfrf32"); }
                if (meth=="read_f64"  && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_buf_read_f64");  if(fn) return builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1))},"bfrf64"); }
                if (meth=="copy"      && e.args.size()>=5) { auto* fn=get_runtime_fn("slua_buf_copy");      if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0)),ci32(ga(1)),c64(ga(2)),ci32(ga(3)),ci32(ga(4))},"bfcp")); }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "thread") {
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto ga  = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto c64 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i64,0);
                    if(v->getType()->isIntegerTy(64)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSExt(v,i64);
                    return v;
                };
                auto sxi = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="join"     && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_thread_join");     if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"thj")); }
                if (meth=="detach"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_thread_detach");   if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"thd")); }
                if (meth=="alive"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_thread_alive");    if(fn) return sxi(builder_.CreateCall(fn,{c64(ga(0))},"tha")); }
                if (meth=="sleep_ms" && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_thread_sleep_ms"); if(fn){ builder_.CreateCall(fn,{c64(ga(0))}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="self_id")                      { auto* fn=get_runtime_fn("slua_thread_self_id");  if(fn) return builder_.CreateCall(fn,{},"thid"); }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "vec") {
                auto* f64 = llvm::Type::getDoubleTy(ctx_);
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto ga   = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto cf64 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantFP::get(f64,0.0);
                    if(v->getType()->isDoubleTy()) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSIToFP(v,f64);
                    return v;
                };
                auto g64 = [&](size_t n) -> llvm::Value* { return cf64(ga(n)); };
                if (meth=="v2_dot"    && e.args.size()>=4) { auto* fn=get_runtime_fn("slua_vec2_dot");    if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2),g64(3)},"v2d"); }
                if (meth=="v2_len"    && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_vec2_len");    if(fn) return builder_.CreateCall(fn,{g64(0),g64(1)},"v2l"); }
                if (meth=="v2_dist"   && e.args.size()>=4) { auto* fn=get_runtime_fn("slua_vec2_dist");   if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2),g64(3)},"v2di"); }
                if (meth=="v2_norm_x" && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_vec2_norm_x"); if(fn) return builder_.CreateCall(fn,{g64(0),g64(1)},"v2nx"); }
                if (meth=="v2_norm_y" && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_vec2_norm_y"); if(fn) return builder_.CreateCall(fn,{g64(0),g64(1)},"v2ny"); }
                if (meth=="v3_dot"    && e.args.size()>=6) { auto* fn=get_runtime_fn("slua_vec3_dot");    if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2),g64(3),g64(4),g64(5)},"v3d"); }
                if (meth=="v3_len"    && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_vec3_len");    if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2)},"v3l"); }
                if (meth=="v3_dist"   && e.args.size()>=6) { auto* fn=get_runtime_fn("slua_vec3_dist");   if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2),g64(3),g64(4),g64(5)},"v3di"); }
                if (meth=="v3_norm_x" && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_vec3_norm_x"); if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2)},"v3nx"); }
                if (meth=="v3_norm_y" && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_vec3_norm_y"); if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2)},"v3ny"); }
                if (meth=="v3_norm_z" && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_vec3_norm_z"); if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2)},"v3nz"); }
                if (meth=="v3_cross_x"&& e.args.size()>=6) { auto* fn=get_runtime_fn("slua_vec3_cross_x");if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2),g64(3),g64(4),g64(5)},"v3cx"); }
                if (meth=="v3_cross_y"&& e.args.size()>=6) { auto* fn=get_runtime_fn("slua_vec3_cross_y");if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2),g64(3),g64(4),g64(5)},"v3cy"); }
                if (meth=="v3_cross_z"&& e.args.size()>=6) { auto* fn=get_runtime_fn("slua_vec3_cross_z");if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2),g64(3),g64(4),g64(5)},"v3cz"); }
                if (meth=="clamp"     && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_math_clamp");  if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2)},"mclamp"); }
                if (meth=="lerp"      && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_math_lerp");   if(fn) return builder_.CreateCall(fn,{g64(0),g64(1),g64(2)},"mlerp"); }
                if (meth=="abs"       && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_math_abs");    if(fn) return builder_.CreateCall(fn,{g64(0)},"mabs"); }
                if (meth=="floor"     && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_math_floor");  if(fn) return builder_.CreateCall(fn,{g64(0)},"mflr"); }
                if (meth=="ceil"      && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_math_ceil");   if(fn) return builder_.CreateCall(fn,{g64(0)},"mcel"); }
                if (meth=="round"     && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_math_round");  if(fn) return builder_.CreateCall(fn,{g64(0)},"mrnd"); }
                if (meth=="min"       && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_math_min2");   if(fn) return builder_.CreateCall(fn,{g64(0),g64(1)},"mmin"); }
                if (meth=="max"       && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_math_max2");   if(fn) return builder_.CreateCall(fn,{g64(0),g64(1)},"mmax"); }
                if (meth=="sign"      && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_math_sign");   if(fn) return builder_.CreateCall(fn,{g64(0)},"msgn"); }
                if (meth=="fract"     && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_math_fract");  if(fn) return builder_.CreateCall(fn,{g64(0)},"mfrc"); }
                if (meth=="mod"       && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_math_mod");    if(fn) return builder_.CreateCall(fn,{g64(0),g64(1)},"mmod"); }
                return llvm::ConstantFP::get(f64,0.0);
            }
            if (mod == "scene") {
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                auto* f64 = llvm::Type::getDoubleTy(ctx_);
                auto* f32t = llvm::Type::getFloatTy(ctx_);
                auto ga    = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto ci32  = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i32,0);
                    if(v->getType()->isIntegerTy(32)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateTrunc(v,i32);
                    if(v->getType()->isDoubleTy()) return builder_.CreateFPToSI(v,i32);
                    return v;
                };
                auto cf32  = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantFP::get(f32t,0.0f);
                    if(v->getType()->isFloatTy()) return v;
                    if(v->getType()->isDoubleTy()) return builder_.CreateFPTrunc(v,f32t);
                    if(v->getType()->isIntegerTy()) return builder_.CreateSIToFP(v,f32t);
                    return v;
                };
                auto cf64  = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantFP::get(f64,0.0);
                    if(v->getType()->isDoubleTy()) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateSIToFP(v,f64);
                    return v;
                };
                auto sxi   = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="camera_set" && e.args.size()>=11) {
                    auto* fn=get_runtime_fn("slua_camera3d_set");
                    if(fn) { builder_.CreateCall(fn,{cf64(ga(0)),cf64(ga(1)),cf64(ga(2)),cf64(ga(3)),cf64(ga(4)),cf64(ga(5)),cf64(ga(6)),cf64(ga(7)),cf64(ga(8)),cf64(ga(9)),ci32(ga(10))}); return llvm::ConstantInt::get(i64,0); }
                }
                if (meth=="camera_update") { auto* fn=get_runtime_fn("slua_camera3d_update"); if(fn) builder_.CreateCall(fn,{}); return llvm::ConstantInt::get(i64,0); }
                if (meth=="begin")         { auto* fn=get_runtime_fn("slua_begin_mode3d");    if(fn) builder_.CreateCall(fn,{}); return llvm::ConstantInt::get(i64,0); }
                if (meth=="end")           { auto* fn=get_runtime_fn("slua_end_mode3d");      if(fn) builder_.CreateCall(fn,{}); return llvm::ConstantInt::get(i64,0); }
                if (meth=="grid" && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_draw_grid"); if(fn) builder_.CreateCall(fn,{ci32(ga(0)),cf32(ga(1))}); return llvm::ConstantInt::get(i64,0); }
                if (meth=="cube" && e.args.size()>=10){ auto* fn=get_runtime_fn("slua_draw_cube"); if(fn) builder_.CreateCall(fn,{cf32(ga(0)),cf32(ga(1)),cf32(ga(2)),cf32(ga(3)),cf32(ga(4)),cf32(ga(5)),ci32(ga(6)),ci32(ga(7)),ci32(ga(8)),ci32(ga(9))}); return llvm::ConstantInt::get(i64,0); }
                if (meth=="cube_wires" && e.args.size()>=10){ auto* fn=get_runtime_fn("slua_draw_cube_wires"); if(fn) builder_.CreateCall(fn,{cf32(ga(0)),cf32(ga(1)),cf32(ga(2)),cf32(ga(3)),cf32(ga(4)),cf32(ga(5)),ci32(ga(6)),ci32(ga(7)),ci32(ga(8)),ci32(ga(9))}); return llvm::ConstantInt::get(i64,0); }
                if (meth=="sphere" && e.args.size()>=8){ auto* fn=get_runtime_fn("slua_draw_sphere"); if(fn) builder_.CreateCall(fn,{cf32(ga(0)),cf32(ga(1)),cf32(ga(2)),cf32(ga(3)),ci32(ga(4)),ci32(ga(5)),ci32(ga(6)),ci32(ga(7))}); return llvm::ConstantInt::get(i64,0); }
                if (meth=="sphere_wires" && e.args.size()>=10){ auto* fn=get_runtime_fn("slua_draw_sphere_wires"); if(fn) builder_.CreateCall(fn,{cf32(ga(0)),cf32(ga(1)),cf32(ga(2)),cf32(ga(3)),ci32(ga(4)),ci32(ga(5)),ci32(ga(6)),ci32(ga(7)),ci32(ga(8)),ci32(ga(9))}); return llvm::ConstantInt::get(i64,0); }
                if (meth=="plane" && e.args.size()>=9){ auto* fn=get_runtime_fn("slua_draw_plane"); if(fn) builder_.CreateCall(fn,{cf32(ga(0)),cf32(ga(1)),cf32(ga(2)),cf32(ga(3)),cf32(ga(4)),ci32(ga(5)),ci32(ga(6)),ci32(ga(7)),ci32(ga(8))}); return llvm::ConstantInt::get(i64,0); }
                if (meth=="line3d" && e.args.size()>=10){ auto* fn=get_runtime_fn("slua_draw_line3d"); if(fn) builder_.CreateCall(fn,{cf32(ga(0)),cf32(ga(1)),cf32(ga(2)),cf32(ga(3)),cf32(ga(4)),cf32(ga(5)),ci32(ga(6)),ci32(ga(7)),ci32(ga(8)),ci32(ga(9))}); return llvm::ConstantInt::get(i64,0); }
                if (meth=="model_load"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_model_load");   if(fn) return sxi(builder_.CreateCall(fn,{ga(0)},"mdll")); }
                if (meth=="model_unload" && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_model_unload"); if(fn){ builder_.CreateCall(fn,{ci32(ga(0))}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="model_draw"   && e.args.size()>=9) { auto* fn=get_runtime_fn("slua_model_draw");   if(fn){ builder_.CreateCall(fn,{ci32(ga(0)),cf32(ga(1)),cf32(ga(2)),cf32(ga(3)),cf32(ga(4)),ci32(ga(5)),ci32(ga(6)),ci32(ga(7)),ci32(ga(8))}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="tex_load"     && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_texture_load");   if(fn) return sxi(builder_.CreateCall(fn,{ga(0)},"texl")); }
                if (meth=="tex_unload"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_texture_unload"); if(fn){ builder_.CreateCall(fn,{ci32(ga(0))}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="tex_draw"     && e.args.size()>=7) { auto* fn=get_runtime_fn("slua_texture_draw");   if(fn){ builder_.CreateCall(fn,{ci32(ga(0)),ci32(ga(1)),ci32(ga(2)),ci32(ga(3)),ci32(ga(4)),ci32(ga(5)),ci32(ga(6))}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="tex_width"    && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_texture_width");  if(fn) return sxi(builder_.CreateCall(fn,{ci32(ga(0))},"texw")); }
                if (meth=="tex_height"   && e.args.size()>=1) { auto* fn=get_runtime_fn("slua_texture_height"); if(fn) return sxi(builder_.CreateCall(fn,{ci32(ga(0))},"texh")); }
                if (meth=="fps_counter"  && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_draw_fps_counter"); if(fn){ builder_.CreateCall(fn,{ci32(ga(0)),ci32(ga(1))}); return llvm::ConstantInt::get(i64,0); } }
                if (meth=="time")                              { auto* fn=get_runtime_fn("slua_get_time"); if(fn) return builder_.CreateCall(fn,{},"gtime"); }
                if (meth=="window_init_3d" && e.args.size()>=3){ auto* fn=get_runtime_fn("slua_window_init_3d"); if(fn){ builder_.CreateCall(fn,{ci32(ga(0)),ci32(ga(1)),ga(2)}); return llvm::ConstantInt::get(i64,0); } }
                return llvm::ConstantInt::get(i64,0);
            }
            if (mod == "regex") {
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                auto ga   = [&](size_t n) -> llvm::Value* { return e.args.size()>n ? emit_expr(*e.args[n]) : nullptr; };
                auto ci32 = [&](llvm::Value* v) -> llvm::Value* {
                    if(!v) return llvm::ConstantInt::get(i32,0);
                    if(v->getType()->isIntegerTy(32)) return v;
                    if(v->getType()->isIntegerTy()) return builder_.CreateTrunc(v,i32);
                    return v;
                };
                auto sxi  = [&](llvm::Value* v) -> llvm::Value* { return builder_.CreateSExt(v,i64); };
                if (meth=="match"    && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_regex_match");    if(fn) return sxi(builder_.CreateCall(fn,{ga(0),ga(1)},"rxm")); }
                if (meth=="find"     && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_regex_find");     if(fn) return sxi(builder_.CreateCall(fn,{ga(0),ga(1),ci32(ga(2))},"rxf")); }
                if (meth=="replace"  && e.args.size()>=3) { auto* fn=get_runtime_fn("slua_regex_replace");  if(fn) return builder_.CreateCall(fn,{ga(0),ga(1),ga(2)},"rxr"); }
                if (meth=="groups"   && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_regex_groups");   if(fn) return builder_.CreateCall(fn,{ga(0),ga(1)},"rxg"); }
                if (meth=="count"    && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_regex_count");    if(fn) return sxi(builder_.CreateCall(fn,{ga(0),ga(1)},"rxc")); }
                if (meth=="find_all" && e.args.size()>=2) { auto* fn=get_runtime_fn("slua_regex_find_all"); if(fn) return builder_.CreateCall(fn,{ga(0),ga(1)},"rxfa"); }
                return llvm::ConstantInt::get(i64,0);
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
                auto* fn  = get_runtime_fn("slua_print_bool");
                auto* i32 = llvm::Type::getInt32Ty(ctx_);
                if (fn) builder_.CreateCall(fn, {builder_.CreateZExt(arg, i32)});
            } else if (ty->isIntegerTy()) {
                auto* fn  = get_runtime_fn("slua_print_int");
                auto* i64 = llvm::Type::getInt64Ty(ctx_);
                if (fn) builder_.CreateCall(fn, {builder_.CreateSExt(arg, i64)});
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

    return builder_.CreateCall(fn, args);
}

llvm::Value* IREmitter::emit_method_call(MethodCall& e, SourceLoc loc) {
    std::string type_name;
    if (auto* id = std::get_if<Ident>(&e.obj->v)) {
        llvm::Value* slot = env_ ? env_->lookup(id->name) : nullptr;
        if (auto* ai = llvm::dyn_cast_or_null<llvm::AllocaInst>(slot)) {
            if (auto* st = llvm::dyn_cast<llvm::StructType>(ai->getAllocatedType())) {
                if (!st->getName().empty())
                    type_name = st->getName().str();
            }
        }
    }

    llvm::Function* fn = nullptr;
    if (!type_name.empty()) {
        auto it = functions_.find(type_name + "." + e.method);
        if (it != functions_.end()) fn = it->second;
    }

    if (!fn) {
        if (auto* id2 = std::get_if<Ident>(&e.obj->v)) {
            for (auto& [sn, _] : struct_fields_) {
                auto fit2 = functions_.find(sn + "." + e.method);
                if (fit2 != functions_.end()) { fn = fit2->second; break; }
            }
        }
    }

    llvm::Value* self = nullptr;
    if (std::holds_alternative<Call>(e.obj->v) || std::holds_alternative<MethodCall>(e.obj->v)) {
        llvm::Value* tmp = emit_expr(*e.obj);
        if (tmp && llvm::isa<llvm::StructType>(tmp->getType())) {
            auto* st = llvm::cast<llvm::StructType>(tmp->getType());
            auto* slot = create_alloca(st, "_mc_tmp");
            builder_.CreateStore(tmp, slot);
            self = slot;
        } else { self = tmp; }
    } else { self = emit_expr(*e.obj); }

    if (!fn) {
        for (auto& arg : e.args) emit_expr(*arg);
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
    }

    std::vector<llvm::Value*> call_args;
    auto param_tys = fn->getFunctionType()->params().vec();

    if (!param_tys.empty() && self)
        self = coerce(self, param_tys[0], loc);
    call_args.push_back(self);

    for (size_t i = 0; i < e.args.size(); i++) {
        llvm::Value* arg = emit_expr(*e.args[i]);
        if (!arg) return nullptr;
        if (i + 1 < param_tys.size())
            arg = coerce(arg, param_tys[i + 1], loc);
        call_args.push_back(arg);
    }

    return builder_.CreateCall(fn, call_args);
}

llvm::Value* IREmitter::emit_field(Field& e, SourceLoc loc) {
    llvm::Value* obj = emit_lvalue(*e.table);
    if (!obj) return nullptr;

    llvm::Type* struct_ty = nullptr;
    std::string sname;
    if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(obj)) {
        struct_ty = ai->getAllocatedType();
    } else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(obj)) {
        struct_ty = gv->getValueType();
    }

    if (!struct_ty || !struct_ty->isStructTy())
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);

    auto* st = llvm::cast<llvm::StructType>(struct_ty);
    sname = st->getName().str();

    unsigned field_idx = 0;
    auto fit = struct_fields_.find(sname);
    if (fit != struct_fields_.end()) {
        for (unsigned i = 0; i < fit->second.size(); i++) {
            if (fit->second[i] == e.name) { field_idx = i; break; }
        }
    }

    if (field_idx >= st->getNumElements())
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);

    auto* gep = builder_.CreateStructGEP(struct_ty, obj, field_idx, e.name);
    return builder_.CreateLoad(st->getElementType(field_idx), gep, e.name);
}

llvm::Value* IREmitter::emit_index(Index& e, SourceLoc loc, TypeNode* result_type) {
    llvm::Value* base = emit_expr(*e.table);
    llvm::Value* key  = emit_expr(*e.key);
    if (!base || !key) return nullptr;

    auto* i64 = llvm::Type::getInt64Ty(ctx_);

    std::string inferred;
    if (result_type) {
        if (auto* p = std::get_if<PrimitiveType>(&result_type->v))
            inferred = p->name;
    }

    bool key_is_str = key->getType()->isPointerTy();
    std::string pfx = key_is_str ? "slua_tbl_sget" : "slua_tbl_iget";

    if (!key_is_str)
        key = builder_.CreateSExt(key, i64);

    std::string suffix = "_i64";
    if (inferred == "number") suffix = "_f64";
    else if (inferred == "string") suffix = "_str";
    else if (inferred == "bool")   suffix = "_bool";

    auto* fn = get_runtime_fn(pfx + suffix);
    if (!fn) return llvm::ConstantInt::get(i64, 0);
    return builder_.CreateCall(fn, {base, key}, "tget");
}

llvm::Value* IREmitter::emit_table_ctor(TableCtor& e, SourceLoc loc) {
    auto* i8p = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
    auto* i64 = llvm::Type::getInt64Ty(ctx_);
    auto* i32 = llvm::Type::getInt32Ty(ctx_);

    auto* new_fn = get_runtime_fn("slua_tbl_new");
    if (!new_fn) return llvm::ConstantPointerNull::get(i8p);
    llvm::Value* tbl = builder_.CreateCall(new_fn, {}, "tbl");

    int64_t auto_idx = 1;
    for (auto& entry : e.entries) {
        llvm::Value* val = emit_expr(*entry.val);
        if (!val) { auto_idx++; continue; }

        llvm::Value* key_val    = nullptr;
        bool         key_is_str = false;

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
                auto* fn  = get_runtime_fn(pfx + "_bool");
                auto* ext = builder_.CreateZExt(val, i32);
                if (fn) builder_.CreateCall(fn, {tbl, key_val, ext});
            } else if (vty->isPointerTy()) {
                auto* fn = get_runtime_fn(pfx + "_str");
                if (fn) builder_.CreateCall(fn, {tbl, key_val, val});
            } else {
                auto* fn  = get_runtime_fn(pfx + "_i64");
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
    auto*    dl      = &mod_->getDataLayout();
    uint64_t elem_sz = dl->getTypeAllocSize(elem_ty);

    auto* i64 = llvm::Type::getInt64Ty(ctx_);
    count     = coerce(count, i64, loc);
    auto* sz  = builder_.CreateMul(count, llvm::ConstantInt::get(i64, elem_sz), "alloc.sz");

    auto* fn = get_runtime_fn("slua_alloc");
    if (!fn) return nullptr;
    llvm::Value* raw = builder_.CreateCall(fn, {sz}, "raw_ptr");

    auto* dst_ty = llvm::PointerType::getUnqual(elem_ty);
    return builder_.CreateBitCast(raw, dst_ty, "typed_ptr");
}

llvm::Value* IREmitter::emit_deref_expr(DerefExpr& e, SourceLoc loc) {
    llvm::Value* ptr = emit_expr(*e.ptr);
    if (!ptr || !ptr->getType()->isPointerTy()) return nullptr;
    return builder_.CreateLoad(llvm::Type::getInt8Ty(ctx_), ptr, "deref");
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
    if (auto* id = std::get_if<Ident>(&e.v))
        return env_ ? env_->lookup(id->name) : nullptr;

    if (auto* fi = std::get_if<Field>(&e.v)) {
        llvm::Value* obj = emit_lvalue(*fi->table);
        if (!obj) return nullptr;
        llvm::Type* sty = nullptr;
        if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(obj))
            sty = ai->getAllocatedType();
        if (!sty || !sty->isStructTy()) return obj;
        auto* st = llvm::cast<llvm::StructType>(sty);
        unsigned idx = 0;
        auto fit = struct_fields_.find(st->getName().str());
        if (fit != struct_fields_.end()) {
            for (unsigned i = 0; i < fit->second.size(); i++)
                if (fit->second[i] == fi->name) { idx = i; break; }
        }
        return builder_.CreateStructGEP(sty, obj, idx, fi->name);
    }

    if (auto* idx = std::get_if<Index>(&e.v)) {
        llvm::Value* base = emit_expr(*idx->table);
        llvm::Value* key  = emit_expr(*idx->key);
        if (!base || !key || !base->getType()->isPointerTy()) return nullptr;
        key = coerce(key, llvm::Type::getInt64Ty(ctx_), {});
        return builder_.CreateGEP(llvm::Type::getInt8Ty(ctx_), base, key);
    }

    if (auto* dr = std::get_if<DerefExpr>(&e.v))
        return emit_expr(*dr->ptr);

    return emit_expr(e);
}

llvm::Value* IREmitter::coerce(llvm::Value* v, llvm::Type* to, SourceLoc) {
    if (!v || !to || v->getType() == to) return v;

    llvm::Type* from = v->getType();

    if (from->isIntegerTy() && to->isDoubleTy())  return builder_.CreateSIToFP(v, to);
    if (from->isDoubleTy()  && to->isIntegerTy()) return builder_.CreateFPToSI(v, to);

    if (from->isIntegerTy() && to->isIntegerTy()) {
        unsigned fb = from->getIntegerBitWidth();
        unsigned tb = to->getIntegerBitWidth();
        if (fb < tb) return builder_.CreateSExt(v, to);
        if (fb > tb) return builder_.CreateTrunc(v, to);
        return v;
    }

    if (from->isPointerTy() && to->isPointerTy())  return builder_.CreateBitCast(v, to);
    if (from->isIntegerTy() && to->isPointerTy())  return builder_.CreateIntToPtr(v, to);
    if (from->isPointerTy() && to->isIntegerTy())  return builder_.CreatePtrToInt(v, to);
    if (from->isIntegerTy(1) && to->isIntegerTy()) return builder_.CreateZExt(v, to);

    return v;
}
void IREmitter::emit_enum_decl(EnumDecl& s, SourceLoc loc) {
    // Processed in first pass of emit(); nothing to do here.
}

void IREmitter::emit_multi_local_decl(MultiLocalDecl& s, SourceLoc loc) {
    if (!s.init) return;
    llvm::Value* result = emit_expr(*s.init);
    if (!result) return;

    auto* st = llvm::dyn_cast<llvm::StructType>(result->getType());

    for (size_t i = 0; i < s.vars.size(); i++) {
        auto& [vname, vtype] = s.vars[i];
        llvm::Type* ty = vtype ? llvm_type(vtype.get()) : llvm::Type::getInt64Ty(ctx_);
        llvm::AllocaInst* slot = create_alloca(ty, vname);

        llvm::Value* elem = nullptr;
        if (st && i < st->getNumElements()) {
            elem = builder_.CreateExtractValue(result, {(unsigned)i});
            elem = coerce(elem, ty, loc);
        } else if (i == 0) {
            elem = coerce(result, ty, loc);
        } else {
            elem = llvm::Constant::getNullValue(ty);
        }

        builder_.CreateStore(elem, slot);
        env_->define(vname, slot);
    }
}
}
#endif
