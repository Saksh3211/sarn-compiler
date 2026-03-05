#pragma once
#include "Lexer.h"

namespace slua {

enum class DiagBehavior { SILENT, WARNING, ERROR };

struct SemanticConfig {
    CompileMode mode;

    DiagBehavior uninitialized_var;
    DiagBehavior type_annotation_viol;
    DiagBehavior null_non_nullable;
    DiagBehavior implicit_any_coerce;
    DiagBehavior missing_return;
    DiagBehavior union_unnarrowed_use;
    DiagBehavior shadowing;

    bool emit_null_propagation;
    bool emit_tag_dispatch_warn;
    bool allow_implicit_dynamic;

    static SemanticConfig for_mode(CompileMode m);
};

} // namespace slua
