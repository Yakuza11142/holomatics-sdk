#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <stdexcept>
#include <cctype>

// ============================================================================
// 1. LEXER & TOKENS
// ============================================================================

enum class TokenType {
    TOKEN_FN, TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, TOKEN_RETURN, TOKEN_LET,
    TOKEN_IDENT, TOKEN_INT_LIT,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_ASSIGN,
    TOKEN_EQ, TOKEN_NEQ, TOKEN_LT, TOKEN_GT,
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_COMMA, TOKEN_SEMI, TOKEN_COLON,
    TOKEN_EOF, TOKEN_UNKNOWN
};

struct Token {
    TokenType type;
    std::string text;
    size_t line, col;
};

class Lexer {
    std::string src;
    size_t pos = 0, line = 1, col = 1;

public:
    explicit Lexer(std::string source) : src(std::move(source)) {}

    Token next_token() {
        while (pos < src.size()) {
            char current = src[pos];

            if (current == '\n') { line++; col = 1; pos++; continue; }
            if (std::isspace(static_cast<unsigned char>(current))) { col++; pos++; continue; }

            // Skip comments
            if (current == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
                while (pos < src.size() && src[pos] != '\n') pos++;
                continue;
            }

            // Identifiers & Keywords
            if (std::isalpha(static_cast<unsigned char>(current)) || current == '_') {
                std::string ident;
                size_t start_col = col;
                while (pos < src.size() && (std::isalnum(static_cast<unsigned char>(src[pos])) || src[pos] == '_')) {
                    ident += src[pos++]; col++;
                }
                if (ident == "fn") return {TokenType::TOKEN_FN, ident, line, start_col};
                if (ident == "if") return {TokenType::TOKEN_IF, ident, line, start_col};
                if (ident == "else") return {TokenType::TOKEN_ELSE, ident, line, start_col};
                if (ident == "while") return {TokenType::TOKEN_WHILE, ident, line, start_col};
                if (ident == "return") return {TokenType::TOKEN_RETURN, ident, line, start_col};
                if (ident == "let") return {TokenType::TOKEN_LET, ident, line, start_col};
                return {TokenType::TOKEN_IDENT, ident, line, start_col};
            }

            // Integers
            if (std::isdigit(static_cast<unsigned char>(current))) {
                std::string num;
                size_t start_col = col;
                while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) {
                    num += src[pos++]; col++;
                }
                return {TokenType::TOKEN_INT_LIT, num, line, start_col};
            }

            // Multi-char operators
            if (pos + 1 < src.size()) {
                std::string sub = src.substr(pos, 2);
                size_t start_col = col;
                if (sub == "==") { pos += 2; col += 2; return {TokenType::TOKEN_EQ, sub, line, start_col}; }
                if (sub == "!=") { pos += 2; col += 2; return {TokenType::TOKEN_NEQ, sub, line, start_col}; }
            }

            // Single-char operators
            size_t start_col = col;
            pos++; col++;
            switch (current) {
                case '+': return {TokenType::TOKEN_PLUS, "+", line, start_col};
                case '-': return {TokenType::TOKEN_MINUS, "-", line, start_col};
                case '*': return {TokenType::TOKEN_STAR, "*", line, start_col};
                case '/': return {TokenType::TOKEN_SLASH, "/", line, start_col};
                case '=': return {TokenType::TOKEN_ASSIGN, "=", line, start_col};
                case '<': return {TokenType::TOKEN_LT, "<", line, start_col};
                case '>': return {TokenType::TOKEN_GT, ">", line, start_col};
                case '(': return {TokenType::TOKEN_LPAREN, "(", line, start_col};
                case ')': return {TokenType::TOKEN_RPAREN, ")", line, start_col};
                case '{': return {TokenType::TOKEN_LBRACE, "{", line, start_col};
                case '}': return {TokenType::TOKEN_RBRACE, "}", line, start_col};
                case ',': return {TokenType::TOKEN_COMMA, ",", line, start_col};
                case ';': return {TokenType::TOKEN_SEMI, ";", line, start_col};
                case ':': return {TokenType::TOKEN_COLON, ":", line, start_col};
                default:  return {TokenType::TOKEN_UNKNOWN, std::string(1, current), line, start_col};
            }
        }
        return {TokenType::TOKEN_EOF, "", line, col};
    }
};

// ============================================================================
// 2. AST NODES
// ============================================================================

