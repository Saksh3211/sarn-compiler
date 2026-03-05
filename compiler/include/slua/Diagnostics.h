#pragma once
#include <string>
#include <vector>
#include "Lexer.h"

namespace slua {

enum class DiagLevel { NOTE, WARNING, ERROR };

struct Diagnostic {
    DiagLevel   level;
    std::string code;     // e.g., "E0011"
    std::string message;
    SourceLoc   loc;
};

class DiagEngine {
public:
    explicit DiagEngine(CompileMode mode) : mode_(mode) {}

    void emit(DiagLevel level, std::string code, std::string msg, SourceLoc loc);
    void error(std::string code, std::string msg, SourceLoc loc);
    void warn (std::string code, std::string msg, SourceLoc loc);
    void note (std::string msg, SourceLoc loc);

    bool has_errors() const { return error_count_ > 0; }
    int  error_count() const { return error_count_; }
    int  warning_count() const { return warn_count_; }

    void dump_all() const;

private:
    CompileMode               mode_;
    std::vector<Diagnostic>   diags_;
    int                       error_count_ = 0;
    int                       warn_count_  = 0;
};

} // namespace slua
