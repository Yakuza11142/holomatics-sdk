#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <filesystem>
#include <queue>
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <cstdlib>

namespace fs = std::filesystem;

// ============================================================================
// 1. DIAGNOSTICS & SPAN TRACKER
// ============================================================================

struct SourceLocation {
    std::string file_path;
    size_t line;
    size_t col;

    std::string to_string() const {
        return file_path + ":" + std::to_string(line) + ":" + std::to_string(col);
    }
};

class CompilerException : public std::runtime_error {
public:
    CompilerException(const SourceLocation& loc, const std::string& msg)
        : std::runtime_error("[COMPILER ERROR] " + loc.to_string() + ": " + msg) {}
};

// ============================================================================
// 2. LEXICAL ANALYZER (LEXER)
// ============================================================================

enum class TokenType {
    TOKEN_IMPORT, TOKEN_FN, TOKEN_STRUCT, TOKEN_LET, TOKEN_RETURN, TOKEN_IF, TOKEN_ELSE,
    TOKEN_IDENT, TOKEN_INT_LIT, TOKEN_FLOAT_LIT, TOKEN_STRING_LIT,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_ASSIGN,
    TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_GREATER, TOKEN_DOT,
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_LBRACK, TOKEN_RBRACK,
    TOKEN_SEMI, TOKEN_COMMA, TOKEN_COLON, TOKEN_EOF
};

struct Token {
    TokenType type;
    std::string text;
    SourceLocation loc;
};

class Lexer {
    std::string src;
    SourceLocation loc;
    size_t pos = 0;
public:
    Lexer(std::string source, std::string path)
        : src(std::move(source)), loc{std::move(path), 1, 1} {}

    Token next_token() {
        while (pos < src.size()) {
            char current = src[pos];

            if (current == '\n') { loc.line++; loc.col = 1; pos++; continue; }
            if (std::isspace(static_cast<unsigned char>(current))) { loc.col++; pos++; continue; }

            // Line Comments
            if (current == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
                while (pos < src.size() && src[pos] != '\n') pos++;
                continue;
            }

            // Identifiers & Keywords
            if (std::isalpha(static_cast<unsigned char>(current)) || current == '_') {
                std::string ident;
                SourceLocation start_loc = loc;
                while (pos < src.size() && (std::isalnum(static_cast<unsigned char>(src[pos])) || src[pos] == '_')) {
                    ident += src[pos++]; loc.col++;
                }
                if (ident == "import") return {TokenType::TOKEN_IMPORT, ident, start_loc};
                if (ident == "fn") return {TokenType::TOKEN_FN, ident, start_loc};
                if (ident == "struct") return {TokenType::TOKEN_STRUCT, ident, start_loc};
                if (ident == "let") return {TokenType::TOKEN_LET, ident, start_loc};
                if (ident == "return") return {TokenType::TOKEN_RETURN, ident, start_loc};
                if (ident == "if") return {TokenType::TOKEN_IF, ident, start_loc};
                if (ident == "else") return {TokenType::TOKEN_ELSE, ident, start_loc};
                return {TokenType::TOKEN_IDENT, ident, start_loc};
            }

            // Numeric Literals
            if (std::isdigit(static_cast<unsigned char>(current))) {
                std::string num;
                SourceLocation start_loc = loc;
                bool is_float = false;
                while (pos < src.size() && (std::isdigit(static_cast<unsigned char>(src[pos])) || src[pos] == '.')) {
                    if (src[pos] == '.') is_float = true;
                    num += src[pos++]; loc.col++;
                }
                return {is_float ? TokenType::TOKEN_FLOAT_LIT : TokenType::TOKEN_INT_LIT, num, start_loc};
            }

            // String Literals
            if (current == '"') {
                std::string str;
                SourceLocation start_loc = loc;
                pos++; loc.col++;
                while (pos < src.size() && src[pos] != '"') {
                    str += src[pos++]; loc.col++;
                }
                if (pos < src.size()) { pos++; loc.col++; }
                return {TokenType::TOKEN_STRING_LIT, str, start_loc};
            }

            // Operators & Punctuation
            SourceLocation start_loc = loc;
            pos++; loc.col++;
            switch (current) {
                case '+': return {TokenType::TOKEN_PLUS, "+", start_loc};
                case '-': return {TokenType::TOKEN_MINUS, "-", start_loc};
                case '*': return {TokenType::TOKEN_STAR, "*", start_loc};
                case '/': return {TokenType::TOKEN_SLASH, "/", start_loc};
                case '.': return {TokenType::TOKEN_DOT, ".", start_loc};
                case '=':
                    if (pos < src.size() && src[pos] == '=') { pos++; loc.col++; return {TokenType::TOKEN_EQUAL, "==", start_loc}; }
                    return {TokenType::TOKEN_ASSIGN, "=", start_loc};
                case '<': return {TokenType::TOKEN_LESS, "<", start_loc};
                case '>': return {TokenType::TOKEN_GREATER, ">", start_loc};
                case '(': return {TokenType::TOKEN_LPAREN, "(", start_loc};
                case ')': return {TokenType::TOKEN_RPAREN, ")", start_loc};
                case '{': return {TokenType::TOKEN_LBRACE, "{", start_loc};
                case '}': return {TokenType::TOKEN_RBRACE, "}", start_loc};
                case '[': return {TokenType::TOKEN_LBRACK, "[", start_loc};
                case ']': return {TokenType::TOKEN_RBRACK, "]", start_loc};
                case ';': return {TokenType::TOKEN_SEMI, ";", start_loc};
                case ':': return {TokenType::TOKEN_COLON, ":", start_loc};
                case ',': return {TokenType::TOKEN_COMMA, ",", start_loc};
                default: throw CompilerException(start_loc, std::string("Unexpected character: ") + current);
            }
        }
        return {TokenType::TOKEN_EOF, "", loc};
    }
};

