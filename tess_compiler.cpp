#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <queue>
#include <filesystem>
#include <cctype>
#include <cstdlib>

namespace fs = std::filesystem;

// ============================================================================
// 1. COMPILER CONFIGURATION & CLI OPTIONS
// ============================================================================

enum class OutputFormat {
    OBJECT_FILE,     // .o / .obj
    SHARED_LIB,      // .so / .dylib / .dll
    STATIC_LIB,      // .a / .lib
    EXECUTABLE       // binary / .exe
};

struct CompilerConfig {
    std::string entry_file;
    std::string output_file = "tesseract_out";
    OutputFormat format = OutputFormat::OBJECT_FILE;
    std::string target_triple = ""; // Empty string means native host target
    bool verbose = false;
};

// ============================================================================
// 2. SOURCE LOCATIONS & TOKENS
// ============================================================================

struct SourceLocation {
    std::string filename;
    size_t line = 1;
    size_t col = 1;
};

enum class TokenType {
    TOKEN_IMPORT, TOKEN_FN, TOKEN_STRUCT, TOKEN_LET, TOKEN_RETURN,
    TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, TOKEN_IDENT, TOKEN_INT_LIT,
    TOKEN_FLOAT_LIT, TOKEN_STRING_LIT, TOKEN_TYPE, TOKEN_LPAREN, TOKEN_RPAREN,
    TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_SEMICOLON, TOKEN_COLON, TOKEN_COMMA, TOKEN_DOT,
    TOKEN_ASSIGN, TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
    TOKEN_EQ, TOKEN_NEQ, TOKEN_LT, TOKEN_GT, TOKEN_EOF, TOKEN_ERROR
};

struct Token {
    TokenType type;
    std::string value;
    SourceLocation loc;
    bool is_public = false;
};

// ============================================================================
// 3. LEXER
// ============================================================================

class Lexer {
private:
    std::string src;
    SourceLocation loc;
    size_t pos = 0;

    char peek() const { return (pos >= src.size()) ? '\0' : src[pos]; }

    char advance() {
        if (pos >= src.size()) return '\0';
        char c = src[pos++];
        if (c == '\n') { loc.line++; loc.col = 1; } else { loc.col++; }
        return c;
    }

    void skip_whitespace_and_comments() {
        while (pos < src.size()) {
            char c = peek();
            if (std::isspace(static_cast<unsigned char>(c))) {
                advance();
            } else if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
                while (pos < src.size() && peek() != '\n') advance();
            } else {
                break;
            }
        }
    }

