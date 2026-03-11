#include "slua/Lexer.h"
#include <stdexcept>
#include <cctype>
#include <unordered_map>

namespace slua {

static const std::unordered_map<std::string, TokenKind> KEYWORDS = {
    {"local",      TokenKind::TK_LOCAL},
    {"const",      TokenKind::TK_CONST},
    {"global",     TokenKind::TK_GLOBAL},
    {"function",   TokenKind::TK_FUNCTION},
    {"return",     TokenKind::TK_RETURN},
    {"end",        TokenKind::TK_END},
    {"if",         TokenKind::TK_IF},
    {"then",       TokenKind::TK_THEN},
    {"elseif",     TokenKind::TK_ELSEIF},
    {"else",       TokenKind::TK_ELSE},
    {"for",        TokenKind::TK_FOR},
    {"do",         TokenKind::TK_DO},
    {"while",      TokenKind::TK_WHILE},
    {"repeat",     TokenKind::TK_REPEAT},
    {"until",      TokenKind::TK_UNTIL},
    {"break",      TokenKind::TK_BREAK},
    {"continue",   TokenKind::TK_CONTINUE},
    {"and",        TokenKind::TK_AND},
    {"or",         TokenKind::TK_OR},
    {"not",        TokenKind::TK_NOT},
    {"null",       TokenKind::TK_NULL},
    {"true",       TokenKind::TK_TRUE},
    {"false",      TokenKind::TK_FALSE},
    {"import",     TokenKind::TK_IMPORT},
    {"export",     TokenKind::TK_EXPORT},
    {"module",     TokenKind::TK_MODULE},
    {"type",       TokenKind::TK_TYPE},
    {"extern",     TokenKind::TK_EXTERN},
    {"defer",      TokenKind::TK_DEFER},
    {"alloc",      TokenKind::TK_ALLOC},
    {"free",       TokenKind::TK_FREE},
    {"alloc_typed",TokenKind::TK_ALLOC_TYPED},
    {"stack_alloc",TokenKind::TK_STACK_ALLOC},
    {"deref",      TokenKind::TK_DEREF},
    {"store",      TokenKind::TK_STORE},
    {"addr",       TokenKind::TK_ADDR},
    {"cast",       TokenKind::TK_CAST},
    {"ptr_cast",   TokenKind::TK_PTR_CAST},
    {"panic",      TokenKind::TK_PANIC},
    {"typeof",     TokenKind::TK_TYPEOF},
    {"sizeof",     TokenKind::TK_SIZEOF},
    {"in",         TokenKind::TK_IN},
    {"comptime",   TokenKind::TK_COMPTIME},
};

Directives detect_directives(const std::string& source,
                              const std::string& filename) {
    Directives d;
    size_t pos = 0;

    if (source.size() >= 3 &&
        (unsigned char)source[0] == 0xEF &&
        (unsigned char)source[1] == 0xBB &&
        (unsigned char)source[2] == 0xBF)
        pos = 3;

    bool found_type = false;

    while (pos < source.size()) {
        size_t eol = source.find('\n', pos);
        if (eol == std::string::npos) eol = source.size();
        std::string line = source.substr(pos, eol - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.substr(0, 4) != "--!!") break;

        if (line == "--!!strict" || line == "--!!type:strict") {
            d.type = CompileMode::STRICT;
            found_type = true;
            if (line == "--!!strict")
                fprintf(stderr, "[W0002] %s:  '--!!strict' is deprecated, use '--!!type:strict'\n",
                        filename.c_str());
        } else if (line == "--!!nonstrict" || line == "--!!type:nonstrict") {
            d.type = CompileMode::NONSTRICT;
            found_type = true;
            if (line == "--!!nonstrict")
                fprintf(stderr, "[W0002] %s:  '--!!nonstrict' is deprecated, use '--!!type:nonstrict'\n",
                        filename.c_str());
        } else if (line == "--!!mem:auto") {
            d.mem = MemoryMode::Auto;
        } else if (line == "--!!mem:man") {
            d.mem = MemoryMode::Manual;
        } else {
            fprintf(stderr, "[E0003] %s: unknown directive: '%s'\n",
                    filename.c_str(), line.c_str());
            exit(1);
        }
        pos = eol + 1;
    }

    if (!found_type)
        fprintf(stderr, "[W0001] %s: missing --!!type directive, defaulting to nonstrict\n",
                filename.c_str());

    return d;
}

CompileMode detect_mode(const std::string& source, const std::string& filename) {
    return detect_directives(source, filename).type;
}

[[maybe_unused]]
static CompileMode detect_mode_unused_sentinel(const std::string& source, const std::string& filename) {
    
    size_t start = 0;
    if (source.size() >= 3 &&
        (unsigned char)source[0] == 0xEF &&
        (unsigned char)source[1] == 0xBB &&
        (unsigned char)source[2] == 0xBF) {
        start = 3;
    }

    
    size_t end = source.find('\n', start);
    if (end == std::string::npos) end = source.size();
    std::string line = source.substr(start, end - start);
    
    if (!line.empty() && line.back() == '\r') line.pop_back();

    if (line == "--!!strict")    return CompileMode::STRICT;
    if (line == "--!!nonstrict") return CompileMode::NONSTRICT;

    if (line.substr(0, 4) == "--!!") {
        fprintf(stderr, "[E0003] %s:1:1 — malformed mode directive: '%s'\n",
                filename.c_str(), line.c_str());
        exit(1);
    }

    fprintf(stderr, "[W0001] %s:1:1 — missing mode directive, defaulting to nonstrict\n",
            filename.c_str());
    return CompileMode::NONSTRICT;
}

Lexer::Lexer(std::string source, std::string filename, CompileMode mode)
    : src_(std::move(source)), filename_(std::move(filename)), mode_(mode) {
    
    size_t nl = src_.find('\n');
    if (nl != std::string::npos) {
        pos_  = nl + 1;
        line_ = 2;
        col_  = 1;
    }
    
    if (pos_ == 0 && src_.size() >= 3 &&
        (unsigned char)src_[0] == 0xEF &&
        (unsigned char)src_[1] == 0xBB &&
        (unsigned char)src_[2] == 0xBF) {
        pos_ = 3;
    }
}

SourceLoc Lexer::loc() const { return {filename_, line_, col_}; }

char Lexer::advance() {
    if (pos_ >= src_.size()) return '\0';
    char c = src_[pos_++];
    if (c == '\n') { line_++; col_ = 1; } else { col_++; }
    return c;
}

char Lexer::peek_char(int offset) const {
    size_t p = pos_ + (size_t)offset;
    return (p < src_.size()) ? src_[p] : '\0';
}

bool Lexer::at_eof() const { return pos_ >= src_.size(); }

Token Lexer::make(TokenKind k, std::string text) {
    return Token{k, std::move(text), loc()};
}

void Lexer::skip_whitespace_and_comments() {
    while (pos_ < src_.size()) {
        char c = src_[pos_];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(); continue;
        }
        
        if (c == '-' && peek_char(1) == '-') {
            
            if (peek_char(2) == '[' && peek_char(3) == '[') {
                pos_ += 4;
                while (pos_ < src_.size()) {
                    if (src_[pos_] == ']' && pos_+1 < src_.size() && src_[pos_+1] == ']') {
                        pos_ += 2; break;
                    }
                    if (src_[pos_] == '\n') { line_++; col_ = 1; } else { col_++; }
                    pos_++;
                }
            } else {
                while (pos_ < src_.size() && src_[pos_] != '\n') advance();
            }
            continue;
        }
        break;
    }
}