// ============================================================================
// 3. ABSTRACT SYNTAX TREE (AST)
// ============================================================================

enum class DataType { INT64, FLOAT64, VOID, STRUCT_TYPE };

struct Type {
    DataType kind = DataType::VOID;
    std::string struct_name = "";

    bool is_float() const { return kind == DataType::FLOAT64; }
    bool is_int() const { return kind == DataType::INT64; }

    std::string to_llvm() const {
        switch (kind) {
            case DataType::INT64: return "i64";
            case DataType::FLOAT64: return "double";
            case DataType::VOID: return "void";
            case DataType::STRUCT_TYPE: return "%struct." + struct_name;
        }
        return "i64";
    }

    std::string to_c() const {
        switch (kind) {
            case DataType::INT64: return "int64_t";
            case DataType::FLOAT64: return "double";
            case DataType::VOID: return "void";
            case DataType::STRUCT_TYPE: return struct_name + "_t";
        }
        return "int64_t";
    }
};

struct ASTNode {
    SourceLocation loc;
    virtual ~ASTNode() = default;
};

struct ExpressionNode : public ASTNode {
    Type evaluated_type{DataType::INT64, ""};
};

struct LiteralExprNode : public ExpressionNode {
    std::string value;
    LiteralExprNode(SourceLocation l, Type t, std::string v) { loc = l; evaluated_type = t; value = std::move(v); }
};

struct VariableExprNode : public ExpressionNode {
    std::string name;
    VariableExprNode(SourceLocation l, std::string n) { loc = l; name = std::move(n); }
};

struct MemberAccessExprNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> base;
    std::string member;
    MemberAccessExprNode(SourceLocation l, std::unique_ptr<ExpressionNode> b, std::string m)
        : base(std::move(b)), member(std::move(m)) { loc = l; }
};

struct ArrayIndexExprNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> base;
    std::unique_ptr<ExpressionNode> index;
    ArrayIndexExprNode(SourceLocation l, std::unique_ptr<ExpressionNode> b, std::unique_ptr<ExpressionNode> i)
        : base(std::move(b)), index(std::move(i)) { loc = l; }
};

struct BinaryExprNode : public ExpressionNode {
    std::string op;
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;
    BinaryExprNode(SourceLocation l, std::string o, std::unique_ptr<ExpressionNode> L, std::unique_ptr<ExpressionNode> R)
        : op(std::move(o)), left(std::move(L)), right(std::move(R)) { loc = l; }
};

struct CallExprNode : public ExpressionNode {
    std::string callee;
    std::vector<std::unique_ptr<ExpressionNode>> args;
    CallExprNode(SourceLocation l, std::string c, std::vector<std::unique_ptr<ExpressionNode>> a)
        : callee(std::move(c)), args(std::move(a)) { loc = l; }
};

struct StatementNode : public ASTNode {};

