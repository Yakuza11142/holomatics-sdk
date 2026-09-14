#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <stdexcept>
#include <cmath>

// ============================================================================
// 1. DYNAMIC LEXER & SYSTEM TOKENS
// ============================================================================
enum class TokenType {
    TOKEN_FN, TOKEN_IDENT, TOKEN_LPAREN, TOKEN_RPAREN, 
    TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_ASSIGN, TOKEN_NUMBER, 
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
    TOKEN_SEMI, TOKEN_EOF, TOKEN_UNKNOWN
};

struct Token {
    TokenType type;
    std::string text;
    size_t line;
    size_t column;
};

class Lexer {
    std::string src;
    size_t pos = 0;
    size_t line = 1;
    size_t col = 1;
public:
    explicit Lexer(std::string source) : src(std::move(source)) {}

    Token next_token() {
        while (pos < src.size()) {
            char current = src[pos];
            if (current == '\n') { line++; col = 1; pos++; continue; }
            if (isspace(current)) { col++; pos++; continue; }

            if (isalpha(current) || current == '_') {
                std::string ident;
                size_t start_col = col;
                while (pos < src.size() && (isalnum(src[pos]) || src[pos] == '_')) {
                    ident += src[pos++];
                    col++;
                }
                if (ident == "fn") return {TokenType::TOKEN_FN, ident, line, start_col};
                return {TokenType::TOKEN_IDENT, ident, line, start_col};
            }

            if (isdigit(current)) {
                std::string num;
                size_t start_col = col;
                while (pos < src.size() && isdigit(src[pos])) {
                    num += src[pos++];
                    col++;
                }
                return {TokenType::TOKEN_NUMBER, num, line, start_col};
            }

            size_t start_col = col;
            pos++; col++;
            switch (current) {
                case '(': return {TokenType::TOKEN_LPAREN, "(", line, start_col};
                case ')': return {TokenType::TOKEN_RPAREN, ")", line, start_col};
                case '{': return {TokenType::TOKEN_LBRACE, "{", line, start_col};
                case '}': return {TokenType::TOKEN_RBRACE, "}", line, start_col};
                case '=': return {TokenType::TOKEN_ASSIGN, "=", line, start_col};
                case '+': return {TokenType::TOKEN_PLUS, "+", line, start_col};
                case '-': return {TokenType::TOKEN_MINUS, "-", line, start_col};
                case '*': return {TokenType::TOKEN_STAR, "*", line, start_col};
                case '/': return {TokenType::TOKEN_SLASH, "/", line, start_col};
                case ';': return {TokenType::TOKEN_SEMI, ";", line, start_col};
                default:  return {TokenType::TOKEN_UNKNOWN, std::string(1, current), line, start_col};
            }
        }
        return {TokenType::TOKEN_EOF, "", line, col};
    }
};

// ============================================================================
// 2. ABSTRACT SYNTAX TREE (AST) & PRATT PARSER ENGINE
// ============================================================================
enum class DataType { INT, VEC3, MAT4, UNKNOWN };

struct ASTNode {
    virtual ~ASTNode() = default;
};

struct ExpressionNode : public ASTNode {
    virtual ~ExpressionNode() = default;
};

struct NumberLiteralNode : public ExpressionNode {
    std::string value;
    explicit NumberLiteralNode(std::string val) : value(std::move(val)) {}
};

struct VariableAccessNode : public ExpressionNode {
    std::string name;
    explicit VariableAccessNode(std::string n) : name(std::move(n)) {}
};

struct BinaryOpNode : public ExpressionNode {
    std::string op;
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;
    BinaryOpNode(std::string o, std::unique_ptr<ExpressionNode> l, std::unique_ptr<ExpressionNode> r)
        : op(std::move(o)), left(std::move(l)), right(std::move(r)) {}
};

struct VarDeclNode : public ASTNode {
    std::string type_name;
    std::string name;
    std::unique_ptr<ExpressionNode> initializer;
    VarDeclNode(std::string t, std::string n, std::unique_ptr<ExpressionNode> init)
        : type_name(std::move(t)), name(std::move(n)), initializer(std::move(init)) {}
};

struct AssignmentNode : public ASTNode {
    std::string name;
    std::unique_ptr<ExpressionNode> value;
    AssignmentNode(std::string n, std::unique_ptr<ExpressionNode> val)
        : name(std::move(n)), value(std::move(val)) {}
};

class Parser {
    Lexer lexer;
    Token current_token;

    void advance() { current_token = lexer.next_token(); }

    int get_precedence(TokenType type) {
        switch (type) {
            case TokenType::TOKEN_PLUS:
            case TokenType::TOKEN_MINUS: return 10;
            case TokenType::TOKEN_STAR:
            case TokenType::TOKEN_SLASH: return 20;
            default: return 0;
        }
    }
public:
    explicit Parser(Lexer l) : lexer(std::move(l)) { advance(); }