Token Lexer::scan_string(char delim) {
    std::string val;
    advance(); 
    while (pos_ < src_.size()) {
        char c = src_[pos_];
        if (c == delim) { advance(); break; }
        if (c == '\\') {
            advance();
            char esc = advance();
            switch (esc) {
                case 'n':  val += '\n'; break;
                case 't':  val += '\t'; break;
                case 'r':  val += '\r'; break;
                case '"':  val += '"';  break;
                case '\'': val += '\''; break;
                case '\\': val += '\\'; break;
                default:   val += esc;  break;
            }
        } else {
            val += advance();
        }
    }
    return Token{TokenKind::TK_STRING_LIT, val, loc()};
}

Token Lexer::scan_number() {
    std::string num;
    bool is_float = false;
    while (pos_ < src_.size() && (std::isdigit(src_[pos_]) || src_[pos_] == '.')) {
        if (src_[pos_] == '.') is_float = true;
        num += advance();
    }
    
    if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
        is_float = true;
        num += advance();
        if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-'))
            num += advance();
        while (pos_ < src_.size() && std::isdigit(src_[pos_]))
            num += advance();
    }
    return Token{is_float ? TokenKind::TK_FLOAT_LIT : TokenKind::TK_INT_LIT, num, loc()};
}

Token Lexer::scan_ident_or_kw() {
    std::string id;
    while (pos_ < src_.size() && (std::isalnum(src_[pos_]) || src_[pos_] == '_'))
        id += advance();
    auto it = KEYWORDS.find(id);
    if (it != KEYWORDS.end()) return Token{it->second, id, loc()};
    return Token{TokenKind::TK_IDENT, id, loc()};
}