struct ASTNode { virtual ~ASTNode() = default; };
struct ExpressionNode : public ASTNode {};

struct IntLiteralNode : public ExpressionNode {
    int64_t value;
    explicit IntLiteralNode(int64_t v) : value(v) {}
};

struct IdentifierNode : public ExpressionNode {
    std::string name;
    explicit IdentifierNode(std::string n) : name(std::move(n)) {}
};

struct BinaryOpNode : public ExpressionNode {
    std::string op;
    std::unique_ptr<ExpressionNode> left, right;
    BinaryOpNode(std::string o, std::unique_ptr<ExpressionNode> l, std::unique_ptr<ExpressionNode> r)
        : op(std::move(o)), left(std::move(l)), right(std::move(r)) {}
};

struct VarDeclNode : public ASTNode {
    std::string var_name;
    std::unique_ptr<ExpressionNode> initializer;
};

struct AssignmentNode : public ASTNode {
    std::string var_name;
    std::unique_ptr<ExpressionNode> value;
};

struct ExpressionStmtNode : public ASTNode {
    std::unique_ptr<ExpressionNode> expr;
    explicit ExpressionStmtNode(std::unique_ptr<ExpressionNode> e) : expr(std::move(e)) {}
};

struct BlockNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;
};

struct IfNode : public ASTNode {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<BlockNode> then_branch;
    std::unique_ptr<BlockNode> else_branch;
};

struct WhileNode : public ASTNode {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<BlockNode> body;
};

struct ReturnNode : public ASTNode {
    std::unique_ptr<ExpressionNode> value;
};

struct FunctionNode : public ASTNode {
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<BlockNode> body;
};

// ============================================================================
// 3. PARSER
// ============================================================================

class Parser {
    Lexer lexer;
    Token current_token;

    void advance() { current_token = lexer.next_token(); }
    bool check(TokenType type) const { return current_token.type == type; }

    Token consume(TokenType type, const std::string& err) {
        if (!check(type)) {
            throw std::runtime_error("Parser Error [Line " + std::to_string(current_token.line) + "]: " + err);
        }
        Token tok = current_token;
        advance();
        return tok;
    }

    int get_precedence(TokenType type) {
        switch (type) {
            case TokenType::TOKEN_EQ:
            case TokenType::TOKEN_NEQ:
            case TokenType::TOKEN_LT:
            case TokenType::TOKEN_GT: return 10;
            case TokenType::TOKEN_PLUS:
            case TokenType::TOKEN_MINUS: return 20;
            case TokenType::TOKEN_STAR:
            case TokenType::TOKEN_SLASH: return 30;
            default: return 0;
        }
    }

public:
    explicit Parser(Lexer l) : lexer(std::move(l)) { advance(); }

    std::unique_ptr<ExpressionNode> parse_primary() {
        if (check(TokenType::TOKEN_INT_LIT)) {
            int64_t val = std::stoll(current_token.text);
            advance();
            return std::make_unique<IntLiteralNode>(val);
        }
        if (check(TokenType::TOKEN_IDENT)) {
            std::string name = current_token.text;
            advance();
            return std::make_unique<IdentifierNode>(name);
        }
        if (check(TokenType::TOKEN_LPAREN)) {
            advance();
            auto expr = parse_expression(0);
            consume(TokenType::TOKEN_RPAREN, "Expected ')'");
            return expr;
        }
        throw std::runtime_error("Unexpected token in expression: " + current_token.text);
    }

    std::unique_ptr<ExpressionNode> parse_expression(int min_prec) {
        auto left = parse_primary();
        while (true) {
            int prec = get_precedence(current_token.type);
            if (prec < min_prec || prec == 0) break;

            std::string op = current_token.text;
            advance();

            auto right = parse_expression(prec + 1);
            left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
        }
        return left;
    }

    std::unique_ptr<BlockNode> parse_block() {
        consume(TokenType::TOKEN_LBRACE, "Expected '{'");
        auto block = std::make_unique<BlockNode>();
        while (!check(TokenType::TOKEN_RBRACE) && !check(TokenType::TOKEN_EOF)) {
            block->statements.push_back(parse_statement());
        }
        consume(TokenType::TOKEN_RBRACE, "Expected '}'");
        return block;
    }