public:
    Lexer(std::string source, std::string filename) : src(std::move(source)) {
        loc.filename = std::move(filename);
    }

    Token next_token() {
        skip_whitespace_and_comments();
        if (pos >= src.size()) return {TokenType::TOKEN_EOF, "", loc};

        SourceLocation start_loc = loc;
        char c = peek();

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string ident;
            while (pos < src.size() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
                ident += advance();
            }

            if (ident == "pub") {
                skip_whitespace_and_comments();
                if (peek() == 'f' && pos + 1 < src.size() && src[pos + 1] == 'n') {
                    advance(); advance();
                    return {TokenType::TOKEN_FN, "pub fn", start_loc, true};
                }
                if (peek() == 's' && pos + 6 <= src.size() && src.substr(pos, 6) == "struct") {
                    for (int i = 0; i < 6; ++i) advance();
                    return {TokenType::TOKEN_STRUCT, "pub struct", start_loc, true};
                }
            }

            if (ident == "import") return {TokenType::TOKEN_IMPORT, ident, start_loc};
            if (ident == "fn") return {TokenType::TOKEN_FN, ident, start_loc};
            if (ident == "struct") return {TokenType::TOKEN_STRUCT, ident, start_loc};
            if (ident == "let") return {TokenType::TOKEN_LET, ident, start_loc};
            if (ident == "return") return {TokenType::TOKEN_RETURN, ident, start_loc};
            if (ident == "if") return {TokenType::TOKEN_IF, ident, start_loc};
            if (ident == "else") return {TokenType::TOKEN_ELSE, ident, start_loc};
            if (ident == "while") return {TokenType::TOKEN_WHILE, ident, start_loc};
            if (ident == "i64" || ident == "f64" || ident == "void" || ident == "bool") {
                return {TokenType::TOKEN_TYPE, ident, start_loc};
            }

            return {TokenType::TOKEN_IDENT, ident, start_loc};
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::string num;
            bool is_float = false;
            while (pos < src.size() && (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.')) {
                if (peek() == '.') {
                    if (is_float) break;
                    is_float = true;
                }
                num += advance();
            }
            return {is_float ? TokenType::TOKEN_FLOAT_LIT : TokenType::TOKEN_INT_LIT, num, start_loc};
        }

        if (c == '"') {
            advance();
            std::string str;
            while (pos < src.size() && peek() != '"') {
                if (peek() == '\\') advance();
                str += advance();
            }
            if (peek() == '"') advance();
            return {TokenType::TOKEN_STRING_LIT, str, start_loc};
        }

        advance();
        switch (c) {
            case '(': return {TokenType::TOKEN_LPAREN, "(", start_loc};
            case ')': return {TokenType::TOKEN_RPAREN, ")", start_loc};
            case '{': return {TokenType::TOKEN_LBRACE, "{", start_loc};
            case '}': return {TokenType::TOKEN_RBRACE, "}", start_loc};
            case '[': return {TokenType::TOKEN_LBRACKET, "[", start_loc};
            case ']': return {TokenType::TOKEN_RBRACKET, "]", start_loc};
            case ';': return {TokenType::TOKEN_SEMICOLON, ";", start_loc};
            case ':': return {TokenType::TOKEN_COLON, ":", start_loc};
            case ',': return {TokenType::TOKEN_COMMA, ",", start_loc};
            case '.': return {TokenType::TOKEN_DOT, ".", start_loc};
            case '+': return {TokenType::TOKEN_PLUS, "+", start_loc};
            case '-': return {TokenType::TOKEN_MINUS, "-", start_loc};
            case '*': return {TokenType::TOKEN_STAR, "*", start_loc};
            case '/': return {TokenType::TOKEN_SLASH, "/", start_loc};
            case '=': 
                if (peek() == '=') { advance(); return {TokenType::TOKEN_EQ, "==", start_loc}; }
                return {TokenType::TOKEN_ASSIGN, "=", start_loc};
            case '!':
                if (peek() == '=') { advance(); return {TokenType::TOKEN_NEQ, "!=", start_loc}; }
                break;
            case '<': return {TokenType::TOKEN_LT, "<", start_loc};
            case '>': return {TokenType::TOKEN_GT, ">", start_loc};
        }

        return {TokenType::TOKEN_ERROR, std::string(1, c), start_loc};
    }
};

// ============================================================================
// 4. AST NODES
// ============================================================================

enum class DataTypeKind { VOID, INT64, FLOAT64, BOOL, STRUCT };

struct TypeNode {
    DataTypeKind kind = DataTypeKind::VOID;
    std::string struct_name;

    std::string to_llvm_type() const {
        switch (kind) {
            case DataTypeKind::INT64: return "i64";
            case DataTypeKind::FLOAT64: return "double";
            case DataTypeKind::BOOL: return "i1";
            case DataTypeKind::STRUCT: return "%struct." + struct_name;
            default: return "void";
        }
    }
};

struct ASTNode { virtual ~ASTNode() = default; };
struct ExpressionNode : public ASTNode {};

struct LiteralExprNode : public ExpressionNode {
    TypeNode type;
    std::string value;
    LiteralExprNode(TypeNode t, std::string v) : type(t), value(std::move(v)) {}
};

struct IdentExprNode : public ExpressionNode {
    std::string name;
    explicit IdentExprNode(std::string n) : name(std::move(n)) {}
};

struct BinaryExprNode : public ExpressionNode {
    std::string op;
    std::unique_ptr<ExpressionNode> left, right;
    BinaryExprNode(std::string o, std::unique_ptr<ExpressionNode> l, std::unique_ptr<ExpressionNode> r)
        : op(std::move(o)), left(std::move(l)), right(std::move(r)) {}
};

struct MemberAccessExprNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> object;
    std::string member;
    MemberAccessExprNode(std::unique_ptr<ExpressionNode> obj, std::string m)
        : object(std::move(obj)), member(std::move(m)) {}
};

struct CallExprNode : public ExpressionNode {
    std::string callee;
    std::vector<std::unique_ptr<ExpressionNode>> args;
    CallExprNode(std::string c, std::vector<std::unique_ptr<ExpressionNode>> a)
        : callee(std::move(c)), args(std::move(a)) {}
};

