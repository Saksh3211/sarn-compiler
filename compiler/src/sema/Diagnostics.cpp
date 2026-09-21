#include "sarn/Diagnostics.h"
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::string source_line(const sarn::SourceLoc& loc) {
    if (loc.filename.empty() || loc.line <= 0)
        return {};

    std::ifstream file(loc.filename);
    if (!file)
        return {};

    std::string line;
    for (int current = 1; current <= loc.line && std::getline(file, line); ++current) {
        if (current == loc.line) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            return line;
        }
    }
    return {};
}

std::string caret_line(const std::string& line, int column) {
    std::string marker;
    const size_t offset = column > 0 ? static_cast<size_t>(column - 1) : 0;
    for (size_t i = 0; i < offset && i < line.size(); ++i)
        marker += line[i] == '\t' ? '\t' : ' ';
    marker += '^';
    return marker;
}
}

namespace sarn {

void DiagEngine::emit(DiagLevel level, std::string code, std::string msg,
        SourceLoc loc, std::string suggestion) 
    {
    diags_.push_back({level, code, msg, suggestion, loc});
    const char* label = (level == DiagLevel::ERROR) ? "error" :
            (level == DiagLevel::WARNING) ? "warning" : "note";

    fprintf(stderr, "%s[%s]: %s\n", label, code.c_str(), msg.c_str());
    if (!loc.filename.empty() && loc.line > 0 && loc.col > 0) {
        std::string line = source_line(loc);

        fprintf(stderr, "  --> %s:%d:%d\n", loc.filename.c_str(), loc.line, loc.col);

        if (!line.empty()) {
            fprintf(stderr, "   |\n%3d | %s\n   | %s\n",
                loc.line, line.c_str(), caret_line(line, loc.col).c_str());
        }
    } else if (!loc.filename.empty()) {
        fprintf(stderr, "  --> %s\n", loc.filename.c_str());
    }
    if (show_suggestions_ && !suggestion.empty())
        fprintf(stderr, "  help: %s\n", suggestion.c_str());
    if (level == DiagLevel::ERROR)   error_count_++;
    else if (level == DiagLevel::WARNING) warn_count_++;
}

void DiagEngine::error(std::string code, std::string msg, SourceLoc loc,
        std::string suggestion) {
    emit(DiagLevel::ERROR, code, msg, loc, std::move(suggestion));
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