    std::unique_ptr<ASTNode> parse_statement() {
        if (check(TokenType::TOKEN_LET)) {
            advance();
            std::string name = consume(TokenType::TOKEN_IDENT, "Expected variable name").text;
            consume(TokenType::TOKEN_ASSIGN, "Expected '='");
            auto init = parse_expression(0);
            if (check(TokenType::TOKEN_SEMI)) advance();
            auto decl = std::make_unique<VarDeclNode>();
            decl->var_name = name;
            decl->initializer = std::move(init);
            return decl;
        }

        if (check(TokenType::TOKEN_RETURN)) {
            advance();
            auto ret = std::make_unique<ReturnNode>();
            if (!check(TokenType::TOKEN_SEMI)) ret->value = parse_expression(0);
            if (check(TokenType::TOKEN_SEMI)) advance();
            return ret;
        }

        if (check(TokenType::TOKEN_IF)) {
            advance();
            consume(TokenType::TOKEN_LPAREN, "Expected '(' after if");
            auto cond = parse_expression(0);
            consume(TokenType::TOKEN_RPAREN, "Expected ')' after condition");
            auto then_b = parse_block();

            std::unique_ptr<BlockNode> else_b = nullptr;
            if (check(TokenType::TOKEN_ELSE)) {
                advance();
                else_b = parse_block();
            }

            auto if_node = std::make_unique<IfNode>();
            if_node->condition = std::move(cond);
            if_node->then_branch = std::move(then_b);
            if_node->else_branch = std::move(else_b);
            return if_node;
        }

        if (check(TokenType::TOKEN_WHILE)) {
            advance();
            consume(TokenType::TOKEN_LPAREN, "Expected '(' after while");
            auto cond = parse_expression(0);
            consume(TokenType::TOKEN_RPAREN, "Expected ')' after condition");
            auto body = parse_block();

            auto while_node = std::make_unique<WhileNode>();
            while_node->condition = std::move(cond);
            while_node->body = std::move(body);
            return while_node;
        }

        // Handle assignment or expression statement
        if (check(TokenType::TOKEN_IDENT)) {
            Token ident_tok = current_token;
            advance();
            if (check(TokenType::TOKEN_ASSIGN)) {
                advance();
                auto val = parse_expression(0);
                if (check(TokenType::TOKEN_SEMI)) advance();
                auto assign = std::make_unique<AssignmentNode>();
                assign->var_name = ident_tok.text;
                assign->value = std::move(val);
                return assign;
            } else {
                // Expression statement fallback
                auto expr = parse_expression(0);
                if (check(TokenType::TOKEN_SEMI)) advance();
                return std::make_unique<ExpressionStmtNode>(std::move(expr));
            }
        }

        throw std::runtime_error("Unrecognized statement: " + current_token.text);
    }

    std::unique_ptr<FunctionNode> parse_function() {
        consume(TokenType::TOKEN_FN, "Expected 'fn'");
        std::string name = consume(TokenType::TOKEN_IDENT, "Expected function name").text;
        consume(TokenType::TOKEN_LPAREN, "Expected '('");

        auto fn = std::make_unique<FunctionNode>();
        fn->name = name;

        while (!check(TokenType::TOKEN_RPAREN) && !check(TokenType::TOKEN_EOF)) {
            std::string pname = consume(TokenType::TOKEN_IDENT, "Expected param name").text;
            fn->params.push_back(pname);
            if (check(TokenType::TOKEN_COMMA)) advance();
        }
        consume(TokenType::TOKEN_RPAREN, "Expected ')'");
        fn->body = parse_block();
        return fn;
    }
};

// ============================================================================
// 4. CODE GENERATOR & ENTRY POINT EMITTER
// ============================================================================

class CodeGen {
    std::unordered_map<std::string, int32_t> var_offsets;
    std::vector<uint8_t> code;

    void emit_bytes(const std::vector<uint8_t>& bytes) {
        code.insert(code.end(), bytes.begin(), bytes.end());
    }

    void emit_mov_rbp_offset_eax(int32_t offset) {
        if (offset >= -128 && offset <= 127) {
            code.push_back(0x89); code.push_back(0x45);
            code.push_back(static_cast<uint8_t>(static_cast<int8_t>(offset)));
        } else {
            code.push_back(0x89); code.push_back(0x85);
            uint32_t uoff = static_cast<uint32_t>(offset);
            emit_bytes({static_cast<uint8_t>(uoff & 0xFF), static_cast<uint8_t>((uoff >> 8) & 0xFF),
                        static_cast<uint8_t>((uoff >> 16) & 0xFF), static_cast<uint8_t>((uoff >> 24) & 0xFF)});
        }
    }