struct StatementNode : public ASTNode {};

struct VarDeclStmtNode : public StatementNode {
    std::string name;
    TypeNode type;
    std::unique_ptr<ExpressionNode> initializer;
    VarDeclStmtNode(std::string n, TypeNode t, std::unique_ptr<ExpressionNode> init)
        : name(std::move(n)), type(t), initializer(std::move(init)) {}
};

struct ReturnStmtNode : public StatementNode {
    std::unique_ptr<ExpressionNode> expr;
    explicit ReturnStmtNode(std::unique_ptr<ExpressionNode> e) : expr(std::move(e)) {}
};

struct ExpressionStmtNode : public StatementNode {
    std::unique_ptr<ExpressionNode> expr;
    explicit ExpressionStmtNode(std::unique_ptr<ExpressionNode> e) : expr(std::move(e)) {}
};

struct BlockStmtNode : public StatementNode {
    std::vector<std::unique_ptr<StatementNode>> statements;
};

struct FieldDecl { std::string name; TypeNode type; };

struct StructDeclNode : public ASTNode {
    std::string name;
    bool is_pub = false;
    std::vector<FieldDecl> fields;
};

struct ParamDecl { std::string name; TypeNode type; };

struct FunctionDeclNode : public ASTNode {
    std::string name;
    bool is_pub = false;
    std::vector<ParamDecl> params;
    TypeNode return_type;
    std::unique_ptr<BlockStmtNode> body;
};

struct ModuleASTNode : public ASTNode {
    std::string module_path;
    std::vector<std::string> imports;
    std::vector<std::unique_ptr<StructDeclNode>> structs;
    std::vector<std::unique_ptr<FunctionDeclNode>> functions;
};

// ============================================================================
// 5. PARSER
// ============================================================================

class Parser {
private:
    Lexer lexer;
    Token current_token;

    void advance() { current_token = lexer.next_token(); }
    bool check(TokenType type) const { return current_token.type == type; }
    bool match(TokenType type) { if (check(type)) { advance(); return true; } return false; }

    void expect(TokenType type, const std::string& err) {
        if (!match(type)) {
            std::cerr << "[" << current_token.loc.filename << ":" << current_token.loc.line 
                      << ":" << current_token.loc.col << "] Error: " << err << "\n";
            std::exit(EXIT_FAILURE);
        }
    }

    TypeNode parse_type() {
        TypeNode type;
        if (current_token.value == "i64") type.kind = DataTypeKind::INT64;
        else if (current_token.value == "f64") type.kind = DataTypeKind::FLOAT64;
        else if (current_token.value == "bool") type.kind = DataTypeKind::BOOL;
        else if (current_token.value == "void") type.kind = DataTypeKind::VOID;
        else { type.kind = DataTypeKind::STRUCT; type.struct_name = current_token.value; }
        advance();
        return type;
    }

    std::unique_ptr<ExpressionNode> parse_primary() {
        if (check(TokenType::TOKEN_INT_LIT)) {
            auto node = std::make_unique<LiteralExprNode>(TypeNode{DataTypeKind::INT64, ""}, current_token.value);
            advance(); return node;
        }
        if (check(TokenType::TOKEN_FLOAT_LIT)) {
            auto node = std::make_unique<LiteralExprNode>(TypeNode{DataTypeKind::FLOAT64, ""}, current_token.value);
            advance(); return node;
        }
        if (check(TokenType::TOKEN_IDENT)) {
            std::string name = current_token.value; advance();
            if (check(TokenType::TOKEN_LPAREN)) {
                advance();
                std::vector<std::unique_ptr<ExpressionNode>> args;
                if (!check(TokenType::TOKEN_RPAREN)) {
                    do { args.push_back(parse_expression()); } while (match(TokenType::TOKEN_COMMA));
                }
                expect(TokenType::TOKEN_RPAREN, "Expected ')' after arguments");
                return std::make_unique<CallExprNode>(name, std::move(args));
            }
            return std::make_unique<IdentExprNode>(name);
        }
        expect(TokenType::TOKEN_ERROR, "Unexpected primary expression");
        return nullptr;
    }

