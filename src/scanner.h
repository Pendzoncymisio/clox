#ifndef clox_scanner_h
#define clox_scanner_h

typedef enum {
    //Single-character tokens
    TOKEN_LEFT_PAREN,       // (
    TOKEN_RIGHT_PAREN,      // )
    TOKEN_LEFT_BRACE,       // {
    TOKEN_RIGHT_BRACE,      // }
    TOKEN_COMMA,            // ,
    TOKEN_DOT,              // .
    TOKEN_MINUS,            // -
    TOKEN_PLUS,             // +
    TOKEN_SEMICOLON,        // ;
    TOKEN_SLASH,            // /
    TOKEN_STAR,             // *
    //One or two character tokens
    TOKEN_BANG,             // !
    TOKEN_BANG_EQUAL,       // !=
    TOKEN_EQUAL,            // =
    TOKEN_EQUAL_EQUAL,      // ==
    TOKEN_GREATER,          // >
    TOKEN_GREATER_EQUAL,    // >=
    TOKEN_LESS,             // <
    TOKEN_LESS_EQUAL,       // <=
    //Literals
    TOKEN_IDENTIFIER,       // Identifier (e.g. variable name)
    TOKEN_STRING,           // String in paretheses - "Lorem ipsum"
    TOKEN_NUMBER,           // e.g. 2137
    //Keywords
    TOKEN_AND,              // and
    TOKEN_CLASS,            // class
    TOKEN_ELSE,             // else
    TOKEN_FALSE,            // false
    TOKEN_FOR,              // for
    TOKEN_FUN,              // fun
    TOKEN_IF,               // if
    TOKEN_NIL,              // nil
    TOKEN_OR,               // or
    TOKEN_PRINT,            // print
    TOKEN_RETURN,           // return
    TOKEN_SUPER,            // super
    TOKEN_THIS,             // this
    TOKEN_TRUE,             // true
    TOKEN_VAR,              // var
    TOKEN_WHILE,            // while
    TOKEN_ERROR,            // Token set when scanning error occured
    TOKEN_EOF               // End of file token
} TokenType;

/* Token struct
*
*   Fields:
*   - TokenType type: type of token
*   - const char* start: pointer to the start of lexeme
*   - int length: length of token
*   - int line: number of line in source code when token is present
*/
typedef struct {
    TokenType type; // type of token
    const char* start; // pointer to the start of lexeme
    int length; // length of token
    int line; // number of line in source code when token is present
} Token;

/* Initialize scanner
*   Arguments:
*   - const char* source: code to be scanned
*/
void initScanner(const char* source);

// Scan next token
Token scanToken();

#endif