    void emit_mov_rbp_offset_reg32(int32_t offset, uint8_t reg_code) {
        code.push_back(0x89);
        code.push_back(0x45 | ((reg_code & 0x07) << 3));
        code.push_back(static_cast<uint8_t>(static_cast<int8_t>(offset)));
    }

    void compile_expression(const ExpressionNode* node) {
        if (auto lit = dynamic_cast<const IntLiteralNode*>(node)) {
            code.push_back(0xB8); // mov eax, imm32
            uint32_t val = static_cast<uint32_t>(lit->value);
            emit_bytes({static_cast<uint8_t>(val & 0xFF), static_cast<uint8_t>((val >> 8) & 0xFF),
                        static_cast<uint8_t>((val >> 16) & 0xFF), static_cast<uint8_t>((val >> 24) & 0xFF)});
            return;
        }

        if (auto ident = dynamic_cast<const IdentifierNode*>(node)) {
            if (var_offsets.find(ident->name) == var_offsets.end()) {
                throw std::runtime_error("Undeclared variable: " + ident->name);
            }
            int32_t offset = var_offsets[ident->name];
            code.push_back(0x8B); code.push_back(0x45);
            code.push_back(static_cast<uint8_t>(static_cast<int8_t>(offset)));
            return;
        }

        if (auto bin = dynamic_cast<const BinaryOpNode*>(node)) {
            compile_expression(bin->left.get());
            code.push_back(0x50); // push rax

            compile_expression(bin->right.get());
            code.push_back(0x89); code.push_back(0xC1); // mov ecx, eax
            code.push_back(0x58);                       // pop rax

            if (bin->op == "+") {
                code.push_back(0x01); code.push_back(0xC8); // add eax, ecx
            } else if (bin->op == "-") {
                code.push_back(0x29); code.push_back(0xC8); // sub eax, ecx
            } else if (bin->op == "*") {
                code.push_back(0x0F); code.push_back(0xAF); code.push_back(0xC1); // imul eax, ecx
            } else if (bin->op == "/") {
                code.push_back(0x99);                       // cdq
                code.push_back(0xF7); code.push_back(0xF9); // idiv ecx
            } else if (bin->op == "<" || bin->op == ">" || bin->op == "==" || bin->op == "!=") {
                code.push_back(0x39); code.push_back(0xC8); // cmp eax, ecx
                code.push_back(0x0F);                       // setcc al
                if (bin->op == "<")  code.push_back(0x9C);
                if (bin->op == ">")  code.push_back(0x9F);
                if (bin->op == "==") code.push_back(0x94);
                if (bin->op == "!=") code.push_back(0x95);
                code.push_back(0x0F); code.push_back(0xB6); code.push_back(0xC0); // movzx eax, al
            }
            return;
        }
    }

