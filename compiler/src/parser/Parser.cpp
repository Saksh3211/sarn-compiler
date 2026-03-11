#include "slua/Parser.h"
#include <stdexcept>

namespace slua {





Parser::Parser(Lexer& lexer, DiagEngine& diag, CompileMode mode)
    : lex_(lexer), diag_(diag), mode_(mode) {
    cur_  = lex_.next();
    peek_ = lex_.peek();
}





Token Parser::advance() {
    Token t = cur_;
    cur_    = lex_.next();
    peek_   = lex_.peek();
    return t;
}

bool Parser::check(TokenKind k) const { return cur_.kind == k; }

bool Parser::match(TokenKind k) {
    if (check(k)) { advance(); return true; }
    return false;
}

Token Parser::expect(TokenKind k, const std::string& ctx) {
    if (!check(k)) {
        diag_.error("E0001",
            "expected '" + token_kind_name(k) + "' in " + ctx +
            ", got '" + cur_.text + "'",
            cur_.loc);
        
        return cur_;
    }
    return advance();
}


std::string Parser::token_kind_name(TokenKind k) {
    switch (k) {
        case TokenKind::TK_LPAREN:    return "(";
        case TokenKind::TK_RPAREN:    return ")";
        case TokenKind::TK_LBRACE:    return "{";
        case TokenKind::TK_RBRACE:    return "}";
        case TokenKind::TK_LBRACKET:  return "[";
        case TokenKind::TK_RBRACKET:  return "]";
        case TokenKind::TK_COMMA:     return ",";
        case TokenKind::TK_COLON:     return ":";
        case TokenKind::TK_ASSIGN:    return "=";
        case TokenKind::TK_DOT:       return ".";
        case TokenKind::TK_DOTDOT:    return "..";
        case TokenKind::TK_ARROW:     return "->";
        case TokenKind::TK_GT:        return ">";
        case TokenKind::TK_THEN:      return "then";
        case TokenKind::TK_DO:        return "do";
        case TokenKind::TK_END:       return "end";
        case TokenKind::TK_UNTIL:     return "until";
        case TokenKind::TK_IDENT:     return "<identifier>";
        case TokenKind::TK_EOF:       return "<eof>";
        default: return "token";
    }
}





std::unique_ptr<Module> Parser::parse_module(const std::string& filename) {
    auto mod      = std::make_unique<Module>();
    mod->filename = filename;
    mod->mode     = mode_;

    while (!check(TokenKind::TK_EOF)) {
        auto stmt = parse_stmt();
        if (stmt) {
            mod->stmts.push_back(std::move(stmt));
        }
        
    }
    return mod;
}





StmtPtr Parser::parse_stmt() {
    SourceLoc loc = cur_.loc;

    switch (cur_.kind) {

        
        case TokenKind::TK_LOCAL:
            return parse_local_decl();

        
        case TokenKind::TK_CONST:
            return parse_const_decl();

        
        case TokenKind::TK_GLOBAL:
            return parse_global_decl();

        
        case TokenKind::TK_FUNCTION:
            return parse_func_decl(false);

        
        
        case TokenKind::TK_EXPORT: {
            advance(); 
            if (check(TokenKind::TK_FUNCTION))
                return parse_func_decl(true);
            if (check(TokenKind::TK_TYPE))
                return parse_type_decl();
            diag_.error("E0001", "expected 'function' or 'type' after 'export'", cur_.loc);
            advance();
            return nullptr;
        }

        
        case TokenKind::TK_IF:
            return parse_if_stmt();

        
        case TokenKind::TK_WHILE:
            return parse_while_stmt();

        
        case TokenKind::TK_REPEAT:
            return parse_repeat_stmt();

        
        case TokenKind::TK_FOR:
            return parse_numeric_for();

        
        case TokenKind::TK_RETURN:
            return parse_return_stmt();

        
        case TokenKind::TK_BREAK: {
            auto s = std::make_unique<Stmt>();
            s->v   = BreakStmt{};
            s->loc = advance().loc;
            return s;
        }

        
        case TokenKind::TK_CONTINUE: {
            auto s = std::make_unique<Stmt>();
            s->v   = ContinueStmt{};
            s->loc = advance().loc;
            return s;
        }

        
        case TokenKind::TK_DEFER:
            return parse_defer_stmt();

        
        case TokenKind::TK_IMPORT:
            return parse_import_decl();

        
        case TokenKind::TK_TYPE:
            return parse_type_decl();

        
        case TokenKind::TK_EXTERN:
            return parse_extern_decl();

        
        case TokenKind::TK_PANIC: {
            advance();
            expect(TokenKind::TK_LPAREN, "panic");
            auto msg = parse_expr();
            expect(TokenKind::TK_RPAREN, "panic");
            auto s = std::make_unique<Stmt>();
            s->v   = PanicStmt{std::move(msg)};
            s->loc = loc;
            return s;
        }

        
        case TokenKind::TK_STORE: {
            advance();
            expect(TokenKind::TK_LPAREN, "store");
            auto ptr = parse_expr();
            expect(TokenKind::TK_COMMA, "store");
            auto val = parse_expr();
            expect(TokenKind::TK_RPAREN, "store");
            auto s = std::make_unique<Stmt>();
            s->v   = StoreStmt{std::move(ptr), std::move(val)};
            s->loc = loc;
            return s;
        }

        
        case TokenKind::TK_FREE: {
            advance();
            expect(TokenKind::TK_LPAREN, "free");
            auto ptr = parse_expr();
            expect(TokenKind::TK_RPAREN, "free");
            auto s = std::make_unique<Stmt>();
            s->v   = FreeStmt{std::move(ptr)};
            s->loc = loc;
            return s;
        }

        
        case TokenKind::TK_DO: {
            advance();
            std::vector<StmtPtr> body;
            while (!check(TokenKind::TK_END) && !check(TokenKind::TK_EOF))
                if (auto st = parse_stmt()) body.push_back(std::move(st));
            expect(TokenKind::TK_END, "do block");
            auto s = std::make_unique<Stmt>();
            s->v   = DoBlock{std::move(body)};
            s->loc = loc;
            return s;
        }

        
        
        
        
        
        
        default: {
            if (check(TokenKind::TK_IDENT) ||
                check(TokenKind::TK_LPAREN)) {
                return parse_assign_or_call();
            }

            
            diag_.error("E0001",
                "unexpected token '" + cur_.text + "' at statement level",
                cur_.loc);
            advance();
            return nullptr;
        }
    }
}







StmtPtr Parser::parse_assign_or_call() {
    SourceLoc loc = cur_.loc;
    auto lhs = parse_postfix_expr();
    if (!lhs) return nullptr;

    if (match(TokenKind::TK_ASSIGN)) {
        auto rhs = parse_expr();
        auto s   = std::make_unique<Stmt>();
        s->v     = Assign{std::move(lhs), std::move(rhs)};
        s->loc   = loc;
        return s;
    }

    
    auto s = std::make_unique<Stmt>();
    s->v   = CallStmt{std::move(lhs)};
    s->loc = loc;
    return s;
}





StmtPtr Parser::parse_local_decl() {
    SourceLoc loc = advance().loc; 
    std::string name = expect(TokenKind::TK_IDENT, "local declaration").text;

    TypeNodePtr type_ann;
    if (match(TokenKind::TK_COLON))
        type_ann = parse_type();

    ExprPtr init;
    if (match(TokenKind::TK_ASSIGN))
        init = parse_expr();

    
    if (mode_ == CompileMode::STRICT && !type_ann && !init) {
        diag_.warn("W0020",
            "local '" + name + "' has no type annotation and no initialiser",
            loc);
    }

    auto s = std::make_unique<Stmt>();
    s->v   = LocalDecl{name, std::move(type_ann), std::move(init), false};
    s->loc = loc;
    return s;
}

StmtPtr Parser::parse_const_decl() {
    SourceLoc loc = advance().loc; 
    std::string name = expect(TokenKind::TK_IDENT, "const declaration").text;

    TypeNodePtr type_ann;
    if (match(TokenKind::TK_COLON))
        type_ann = parse_type();

    expect(TokenKind::TK_ASSIGN, "const declaration (must have initialiser)");
    auto init = parse_expr();

    auto s = std::make_unique<Stmt>();
    s->v   = LocalDecl{name, std::move(type_ann), std::move(init), true};
    s->loc = loc;
    return s;
}

StmtPtr Parser::parse_global_decl() {
    SourceLoc loc = advance().loc; 
    std::string name = expect(TokenKind::TK_IDENT, "global declaration").text;

    TypeNodePtr type_ann;
    if (match(TokenKind::TK_COLON))
        type_ann = parse_type();

    ExprPtr init;
    if (match(TokenKind::TK_ASSIGN))
        init = parse_expr();

    auto s = std::make_unique<Stmt>();
    s->v   = GlobalDecl{name, std::move(type_ann), std::move(init)};
    s->loc = loc;
    return s;
}






StmtPtr Parser::parse_func_decl(bool exported) {
    SourceLoc loc = advance().loc; 
    std::string name = expect(TokenKind::TK_IDENT, "function name").text;

    
    std::vector<std::string> type_params;
    if (match(TokenKind::TK_LT)) {
        do {
            type_params.push_back(
                expect(TokenKind::TK_IDENT, "type parameter").text);
        } while (match(TokenKind::TK_COMMA));
        expect(TokenKind::TK_GT, "type parameter list");
    }

    
    expect(TokenKind::TK_LPAREN, "function parameters");
    std::vector<std::pair<std::string, TypeNodePtr>> params;
    if (!check(TokenKind::TK_RPAREN)) {
        do {
            std::string pname = expect(TokenKind::TK_IDENT, "parameter name").text;
            TypeNodePtr ptype;
            if (match(TokenKind::TK_COLON))
                ptype = parse_type();
            params.push_back({pname, std::move(ptype)});
        } while (match(TokenKind::TK_COMMA));
    }
    expect(TokenKind::TK_RPAREN, "function parameters");

    
    TypeNodePtr ret_type;
    if (match(TokenKind::TK_COLON))
        ret_type = parse_type();

    
    std::vector<StmtPtr> body;
    while (!check(TokenKind::TK_END) && !check(TokenKind::TK_EOF))
        if (auto st = parse_stmt()) body.push_back(std::move(st));
    expect(TokenKind::TK_END, "function body");

    auto s = std::make_unique<Stmt>();
    s->v   = FuncDecl{name, exported, std::move(type_params),
                      std::move(params), std::move(ret_type), std::move(body)};
    s->loc = loc;
    return s;
}





StmtPtr Parser::parse_if_stmt() {
    SourceLoc loc = advance().loc; 
    auto cond = parse_expr();
    expect(TokenKind::TK_THEN, "if condition");

    std::vector<StmtPtr> then_body;
    while (!check(TokenKind::TK_ELSEIF) &&
           !check(TokenKind::TK_ELSE)   &&
           !check(TokenKind::TK_END)    &&
           !check(TokenKind::TK_EOF)) {
        if (auto st = parse_stmt()) then_body.push_back(std::move(st));
    }

    
    std::vector<std::pair<ExprPtr, std::vector<StmtPtr>>> elseif_clauses;
    while (check(TokenKind::TK_ELSEIF)) {
        advance();
        auto ei_cond = parse_expr();
        expect(TokenKind::TK_THEN, "elseif condition");
        std::vector<StmtPtr> ei_body;
        while (!check(TokenKind::TK_ELSEIF) &&
               !check(TokenKind::TK_ELSE)   &&
               !check(TokenKind::TK_END)    &&
               !check(TokenKind::TK_EOF)) {
            if (auto st = parse_stmt()) ei_body.push_back(std::move(st));
        }
        elseif_clauses.push_back({std::move(ei_cond), std::move(ei_body)});
    }

    
    std::optional<std::vector<StmtPtr>> else_body;
    if (match(TokenKind::TK_ELSE)) {
        std::vector<StmtPtr> eb;
        while (!check(TokenKind::TK_END) && !check(TokenKind::TK_EOF))
            if (auto st = parse_stmt()) eb.push_back(std::move(st));
        else_body = std::move(eb);
    }

    expect(TokenKind::TK_END, "if statement");

    auto s = std::make_unique<Stmt>();
    s->v   = IfStmt{std::move(cond), std::move(then_body),
                    std::move(elseif_clauses), std::move(else_body)};
    s->loc = loc;
    return s;
}

StmtPtr Parser::parse_while_stmt() {
    SourceLoc loc = advance().loc; 
    auto cond = parse_expr();
    expect(TokenKind::TK_DO, "while condition");

    std::vector<StmtPtr> body;
    while (!check(TokenKind::TK_END) && !check(TokenKind::TK_EOF))
        if (auto st = parse_stmt()) body.push_back(std::move(st));
    expect(TokenKind::TK_END, "while loop");

    auto s = std::make_unique<Stmt>();
    s->v   = WhileStmt{std::move(cond), std::move(body)};
    s->loc = loc;
    return s;
}

StmtPtr Parser::parse_repeat_stmt() {
    SourceLoc loc = advance().loc; 

    std::vector<StmtPtr> body;
    while (!check(TokenKind::TK_UNTIL) && !check(TokenKind::TK_EOF))
        if (auto st = parse_stmt()) body.push_back(std::move(st));
    expect(TokenKind::TK_UNTIL, "repeat loop");
    auto cond = parse_expr();

    auto s = std::make_unique<Stmt>();
    s->v   = RepeatStmt{std::move(body), std::move(cond)};
    s->loc = loc;
    return s;
}

StmtPtr Parser::parse_numeric_for() {
    SourceLoc loc = advance().loc; 
    std::string var = expect(TokenKind::TK_IDENT, "for variable").text;
    expect(TokenKind::TK_ASSIGN, "for initialiser");
    auto start = parse_expr();
    expect(TokenKind::TK_COMMA, "for range");
    auto stop  = parse_expr();

    ExprPtr step;
    if (match(TokenKind::TK_COMMA))
        step = parse_expr();

    expect(TokenKind::TK_DO, "for range");

    std::vector<StmtPtr> body;
    while (!check(TokenKind::TK_END) && !check(TokenKind::TK_EOF))
        if (auto st = parse_stmt()) body.push_back(std::move(st));
    expect(TokenKind::TK_END, "for loop");

    auto s = std::make_unique<Stmt>();
    s->v   = NumericFor{var, std::move(start), std::move(stop),
                        std::move(step), std::move(body)};
    s->loc = loc;
    return s;
}

StmtPtr Parser::parse_return_stmt() {
    SourceLoc loc = advance().loc; 

    std::vector<ExprPtr> values;
    
    if (!check(TokenKind::TK_END)    &&
        !check(TokenKind::TK_ELSE)   &&
        !check(TokenKind::TK_ELSEIF) &&
        !check(TokenKind::TK_UNTIL)  &&
        !check(TokenKind::TK_EOF)) {
        values.push_back(parse_expr());
        while (match(TokenKind::TK_COMMA))
            values.push_back(parse_expr());
    }

    auto s = std::make_unique<Stmt>();
    s->v   = ReturnStmt{std::move(values)};
    s->loc = loc;
    return s;
}

StmtPtr Parser::parse_defer_stmt() {
    SourceLoc loc = advance().loc; 
    auto action   = parse_stmt();

    auto s = std::make_unique<Stmt>();
    s->v   = DeferStmt{std::move(action)};
    s->loc = loc;
    return s;
}

StmtPtr Parser::parse_import_decl() {
    SourceLoc loc = advance().loc; 
    std::string mod = expect(TokenKind::TK_IDENT, "import module name").text;

    auto s = std::make_unique<Stmt>();
    s->v   = ImportDecl{mod};
    s->loc = loc;
    return s;
}

StmtPtr Parser::parse_type_decl() {
    SourceLoc loc = advance().loc; 
    std::string name = expect(TokenKind::TK_IDENT, "type name").text;

    
    std::vector<std::string> type_params;
    if (match(TokenKind::TK_LT)) {
        do {
            type_params.push_back(
                expect(TokenKind::TK_IDENT, "type parameter").text);
        } while (match(TokenKind::TK_COMMA));
        expect(TokenKind::TK_GT, "type parameter list");
    }

    expect(TokenKind::TK_ASSIGN, "type declaration");
    auto def = parse_type();

    auto s = std::make_unique<Stmt>();
    s->v   = TypeDecl{name, std::move(type_params), std::move(def)};
    s->loc = loc;
    return s;
}

StmtPtr Parser::parse_extern_decl() {
    SourceLoc loc = advance().loc; 
    expect(TokenKind::TK_FUNCTION, "extern declaration");
    std::string name = expect(TokenKind::TK_IDENT, "extern function name").text;

    
    expect(TokenKind::TK_LPAREN, "extern params");
    std::vector<TypeNodePtr> param_types;
    if (!check(TokenKind::TK_RPAREN)) {
        do {
            
            if (check(TokenKind::TK_IDENT) && peek_.kind == TokenKind::TK_COLON) {
                advance(); 
                advance(); 
            }
            param_types.push_back(parse_type());
        } while (match(TokenKind::TK_COMMA));
    }
    expect(TokenKind::TK_RPAREN, "extern params");

    TypeNodePtr ret_type;
    if (match(TokenKind::TK_COLON))
        ret_type = parse_type();

    auto ft  = std::make_unique<TypeNode>();
    ft->v    = FuncType{std::move(param_types), std::move(ret_type)};

    auto s = std::make_unique<Stmt>();
    s->v   = ExternDecl{name, std::move(ft)};
    s->loc = loc;
    return s;
}
















ExprPtr Parser::parse_expr()       { return parse_or_expr(); }

ExprPtr Parser::parse_or_expr() {
    auto lhs = parse_and_expr();
    while (check(TokenKind::TK_OR)) {
        SourceLoc loc = cur_.loc;
        std::string op = advance().text;
        auto rhs = parse_and_expr();
        auto e = std::make_unique<Expr>();
        e->v   = Binop{op, std::move(lhs), std::move(rhs)};
        e->loc = loc;
        lhs = std::move(e);
    }
    return lhs;
}

ExprPtr Parser::parse_and_expr() {
    auto lhs = parse_cmp_expr();
    while (check(TokenKind::TK_AND)) {
        SourceLoc loc = cur_.loc;
        std::string op = advance().text;
        auto rhs = parse_cmp_expr();
        auto e = std::make_unique<Expr>();
        e->v   = Binop{op, std::move(lhs), std::move(rhs)};
        e->loc = loc;
        lhs = std::move(e);
    }
    return lhs;
}

ExprPtr Parser::parse_cmp_expr() {
    auto lhs = parse_concat_expr();
    while (check(TokenKind::TK_EQ)  || check(TokenKind::TK_NEQ) ||
           check(TokenKind::TK_LT)  || check(TokenKind::TK_GT)  ||
           check(TokenKind::TK_LEQ) || check(TokenKind::TK_GEQ)) {
        SourceLoc loc = cur_.loc;
        std::string op = advance().text;
        auto rhs = parse_concat_expr();
        auto e = std::make_unique<Expr>();
        e->v   = Binop{op, std::move(lhs), std::move(rhs)};
        e->loc = loc;
        lhs = std::move(e);
    }
    return lhs;
}

ExprPtr Parser::parse_concat_expr() {
    auto lhs = parse_add_expr();
    if (check(TokenKind::TK_DOTDOT)) {
        SourceLoc loc = cur_.loc;
        std::string op = advance().text;
        
        auto rhs = parse_concat_expr();
        auto e = std::make_unique<Expr>();
        e->v   = Binop{op, std::move(lhs), std::move(rhs)};
        e->loc = loc;
        return e;
    }
    return lhs;
}

ExprPtr Parser::parse_add_expr() {
    auto lhs = parse_mul_expr();
    while (check(TokenKind::TK_PLUS) || check(TokenKind::TK_MINUS)) {
        SourceLoc loc = cur_.loc;
        std::string op = advance().text;
        auto rhs = parse_mul_expr();
        auto e = std::make_unique<Expr>();
        e->v   = Binop{op, std::move(lhs), std::move(rhs)};
        e->loc = loc;
        lhs = std::move(e);
    }
    return lhs;
}

ExprPtr Parser::parse_mul_expr() {
    auto lhs = parse_unary_expr();
    while (check(TokenKind::TK_STAR)    ||
           check(TokenKind::TK_SLASH)   ||
           check(TokenKind::TK_PERCENT)) {
        SourceLoc loc = cur_.loc;
        std::string op = advance().text;
        auto rhs = parse_unary_expr();
        auto e = std::make_unique<Expr>();
        e->v   = Binop{op, std::move(lhs), std::move(rhs)};
        e->loc = loc;
        lhs = std::move(e);
    }
    return lhs;
}

ExprPtr Parser::parse_unary_expr() {
    SourceLoc loc = cur_.loc;

    if (check(TokenKind::TK_NOT)) {
        std::string op = advance().text;
        auto operand = parse_unary_expr();
        auto e = std::make_unique<Expr>();
        e->v   = Unop{op, std::move(operand)};
        e->loc = loc;
        return e;
    }
    if (check(TokenKind::TK_MINUS)) {
        std::string op = advance().text;
        auto operand = parse_unary_expr();
        auto e = std::make_unique<Expr>();
        e->v   = Unop{"-", std::move(operand)};
        e->loc = loc;
        return e;
    }
    if (check(TokenKind::TK_HASH)) {
        std::string op = advance().text;
        auto operand = parse_unary_expr();
        auto e = std::make_unique<Expr>();
        e->v   = Unop{"#", std::move(operand)};
        e->loc = loc;
        return e;
    }

    return parse_postfix_expr();
}





ExprPtr Parser::parse_postfix_expr() {
    auto base = parse_primary_expr();
    if (!base) return nullptr;

    while (true) {
        SourceLoc loc = cur_.loc;

        
        if (check(TokenKind::TK_DOT)) {
            advance();
            std::string field = expect(TokenKind::TK_IDENT, "field access").text;
            auto e = std::make_unique<Expr>();
            e->v   = Field{std::move(base), field};
            e->loc = loc;
            base   = std::move(e);
            continue;
        }

        
        if (check(TokenKind::TK_LBRACKET)) {
            advance();
            auto key = parse_expr();
            expect(TokenKind::TK_RBRACKET, "index");
            auto e = std::make_unique<Expr>();
            e->v   = Index{std::move(base), std::move(key)};
            e->loc = loc;
            base   = std::move(e);
            continue;
        }

        
        if (check(TokenKind::TK_COLON)) {
            advance();
            std::string method = expect(TokenKind::TK_IDENT, "method name").text;
            expect(TokenKind::TK_LPAREN, "method call");
            auto args = parse_arg_list();
            expect(TokenKind::TK_RPAREN, "method call");
            auto e = std::make_unique<Expr>();
            e->v   = MethodCall{std::move(base), method, std::move(args)};
            e->loc = loc;
            base   = std::move(e);
            continue;
        }

        
        if (check(TokenKind::TK_LPAREN)) {
            advance();
            auto args = parse_arg_list();
            expect(TokenKind::TK_RPAREN, "function call");
            auto e = std::make_unique<Expr>();
            e->v   = Call{std::move(base), std::move(args)};
            e->loc = loc;
            base   = std::move(e);
            continue;
        }

        break;
    }
    return base;
}

std::vector<ExprPtr> Parser::parse_arg_list() {
    std::vector<ExprPtr> args;
    if (!check(TokenKind::TK_RPAREN)) {
        args.push_back(parse_expr());
        while (match(TokenKind::TK_COMMA))
            args.push_back(parse_expr());
    }
    return args;
}





ExprPtr Parser::parse_primary_expr() {
    SourceLoc loc = cur_.loc;
    auto e = std::make_unique<Expr>();
    e->loc = loc;

    switch (cur_.kind) {

        
        case TokenKind::TK_NULL:
            e->v = NullLit{};
            advance();
            return e;

        case TokenKind::TK_TRUE:
            e->v = BoolLit{true};
            advance();
            return e;

        case TokenKind::TK_FALSE:
            e->v = BoolLit{false};
            advance();
            return e;

        case TokenKind::TK_INT_LIT:
            e->v = IntLit{std::stoll(cur_.text)};
            advance();
            return e;

        case TokenKind::TK_FLOAT_LIT:
            e->v = FloatLit{std::stod(cur_.text)};
            advance();
            return e;

        case TokenKind::TK_STRING_LIT:
            e->v = StrLit{cur_.text};
            advance();
            return e;

        
        case TokenKind::TK_IDENT:
            e->v = Ident{cur_.text};
            advance();
            return e;

        
        case TokenKind::TK_LPAREN: {
            advance();
            auto inner = parse_expr();
            expect(TokenKind::TK_RPAREN, "grouped expression");
            return inner;
        }

        
        case TokenKind::TK_LBRACE:
            return parse_table_ctor();

        
        case TokenKind::TK_FUNCTION:
            return parse_func_expr();

        
        case TokenKind::TK_ALLOC: {
            advance();
            expect(TokenKind::TK_LPAREN, "alloc");
            auto count = parse_expr();
            expect(TokenKind::TK_RPAREN, "alloc");
            
            auto vt = std::make_unique<TypeNode>();
            vt->v  = PrimitiveType{"void"};
            e->v   = AllocExpr{std::move(vt), std::move(count)};
            return e;
        }

        case TokenKind::TK_ALLOC_TYPED: {
            advance();
            expect(TokenKind::TK_LPAREN, "alloc_typed");
            auto elem_type = parse_type();
            expect(TokenKind::TK_COMMA, "alloc_typed");
            auto count = parse_expr();
            expect(TokenKind::TK_RPAREN, "alloc_typed");
            e->v = AllocExpr{std::move(elem_type), std::move(count)};
            return e;
        }

        case TokenKind::TK_STACK_ALLOC: {
            advance();
            expect(TokenKind::TK_LPAREN, "stack_alloc");
            auto elem_type = parse_type();
            expect(TokenKind::TK_COMMA, "stack_alloc");
            auto count = parse_expr();
            expect(TokenKind::TK_RPAREN, "stack_alloc");
            e->v = AllocExpr{std::move(elem_type), std::move(count)};
            return e;
        }

        
        case TokenKind::TK_DEREF: {
            advance();
            expect(TokenKind::TK_LPAREN, "deref");
            auto ptr = parse_expr();
            expect(TokenKind::TK_RPAREN, "deref");
            e->v = DerefExpr{std::move(ptr)};
            return e;
        }

        
        case TokenKind::TK_ADDR: {
            advance();
            expect(TokenKind::TK_LPAREN, "addr");
            auto target = parse_expr();
            expect(TokenKind::TK_RPAREN, "addr");
            e->v = AddrExpr{std::move(target)};
            return e;
        }

        
        case TokenKind::TK_CAST: {
            advance();
            expect(TokenKind::TK_LPAREN, "cast");
            auto to   = parse_type();
            expect(TokenKind::TK_COMMA, "cast");
            auto expr = parse_expr();
            expect(TokenKind::TK_RPAREN, "cast");
            e->v = CastExpr{std::move(to), std::move(expr)};
            return e;
        }

        
        case TokenKind::TK_PTR_CAST: {
            advance();
            expect(TokenKind::TK_LPAREN, "ptr_cast");
            auto to   = parse_type();
            expect(TokenKind::TK_COMMA, "ptr_cast");
            auto expr = parse_expr();
            expect(TokenKind::TK_RPAREN, "ptr_cast");
            e->v = CastExpr{std::move(to), std::move(expr)};
            return e;
        }

        
        case TokenKind::TK_TYPEOF: {
            advance();
            expect(TokenKind::TK_LPAREN, "typeof");
            auto expr = parse_expr();
            expect(TokenKind::TK_RPAREN, "typeof");
            e->v = TypeofExpr{std::move(expr)};
            return e;
        }

        
        case TokenKind::TK_SIZEOF: {
            advance();
            expect(TokenKind::TK_LPAREN, "sizeof");
            auto t = parse_type();
            expect(TokenKind::TK_RPAREN, "sizeof");
            e->v = SizeofExpr{std::move(t)};
            return e;
        }

        default:
            diag_.error("E0001",
                "unexpected token '" + cur_.text + "' in expression",
                cur_.loc);
            advance();
            return nullptr;
    }
}






ExprPtr Parser::parse_table_ctor() {
    SourceLoc loc = advance().loc; 
    std::vector<TableCtor::Entry> entries;

    while (!check(TokenKind::TK_RBRACE) && !check(TokenKind::TK_EOF)) {

        TableCtor::Entry entry;

        
        if (check(TokenKind::TK_LBRACKET)) {
            advance();
            auto key = parse_expr();
            expect(TokenKind::TK_RBRACKET, "table key");
            expect(TokenKind::TK_ASSIGN,   "table entry");
            auto val    = parse_expr();
            entry.key   = std::move(key);
            entry.val   = std::move(val);
        }
        
        else if (check(TokenKind::TK_IDENT) && peek_.kind == TokenKind::TK_ASSIGN) {
            std::string field = advance().text; 
            advance();                          
            auto key = std::make_unique<Expr>();
            key->v   = StrLit{field};
            key->loc = loc;
            auto val    = parse_expr();
            entry.key   = std::move(key);
            entry.val   = std::move(val);
        }
        
        else {
            entry.val = parse_expr();
        }

        entries.push_back(std::move(entry));
        if (!match(TokenKind::TK_COMMA) && !match(TokenKind::TK_SEMICOLON))
            break;
    }

    expect(TokenKind::TK_RBRACE, "table constructor");

    auto e = std::make_unique<Expr>();
    e->v   = TableCtor{std::move(entries)};
    e->loc = loc;
    return e;
}






ExprPtr Parser::parse_func_expr() {
    SourceLoc loc = advance().loc; 

    expect(TokenKind::TK_LPAREN, "function expression params");
    std::vector<std::pair<std::string, TypeNodePtr>> params;
    if (!check(TokenKind::TK_RPAREN)) {
        do {
            std::string pname = expect(TokenKind::TK_IDENT, "param name").text;
            TypeNodePtr ptype;
            if (match(TokenKind::TK_COLON))
                ptype = parse_type();
            params.push_back({pname, std::move(ptype)});
        } while (match(TokenKind::TK_COMMA));
    }
    expect(TokenKind::TK_RPAREN, "function expression params");

    TypeNodePtr ret_type;
    if (match(TokenKind::TK_COLON))
        ret_type = parse_type();

    std::vector<StmtPtr> body;
    while (!check(TokenKind::TK_END) && !check(TokenKind::TK_EOF))
        if (auto st = parse_stmt()) body.push_back(std::move(st));
    expect(TokenKind::TK_END, "function expression");

    auto e = std::make_unique<Expr>();
    e->v   = FuncExpr{std::move(params), std::move(ret_type), std::move(body)};
    e->loc = loc;
    return e;
}





TypeNodePtr Parser::parse_type() {
    return parse_union_type();
}

TypeNodePtr Parser::parse_union_type() {
    auto first = parse_optional_type(parse_primary_type());
    if (!check(TokenKind::TK_PIPE)) return first;

    std::vector<TypeNodePtr> members;
    members.push_back(std::move(first));
    while (match(TokenKind::TK_PIPE))
        members.push_back(parse_optional_type(parse_primary_type()));

    auto t = std::make_unique<TypeNode>();
    t->v   = UnionType{std::move(members)};
    return t;
}

TypeNodePtr Parser::parse_optional_type(TypeNodePtr base) {
    if (!base) return base;
    if (match(TokenKind::TK_QUESTION)) {
        auto opt = std::make_unique<TypeNode>();
        opt->v   = OptionalType{std::move(base)};
        return opt;
    }
    return base;
}

TypeNodePtr Parser::parse_primary_type() {
    SourceLoc loc = cur_.loc;

    
    if (check(TokenKind::TK_LBRACE))
        return parse_record_type();

    
    if (check(TokenKind::TK_LPAREN))
        return parse_func_type();

    
    if (check(TokenKind::TK_IDENT)) {
        std::string name = advance().text;
        auto t  = std::make_unique<TypeNode>();
        t->loc  = loc;

        if (match(TokenKind::TK_LT)) {
            std::vector<TypeNodePtr> args;
            args.push_back(parse_type());
            while (match(TokenKind::TK_COMMA))
                args.push_back(parse_type());
            expect(TokenKind::TK_GT, "generic type arguments");
            t->v = GenericType{name, std::move(args)};
        } else {
            t->v = PrimitiveType{name};
        }
        return t;
    }

    diag_.error("E0001", "expected type, got '" + cur_.text + "'", loc);
    advance();
    return nullptr;
}

TypeNodePtr Parser::parse_record_type() {
    advance(); 
    std::vector<std::pair<std::string, TypeNodePtr>> fields;

    while (!check(TokenKind::TK_RBRACE) && !check(TokenKind::TK_EOF)) {
        std::string fname = expect(TokenKind::TK_IDENT, "record field name").text;
        expect(TokenKind::TK_COLON, "record field type");
        auto ftype = parse_type();
        fields.push_back({fname, std::move(ftype)});
        match(TokenKind::TK_COMMA);
    }
    expect(TokenKind::TK_RBRACE, "record type");

    auto t = std::make_unique<TypeNode>();
    t->v   = RecordType{std::move(fields)};
    return t;
}

TypeNodePtr Parser::parse_func_type() {
    advance(); 
    std::vector<TypeNodePtr> params;
    if (!check(TokenKind::TK_RPAREN)) {
        do {
            
            if (check(TokenKind::TK_IDENT) && peek_.kind == TokenKind::TK_COLON) {
                advance(); advance(); 
            }
            params.push_back(parse_type());
        } while (match(TokenKind::TK_COMMA));
    }
    expect(TokenKind::TK_RPAREN, "function type params");
    expect(TokenKind::TK_ARROW,  "function type return");
    auto ret = parse_type();

    auto t = std::make_unique<TypeNode>();
    t->v   = FuncType{std::move(params), std::move(ret)};
    return t;
}

TypeNodePtr Parser::parse_ptr_type() {
    
    
    return parse_primary_type();
}

} 
