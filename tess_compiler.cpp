#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <unordered_map>

// --- 1. LEXER & TOKENS ---

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
            if (isspace(current)) { col++; pos++; continue; }

            if (isalpha(current) || current == '_') {
                std::string ident;
                size_t start_col = col;
                while (pos < src.size() && (isalnum(src[pos]) || src[pos] == '_')) {
                    ident += src[pos++];
                    col++;
                }
                if (ident == "fn") return {TokenType::TOKEN_FN, ident, line, start_col};
                if (ident == "VEC3") return {TokenType::TOKEN_VEC3, ident, line, start_col};
                if (ident == "MAT4") return {TokenType::TOKEN_MAT4, ident, line, start_col};
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
                case ';': return {TokenType::TOKEN_SEMI, ";", line, start_col};
                default:  return {TokenType::TOKEN_UNKNOWN, std::string(1, current), line, start_col};
            }
        }
        return {TokenType::TOKEN_EOF, "", line, col};
    }
};

// --- 2. AST & PARSER ---

struct ASTNode {
    virtual ~ASTNode() = default;
};

struct FunctionNode : public ASTNode {
    std::string name;
    std::vector<std::string> body_stmts;
};

class Parser {
    Lexer lexer;
    Token current_token;

    void advance() {
        current_token = lexer.next_token();
    }

public:
    explicit Parser(Lexer l) : lexer(std::move(l)) { advance(); }

    std::unique_ptr<FunctionNode> parse_function() {
        if (current_token.type != TokenType::TOKEN_FN) return nullptr;
        advance(); // consume 'fn'

        if (current_token.type != TokenType::TOKEN_IDENT) return nullptr;
        std::string func_name = current_token.text;
        advance(); // consume name

        if (current_token.type != TokenType::TOKEN_LPAREN) return nullptr;
        advance(); // consume '('
        if (current_token.type != TokenType::TOKEN_RPAREN) return nullptr;
        advance(); // consume ')'

        if (current_token.type != TokenType::TOKEN_LBRACE) return nullptr;
        advance(); // consume '{'

        auto func = std::make_unique<FunctionNode>();
        func->name = func_name;

        while (current_token.type != TokenType::TOKEN_RBRACE && current_token.type != TokenType::TOKEN_EOF) {
            func->body_stmts.push_back(current_token.text);
            advance();
        }
        advance(); // consume '}'
        return func;
    }
};

// --- 3. MEMORY ARENA & RUNTIME SUBSYSTEM ---

class MemoryArena {
    std::vector<char> pool;
    size_t offset = 0;
public:
    explicit MemoryArena(size_t size = 1024 * 1024) { pool.resize(size); }
    
    void* allocate(size_t bytes) {
        if (offset + bytes > pool.size()) throw std::runtime_error("Arena out of memory");
        void* ptr = &pool[offset];
        offset += bytes;
        return ptr;
    }
    void reset() { offset = 0; }
};

// --- 4. x86-64 ELF MACHINE CODE EMITTER ---

class ELFEmitter {
public:
    static void emit_elf_binary(const std::string& filename, const std::vector<uint8_t>& machine_code) {
        std::ofstream outfile(filename, std::ios::out | std::ios::binary);
        if (!outfile) {
            std::cerr << "Failed to open output file: " << filename << "\n";
            return;
        }

        // ELF Header structure layout (64-bit)
        unsigned char elf_header[64] = {
            0x7F, 'E', 'L', 'F', // Magic
            2, 1, 1, 0,          // 64-bit, Little Endian, ELF Version 1, OS/ABI
            0, 0, 0, 0, 0, 0, 0, 0,
            2, 0,                // Type: Executable
            38, 0,               // Machine: x86-64
            1, 0, 0, 0,          // Version 1
            0, 0, 40, 0,         // Entry point address (0x400078)
            64, 0, 0, 0, 0, 0, 0, 0, // Program header table offset
            0, 0, 0, 0, 0, 0, 0, 0, // Section header table offset
            0, 0, 0, 0,          // Flags
            64, 0,               // ELF header size
            56, 0,               // Program header table entry size
            1, 0,                // Program header table entry count
            0, 0,                // Section header entry size
            0, 0,                // Section header entry count
            0, 0                 // Section header string table index
        };

        // Program Header (`PT_LOAD`)
        unsigned char program_header[56] = {
            1, 0, 0, 0,          // Segment type: PT_LOAD
            5, 0, 0, 0,          // Segment flags: Read (4) + Execute (1)
            0, 0, 0, 0, 0, 0, 0, 0, // Offset in file
            0, 0, 64, 0, 0, 0, 0, 40, // Virtual address (0x400000)
            0, 0, 64, 0, 0, 0, 0, 40, // Physical address
            0, 16, 0, 0, 0, 0, 0, 0, // Size in file (4096 bytes block)
            0, 16, 0, 0, 0, 0, 0, 0, // Size in memory
            0, 209, 21, 0, 0, 0, 0, 0 // Alignment (2MB / standard 0x200000)
        };

        outfile.write(reinterpret_cast<char*>(elf_header), sizeof(elf_header));
        outfile.write(reinterpret_cast<char*>(program_header), sizeof(program_header));
        
        // Pad header gap and write raw instruction machine code payload
        std::vector<uint8_t> padding(64, 0);
        outfile.write(reinterpret_cast<char*>(padding.data()), padding.size());
        outfile.write(reinterpret_cast<const char*>(machine_code.data()), machine_code.size());

        outfile.close();
        std::cout << "Successfully emitted native ELF binary: " << filename << "\n";
    }
};

// --- 5. MAIN ENTRY POINT ---

int main() {
    std::string source_code = "fn main() { VEC3 pos = 100; }";
    
    // Lexing & Parsing Pipeline
    Lexer lexer(source_code);
    Parser parser(std::move(lexer));
    auto ast = parser.parse_function();

    if (ast) {
        std::cout << "Parsed .tess routine successfully: " << ast->name << "\n";
    }

    // Memory Arena test allocation
    MemoryArena arena;
    void* block = arena.allocate(256);
    if (block) {
        std::cout << "Allocated 256 bytes from High-Performance Memory Arena.\n";
    }

    // Machine Code Emitter Stub: x86-64 (mov rax, 60; xor rdi, rdi; syscall -> clean exit)
    std::vector<uint8_t> x86_machine_code = {
        0x48, 0xC7, 0xC0, 0x3C, 0x00, 0x00, 0x00, // mov rax, 60 (sys_exit)
        0x48, 0x31, 0xFF,                         // xor rdi, rdi (exit code 0)
        0x0F, 0x05                                // syscall
    };

    // Output target ELF binary
    ELFEmitter::emit_elf_binary("build/tesseract_sdk", x86_machine_code);

    return 0;
}