    void scan_vars(const ASTNode* stmt, int32_t& current_offset) {
        if (auto var = dynamic_cast<const VarDeclNode*>(stmt)) {
            if (var_offsets.find(var->var_name) == var_offsets.end()) {
                var_offsets[var->var_name] = current_offset;
                current_offset -= 4;
            }
        } else if (auto block = dynamic_cast<const BlockNode*>(stmt)) {
            for (const auto& s : block->statements) scan_vars(s.get(), current_offset);
        } else if (auto if_node = dynamic_cast<const IfNode*>(stmt)) {
            scan_vars(if_node->then_branch.get(), current_offset);
            if (if_node->else_branch) scan_vars(if_node->else_branch.get(), current_offset);
        } else if (auto while_node = dynamic_cast<const WhileNode*>(stmt)) {
            scan_vars(while_node->body.get(), current_offset);
        }
    }

public:
    void compile_statement(const ASTNode* stmt) {
        if (auto var = dynamic_cast<const VarDeclNode*>(stmt)) {
            compile_expression(var->initializer.get());
            emit_mov_rbp_offset_eax(var_offsets[var->var_name]);
        } else if (auto assign = dynamic_cast<const AssignmentNode*>(stmt)) {
            compile_expression(assign->value.get());
            emit_mov_rbp_offset_eax(var_offsets[assign->var_name]);
        } else if (auto expr_stmt = dynamic_cast<const ExpressionStmtNode*>(stmt)) {
            compile_expression(expr_stmt->expr.get());
        } else if (auto ret = dynamic_cast<const ReturnNode*>(stmt)) {
            if (ret->value) compile_expression(ret->value.get());
            code.push_back(0x48); code.push_back(0x89); code.push_back(0xEC); // mov rsp, rbp
            code.push_back(0x5D);                                             // pop rbp
            code.push_back(0xC3);                                             // ret
        } else if (auto block = dynamic_cast<const BlockNode*>(stmt)) {
            for (const auto& s : block->statements) compile_statement(s.get());
        } else if (auto if_node = dynamic_cast<const IfNode*>(stmt)) {
            compile_expression(if_node->condition.get());
            code.push_back(0x85); code.push_back(0xC0); // test eax, eax

            code.push_back(0x0F); code.push_back(0x84);
            size_t je_patch_pos = code.size();
            emit_bytes({0, 0, 0, 0});

            compile_statement(if_node->then_branch.get());

            code.push_back(0xE9);
            size_t jmp_patch_pos = code.size();
            emit_bytes({0, 0, 0, 0});

            int32_t else_offset = static_cast<int32_t>(code.size() - (je_patch_pos + 4));
            std::memcpy(&code[je_patch_pos], &else_offset, sizeof(else_offset));

            if (if_node->else_branch) {
                compile_statement(if_node->else_branch.get());
            }

            int32_t exit_offset = static_cast<int32_t>(code.size() - (jmp_patch_pos + 4));
            std::memcpy(&code[jmp_patch_pos], &exit_offset, sizeof(exit_offset));
        } else if (auto while_node = dynamic_cast<const WhileNode*>(stmt)) {
            size_t loop_start = code.size();
            compile_expression(while_node->condition.get());
            code.push_back(0x85); code.push_back(0xC0); // test eax, eax

            code.push_back(0x0F); code.push_back(0x84);
            size_t exit_patch_pos = code.size();
            emit_bytes({0, 0, 0, 0});

            compile_statement(while_node->body.get());

            code.push_back(0xE9);
            int32_t jump_back = static_cast<int32_t>(loop_start - (code.size() + 4));
            uint32_t ujump = static_cast<uint32_t>(jump_back);
            emit_bytes({static_cast<uint8_t>(ujump & 0xFF), static_cast<uint8_t>((ujump >> 8) & 0xFF),
                        static_cast<uint8_t>((ujump >> 16) & 0xFF), static_cast<uint8_t>((ujump >> 24) & 0xFF)});

            int32_t exit_offset = static_cast<int32_t>(code.size() - (exit_patch_pos + 4));
            std::memcpy(&code[exit_patch_pos], &exit_offset, sizeof(exit_offset));
        }
    }

    std::vector<uint8_t> generate(const FunctionNode& func) {
        // --- 1. Generate Runtime Entry Point Stub (_start) ---
        // xor rdi, rdi; xor rsi, rsi; call main; mov rdi, rax; mov rax, 60; syscall;
        std::vector<uint8_t> start_stub = {
            0x48, 0x31, 0xFF,                         // xor rdi, rdi
            0x48, 0x31, 0xF6,                         // xor rsi, rsi
            0xE8, 0x0C, 0x00, 0x00, 0x00,             // call main (12 bytes forward)
            0x48, 0x89, 0xC7,                         // mov rdi, rax
            0x48, 0xC7, 0xC0, 0x3C, 0x00, 0x00, 0x00, // mov rax, 60 (sys_exit)
            0x0F, 0x05                                // syscall
        };
        emit_bytes(start_stub);

        // --- 2. Generate Main Function ---
        int32_t current_offset = -4;
        for (const auto& param : func.params) {
            var_offsets[param] = current_offset;
            current_offset -= 4;
        }

        scan_vars(func.body.get(), current_offset);

        size_t raw_space = var_offsets.size() * 4;
        size_t stack_alloc = ((raw_space + 15) / 16) * 16 + 8;

        // Prologue
        code.push_back(0x55); // push rbp
        code.push_back(0x48); code.push_back(0x89); code.push_back(0xE5); // mov rbp, rsp
        code.push_back(0x48); code.push_back(0x81); code.push_back(0xEC); // sub rsp, alloc
        uint32_t sz = static_cast<uint32_t>(stack_alloc);
        emit_bytes({static_cast<uint8_t>(sz & 0xFF), static_cast<uint8_t>((sz >> 8) & 0xFF),
                    static_cast<uint8_t>((sz >> 16) & 0xFF), static_cast<uint8_t>((sz >> 24) & 0xFF)});

        // Spill parameter registers
        uint8_t param_regs[] = {7, 6, 2, 1}; // rdi, rsi, rdx, rcx
        for (size_t i = 0; i < func.params.size() && i < 4; ++i) {
            emit_mov_rbp_offset_reg32(var_offsets[func.params[i]], param_regs[i]);
        }

        // Body Execution
        compile_statement(func.body.get());

        // Default epilogue fallback
        code.push_back(0x48); code.push_back(0x89); code.push_back(0xEC); // mov rsp, rbp
        code.push_back(0x5D);                                             // pop rbp
        code.push_back(0xC3);                                             // ret

        return code;
    }
};