    std::unique_ptr<ExpressionNode> parse_postfix() {
        auto expr = parse_primary();
        while (check(TokenType::TOKEN_DOT)) {
            advance();
            std::string member = current_token.value;
            expect(TokenType::TOKEN_IDENT, "Expected member name after '.'");
            expr = std::make_unique<MemberAccessExprNode>(std::move(expr), member);
        }
        return expr;
    }

    std::unique_ptr<ExpressionNode> parse_expression() {
        auto left = parse_postfix();
        if (check(TokenType::TOKEN_PLUS) || check(TokenType::TOKEN_MINUS) ||
            check(TokenType::TOKEN_STAR) || check(TokenType::TOKEN_SLASH) ||
            check(TokenType::TOKEN_EQ) || check(TokenType::TOKEN_LT) || check(TokenType::TOKEN_GT)) {
            std::string op = current_token.value; advance();
            auto right = parse_expression();
            return std::make_unique<BinaryExprNode>(op, std::move(left), std::move(right));
        }
        return left;
    }

    std::unique_ptr<StatementNode> parse_statement() {
        if (match(TokenType::TOKEN_LET)) {
            std::string name = current_token.value;
            expect(TokenType::TOKEN_IDENT, "Expected variable name");
            expect(TokenType::TOKEN_COLON, "Expected ':' after variable name");
            TypeNode type = parse_type();
            std::unique_ptr<ExpressionNode> init = nullptr;
            if (match(TokenType::TOKEN_ASSIGN)) init = parse_expression();
            expect(TokenType::TOKEN_SEMICOLON, "Expected ';' after variable declaration");
            return std::make_unique<VarDeclStmtNode>(name, type, std::move(init));
        }
        if (match(TokenType::TOKEN_RETURN)) {
            std::unique_ptr<ExpressionNode> expr = nullptr;
            if (!check(TokenType::TOKEN_SEMICOLON)) expr = parse_expression();
            expect(TokenType::TOKEN_SEMICOLON, "Expected ';' after return");
            return std::make_unique<ReturnStmtNode>(std::move(expr));
        }
        auto expr = parse_expression();
        expect(TokenType::TOKEN_SEMICOLON, "Expected ';' after statement");
        return std::make_unique<ExpressionStmtNode>(std::move(expr));
    }

    std::unique_ptr<BlockStmtNode> parse_block() {
        expect(TokenType::TOKEN_LBRACE, "Expected '{'");
        auto block = std::make_unique<BlockStmtNode>();
        while (!check(TokenType::TOKEN_RBRACE) && !check(TokenType::TOKEN_EOF)) {
            block->statements.push_back(parse_statement());
        }
        expect(TokenType::TOKEN_RBRACE, "Expected '}'");
        return block;
    }

public:
    Parser(Lexer l) : lexer(std::move(l)) { advance(); }

    std::unique_ptr<ModuleASTNode> parse_module(const std::string& filepath) {
        auto module_ast = std::make_unique<ModuleASTNode>();
        module_ast->module_path = filepath;

        while (!check(TokenType::TOKEN_EOF)) {
            if (match(TokenType::TOKEN_IMPORT)) {
                std::string import_path = current_token.value;
                expect(TokenType::TOKEN_STRING_LIT, "Expected string literal for import");
                expect(TokenType::TOKEN_SEMICOLON, "Expected ';' after import");
                module_ast->imports.push_back(import_path);
            } else if (check(TokenType::TOKEN_STRUCT)) {
                bool is_pub = current_token.is_public; advance();
                std::string name = current_token.value;
                expect(TokenType::TOKEN_IDENT, "Expected struct name");
                expect(TokenType::TOKEN_LBRACE, "Expected '{'");
                
                auto struct_node = std::make_unique<StructDeclNode>();
                struct_node->name = name; struct_node->is_pub = is_pub;

                while (!check(TokenType::TOKEN_RBRACE) && !check(TokenType::TOKEN_EOF)) {
                    std::string fname = current_token.value;
                    expect(TokenType::TOKEN_IDENT, "Expected field name");
                    expect(TokenType::TOKEN_COLON, "Expected ':'");
                    TypeNode ftype = parse_type();
                    expect(TokenType::TOKEN_SEMICOLON, "Expected ';'");
                    struct_node->fields.push_back({fname, ftype});
                }
                expect(TokenType::TOKEN_RBRACE, "Expected '}'");
                module_ast->structs.push_back(std::move(struct_node));
            } else if (check(TokenType::TOKEN_FN)) {
                bool is_pub = current_token.is_public; advance();
                std::string name = current_token.value;
                expect(TokenType::TOKEN_IDENT, "Expected function name");
                expect(TokenType::TOKEN_LPAREN, "Expected '('");

                auto fn_node = std::make_unique<FunctionDeclNode>();
                fn_node->name = name; fn_node->is_pub = is_pub;

                if (!check(TokenType::TOKEN_RPAREN)) {
                    do {
                        std::string pname = current_token.value;
                        expect(TokenType::TOKEN_IDENT, "Expected parameter name");
                        expect(TokenType::TOKEN_COLON, "Expected ':'");
                        TypeNode ptype = parse_type();
                        fn_node->params.push_back({pname, ptype});
                    } while (match(TokenType::TOKEN_COMMA));
                }
                expect(TokenType::TOKEN_RPAREN, "Expected ')'");

                if (match(TokenType::TOKEN_COLON)) fn_node->return_type = parse_type();
                else fn_node->return_type = TypeNode{DataTypeKind::VOID, ""};

                fn_node->body = parse_block();
                module_ast->functions.push_back(std::move(fn_node));
            } else {
                advance();
            }
        }
        return module_ast;
    }
};