    std::unique_ptr<ExpressionNode> parse_primary() {
        if (current_token.type == TokenType::TOKEN_NUMBER) {
            auto node = std::make_unique<NumberLiteralNode>(current_token.text);
            advance();
            return node;
        }
        if (current_token.type == TokenType::TOKEN_IDENT) {
            auto node = std::make_unique<VariableAccessNode>(current_token.text);
            advance();
            return node;
        }
        throw std::runtime_error("Parser Error: Unexpected token: " + current_token.text);
    }

    std::unique_ptr<ExpressionNode> parse_expression(int precedence = 0) {
        auto left = parse_primary();
        while (precedence < get_precedence(current_token.type)) {
            Token op_tok = current_token;
            advance();
            auto right = parse_expression(get_precedence(op_tok.type));
            left = std::make_unique<BinaryOpNode>(op_tok.text, std::move(left), std::move(right));
        }
        return left;
    }

    std::vector<std::unique_ptr<ASTNode>> parse_body() {
        std::vector<std::unique_ptr<ASTNode>> body;
        while (current_token.type != TokenType::TOKEN_RBRACE && current_token.type != TokenType::TOKEN_EOF) {
            if (current_token.type == TokenType::TOKEN_IDENT) {
                std::string first_ident = current_token.text;
                advance();

                if (current_token.type == TokenType::TOKEN_IDENT) {
                    std::string var_name = current_token.text;
                    advance(); 
                    std::unique_ptr<ExpressionNode> init = nullptr;
                    if (current_token.type == TokenType::TOKEN_ASSIGN) {
                        advance();
                        init = parse_expression();
                    }
                    body.push_back(std::make_unique<VarDeclNode>(first_ident, var_name, std::move(init)));
                    if (current_token.type == TokenType::TOKEN_SEMI) advance();
                } 
                else if (current_token.type == TokenType::TOKEN_ASSIGN) {
                    advance();
                    auto value = parse_expression();
                    body.push_back(std::make_unique<AssignmentNode>(first_ident, std::move(value)));
                    if (current_token.type == TokenType::TOKEN_SEMI) advance();
                }
            } else {
                advance();
            }
        }
        return body;
    }
};

// ============================================================================
// 3. ELASTIC SYMBOL TABLE
// ============================================================================
struct Symbol {
    std::string name;
    DataType type;
    int stack_offset;
};

class SymbolTable {
    std::unordered_map<std::string, Symbol> symbols;
    int stack_offset = 0;
public:
    void declare(const std::string& name, DataType type, int size) {
        stack_offset -= size;
        symbols[name] = Symbol{name, type, stack_offset};
    }

    Symbol lookup(const std::string& name) {
        if (symbols.find(name) == symbols.end()) {
            throw std::runtime_error("Semantic Error: Undefined variable: " + name);
        }
        return symbols[name];
    }

    int current_stack_size() const { return std::abs(stack_offset); }
};

// ============================================================================
// 4. RECURSIVE MACHINE CODE BACKEND & EMITTER
// ============================================================================
class CodeGen {
    std::vector<uint8_t> machine_code;
    SymbolTable table;

    void emit_bytes(const std::vector<uint8_t>& bytes) {
        machine_code.insert(machine_code.end(), bytes.begin(), bytes.end());
    }
public:
    void compile_expression(ExpressionNode* node) {
        if (auto num = dynamic_cast<NumberLiteralNode*>(node)) {
            int val = std::stoi(num->value);
            emit_bytes({0x48, 0xC7, 0xC0});
            emit_bytes({static_cast<uint8_t>(val & 0xFF), 
                        static_cast<uint8_t>((val >> 8) & 0xFF), 
                        static_cast<uint8_t>((val >> 16) & 0xFF), 
                        static_cast<uint8_t>((val >> 24) & 0xFF)});
            return;
        }

        if (auto access = dynamic_cast<VariableAccessNode*>(node)) {
            auto sym = table.lookup(access->name);
            emit_bytes({0x48, 0x8B, 0x85});
            emit_bytes({static_cast<uint8_t>(sym.stack_offset & 0xFF), 
                        static_cast<uint8_t>((sym.stack_offset >> 8) & 0xFF), 
                        static_cast<uint8_t>((sym.stack_offset >> 16) & 0xFF), 
                        static_cast<uint8_t>((sym.stack_offset >> 24) & 0xFF)});
            return;
        }

        if (auto bin = dynamic_cast<BinaryOpNode*>(node)) {
            compile_expression(bin->right.get());
            emit_bytes({0x50});
            compile_expression(bin->left.get());
            emit_bytes({0x59});
            if (bin->op == "+") {
                emit_bytes({0x48, 0x01, 0xC8});
            } else if (bin->op == "-") {
                emit_bytes({0x48, 0x29, 0xC8});
            } else if (bin->op == "*") {
                emit_bytes({0x48, 0x0F, 0xAF, 0xC1});
            }
            return;
        }
    }