struct VarDeclStmtNode : public StatementNode {
    std::string var_name;
    Type var_type;
    std::unique_ptr<ExpressionNode> initializer;
    VarDeclStmtNode(SourceLocation l, std::string n, Type t, std::unique_ptr<ExpressionNode> init)
        : var_name(std::move(n)), var_type(t), initializer(std::move(init)) { loc = l; }
};

struct ReturnStmtNode : public StatementNode {
    std::unique_ptr<ExpressionNode> expr;
    ReturnStmtNode(SourceLocation l, std::unique_ptr<ExpressionNode> e) : expr(std::move(e)) { loc = l; }
};

struct BlockStmtNode : public StatementNode {
    std::vector<std::unique_ptr<StatementNode>> statements;
};

struct FunctionDeclNode : public ASTNode {
    std::string name;
    std::vector<std::pair<std::string, Type>> params;
    Type return_type;
    std::unique_ptr<BlockStmtNode> body;
};

struct StructDeclNode : public ASTNode {
    std::string name;
    std::vector<std::pair<std::string, Type>> fields;
};

struct ModuleAST {
    std::string file_path;
    std::string module_name;
    std::vector<std::string> imports;
    std::vector<std::unique_ptr<StructDeclNode>> structs;
    std::vector<std::unique_ptr<FunctionDeclNode>> functions;
};

// ============================================================================
// 4. PARSER IMPLEMENTATION
// ============================================================================

class Parser {
    Lexer lexer;
    Token current_token;

    void advance() { current_token = lexer.next_token(); }
    bool check(TokenType type) const { return current_token.type == type; }

    Token consume(TokenType type, const std::string& err) {
        if (!check(type)) throw CompilerException(current_token.loc, err);
        Token tok = current_token; advance(); return tok;
    }

    Type parse_type() {
        if (check(TokenType::TOKEN_IDENT)) {
            std::string t_name = current_token.text; advance();
            if (t_name == "int64" || t_name == "int") return Type{DataType::INT64, ""};
            if (t_name == "float" || t_name == "f64") return Type{DataType::FLOAT64, ""};
            if (t_name == "void") return Type{DataType::VOID, ""};
            return Type{DataType::STRUCT_TYPE, t_name};
        }
        return Type{DataType::INT64, ""};
    }

    std::unique_ptr<ExpressionNode> parse_primary() {
        SourceLocation loc = current_token.loc;
        if (check(TokenType::TOKEN_INT_LIT)) {
            std::string val = current_token.text; advance();
            return std::make_unique<LiteralExprNode>(loc, Type{DataType::INT64, ""}, val);
        }
        if (check(TokenType::TOKEN_FLOAT_LIT)) {
            std::string val = current_token.text; advance();
            return std::make_unique<LiteralExprNode>(loc, Type{DataType::FLOAT64, ""}, val);
        }
        if (check(TokenType::TOKEN_IDENT)) {
            std::string name = current_token.text; advance();
            if (check(TokenType::TOKEN_LPAREN)) {
                advance();
                std::vector<std::unique_ptr<ExpressionNode>> args;
                while (!check(TokenType::TOKEN_RPAREN) && !check(TokenType::TOKEN_EOF)) {
                    args.push_back(parse_expression());
                    if (check(TokenType::TOKEN_COMMA)) advance();
                }
                consume(TokenType::TOKEN_RPAREN, "Expected ')'");
                return std::make_unique<CallExprNode>(loc, name, std::move(args));
            }
            std::unique_ptr<ExpressionNode> expr = std::make_unique<VariableExprNode>(loc, name);
            while (check(TokenType::TOKEN_DOT) || check(TokenType::TOKEN_LBRACK)) {
                if (check(TokenType::TOKEN_DOT)) {
                    advance();
                    std::string member = consume(TokenType::TOKEN_IDENT, "Expected member name").text;
                    expr = std::make_unique<MemberAccessExprNode>(loc, std::move(expr), member);
                } else if (check(TokenType::TOKEN_LBRACK)) {
                    advance();
                    auto idx = parse_expression();
                    consume(TokenType::TOKEN_RBRACK, "Expected ']'");
                    expr = std::make_unique<ArrayIndexExprNode>(loc, std::move(expr), std::move(idx));
                }
            }
            return expr;
        }
        if (check(TokenType::TOKEN_LPAREN)) {
            advance();
            auto expr = parse_expression();
            consume(TokenType::TOKEN_RPAREN, "Expected ')'");
            return expr;
        }
        throw CompilerException(loc, "Invalid expression syntax near " + current_token.text);
    }

