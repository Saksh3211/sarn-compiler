#pragma once
#include <string>
#include <vector>

namespace slua {

enum class TokenKind {
    // Literals
    TK_INT_LIT, TK_FLOAT_LIT, TK_STRING_LIT, TK_NULL, TK_TRUE, TK_FALSE,
    // Keywords
    TK_LOCAL, TK_CONST, TK_GLOBAL, TK_FUNCTION, TK_RETURN, TK_END,
    TK_IF, TK_THEN, TK_ELSEIF, TK_ELSE, TK_FOR, TK_DO, TK_WHILE,
    TK_REPEAT, TK_UNTIL, TK_BREAK, TK_CONTINUE, TK_AND, TK_OR, TK_NOT,
    TK_IMPORT, TK_EXPORT, TK_MODULE, TK_TYPE, TK_EXTERN, TK_DEFER,
    TK_ALLOC, TK_FREE, TK_ALLOC_TYPED, TK_STACK_ALLOC,
    TK_DEREF, TK_STORE, TK_ADDR, TK_CAST, TK_PTR_CAST,
    TK_PANIC, TK_TYPEOF, TK_SIZEOF,
    TK_IN, TK_COMPTIME,
    // Symbols
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_LBRACKET, TK_RBRACKET,
    TK_COMMA, TK_SEMICOLON, TK_COLON, TK_DOUBLECOLON, TK_DOT, TK_DOTDOT,
    TK_DOTDOTDOT, TK_ASSIGN, TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH,
    TK_PERCENT, TK_CARET, TK_HASH, TK_AMP, TK_PIPE, TK_TILDE,
    TK_LSHIFT, TK_RSHIFT, TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LEQ, TK_GEQ,
    TK_ARROW, TK_QUESTION, TK_BANG,
    // Meta
    TK_IDENT, TK_DIRECTIVE, TK_COMMENT, TK_NEWLINE, TK_EOF, TK_UNKNOWN
};

struct SourceLoc {
    std::string filename;
    int line;
    int col;
};

struct Token {
    TokenKind    kind;
    std::string  text;
    SourceLoc    loc;
};

enum class CompileMode { STRICT, NONSTRICT };

/// Scans the first raw line to detect --!!strict / --!!nonstrict.
/// Returns NONSTRICT and emits W0001 if directive is missing.
CompileMode detect_mode(const std::string& source, const std::string& filename);

class Lexer {
public:
    Lexer(std::string source, std::string filename, CompileMode mode);

    Token next();
    Token peek();
    bool  at_eof() const;

private:
    std::string  src_;
    std::string  filename_;
    CompileMode  mode_;
    size_t       pos_   = 0;
    int          line_  = 1;
    int          col_   = 1;
    Token        lookahead_;
    bool         has_lookahead_ = false;

    char advance();
    char peek_char(int offset = 0) const;
    void skip_whitespace_and_comments();
    Token scan_token();
    Token scan_string(char delim);
    Token scan_number();
    Token scan_ident_or_kw();
    Token make(TokenKind k, std::string text);
    SourceLoc loc() const;
};

} // namespace slua