// ============================================================================
// 6. DEPENDENCY RESOLVER
// ============================================================================

class DependencyResolver {
public:
    static std::vector<std::string> resolve_compilation_order(const std::string& root_file) {
        std::map<std::string, std::vector<std::string>> adj;
        std::map<std::string, int> in_degree;
        std::set<std::string> visited;

        std::queue<std::string> worklist;
        worklist.push(fs::canonical(root_file).string());

        while (!worklist.empty()) {
            std::string curr = worklist.front(); worklist.pop();
            if (visited.count(curr)) continue;
            visited.insert(curr);
            if (!in_degree.count(curr)) in_degree[curr] = 0;

            std::ifstream file(curr);
            if (!file.is_open()) {
                std::cerr << "Dependency Error: Cannot open file " << curr << "\n";
                std::exit(EXIT_FAILURE);
            }

            std::stringstream buffer; buffer << file.rdbuf();
            Lexer lexer(buffer.str(), curr);
            Parser parser(lexer);
            auto ast = parser.parse_module(curr);

            for (const auto& imp : ast->imports) {
                fs::path parent = fs::path(curr).parent_path();
                std::string resolved_imp = fs::canonical(parent / imp).string();
                adj[resolved_imp].push_back(curr);
                in_degree[curr]++;
                if (!visited.count(resolved_imp)) worklist.push(resolved_imp);
            }
        }

        std::queue<std::string> q;
        for (const auto& [node, deg] : in_degree) if (deg == 0) q.push(node);

        std::vector<std::string> ordered;
        while (!q.empty()) {
            std::string u = q.front(); q.pop();
            ordered.push_back(u);
            for (const auto& v : adj[u]) {
                if (--in_degree[v] == 0) q.push(v);
            }
        }

        if (ordered.size() != visited.size()) {
            std::cerr << "Dependency Error: Cyclic imports detected!\n";
            std::exit(EXIT_FAILURE);
        }

        return ordered;
    }
};

// ============================================================================
// 7. MULTI-TARGET LLVM IR CODE GENERATOR
// ============================================================================

class SymbolScope {
public:
    std::map<std::string, std::string> var_allocas;
    std::map<std::string, TypeNode> var_types;
};

class ProductionLLVMGenerator {
private:
    size_t reg_counter = 1;
    std::stringstream ir;
    std::vector<SymbolScope> scope_table;
    std::map<std::string, StructDeclNode*> struct_table;

    std::string new_reg() { return "%" + std::to_string(reg_counter++); }
    void push_scope() { scope_table.emplace_back(); }
    void pop_scope() { scope_table.pop_back(); }

    void register_variable(const std::string& name, const std::string& reg, TypeNode type) {
        scope_table.back().var_allocas[name] = reg;
        scope_table.back().var_types[name] = type;
    }

    std::pair<std::string, TypeNode> lookup_variable(const std::string& name) {
        for (auto it = scope_table.rbegin(); it != scope_table.rend(); ++it) {
            if (it->var_allocas.count(name)) return {it->var_allocas[name], it->var_types[name]};
        }
        return {"", {DataTypeKind::VOID, ""}};
    }

