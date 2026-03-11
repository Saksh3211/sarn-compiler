#include "slua/SemanticConfig.h"

namespace slua {

SemanticConfig SemanticConfig::for_mode(CompileMode m) {
    using DB = DiagBehavior;
    SemanticConfig c;
    c.mode = m;

    if (m == CompileMode::STRICT) {
        c.uninitialized_var     = DB::ERROR;
        c.type_annotation_viol  = DB::ERROR;
        c.null_non_nullable     = DB::ERROR;
        c.implicit_any_coerce   = DB::ERROR;
        c.missing_return        = DB::ERROR;
        c.union_unnarrowed_use  = DB::ERROR;
        c.shadowing             = DB::WARNING;
        c.emit_null_propagation = false;
        c.emit_tag_dispatch_warn= false;
        c.allow_implicit_dynamic= false;
        c.mem_mode              = MemoryMode::Manual;
    } else {
        c.uninitialized_var     = DB::WARNING;
        c.type_annotation_viol  = DB::WARNING;
        c.null_non_nullable     = DB::WARNING;
        c.implicit_any_coerce   = DB::SILENT;
        c.missing_return        = DB::WARNING;
        c.union_unnarrowed_use  = DB::WARNING;
        c.shadowing             = DB::SILENT;
        c.emit_null_propagation = true;
        c.emit_tag_dispatch_warn= true;
        c.allow_implicit_dynamic= true;
        c.mem_mode              = MemoryMode::Manual;
    }
    return c;
}

} 

