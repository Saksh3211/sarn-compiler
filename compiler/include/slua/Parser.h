#pragma once
#include "Lexer.h"
#include "AST.h"
#include "Diagnostics.h"
#include <memory>
#include <vector>
#include <string>

namespace slua {
    class Parser {
    public:
        Parser(Lexer& lexer, DiagEngine& diag, CompileMode mode);
        std::unique_ptr<Module> parse_module(const std::string& filename);

    private:
        Lexer&      lex_;
        DiagEngine& diag_;
        CompileMode mode_;
        Token       cur_;
        Token       peek_;

        
        Token       advance();
        Token       expect(TokenKind k, const std::string& ctx);
        bool        check(TokenKind k) const;
        bool        match(TokenKind k);
        std::string token_kind_name(TokenKind k);

        
        StmtPtr parse_stmt();
        StmtPtr parse_assign_or_call();
        StmtPtr parse_local_decl();
        StmtPtr parse_const_decl();
        StmtPtr parse_global_decl();
        StmtPtr parse_func_decl(bool exported);
        StmtPtr parse_if_stmt();
        StmtPtr parse_while_stmt();
        StmtPtr parse_repeat_stmt();
        StmtPtr parse_numeric_for();
        StmtPtr parse_return_stmt();
        StmtPtr parse_defer_stmt();
        StmtPtr parse_import_decl();
        StmtPtr parse_type_decl();
        StmtPtr parse_extern_decl();
        StmtPtr parse_enum_decl();

        
        ExprPtr parse_expr();
        ExprPtr parse_or_expr();
        ExprPtr parse_and_expr();
        ExprPtr parse_cmp_expr();
        ExprPtr parse_concat_expr();
        ExprPtr parse_add_expr();
        ExprPtr parse_mul_expr();
        ExprPtr parse_unary_expr();
        ExprPtr parse_postfix_expr();
        ExprPtr parse_primary_expr();
        ExprPtr parse_table_ctor();
        ExprPtr parse_func_expr();
        std::vector<ExprPtr> parse_arg_list();

        
        TypeNodePtr parse_type();
        TypeNodePtr parse_union_type();
        TypeNodePtr parse_optional_type(TypeNodePtr base);
        TypeNodePtr parse_primary_type();
        TypeNodePtr parse_record_type();
        TypeNodePtr parse_func_type();
        TypeNodePtr parse_ptr_type();
    };

}