    std::string lower_expression(ExpressionNode* expr, TypeNode& out_type) {
        if (auto lit = dynamic_cast<LiteralExprNode*>(expr)) {
            out_type = lit->type; return lit->value;
        }

        if (auto ident = dynamic_cast<IdentExprNode*>(expr)) {
            auto [reg, type] = lookup_variable(ident->name);
            out_type = type;
            std::string load_reg = new_reg();
            ir << "  " << load_reg << " = load " << type.to_llvm_type() << ", " 
               << type.to_llvm_type() << "* " << reg << "\n";
            return load_reg;
        }

        if (auto bin = dynamic_cast<BinaryExprNode*>(expr)) {
            TypeNode lt, rt;
            std::string lreg = lower_expression(bin->left.get(), lt);
            std::string rreg = lower_expression(bin->right.get(), rt);
            out_type = lt;

            std::string res_reg = new_reg();
            if (bin->op == "+") ir << "  " << res_reg << " = add " << lt.to_llvm_type() << " " << lreg << ", " << rreg << "\n";
            else if (bin->op == "-") ir << "  " << res_reg << " = sub " << lt.to_llvm_type() << " " << lreg << ", " << rreg << "\n";
            else if (bin->op == "*") ir << "  " << res_reg << " = mul " << lt.to_llvm_type() << " " << lreg << ", " << rreg << "\n";
            else if (bin->op == "/") ir << "  " << res_reg << " = sdiv " << lt.to_llvm_type() << " " << lreg << ", " << rreg << "\n";
            return res_reg;
        }

        if (auto member = dynamic_cast<MemberAccessExprNode*>(expr)) {
            TypeNode obj_type; std::string obj_ref;
            if (auto ident = dynamic_cast<IdentExprNode*>(member->object.get())) {
                auto [reg, type] = lookup_variable(ident->name);
                obj_ref = reg; obj_type = type;
            } else {
                obj_ref = lower_expression(member->object.get(), obj_type);
            }

            StructDeclNode* st = struct_table[obj_type.struct_name];
            int field_idx = -1; TypeNode field_type;
            for (size_t i = 0; i < st->fields.size(); ++i) {
                if (st->fields[i].name == member->member) {
                    field_idx = static_cast<int>(i); field_type = st->fields[i].type; break;
                }
            }

            std::string gep_reg = new_reg();
            ir << "  " << gep_reg << " = getelementptr inbounds " << obj_type.to_llvm_type()
               << ", " << obj_type.to_llvm_type() << "* " << obj_ref << ", i32 0, i32 " << field_idx << "\n";

            std::string val_reg = new_reg();
            ir << "  " << val_reg << " = load " << field_type.to_llvm_type() << ", "
               << field_type.to_llvm_type() << "* " << gep_reg << "\n";

            out_type = field_type; return val_reg;
        }

        if (auto call = dynamic_cast<CallExprNode*>(expr)) {
            std::vector<std::string> compiled_args;
            std::vector<TypeNode> arg_types;
            for (const auto& arg : call->args) {
                TypeNode t;
                compiled_args.push_back(lower_expression(arg.get(), t));
                arg_types.push_back(t);
            }

            std::string res_reg = new_reg();
            ir << "  " << res_reg << " = call i64 @" << call->callee << "(";
            for (size_t i = 0; i < compiled_args.size(); ++i) {
                ir << arg_types[i].to_llvm_type() << " " << compiled_args[i];
                if (i + 1 < compiled_args.size()) ir << ", ";
            }
            ir << ")\n";
            out_type = TypeNode{DataTypeKind::INT64, ""};
            return res_reg;
        }

        return "";
    }

    void lower_statement(StatementNode* stmt) {
        if (auto vdecl = dynamic_cast<VarDeclStmtNode*>(stmt)) {
            std::string alloc_reg = new_reg();
            ir << "  " << alloc_reg << " = alloca " << vdecl->type.to_llvm_type() << "\n";
            register_variable(vdecl->name, alloc_reg, vdecl->type);

            if (vdecl->initializer) {
                TypeNode init_type;
                std::string init_reg = lower_expression(vdecl->initializer.get(), init_type);
                ir << "  store " << vdecl->type.to_llvm_type() << " " << init_reg 
                   << ", " << vdecl->type.to_llvm_type() << "* " << alloc_reg << "\n";
            }
        } else if (auto ret = dynamic_cast<ReturnStmtNode*>(stmt)) {
            if (ret->expr) {
                TypeNode ret_type;
                std::string ret_reg = lower_expression(ret->expr.get(), ret_type);
                ir << "  ret " << ret_type.to_llvm_type() << " " << ret_reg << "\n";
            } else {
                ir << "  ret void\n";
            }
        } else if (auto estmt = dynamic_cast<ExpressionStmtNode*>(stmt)) {
            TypeNode dummy; lower_expression(estmt->expr.get(), dummy);
        }
    }

public:
    void emit_header(const std::string& target_triple) {
        ir << "; ModuleID = 'tesseract_master'\n";
        if (!target_triple.empty()) {
            ir << "target triple = \"" << target_triple << "\"\n";
        }
        ir << "\n";
    }

