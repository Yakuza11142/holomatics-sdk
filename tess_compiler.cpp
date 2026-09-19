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
#include <cassert>

// ============================================================================
// 1. LEXER & TOKENS
// ============================================================================

enum class TokenType {
    TOKEN_FN, TOKEN_IDENT, TOKEN_LPAREN, TOKEN_RPAREN, 
    TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_VEC3, TOKEN_MAT4, 
    TOKEN_ASSIGN, TOKEN_NUMBER, TOKEN_SEMI, TOKEN_EOF, TOKEN_UNKNOWN
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
            if (isspace(static_cast<unsigned char>(current))) { col++; pos++; continue; }

            if (isalpha(static_cast<unsigned char>(current)) || current == '_') {
                std::string ident;
                size_t start_col = col;
                while (pos < src.size() && (isalnum(static_cast<unsigned char>(src[pos])) || src[pos] == '_')) {
                    ident += src[pos++];
                    col++;
                }
                if (ident == "fn") return {TokenType::TOKEN_FN, ident, line, start_col};
                if (ident == "VEC3") return {TokenType::TOKEN_VEC3, ident, line, start_col};
                if (ident == "MAT4") return {TokenType::TOKEN_MAT4, ident, line, start_col};
                return {TokenType::TOKEN_IDENT, ident, line, start_col};
            }

            if (isdigit(static_cast<unsigned char>(current))) {
                std::string num;
                size_t start_col = col;
                while (pos < src.size() && isdigit(static_cast<unsigned char>(src[pos]))) {
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
                case ';': return {TokenType::TOKEN_SEMI, ";", line, start_col};
                default:  return {TokenType::TOKEN_UNKNOWN, std::string(1, current), line, start_col};
            }
        }
        return {TokenType::TOKEN_EOF, "", line, col};
    }
};

// ============================================================================
// 2. ABSTRACT SYNTAX TREE (AST)
// ============================================================================

struct ASTNode {
    virtual ~ASTNode() = default;
};

struct ExpressionNode : public ASTNode {};

struct NumberLiteralNode : public ExpressionNode {
    std::string value;
    explicit NumberLiteralNode(std::string val) : value(std::move(val)) {}
};

struct VarDeclNode : public ASTNode {
    std::string type_name;
    std::string var_name;
    std::unique_ptr<ExpressionNode> initializer;

    VarDeclNode(std::string type, std::string name, std::unique_ptr<ExpressionNode> init)
        : type_name(std::move(type)), var_name(std::move(name)), initializer(std::move(init)) {}
};

struct FunctionNode : public ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> body;
};

// ============================================================================
// 3. RECURSIVE DESCENT PARSER
// ============================================================================

class Parser {
    Lexer lexer;
    Token current_token;

    void advance() {
        current_token = lexer.next_token();
    }

    void consume(TokenType type, const std::string& err_msg) {
        if (current_token.type != type) {
            throw std::runtime_error("Parser Error [Line " + std::to_string(current_token.line) + 
                                     ", Col " + std::to_string(current_token.column) + "]: " + err_msg);
        }
        advance();
    }

public:
    explicit Parser(Lexer l) : lexer(std::move(l)) { advance(); }

    std::unique_ptr<ExpressionNode> parse_expression() {
        if (current_token.type == TokenType::TOKEN_NUMBER) {
            auto node = std::make_unique<NumberLiteralNode>(current_token.text);
            advance();
            return node;
        }
        throw std::runtime_error("Expected expression layout");
    }

    std::unique_ptr<ASTNode> parse_statement() {
        if (current_token.type == TokenType::TOKEN_VEC3 || current_token.type == TokenType::TOKEN_MAT4) {
            std::string type_str = current_token.text;
            advance();

            Token ident_tok = current_token;
            consume(TokenType::TOKEN_IDENT, "Expected variable name identifier following type declaration");
            
            consume(TokenType::TOKEN_ASSIGN, "Expected '=' assignment operator in variable declaration");
            
            auto expr = parse_expression();
            consume(TokenType::TOKEN_SEMI, "Missing trailing ';' terminating variable statement context");

            return std::make_unique<VarDeclNode>(type_str, ident_tok.text, std::move(expr));
        }
        throw std::runtime_error("Unknown statement sequence encountered");
    }

