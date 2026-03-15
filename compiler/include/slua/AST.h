#pragma once
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include "Lexer.h"

namespace slua {



struct TypeNode;
using TypeNodePtr = std::unique_ptr<TypeNode>;

struct PrimitiveType { std::string name; };              
struct OptionalType { TypeNodePtr inner; };             
struct UnionType { std::vector<TypeNodePtr> members; };
struct PtrType  { TypeNodePtr pointee; };           
struct FuncType { std::vector<TypeNodePtr> params; TypeNodePtr ret; };
struct RecordType  { std::vector<std::pair<std::string, TypeNodePtr>> fields; };
struct GenericType { std::string name; std::vector<TypeNodePtr> args; };
struct TupleType { std::vector<TypeNodePtr> members; };

struct TypeNode {
    using Variant = std::variant<
        PrimitiveType, OptionalType, UnionType,
        PtrType, FuncType, RecordType, GenericType, TupleType>;
    Variant v;
    SourceLoc loc;
};



struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct NullLit   {};
struct BoolLit   { bool val; };
struct IntLit    { int64_t val; };
struct FloatLit  { double val; };
struct StrLit    { std::string val; };
struct Ident     { std::string name; };
struct Binop     { std::string op; ExprPtr lhs, rhs; };
struct Unop      { std::string op; ExprPtr operand; };
struct Index     { ExprPtr table; ExprPtr key; };
struct Field     { ExprPtr table; std::string name; };
struct Call      { ExprPtr callee; std::vector<ExprPtr> args; };
struct MethodCall{ ExprPtr obj; std::string method; std::vector<ExprPtr> args; };
struct TableCtor { struct Entry { std::optional<ExprPtr> key; ExprPtr val; };
                std::vector<Entry> entries; };
struct FuncExpr  { std::vector<std::pair<std::string, TypeNodePtr>> params;
                TypeNodePtr ret_type;
                std::vector<std::unique_ptr<struct Stmt>> body; };
struct AllocExpr { TypeNodePtr elem_type; ExprPtr count; };
struct DerefExpr { ExprPtr ptr; };
struct AddrExpr  { ExprPtr target; };
struct CastExpr  { TypeNodePtr to; ExprPtr expr; };
struct TypeofExpr{ ExprPtr expr; };
struct SizeofExpr{ TypeNodePtr type; };

struct Expr {
    using Variant = std::variant<
        NullLit, BoolLit, IntLit, FloatLit, StrLit,
        Ident, Binop, Unop, Index, Field, Call, MethodCall,
        TableCtor, FuncExpr, AllocExpr, DerefExpr, AddrExpr,
        CastExpr, TypeofExpr, SizeofExpr>;
    Variant v;
    SourceLoc loc;
    TypeNodePtr inferred_type; 
};



struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;

struct LocalDecl  { std::string name; TypeNodePtr type_ann; ExprPtr init; bool is_const; };
struct GlobalDecl { std::string name; TypeNodePtr type_ann; ExprPtr init; };
struct Assign     { ExprPtr target; ExprPtr value; };
struct CallStmt   { ExprPtr call; };
struct DoBlock    { std::vector<StmtPtr> body; };
struct IfStmt     { ExprPtr cond; std::vector<StmtPtr> then_body;
                    std::vector<std::pair<ExprPtr, std::vector<StmtPtr>>> elseif_clauses;
                    std::optional<std::vector<StmtPtr>> else_body; };
struct WhileStmt  { ExprPtr cond; std::vector<StmtPtr> body; };
struct RepeatStmt { std::vector<StmtPtr> body; ExprPtr until_cond; };
struct NumericFor { std::string var; ExprPtr start, stop, step; std::vector<StmtPtr> body; };
struct CStyleFor  { std::string var; ExprPtr init, cond, step;  std::vector<StmtPtr> body; };
struct ReturnStmt { std::vector<ExprPtr> values; };
struct BreakStmt  {};
struct ContinueStmt{};
struct DeferStmt  { StmtPtr action; };
struct FuncDecl   { std::string name; bool exported;
                    std::vector<std::string> type_params;
                    std::vector<std::pair<std::string, TypeNodePtr>> params;
                    TypeNodePtr ret_type;
                    std::vector<StmtPtr> body; };
struct ExternDecl { std::string name; TypeNodePtr func_type; };
struct FileImportDecl { std::string path; };
struct ImportDecl { std::string module_name; };
struct PanicStmt  { ExprPtr msg; };
struct StoreStmt  { ExprPtr ptr; ExprPtr val; };
struct FreeStmt   { ExprPtr ptr; };
struct EnumDecl {
    std::string name;
    std::vector<std::pair<std::string, std::optional<int64_t>>> members;
};
struct MultiLocalDecl {
    std::vector<std::pair<std::string, TypeNodePtr>> vars;
    ExprPtr init;
};
struct TypeDecl   { std::string name;
                    std::vector<std::string> type_params;
                    TypeNodePtr def; };

struct Stmt {
    using Variant = std::variant<
        LocalDecl, GlobalDecl, Assign, CallStmt, DoBlock,
        IfStmt, WhileStmt, RepeatStmt, NumericFor,
        ReturnStmt, BreakStmt, ContinueStmt, DeferStmt, CStyleFor,
        FuncDecl, ExternDecl, ImportDecl, FileImportDecl, PanicStmt,
        StoreStmt, FreeStmt, TypeDecl, EnumDecl, MultiLocalDecl>;
    Variant v;
    SourceLoc loc;
};



struct Module {
    std::string           filename;
    CompileMode           mode;
    std::vector<StmtPtr>  stmts;
};

} 