    void emit_struct(const StructDeclNode& st) {
        struct_table[st.name] = const_cast<StructDeclNode*>(&st);
        ir << "%struct." << st.name << " = type { ";
        for (size_t i = 0; i < st.fields.size(); ++i) {
            ir << st.fields[i].type.to_llvm_type();
            if (i + 1 < st.fields.size()) ir << ", ";
        }
        ir << " }\n\n";
    }

    void emit_function(const FunctionDeclNode& fn) {
        push_scope(); reg_counter = 1;
        ir << "define " << fn.return_type.to_llvm_type() << " @" << fn.name << "(";
        for (size_t i = 0; i < fn.params.size(); ++i) {
            ir << fn.params[i].type.to_llvm_type() << " %" << fn.params[i].name;
            if (i + 1 < fn.params.size()) ir << ", ";
        }
        ir << ") {\nentry:\n";

        for (const auto& param : fn.params) {
            std::string alloc_reg = new_reg();
            ir << "  " << alloc_reg << " = alloca " << param.type.to_llvm_type() << "\n";
            ir << "  store " << param.type.to_llvm_type() << " %" << param.name 
               << ", " << param.type.to_llvm_type() << "* " << alloc_reg << "\n";
            register_variable(param.name, alloc_reg, param.type);
        }

        for (const auto& stmt : fn.body->statements) lower_statement(stmt.get());

        if (fn.return_type.kind == DataTypeKind::VOID) ir << "  ret void\n";
        else if (fn.return_type.kind == DataTypeKind::INT64) ir << "  ret i64 0\n";
        else if (fn.return_type.kind == DataTypeKind::FLOAT64) ir << "  ret double 0.0\n";

        ir << "}\n\n";
        pop_scope();
    }

    std::string get_ir() const { return ir.str(); }
};

// ============================================================================
// 8. C-FFI HEADER GENERATOR
// ============================================================================

class CHeaderGenerator {
public:
    static std::string generate_c_header(const std::vector<std::unique_ptr<ModuleASTNode>>& modules) {
        std::stringstream header;
        header << "#ifndef TESSERACT_CORE_H\n#define TESSERACT_CORE_H\n\n";
        header << "#include <stdint.h>\n#include <stdbool.h>\n\n";
        header << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";

        for (const auto& mod : modules) {
            for (const auto& st : mod->structs) {
                if (!st->is_pub) continue;
                header << "typedef struct {\n";
                for (const auto& f : st->fields) {
                    std::string ctype = "int64_t";
                    if (f.type.kind == DataTypeKind::FLOAT64) ctype = "double";
                    else if (f.type.kind == DataTypeKind::BOOL) ctype = "bool";
                    header << "    " << ctype << " " << f.name << ";\n";
                }
                header << "} " << st->name << ";\n\n";
            }

            for (const auto& fn : mod->functions) {
                if (!fn->is_pub) continue;
                std::string ret_type = "int64_t";
                if (fn->return_type.kind == DataTypeKind::VOID) ret_type = "void";
                else if (fn->return_type.kind == DataTypeKind::FLOAT64) ret_type = "double";

                header << ret_type << " " << fn->name << "(";
                for (size_t i = 0; i < fn->params.size(); ++i) {
                    std::string ptype = "int64_t";
                    if (fn->params[i].type.kind == DataTypeKind::FLOAT64) ptype = "double";
                    header << ptype << " " << fn->params[i].name;
                    if (i + 1 < fn->params.size()) header << ", ";
                }
                header << ");\n";
            }
        }

        header << "\n#ifdef __cplusplus\n}\n#endif\n\n#endif\n";
        return header.str();
    }
};