    std::unique_ptr<ExpressionNode> parse_expression() {
        auto left = parse_primary();
        while (check(TokenType::TOKEN_PLUS) || check(TokenType::TOKEN_MINUS) ||
               check(TokenType::TOKEN_STAR) || check(TokenType::TOKEN_SLASH) ||
               check(TokenType::TOKEN_EQUAL) || check(TokenType::TOKEN_LESS) || check(TokenType::TOKEN_GREATER)) {
            SourceLocation loc = current_token.loc;
            std::string op = current_token.text; advance();
            auto right = parse_primary();
            left = std::make_unique<BinaryExprNode>(loc, op, std::move(left), std::move(right));
        }
        return left;
    }

    std::unique_ptr<StatementNode> parse_statement() {
        SourceLocation loc = current_token.loc;
        if (check(TokenType::TOKEN_LET)) {
            advance();
            std::string vname = consume(TokenType::TOKEN_IDENT, "Expected variable name").text;
            Type vtype{DataType::INT64, ""};
            if (check(TokenType::TOKEN_COLON)) { advance(); vtype = parse_type(); }
            consume(TokenType::TOKEN_ASSIGN, "Expected '='");
            auto init = parse_expression();
            if (check(TokenType::TOKEN_SEMI)) advance();
            return std::make_unique<VarDeclStmtNode>(loc, vname, vtype, std::move(init));
        }
        if (check(TokenType::TOKEN_RETURN)) {
            advance();
            auto expr = parse_expression();
            if (check(TokenType::TOKEN_SEMI)) advance();
            return std::make_unique<ReturnStmtNode>(loc, std::move(expr));
        }
        throw CompilerException(loc, "Unsupported statement near " + current_token.text);
    }

public:
    Parser(Lexer l) : lexer(std::move(l)) { advance(); }

    ModuleAST parse_module(const std::string& file_path, const std::string& mod_name) {
        ModuleAST module; module.file_path = file_path; module.module_name = mod_name;

        while (!check(TokenType::TOKEN_EOF)) {
            if (check(TokenType::TOKEN_IMPORT)) {
                advance();
                std::string imp = consume(TokenType::TOKEN_STRING_LIT, "Expected import path").text;
                if (check(TokenType::TOKEN_SEMI)) advance();
                module.imports.push_back(imp);
            } else if (check(TokenType::TOKEN_STRUCT)) {
                advance();
                std::string sname = consume(TokenType::TOKEN_IDENT, "Expected struct name").text;
                consume(TokenType::TOKEN_LBRACE, "Expected '{'");
                auto st = std::make_unique<StructDeclNode>(); st->loc = current_token.loc; st->name = sname;
                while (!check(TokenType::TOKEN_RBRACE) && !check(TokenType::TOKEN_EOF)) {
                    std::string fname = consume(TokenType::TOKEN_IDENT, "Expected field name").text;
                    consume(TokenType::TOKEN_COLON, "Expected ':'");
                    Type ftype = parse_type();
                    st->fields.push_back({fname, ftype});
                    if (check(TokenType::TOKEN_COMMA)) advance();
                }
                consume(TokenType::TOKEN_RBRACE, "Expected '}'");
                module.structs.push_back(std::move(st));
            } else if (check(TokenType::TOKEN_FN)) {
                advance();
                std::string fname = consume(TokenType::TOKEN_IDENT, "Expected function name").text;
                consume(TokenType::TOKEN_LPAREN, "Expected '('");
                auto fn = std::make_unique<FunctionDeclNode>(); fn->loc = current_token.loc; fn->name = fname;
                while (!check(TokenType::TOKEN_RPAREN) && !check(TokenType::TOKEN_EOF)) {
                    std::string pname = consume(TokenType::TOKEN_IDENT, "Expected param name").text;
                    consume(TokenType::TOKEN_COLON, "Expected ':'");
                    Type ptype = parse_type();
                    fn->params.push_back({pname, ptype});
                    if (check(TokenType::TOKEN_COMMA)) advance();
                }
                consume(TokenType::TOKEN_RPAREN, "Expected ')'");
                fn->return_type = Type{DataType::INT64, ""};
                if (check(TokenType::TOKEN_COLON)) { advance(); fn->return_type = parse_type(); }

                consume(TokenType::TOKEN_LBRACE, "Expected '{'");
                fn->body = std::make_unique<BlockStmtNode>();
                while (!check(TokenType::TOKEN_RBRACE) && !check(TokenType::TOKEN_EOF)) {
                    fn->body->statements.push_back(parse_statement());
                }
                consume(TokenType::TOKEN_RBRACE, "Expected '}'");
                module.functions.push_back(std::move(fn));
            } else {
                advance();
            }
        }
        return module;
    }
};

