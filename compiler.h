#include <stdint.h>
#include <map>
#include <string>
#include "opcodes.h"

namespace tinysharp {

enum { 
	K_TYPE = 256, K_INTLIT, K_FLOATLIT, K_STRLIT, K_static, K_class, K_struct, K_switch, K_case,
    K_if, K_for, K_else, K_while, K_do, K_public, K_private, K_protected, K_virtual, K_namespace,
    K_interface, K_in, K_out, K_ref, K_continue, K_break, K_INCR, K_DECR
};

constexpr uint32_t cthash(const char *key) {
    uint32_t seed = 0;
    while (*key) {
        seed += *key++;
        seed += seed << 10;
        seed ^= seed >> 6;
    }
    seed += seed << 3;
    seed ^= seed >> 11;
    seed += seed << 15;
    return seed;
}

inline uint32_t hash(const char *key,uint32_t length) {
    uint32_t seed = 0;
    while (length--) {
        seed += *key++;
        seed += seed << 10;
        seed ^= seed >> 6;
    }
    seed += seed << 3;
    seed ^= seed >> 11;
    seed += seed << 15;
    return seed;
}

/* identifiers in the source code being processed are referenced with a 24:8 integer
    encoding the offset from the input base */
struct token_t {
    uint32_t m_offset;
};

class stmt;
class expr;

class compiler {
public:
    void setup();
    void compile(const char*input, uint32_t inputSize);
    void block();
    int nextChar();
    int nextToken();
    int matchNextToken(int);

    stmt* declaration_statement();
    stmt* embedded_statement();
    stmt* statement();
    stmt* statements();
    expr* boolean_expression();
    expr* expression();
    int m_token;
    std::map<std::string,uint32_t> m_types;
    union {
        unsigned u;
        int i;
        float f;
        size_t s;
    } m_lval;
    const char *m_fileBase;
    int m_ch;
    uint32_t m_fileOffset, m_fileSize;
};

}