Token Lexer::scan_token() {
    skip_whitespace_and_comments();
    if (pos_ >= src_.size()) return make(TokenKind::TK_EOF, "");

    char c = src_[pos_];
    SourceLoc l = loc();

    if (c == '"' || c == '\'') return scan_string(c);
    if (std::isdigit(c)) return scan_number();
    if (std::isalpha(c) || c == '_') return scan_ident_or_kw();

    advance();
    switch (c) {
        case '(': return Token{TokenKind::TK_LPAREN,   "(", l};
        case ')': return Token{TokenKind::TK_RPAREN,   ")", l};
        case '{': return Token{TokenKind::TK_LBRACE,   "{", l};
        case '}': return Token{TokenKind::TK_RBRACE,   "}", l};
        case '[': return Token{TokenKind::TK_LBRACKET, "[", l};
        case ']': return Token{TokenKind::TK_RBRACKET, "]", l};
        case ',': return Token{TokenKind::TK_COMMA,    ",", l};
        case ';': return Token{TokenKind::TK_SEMICOLON,";", l};
        case '#': return Token{TokenKind::TK_HASH,     "#", l};
        case '^': return Token{TokenKind::TK_CARET,    "^", l};
        case '%': return Token{TokenKind::TK_PERCENT,  "%", l};
        case '&': return Token{TokenKind::TK_AMP,      "&", l};
        case '|': return Token{TokenKind::TK_PIPE,     "|", l};
        case '+': return Token{TokenKind::TK_PLUS,     "+", l};
        case '*': return Token{TokenKind::TK_STAR,     "*", l};
        case '?': return Token{TokenKind::TK_QUESTION, "?", l};
        case '!': return Token{TokenKind::TK_BANG,     "!", l};
        case '/': return Token{TokenKind::TK_SLASH,    "/", l};
        case '~':
            if (pos_ < src_.size() && src_[pos_] == '=') { advance(); return Token{TokenKind::TK_NEQ,    "~=", l}; }
            return Token{TokenKind::TK_TILDE, "~", l};
        case '<':
            if (pos_ < src_.size() && src_[pos_] == '=') { advance(); return Token{TokenKind::TK_LEQ, "<=", l}; }
            if (pos_ < src_.size() && src_[pos_] == '<') { advance(); return Token{TokenKind::TK_LSHIFT, "<<", l}; }
            return Token{TokenKind::TK_LT, "<", l};
        case '>':
            if (pos_ < src_.size() && src_[pos_] == '=') { advance(); return Token{TokenKind::TK_GEQ, ">=", l}; }
            if (pos_ < src_.size() && src_[pos_] == '>') { advance(); return Token{TokenKind::TK_RSHIFT, ">>", l}; }
            return Token{TokenKind::TK_GT, ">", l};
        case '=':
            if (pos_ < src_.size() && src_[pos_] == '=') { advance(); return Token{TokenKind::TK_EQ, "==", l}; }
            return Token{TokenKind::TK_ASSIGN, "=", l};
        case ':':
            if (pos_ < src_.size() && src_[pos_] == ':') { advance(); return Token{TokenKind::TK_DOUBLECOLON, "::", l}; }
            return Token{TokenKind::TK_COLON, ":", l};
        case '.':
            if (pos_ < src_.size() && src_[pos_] == '.') {
                advance();
                if (pos_ < src_.size() && src_[pos_] == '.') { advance(); return Token{TokenKind::TK_DOTDOTDOT, "...", l}; }
                return Token{TokenKind::TK_DOTDOT, "..", l};
            }
            if (pos_ < src_.size() && std::isdigit(src_[pos_])) {
                pos_--; return scan_number();
            }
            return Token{TokenKind::TK_DOT, ".", l};
        case '-':
            if (pos_ < src_.size() && src_[pos_] == '>') { advance(); return Token{TokenKind::TK_ARROW, "->", l}; }
            return Token{TokenKind::TK_MINUS, "-", l};
        default:
            return Token{TokenKind::TK_UNKNOWN, std::string(1,c), l};
    }
}

Token Lexer::next() {
    if (has_lookahead_) { has_lookahead_ = false; return lookahead_; }
    return scan_token();
}

Token Lexer::peek() {
    if (!has_lookahead_) { lookahead_ = scan_token(); has_lookahead_ = true; }
    return lookahead_;
}

} 

