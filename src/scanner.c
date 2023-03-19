#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

/* Scanner struct
*
*   Fields:
*   - const char* start: start of the current lexeme being scanned
*   - const char* current: current character
*   - int line: line number of the scanned lexeme for error logging
*/
typedef struct {
    const char* start; // start of the current lexeme being scanned
    const char* current; // current character
    int line; // line number of the scanned lexeme for error logging
} Scanner;

Scanner scanner;

/* Initialize global scanner variable
*   Arguments:
*   - const char* source: code to be scanned
*/
void initScanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

/* Check if character is alphanumeric
*   Arguments:
*   - char c: character to be checked
*/
static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
            c == '_';
}

/* Check if character is a digit
*   Arguments:
*   - char c: character to be checked
*/
static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

// Check if scenner reached end of file
static bool isAtEnd() {
    return *scanner.current == '\0';
}

/* Advance scanner to the next character
*
*   Return just consumed character
*/
static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

// Return currently scanned character without advancing scanner
static char peek() {
    return *scanner.current;
}

// Return next character without advancing scanner
static char peekNext() {
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

/* Advance scanner when character is expected
*   Arguments:
*   - char expected: character
*   
*   Return whether match successful
*/
static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*scanner.current != expected) return false;
    scanner.current++;
    return true;
}

/* Make token after scanning whole lexeme
*   Arguments:
*   - TokenType type: of the token to be created
*   
*   Return token with fields populated
*/
static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start; // Start of the token is character at the start of the lexeme
    token.length = (int)(scanner.current - scanner.start); // Difference between pointers casted to integer
    token.line = scanner.line;
    return token;
}

/* Make error token
*   Arguments:
*   - const char* message: error message to be displayed
*   
*   Return token with fields populated
*/
static Token errorToken(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

// Skip whitespaces until not-whitespace
static void skipWhitespace() {
    for (;;) {
        char c = peek();
        switch (c)
        {
        case ' ':
        case '\r':
        case '\t':
            advance();
            break;
        case '\n':
            scanner.line++;
            advance();
            break;
        case '/':
            if (peekNext() == '/') { // Handling one-line comments
                while (peek() != '\n' && !isAtEnd()) advance(); // Skipping until line break or end of file
            } else {
                return;
            }
        default:
            return;
        }
    }
}

/* Check if the rest of the lexeme is equal to argument
*   Arguments:
*   - int start: how many character were already scanned
*   - int length: length of the <rest> argument
*   - const char* rest: string to campare rest of lexeme to
*   - TokenType type: of token that should be generated
*   
*   Return <type> if check successful, TOKEN_IDENTIFIER otherwise
*/
static TokenType checkKeyword(int start, int length, const char* rest, TokenType type) {
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

/* Check if lexeme is a keyword
*   
*   Return appropriate TokenType when found a keyword, TOKEN_IDENTIFIER otherwise
*/
static TokenType identifierType() {
    switch (scanner.start[0]) {
        case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
        case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

// Scan and return identifier
static Token identifier() {
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
}

// Scan and return number
static Token number() {
    while (isDigit(peek())) advance();

    if(peek() == '.' && isDigit(peekNext())) {
        advance(); //Consume the dot
        while (isDigit(peek())) advance();
    }

    return makeToken(TOKEN_NUMBER);
}

// Scan and return string
static Token string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') scanner.line++;
        advance();
    }
    if (isAtEnd()) return errorToken("Unterminated string.");

    advance();  //The closing quote.
    return makeToken(TOKEN_STRING);
}

// Scan and return next token
Token scanToken() {
    skipWhitespace();
    scanner.start = scanner.current;
    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();
    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c)
    {
    case '(': return makeToken(TOKEN_LEFT_PAREN);
    case ')': return makeToken(TOKEN_RIGHT_PAREN);
    case '{': return makeToken(TOKEN_LEFT_BRACE);
    case '}': return makeToken(TOKEN_RIGHT_BRACE);
    case ';': return makeToken(TOKEN_SEMICOLON);
    case ',': return makeToken(TOKEN_COMMA);
    case '.': return makeToken(TOKEN_DOT);
    case '-': return makeToken(TOKEN_MINUS);
    case '+': return makeToken(TOKEN_PLUS);
    case '/': return makeToken(TOKEN_SLASH);
    case '*': return makeToken(TOKEN_STAR);
    case '!': return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=': return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"': return string();
    default:
        break;
    }

    return errorToken("Unexpected character.");
}