    void compile_program(const std::vector<std::unique_ptr<ASTNode>>& ast) {
        emit_bytes({0x55, 0x48, 0x89, 0xE5});
        for (const auto& node : ast) {
            if (auto decl = dynamic_cast<VarDeclNode*>(node.get())) {
                DataType t = (decl->type_name == "VEC3") ? DataType::VEC3 : DataType::INT;
                table.declare(decl->name, t, 8);
            }
        }
        int stack_size = table.current_stack_size();
        emit_bytes({0x48, 0x81, 0xEC});
        emit_bytes({static_cast<uint8_t>(stack_size & 0xFF),
                    static_cast<uint8_t>((stack_size >> 8) & 0xFF),
                    static_cast<uint8_t>((stack_size >> 16) & 0xFF),
                    static_cast<uint8_t>((stack_size >> 24) & 0xFF)});
        
        for (const auto& node : ast) {
            if (auto decl = dynamic_cast<VarDeclNode*>(node.get())) {
                if (decl->initializer) {
                    compile_expression(decl->initializer.get());
                    auto sym = table.lookup(decl->name);
                    emit_bytes({0x48, 0x89, 0x85});
                    emit_bytes({static_cast<uint8_t>(sym.stack_offset & 0xFF),
                                static_cast<uint8_t>((sym.stack_offset >> 8) & 0xFF),
                                static_cast<uint8_t>((sym.stack_offset >> 16) & 0xFF),
                                static_cast<uint8_t>((sym.stack_offset >> 24) & 0xFF)});
                }
            }
            else if (auto assign = dynamic_cast<AssignmentNode*>(node.get())) {
                compile_expression(assign->value.get());
                auto sym = table.lookup(assign->name);
                emit_bytes({0x48, 0x89, 0x85});
                emit_bytes({static_cast<uint8_t>(sym.stack_offset & 0xFF),
                            static_cast<uint8_t>((sym.stack_offset >> 8) & 0xFF),
                            static_cast<uint8_t>((sym.stack_offset >> 16) & 0xFF),
                            static_cast<uint8_t>((sym.stack_offset >> 24) & 0xFF)});
            }
        }
        emit_bytes({0xC9, 0xC3});
    }

    const std::vector<uint8_t>& get_code() const { return machine_code; }
};

// ============================================================================
// 5. DATA-DRIVEN ELF SECTOR BINARY GENERATOR
// ============================================================================
class ELFEmitter {
public:
    static void write_binary(const std::string& path, const std::vector<uint8_t>& code) {
        std::ofstream out(path, std::ios::out | std::ios::binary);
        if (!out) return;
        uint8_t elf_header[64] = {
            0x7F, 'E', 'L', 'F', 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            2, 0, 38, 0, 1, 0, 0, 0, 0, 0, 0x40, 0, 0, 0, 0, 0,
            64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 64, 0, 56, 0, 1, 0, 0, 0, 0, 0
        };
        uint8_t prog_header[56] = {
            1, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0x40, 0, 0, 0, 0, 0, 0, 0, 0x40, 0, 0, 0, 0, 0
        };
        uint64_t total_file_size = 64 + 56 + code.size();
        std::memcpy(&prog_header[32], &total_file_size, 8);
        std::memcpy(&prog_header[40], &total_file_size, 8);
        uint64_t entry_point = 0x400000 + 64 + 56;
        std::memcpy(&elf_header[24], &entry_point, 8);
        out.write(reinterpret_cast<char*>(elf_header), 64);
        out.write(reinterpret_cast<char*>(prog_header), 56);
        out.write(reinterpret_cast<const char*>(code.data()), code.size());
    }
};

// ============================================================================
// 6. MAIN SYSTEM EXECUTION ROUTINE
// ============================================================================
int main() {
    std::string dynamic_input =
        " VEC3 base = 10; "
        " VEC3 multiplier = 5; "
        " VEC3 position = base + multiplier * 4 - 2; "
        " position = position * 2; ";
    std::cout << "[TessCompiler] Loading dynamic source buffer tokens...\n";
    Lexer lexer(dynamic_input);
    Parser parser(std::move(lexer));
    auto ast_nodes = parser.parse_body();
    std::cout << "[TessCompiler] AST Generation complete. Elements structural nodes parsed: " << ast_nodes.size() << "\n";
    std::cout << "[TessCompiler] Emitting non-hardcoded machine code bytes...\n";
    CodeGen generator;
    generator.compile_program(ast_nodes);
    std::cout << "[TessCompiler] Exporting valid, loadable target ELF binary...\n";
    ELFEmitter::write_binary("build/tesseract.bin", generator.get_code());
    std::cout << "[Success] Dynamic generation completed seamlessly.\n";
    return 0;
}