    std::unique_ptr<FunctionNode> parse_function() {
        consume(TokenType::TOKEN_FN, "Expected keyword 'fn' to start function declaration structural wrapper");
        
        Token func_name_tok = current_token;
        consume(TokenType::TOKEN_IDENT, "Expected valid subroutine function title string identifier target");
        
        consume(TokenType::TOKEN_LPAREN, "Missing '(' symbol surrounding function parameters token frame");
        consume(TokenType::TOKEN_RPAREN, "Missing ')' symbol surrounding function parameters token frame");
        consume(TokenType::TOKEN_LBRACE, "Missing opening function statement scoped brace signature '{'");

        auto func = std::make_unique<FunctionNode>();
        func->name = func_name_tok.text;

        while (current_token.type != TokenType::TOKEN_RBRACE && current_token.type != TokenType::TOKEN_EOF) {
            func->body.push_back(parse_statement());
        }
        
        consume(TokenType::TOKEN_RBRACE, "Missing matching scoped function layout close structural brace signature '}'");
        return func;
    }
};

// ============================================================================
// 4. MEMORY ARENA ARCHITECTURE (SAFE BOUNDARY POOL)
// ============================================================================

class MemoryArena {
    std::vector<char> pool;
    size_t offset = 0;
public:
    explicit MemoryArena(size_t size = 1024 * 1024) { pool.resize(size); }

    void* allocate(size_t bytes, size_t alignment = 8) {
        uintptr_t current_ptr = reinterpret_cast<uintptr_t>(pool.data() + offset);
        size_t padding = (alignment - (current_ptr % alignment)) % alignment;
        
        if (offset + padding + bytes > pool.size()) {
            throw std::runtime_error("Arena allocation threshold overflow: out of available system memory");
        }
        
        offset += padding;
        void* allocated_address = pool.data() + offset;
        offset += bytes;
        return allocated_address;
    }

    void reset() { offset = 0; }
};

// ============================================================================
// 5. PRODUCTION TARGET ARCHITECTURE GENERATOR (BOUNDED x86-64 ASSEMBLER)
// ============================================================================

class CodeGen {
public:
    struct StackLayoutInfo {
        size_t stack_alloc_size;
        size_t var_count;
    };

    static StackLayoutInfo analyze_stack_frame(const FunctionNode& func) {
        size_t var_count = 0;
        for (const auto& stmt : func.body) {
            if (dynamic_cast<VarDeclNode*>(stmt.get())) {
                var_count++;
            }
        }

        size_t raw_space_needed = var_count * 4;
        size_t stack_alloc_size = ((raw_space_needed + 15) / 16) * 16;
        if (stack_alloc_size < 16) {
            stack_alloc_size = 16;
        }

        return {stack_alloc_size, var_count};
    }

    static std::vector<uint8_t> compile_function(const FunctionNode& func) {
        StackLayoutInfo layout = analyze_stack_frame(func);

        std::vector<uint8_t> code;

        // --- Function Prologue Layout Frame ---
        code.push_back(0x55);                               // push rbp
        code.push_back(0x48); code.push_back(0x89); code.push_back(0xE5); // mov rbp, rsp
        
        // Emitting dynamic stack allocation: sub rsp, stack_alloc_size
        code.push_back(0x48); code.push_back(0x81); code.push_back(0xEC);
        code.push_back(static_cast<uint8_t>(layout.stack_alloc_size & 0xFF));
        code.push_back(static_cast<uint8_t>((layout.stack_alloc_size >> 8) & 0xFF));
        code.push_back(static_cast<uint8_t>((layout.stack_alloc_size >> 16) & 0xFF));
        code.push_back(static_cast<uint8_t>((layout.stack_alloc_size >> 24) & 0xFF));

        // --- Execute Compilation Pass 2: Instruction Emission with Boundary Tracking ---
        int stack_offset = -4;
        for (const auto& stmt : func.body) {
            if (auto var_decl = dynamic_cast<VarDeclNode*>(stmt.get())) {
                if (static_cast<size_t>(-stack_offset) > layout.stack_alloc_size) {
                    throw std::runtime_error("Stack boundary violation: local variables exceed allocated stack frame size.");
                }

                if (auto num_lit = dynamic_cast<NumberLiteralNode*>(var_decl->initializer.get())) {
                    int val = std::stoi(num_lit->value);
                    
                    // Adaptive ModR/M displacement selection (8-bit vs 32-bit offset)
                    if (stack_offset >= -128 && stack_offset <= 127) {
                        code.push_back(0xC7); 
                        code.push_back(0x45); 
                        code.push_back(static_cast<uint8_t>(stack_offset));
                    } else {
                        code.push_back(0xC7); 
                        code.push_back(0x85); 
                        int32_t off32 = static_cast<int32_t>(stack_offset);
                        code.push_back(static_cast<uint8_t>(off32 & 0xFF));
                        code.push_back(static_cast<uint8_t>((off32 >> 8) & 0xFF));
                        code.push_back(static_cast<uint8_t>((off32 >> 16) & 0xFF));
                        code.push_back(static_cast<uint8_t>((off32 >> 24) & 0xFF));
                    }
                    
                    // 32-bit scalar values (Little Endian Encoding)
                    code.push_back(static_cast<uint8_t>(val & 0xFF));
                    code.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
                    code.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
                    code.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
                    
                    stack_offset -= 4;
                }
            }
        }

        // --- System Exit Assembly Frame Cleanup (sys_exit layout mapping) ---
        code.push_back(0x48); code.push_back(0xC7); code.push_back(0xC0);
        code.push_back(0x3C); code.push_back(0x00); code.push_back(0x00); code.push_back(0x00);
        
        code.push_back(0x48); code.push_back(0x31); code.push_back(0xFF);
        
        code.push_back(0x0F); code.push_back(0x05);

        // --- Function Epilogue Layout Frame ---
        code.push_back(0xC9); // leave
        code.push_back(0xC3); // ret

        return code;
    }
};