// ============================================================================
// 5. DEPENDENCY GRAPH SOLVER
// ============================================================================

class DependencySolver {
    std::unordered_map<std::string, ModuleAST> modules;
    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_map<std::string, int> in_degree;
public:
    void add_module(ModuleAST mod) {
        std::string name = mod.module_name;
        modules[name] = std::move(mod);
        if (in_degree.find(name) == in_degree.end()) in_degree[name] = 0;
        for (const auto& imp : modules[name].imports) {
            adj[imp].push_back(name);
            in_degree[name]++;
        }
    }

    std::vector<std::string> resolve() {
        std::queue<std::string> q;
        for (const auto& [mod, deg] : in_degree) if (deg == 0) q.push(mod);
        std::vector<std::string> order;
        while (!q.empty()) {
            std::string curr = q.front(); q.pop();
            order.push_back(curr);
            for (const auto& next : adj[curr]) {
                in_degree[next]--;
                if (in_degree[next] == 0) q.push(next);
            }
        }
        if (order.size() != modules.size()) {
            throw std::runtime_error("Circular module dependency detected!");
        }
        return order;
    }

    const ModuleAST& get(const std::string& name) const { return modules.at(name); }
};

// ============================================================================
// 6. SCOPED SYMBOL TABLE & PERFECT LLVM GENERATOR
// ============================================================================

struct SymbolInfo {
    std::string llvm_ptr;
    Type type;
};

class ScopeTable {
    std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;
public:
    void push_scope() { scopes.emplace_back(); }
    void pop_scope() { if (!scopes.empty()) scopes.pop_back(); }

    void insert(const std::string& name, const SymbolInfo& info) {
        if (scopes.empty()) push_scope();
        scopes.back()[name] = info;
    }

    SymbolInfo* lookup(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->find(name) != it->end()) return &((*it)[name]);
        }
        return nullptr;
    }
};

class ProductionLLVMGenerator {
    std::stringstream ir;
    std::stringstream header;
    size_t reg_counter = 0;
    ScopeTable scope_table;
    std::unordered_map<std::string, StructDeclNode> known_structs;

    std::string new_reg() { return "%r" + std::to_string(reg_counter++); }

