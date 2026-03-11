#include "slua/Diagnostics.h"
#include <stdio.h>
#include <iostream>

namespace slua {

void DiagEngine::emit(DiagLevel level, std::string code, std::string msg, SourceLoc loc) {
    diags_.push_back({level, code, msg, loc});
    const char* prefix = (level == DiagLevel::ERROR)   ? "E" :
                         (level == DiagLevel::WARNING)  ? "W" : "N";
    fprintf(stderr, "[%s%s] %s:%d:%d  %s\n",
            prefix, code.c_str(),
            loc.filename.c_str(), loc.line, loc.col,
            msg.c_str());
    if (level == DiagLevel::ERROR)   error_count_++;
    else if (level == DiagLevel::WARNING) warn_count_++;
}

void DiagEngine::error(std::string code, std::string msg, SourceLoc loc) {
    emit(DiagLevel::ERROR, code, msg, loc);
}

void DiagEngine::warn(std::string code, std::string msg, SourceLoc loc) {
    emit(DiagLevel::WARNING, code, msg, loc);
}

void DiagEngine::note(std::string msg, SourceLoc loc) {
    emit(DiagLevel::NOTE, "0000", msg, loc);
}

void DiagEngine::dump_all() const {
    
    std::cerr << error_count_ << " error(s), " << warn_count_ << " warning(s)\n";
}

} 