// ============================================================================
// 6. NATIVE COMPLIANT ELF BINARY EMITTER (SYSTEM LINUX TARGET LOADER)
// ============================================================================

class ELFEmitter {
public:
    static void emit_elf_binary(const std::string& filename, const std::vector<uint8_t>& machine_code) {
        std::filesystem::path filepath(filename);
        if (filepath.has_parent_path()) {
            std::filesystem::create_directories(filepath.parent_path());
        }

        std::ofstream outfile(filename, std::ios::out | std::ios::binary);
        if (!outfile) {
            throw std::runtime_error("Failed to generate target physical executable payload at path: " + filename);
        }

        uint64_t entry_point = 0x400078; 
        uint64_t full_segment_file_size = 64 + 56 + machine_code.size();

        unsigned char elf_header[64] = {
            0x7F, 'E', 'L', 'F',             
            2,                               
            1,                               
            1,                               
            0,                               
            0, 0, 0, 0, 0, 0, 0, 0,          
            2, 0,                            
            38, 0,                           
            1, 0, 0, 0,                      
        };

        std::memcpy(&elf_header[24], &entry_point, sizeof(entry_point));

        uint64_t phoff = 64; 
        std::memcpy(&elf_header[32], &phoff, sizeof(phoff));

        elf_header[52] = 64;                 
        elf_header[54] = 56;                 
        elf_header[56] = 1;                  

        unsigned char program_header[56] = {
            1, 0, 0, 0,                      
            5, 0, 0, 0,                      
        };

        uint64_t zero_offset = 0;
        uint64_t vaddr = 0x400000;
        std::memcpy(&program_header[8],  &zero_offset,           sizeof(zero_offset)); 
        std::memcpy(&program_header[16], &vaddr,                 sizeof(vaddr));       
        std::memcpy(&program_header[24], &vaddr,                 sizeof(vaddr));       
        std::memcpy(&program_header[32], &full_segment_file_size, sizeof(full_segment_file_size)); 
        std::memcpy(&program_header[40], &full_segment_file_size, sizeof(full_segment_file_size)); 

        uint64_t align_val = 0x200000;
        std::memcpy(&program_header[48], &align_val, sizeof(align_val));

        outfile.write(reinterpret_cast<char*>(elf_header), sizeof(elf_header));
        outfile.write(reinterpret_cast<char*>(program_header), sizeof(program_header));
        outfile.write(reinterpret_cast<const char*>(machine_code.data()), machine_code.size());

        outfile.close();
        std::cout << "[ELF Emission Stage Successful]: Native running binary compiled safely to -> " << filename << "\n";
    }
};

// ============================================================================
// 7. COMPILER ENGINE SYSTEM PIPELINE INTEGRATION RUNTIME
// ============================================================================

int main() {
    try {
        std::string source_code = "fn main() { VEC3 pos = 100; }";
        std::cout << "[Initializing Structural Workspace Compile Pass Over String Payload Block Data]...\n";

        Lexer lexer(source_code);
        Parser parser(std::move(lexer));
        auto ast_root_node = parser.parse_function();

        if (ast_root_node) {
            std::cout << "[AST Construction Phase Validated]: High-level function routine identified successfully -> " << ast_root_node->name << "\n";
        }

        MemoryArena hardware_arena;
        void* simd_aligned_block_pointer = hardware_arena.allocate(256, 32); 
        if (simd_aligned_block_pointer) {
            std::cout << "[Memory Arena Subsystem Active]: Aligned payload blocks extracted securely.\n";
        }

        std::vector<uint8_t> runtime_native_machine_instructions = CodeGen::compile_function(*ast_root_node);

        ELFEmitter::emit_elf_binary("generated/tesseract_sdk", runtime_native_machine_instructions);

    } catch (const std::exception& error_trace_log) {
        std::cerr << "\n[Fatal Execution Compiler Break Down]: " << error_trace_log.what() << "\n";
        return 1;
    }

    return 0;
}
