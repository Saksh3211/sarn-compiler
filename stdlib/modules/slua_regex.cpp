#include "slua_regex.h"
#include <regex>
#include <string>
#include <cstring>
#include <cstdlib>
#include <iterator>
extern "C" {
int32_t slua_regex_match(const char* str, const char* pat) {
    try { return std::regex_search(std::string(str), std::regex(pat)) ? 1 : 0; } catch (...) { return 0; }
}
int32_t slua_regex_find(const char* str, const char* pat, int32_t from) {
    try {
        std::string s(str); std::regex re(pat); std::smatch m;
        std::string sub = (from>0&&(size_t)from<s.size()) ? s.substr((size_t)from) : s;
        if (std::regex_search(sub, m, re)) return (int32_t)(m.position(0)+(from>0?from:0));
        return -1;
    } catch (...) { return -1; }
}
char* slua_regex_replace(const char* str, const char* pat, const char* repl) {
    try {
        std::string r = std::regex_replace(std::string(str), std::regex(pat), std::string(repl));
        char* out = (char*)malloc(r.size()+1); if(!out) return strdup(str);
        memcpy(out, r.c_str(), r.size()+1); return out;
    } catch (...) { return strdup(str); }
}
char* slua_regex_groups(const char* str, const char* pat) {
    try {
        std::smatch m; std::string s(str);
        if (!std::regex_search(s, m, std::regex(pat))) return strdup("");
        std::string r;
        for (size_t i=1; i<m.size(); i++) { if(i>1) r+='\n'; r+=m[i].str(); }
        char* out=(char*)malloc(r.size()+1); memcpy(out,r.c_str(),r.size()+1); return out;
    } catch (...) { return strdup(""); }
}
int32_t slua_regex_count(const char* str, const char* pat) {
    try {
        std::string s(str); std::regex re(pat);
        return (int32_t)std::distance(std::sregex_iterator(s.begin(),s.end(),re), std::sregex_iterator());
    } catch (...) { return 0; }
}
char* slua_regex_find_all(const char* str, const char* pat) {
    try {
        std::string s(str); std::regex re(pat); std::string r;
        for (auto it=std::sregex_iterator(s.begin(),s.end(),re); it!=std::sregex_iterator(); ++it) {
            if (!r.empty()) r+='\n'; r+=(*it)[0].str();
        }
        char* out=(char*)malloc(r.size()+1); memcpy(out,r.c_str(),r.size()+1); return out;
    } catch (...) { return strdup(""); }
}
}