    std::pair<std::string, Type> lower_expression(ExpressionNode* expr) {
        if (auto lit = dynamic_cast<LiteralExprNode*>(expr)) {
            return {lit->value, lit->evaluated_type};
        }
        if (auto var = dynamic_cast<VariableExprNode*>(expr)) {
            SymbolInfo* sym = scope_table.lookup(var->name);
            if (!sym) throw CompilerException(var->loc, "Undefined variable identifier: " + var->name);
            std::string reg = new_reg();
            ir << "  " << reg << " = load " << sym->type.to_llvm() << ", " << sym->type.to_llvm() << "* " << sym->llvm_ptr << "\n";
            return {reg, sym->type};
        }
        if (auto mem = dynamic_cast<MemberAccessExprNode*>(expr)) {
            auto [base_reg, base_type] = lower_expression(mem->base.get());
            if (base_type.kind != DataType::STRUCT_TYPE) {
                throw CompilerException(mem->loc, "Member access target is not a struct type");
            }
            if (known_structs.find(base_type.struct_name) == known_structs.end()) {
                throw CompilerException(mem->loc, "Unknown struct definition: " + base_type.struct_name);
            }

            const auto& st = known_structs[base_type.struct_name];
            int field_idx = -1;
            Type field_type{DataType::INT64, ""};

            for (size_t i = 0; i < st.fields.size(); ++i) {
                if (st.fields[i].first == mem->member) {
                    field_idx = static_cast<int>(i);
                    field_type = st.fields[i].second;
                    break;
                }
            }
            if (field_idx == -1) throw CompilerException(mem->loc, "Field '" + mem->member + "' not found in struct " + base_type.struct_name);

            std::string gep_reg = new_reg();
            ir << "  " << gep_reg << " = getelementptr inbounds " << base_type.to_llvm() << ", " << base_type.to_llvm() << "* " << base_reg
               << ", i32 0, i32 " << field_idx << "\n";
            std::string load_reg = new_reg();
            ir << "  " << load_reg << " = load " << field_type.to_llvm() << ", " << field_type.to_llvm() << "* " << gep_reg << "\n";
            return {load_reg, field_type};
        }
        if (auto bin = dynamic_cast<BinaryExprNode*>(expr)) {
            auto [L_reg, L_type] = lower_expression(bin->left.get());
            auto [R_reg, R_type] = lower_expression(bin->right.get());

            Type target_type = L_type;
            if (L_type.is_float() || R_type.is_float()) {
                target_type = Type{DataType::FLOAT64, ""};
                if (L_type.is_int()) {
                    std::string conv = new_reg();
                    ir << "  " << conv << " = sitofp i64 " << L_reg << " to double\n";
                    L_reg = conv;
                }
                if (R_type.is_int()) {
                    std::string conv = new_reg();
                    ir << "  " << conv << " = sitofp i64 " << R_reg << " to double\n";
                    R_reg = conv;
                }
            }

            std::string reg = new_reg();
            std::string llvm_op = target_type.is_float() ? "fadd" : "add";
            if (bin->op == "-") llvm_op = target_type.is_float() ? "fsub" : "sub";
            if (bin->op == "*") llvm_op = target_type.is_float() ? "fmul" : "mul";
            if (bin->op == "/") llvm_op = target_type.is_float() ? "fdiv" : "sdiv";

            ir << "  " << reg << " = " << llvm_op << " " << target_type.to_llvm() << " " << L_reg << ", " << R_reg << "\n";
            return {reg, target_type};
        }
        if (auto call = dynamic_cast<CallExprNode*>(expr)) {
            std::vector<std::string> arg_regs;
            for (auto& arg : call->args) arg_regs.push_back(lower_expression(arg.get()).first);
            std::string reg = new_reg();
            ir << "  " << reg << " = call i64 @" << call->callee << "(";
            for (size_t i = 0; i < arg_regs.size(); ++i) {
                ir << "i64 " << arg_regs[i] << (i + 1 < arg_regs.size() ? ", " : "");
            }
            ir << ")\n";
            return {reg, Type{DataType::INT64, ""}};
        }
        return {"0", Type{DataType::INT64, ""}};
    }

public:
    ProductionLLVMGenerator() {
        ir << "; Tesseract Assembly Output\n";
        ir << "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"\n";
        ir << "target triple = \"aarch64-unknown-linux-android\"\n\n";

        header << "/* Auto-generated C-FFI Header for Tesseract Engine */\n";
        header << "#ifndef TESS_CORE_H\n#define TESS_CORE_H\n\n#include <stdint.h>\n#include <stdbool.h>\n\n";
        header << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";
    }

    void register_struct(const StructDeclNode& st) {
        known_structs[st.name] = st;
    }

    void emit_struct(const StructDeclNode& st) {
        ir << "%struct." << st.name << " = type { ";
        header << "typedef struct " << st.name << " {\n";
        for (size_t i = 0; i < st.fields.size(); ++i) {
            ir << st.fields[i].second.to_llvm() << (i + 1 < st.fields.size() ? ", " : "");
            header << "    " << st.fields[i].second.to_c() << " " << st.fields[i].first << ";\n";
        }
        ir << " }\n\n";
        header << "} " << st.name << "_t;\n\n";
    }