// ============================================================================
// 9. CROSS-PLATFORM DRIVER & CLI PARSER
// ============================================================================

CompilerConfig parse_cli_args(int argc, char* argv[]) {
    CompilerConfig config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            config.output_file = argv[++i];
        } else if (arg == "--target" && i + 1 < argc) {
            config.target_triple = argv[++i];
        } else if (arg == "--emit-so" || arg == "--emit-dylib") {
            config.format = OutputFormat::SHARED_LIB;
        } else if (arg == "--emit-static") {
            config.format = OutputFormat::STATIC_LIB;
        } else if (arg == "--emit-exe") {
            config.format = OutputFormat::EXECUTABLE;
        } else if (arg == "--emit-obj") {
            config.format = OutputFormat::OBJECT_FILE;
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg[0] != '-') {
            config.entry_file = arg;
        }
    }
    return config;
}

int main(int argc, char* argv[]) {
    CompilerConfig config = parse_cli_args(argc, argv);

    if (config.entry_file.empty()) {
        std::cout << "Usage: tess_compiler <entrypoint.tess> [options]\n\n"
                  << "Options:\n"
                  << "  -o <file>          Specify output filename\n"
                  << "  --target <triple>  Specify target triple (e.g. aarch64-linux-android, x86_64-pc-linux-gnu)\n"
                  << "  --emit-obj         Output object file (.o / .obj) [Default]\n"
                  << "  --emit-so          Output shared library (.so / .dylib / .dll)\n"
                  << "  --emit-static      Output static archive (.a / .lib)\n"
                  << "  --emit-exe         Output standalone binary executable\n";
        return EXIT_FAILURE;
    }

    std::cout << "[TESS] Resolving dependencies for: " << config.entry_file << "\n";
    auto compilation_order = DependencyResolver::resolve_compilation_order(config.entry_file);
    std::vector<std::unique_ptr<ModuleASTNode>> parsed_modules;

    ProductionLLVMGenerator llvm_gen;
    llvm_gen.emit_header(config.target_triple);

    for (const auto& file : compilation_order) {
        if (config.verbose) std::cout << "[TESS] Compiling module AST: " << file << "\n";
        std::ifstream ifs(file);
        std::stringstream ss; ss << ifs.rdbuf();

        Lexer lexer(ss.str(), file);
        Parser parser(lexer);
        auto mod = parser.parse_module(file);

        for (const auto& st : mod->structs) llvm_gen.emit_struct(*st);
        for (const auto& fn : mod->functions) llvm_gen.emit_function(*fn);

        parsed_modules.push_back(std::move(mod));
    }

    std::cout << "[TESS] Emitting LLVM IR to 'tesseract_master.ll'...\n";
    std::ofstream ir_file("tesseract_master.ll");
    ir_file << llvm_gen.get_ir();
    ir_file.close();

    std::cout << "[TESS] Generating C Header 'tess_core.h'...\n";
    std::ofstream h_file("tess_core.h");
    h_file << CHeaderGenerator::generate_c_header(parsed_modules);
    h_file.close();

    // System compilation execution string
    std::string clang_cmd = "clang -O3 ";
    if (!config.target_triple.empty()) {
        clang_cmd += "--target=" + config.target_triple + " ";
    }

    switch (config.format) {
        case OutputFormat::OBJECT_FILE:
            clang_cmd += "-c tesseract_master.ll -o " + config.output_file + ".o";
            break;
        case OutputFormat::SHARED_LIB:
            clang_cmd += "-shared -fPIC tesseract_master.ll -o " + config.output_file + ".so";
            break;
        case OutputFormat::STATIC_LIB:
            clang_cmd += "-c tesseract_master.ll -o temp_tess.o && ar rcs " + config.output_file + ".a temp_tess.o";
            break;
        case OutputFormat::EXECUTABLE:
            clang_cmd += "tesseract_master.ll -o " + config.output_file;
            break;
    }

    std::cout << "[TESS] Running backend toolchain: " << clang_cmd << "\n";
    int status = std::system(clang_cmd.c_str());

    if (status != 0) {
        std::cerr << "[TESS Error] Clang invocation failed. Ensure LLVM/Clang is installed on host path.\n";
        return EXIT_FAILURE;
    }

    std::cout << "[TESS] Build Succeeded!\n";
    return EXIT_SUCCESS;
}