// ============================================================================
// 5. ELF LINKER
// ============================================================================

class ELFEmitter {
public:
    static void write_executable(const std::string& filename, const std::vector<uint8_t>& machine_code) {
        std::filesystem::path filepath(filename);
        if (filepath.has_parent_path()) {
            std::filesystem::create_directories(filepath.parent_path());
        }

        std::ofstream outfile(filename, std::ios::out | std::ios::binary);
        if (!outfile) throw std::runtime_error("Could not create output executable: " + filename);

        uint64_t entry_point = 0x400078; // Start offset pointing to _start
        uint64_t full_segment_file_size = 64 + 56 + machine_code.size();

        unsigned char elf_header[64] = {
            0x7F, 'E', 'L', 'F', 2, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            2, 0, 38, 0, 1, 0, 0, 0
        };

        std::memcpy(&elf_header[24], &entry_point, sizeof(entry_point));
        uint64_t phoff = 64;
        std::memcpy(&elf_header[32], &phoff, sizeof(phoff));

        elf_header[52] = 64; elf_header[54] = 56; elf_header[56] = 1;

        unsigned char program_header[56] = {
            1, 0, 0, 0,
            5, 0, 0, 0
        };

        uint64_t zero_offset = 0;
        uint64_t vaddr = 0x400000;
        std::memcpy(&program_header[8],  &zero_offset, sizeof(zero_offset));
        std::memcpy(&program_header[16], &vaddr, sizeof(vaddr));
        std::memcpy(&program_header[24], &vaddr, sizeof(vaddr));
        std::memcpy(&program_header[32], &full_segment_file_size, sizeof(full_segment_file_size));
        std::memcpy(&program_header[40], &full_segment_file_size, sizeof(full_segment_file_size));

        uint64_t align_val = 0x200000;
        std::memcpy(&program_header[48], &align_val, sizeof(align_val));

        outfile.write(reinterpret_cast<char*>(elf_header), sizeof(elf_header));
        outfile.write(reinterpret_cast<char*>(program_header), sizeof(program_header));
        outfile.write(reinterpret_cast<const char*>(machine_code.data()), machine_code.size());

        outfile.close();

        std::filesystem::permissions(filename, std::filesystem::perms::owner_exec | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
    }
};

// ============================================================================
// 6. DRIVER & TEST SUITE
// ============================================================================

int main() {
    try {
        std::string source_code = R"(
            fn main(a, b) {
                let x = a + 50 * 2;
                if (x > 100) {
                    x = x / 2;
                } else {
                    x = x + 10;
                }

                while (x < 150) {
                    x = x + 1;
                }

                return x;
            }
        )";

        std::cout << "[Compiler Engine]: Parsing source program...\n";
        Lexer lexer(source_code);
        Parser parser(std::move(lexer));
        auto ast = parser.parse_function();

        std::cout << "[Compiler Engine]: Lowering to x86-64 native instructions...\n";
        CodeGen codegen;
        std::vector<uint8_t> machine_code = codegen.generate(*ast);

        std::cout << "[Compiler Engine]: Linking ELF binary target...\n";
        ELFEmitter::write_executable("generated_binary", machine_code);

        std::cout << "[Success]: Binary compiled (" << machine_code.size() 
                  << " bytes). Execute with './generated_binary; echo $?'\n";

    } catch (const std::exception& e) {
        std::cerr << "\n[Fatal Compiler Exception]: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