    void emit_function(const FunctionDeclNode& fn) {
        reg_counter = 0;
        scope_table.push_scope();

        ir << "define " << fn.return_type.to_llvm() << " @" << fn.name << "(";
        header << fn.return_type.to_c() << " " << fn.name << "(";

        for (size_t i = 0; i < fn.params.size(); ++i) {
            ir << fn.params[i].second.to_llvm() << " %p_" << fn.params[i].first << (i + 1 < fn.params.size() ? ", " : "");
            header << fn.params[i].second.to_c() << " " << fn.params[i].first << (i + 1 < fn.params.size() ? ", " : "");
        }
        ir << ") {\nentry:\n";
        header << ");\n";

        for (const auto& param : fn.params) {
            std::string ptr = "%" + param.first + ".addr";
            ir << "  " << ptr << " = alloca " << param.second.to_llvm() << "\n";
            ir << "  store " << param.second.to_llvm() << " %p_" << param.first << ", " << param.second.to_llvm() << "* " << ptr << "\n";
            scope_table.insert(param.first, {ptr, param.second});
        }

        if (fn.body) {
            for (auto& stmt : fn.body->statements) {
                if (auto var_decl = dynamic_cast<VarDeclStmtNode*>(stmt.get())) {
                    std::string ptr = "%" + var_decl->var_name + ".addr";
                    ir << "  " << ptr << " = alloca " << var_decl->var_type.to_llvm() << "\n";
                    auto [val_reg, val_type] = lower_expression(var_decl->initializer.get());
                    ir << "  store " << var_decl->var_type.to_llvm() << " " << val_reg << ", " << var_decl->var_type.to_llvm() << "* " << ptr << "\n";
                    scope_table.insert(var_decl->var_name, {ptr, var_decl->var_type});
                } else if (auto ret = dynamic_cast<ReturnStmtNode*>(stmt.get())) {
                    auto [ret_reg, ret_type] = lower_expression(ret->expr.get());

                    if (fn.return_type.is_float() && ret_type.is_int()) {
                        std::string conv = new_reg();
                        ir << "  " << conv << " = sitofp i64 " << ret_reg << " to double\n";
                        ret_reg = conv;
                    } else if (fn.return_type.is_int() && ret_type.is_float()) {
                        std::string conv = new_reg();
                        ir << "  " << conv << " = fptosi double " << ret_reg << " to i64\n";
                        ret_reg = conv;
                    }

                    ir << "  ret " << fn.return_type.to_llvm() << " " << ret_reg << "\n";
                }
            }
        }
        ir << "}\n\n";
        scope_table.pop_scope();
    }

    void finalize(const std::string& ir_file_path, const std::string& header_file_path) {
        header << "\n#ifdef __cplusplus\n}\n#endif\n#endif /* TESS_CORE_H */\n";

        std::ofstream ir_out(ir_file_path); ir_out << ir.str(); ir_out.close();
        std::ofstream h_out(header_file_path); h_out << header.str(); h_out.close();
    }
};

// ============================================================================
// 7. COMPILER MAIN DRIVER (Builds IR and invokes Clang for .so generation)
// ============================================================================

int main(int argc, char* argv[]) {
    try {
        std::string workspace_dir = ".";
        if (argc > 1) workspace_dir = argv[1];

        std::cout << "[Tesseract Compiler]: Indexing workspace: " << fs::absolute(workspace_dir) << "\n";

        DependencySolver solver;
        size_t count = 0;

        for (const auto& entry : fs::directory_iterator(workspace_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".tess") {
                std::string path = entry.path().string();
                std::string mod_name = entry.path().stem().string();

                std::ifstream stream(path); if (!stream) continue;
                std::stringstream ss; ss << stream.rdbuf();

                Lexer lexer(ss.str(), path);
                Parser parser(std::move(lexer));
                solver.add_module(parser.parse_module(path, mod_name));
                count++;
            }
        }

        std::cout << "[Tesseract Compiler]: Loaded " << count << " source files.\n";
        std::cout << "[Tesseract Compiler]: Resolving module dependencies...\n";
        std::vector<std::string> order = solver.resolve();

        ProductionLLVMGenerator gen;

        for (const auto& mod : order) {
            const auto& m = solver.get(mod);
            for (const auto& st : m.structs) gen.register_struct(*st);
        }

        for (const auto& mod : order) {
            const auto& m = solver.get(mod);
            for (const auto& st : m.structs) gen.emit_struct(*st);
            for (const auto& fn : m.functions) gen.emit_function(*fn);
        }

        gen.finalize("tesseract_master.ll", "tess_core.h");
        std::cout << "[Tesseract Compiler]: Successfully output 'tesseract_master.ll' and 'tess_core.h'.\n";

        std::cout << "[Tesseract Compiler]: Compiling dynamic shared library (libtesseract.so) via Clang...\n";
        int compile_res = std::system("clang -shared -fPIC tesseract_master.ll -o libtesseract.so");

        if (compile_res == 0) {
            std::cout << "[Tesseract Compiler]: BUILD SUCCESS -> libtesseract.so created!\n";
        } else {
            std::cerr << "[Tesseract Compiler]: Build error -> Clang failed to compile libtesseract.so.\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "\n" << e.what() << "\n";
        return 1;
    }
    return 0;
}
