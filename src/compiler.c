#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

/* Struct for parsing of tokenized source code
*
*   Fields:
*   - Token current: token being scanned
*   - Token previous: token that was just scanned
*   - bool hadError: whether parser had error
*   - bool panicMode: whether parser is in panic mode, meaning it won't generate error until next synchronization

*/
typedef struct {
    Token current; // token being scanned
    Token previous; // token that was just scanned
    bool hadError; // whether parser had error
    bool panicMode; // whether parser is in panic mode, meaning it won't generate error until next synchronization
} Parser;

//Precedence enum, with types lower in this table having higher precedence
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,    // =
    PREC_OR,            // or
    PREC_AND,           // and
    PREC_EQUALITY,      // == !=
    PREC_COMPARISON,    // < > <= >=
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_UNARY,         // ! -
    PREC_CALL,          // . ()
    PREC_PRIMARY
} Precedence;

//Typedef for the function pointer to the function that later will result from ParseRule table
typedef void (*ParseFn)(bool canAssign);

/* Struct for row of ParseRules table, with information how token should be resolved
*
*   Fields:
*   - ParseFn prefix: function to use when token used as prefix
*   - ParseFn infix: function to use when token used as infix
*   - Precedence precedence: of the token
*/
typedef struct {
    ParseFn prefix; // function to use when token used as prefix
    ParseFn infix; // function to use when token used as infix
    Precedence precedence; // of the token
} ParseRule;

/* Local variable struct
*
*   Fields:
*   - Token name: of the variable
*   - int depth: of the scope variable is in
*   - bool isCaptured: whether is captured by upvalue
*/
typedef struct {
    Token name; // of the variable
    int depth;  // of the scope variable is in
    bool isCaptured; // whether is captured by upvalue
} Local;

/* Local variable struct
*
*   Fields:
*   - uint8_t index: which local slot the upvalue is capturing
*   - bool isLocal: whether upvalue in local scope or referencing other upvalue required
*/
typedef struct {
    uint8_t index; //  which local slot the upvalue is capturing
    bool isLocal; // whether upvalue in local scope or referencing other upvalue required
} Upvalue;

typedef enum {
    TYPE_FUNCTION, //Basic function
    TYPE_INITIALIZER, //Class initializer
    TYPE_METHOD, //Class method
    TYPE_SCRIPT //"Main" function containing whole source-code
} FunctionType;

/* Compiler struct for compiling one fuction
*
*   Fields:
*   - struct Compiler* enclosing: compiler
*   - ObjFunction* function: that compiler is working on
*   - FunctionType type: of the function that compiler is working on
*   - Local locals[UINT8_COUNT]: locals array
*   - int localCount: length of locals array
*   - Upvalue upvalues[UINT8_COUNT]: upvalues array
*   - int scopeDepth: compiler's scope depth, 0 for script
*/
typedef struct Compiler
{
    struct Compiler* enclosing; // enclosing compiler
    ObjFunction* function; // that compiler is working on
    FunctionType type; // of the function that compiler is working on

    Local locals[UINT8_COUNT]; // locals array
    int localCount; // length of locals array
    Upvalue upvalues[UINT8_COUNT]; // upvalues array
    int scopeDepth; // compiler's scope depth, 0 for script
} Compiler;

/* Compiler struct for compiling class
*
*   Fields:
*   - struct ClassCompiler* enclosing: compiler
*   - bool hasSuperclass: whether class has superclass
*/
typedef struct ClassCompiler {
    struct ClassCompiler* enclosing; // enclosing compiler
    bool hasSuperclass; // whether class has superclass
} ClassCompiler;

Parser parser; // Global parser
Compiler* current = NULL; // Global pointer to current compiler
ClassCompiler* currentClass = NULL; // Global pointer to current class compiler

//Returns chunk of currently compiled function
static Chunk* currentChunk() {
    return &current->function->chunk;
}

/* Outputs error
*   Arguments:
*   - Token* token: pointer to token for localization purposes
*   - const char* message: to be displayed
*/
static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        //Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

/* Outputs error at previous token localization
*   Arguments:
*   - const char* message: to be displayed
*/
static void error(const char* message) {
    errorAt(&parser.previous, message);
}

/* Outputs error at current token localization
*   Arguments:
*   - const char* message: to be displayed
*/
static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

//Scan and advances parser to the next token
static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
}

/* Check if current token is of given type
*   Arguments:
*   - TokenType type: to compare with current token
*/
static bool check(TokenType type) {
    return parser.current.type == type;
}

/* Advance parser when token is expected, with error otherwise
*   Arguments:
*   - TokenType type: of the expected token
*   - const char* message: of the error when found TokenType diffrent than expected
*/
static void consume(TokenType type, const char* message) {
    if (check(type)) {
        advance();
        return;
    }
    errorAtCurrent(message); //Raise error if token was other than expected
}

/* Advance parser when token is expected
*   Arguments:
*   - TokenType type: of the expected token
*   
*   Return whether match successful
*/
static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

/* Add byte and line number to the current chunk
*   Arguments:
*   - uint8_t byte: to add
*/
static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

/* Add two bytes and line numbers to the current chunk
*   Arguments:
*   - uint8_t byte1: to add
*   - uint8_t byte2: to add
*/
static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

/* Emit opcode and operator for looping
*   Arguments:
*   - int loopStart: address of first operation of the loop
*/
static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2; //Calculate the offset from current chunk byte to the loopStart
    if (offset > UINT16_MAX) error("Loop body to large."); //We need to fit offset in two bytes, so error if larger

    emitByte((offset >> 8) & 0xff); //Emit higher bits of offset
    emitByte((offset & 0xff));  //Emit lower bits of offset
}

/* Emit opcode and two bytes to be patched later
*   Arguments:
*   - uint8_t instruction: to be emitted
*/
static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

//Emit bytes for return statement without expressions
static void emitReturn() {
    if (current->type == TYPE_INITIALIZER) {
        emitBytes(OP_GET_LOCAL, 0); //Load slot 0 that contains instance
    } else {
        emitByte(OP_NIL); //Load nil onto the stack
    }
    emitByte(OP_RETURN);
}

/* Add value to the current chunk constants table
*   Arguments:
*   - Value value: to be addded to the table
*   
*   Return constant index in table
*/
static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}

/* Emit bytes for constants handling
*   Arguments:
*   - Value value: to be emitted
*/
static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

/* Patch jump instruction
*   Arguments:
*   - int offset: location of operator to patch
*/
static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself
    int jump = currentChunk()->count - offset - 2; // Calculate how many bytes to jump over

    if (jump > UINT16_MAX) {
        error("Too much code to jump over;");
    }

    currentChunk()->code[offset] = (jump>>8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

/* Initialize compiler
*   Arguments:
*   - Compiler* compiler: current compiler
*   - FunctionType type: of function that compiler will be working on
*/
static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current; // Set current compiler as enclosing compiler
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction(); // Create new function object
    current = compiler; // Set current compiler to just initialized one

    // If in named function then copy string from source code to the heap
    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    // Claim slot 0 of locals array to optionally store "this" instance if inside a method
    Local* local = &current->locals[current->localCount++]; 
    local->depth = 0;
    local->isCaptured = false;
    if (type != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

/* End compiler after function has been compiled
*
*   Return compiled function
*/
static ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction* function = current->function;
    #ifdef DEBUG_PRINT_CODE
        if (!parser.hadError) {
            disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
            printf("\n");
        }
    #endif

    current = current->enclosing; // Go back to enclosing compiler
    return function;
}

// Increment current compiler's scopeDepth counter
static void beginScope() {
    current->scopeDepth++;
}

// Decrement current compiler's scopeDepth counter and clean locals
static void endScope() {
    current->scopeDepth--;

    // Loop through locals
    while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
        if (current->locals[current->localCount-1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE); // If local need do be captured move it to the heap
        } else {
            emitByte(OP_POP); // Pop local from the stack if not being captured
        }
        current->localCount--;
    }
}

// Parse expression until token with PREC_ASSIGNMENT found
static void expression();

// Parse statement
static void statement();

// Parse declaration
static void declaration();

/* Get parse rule of token from rules table
*   Arguments:
*   - Precedence precedence: to compare with
*/
static ParseRule* getRule(TokenType type);

/* Parse tokens until token with the lower precedence found
*   Arguments:
*   - Precedence precedence: to compare with
*/
static void parsePrecedence(Precedence precedence);

/* Make constant out of identifier
*   Arguments:
*   - Token* name: that should be created on the heap
*
*   Return index in constants table
*/
static uint8_t identifierConstant(Token* name) {
    //Copy string from source code and allocate in in the constants table
    return makeConstant(OBJ_VAL(copyString(name->start,name->length)));
}

/* Check if identifiers equal
*   Arguments:
*   - Token* a: first identifier to compare
*   - Token* b: second identifier to compare
*
*   Return whether identifiers equal
*/
static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

/* Resolve local variable
*   Arguments:
*   - Compiler* compiler: to search variable in
*   - Token* name: of the variable to find
*
*   Return slot within locals array, which corresponds to index on the stack
*/
static int resolveLocal(Compiler* compiler, Token* name) {
    // Walking locals array backward so we find last declared variable. Required for shadowing to work
    for (int i = compiler->localCount -1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            /* Before fully initialized variable gets depth = -1
            Prevents e.g. var a = a * 2;
            */
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1; // Local wasn't found, maybe check the globals
}

/* Add upvalue to compiler's upvalues array
*   Arguments:
*   - Compiler* compiler: to add upvalue to
*   - uint8_t index: of the variable on the stack
*   - bool isLocal: whether variable is in direct enclosing or upvalues indirection will be necessary
*
*   Return slot within upvalues array
*/
static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    // Check if upvalue previously added. Return it if so.
    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }

    // Add at the end of upvalues array and increment count
    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

/* Resolve upvalue variable within enclosing functions
*   Arguments:
*   - Compiler* compiler: to search variable in
*   - Token* name: of the variable to find
*
*   Return upvalue index within compiler upvalues array
*/
static int resolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1; // Cannot loop through enclosings if no enclosing exist

    int local = resolveLocal(compiler->enclosing, name); // Try to find in direct parent enclosing
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true; // Indicate that upvalue needs to be hoisted onto the heap when clearing stack
        // If found in direct parent enclosing then upvalue indirection is not needed
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    // Recursively walk enclosing in search of upvalue
    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        // If found in direct higher enclosing then upvalue indirection is needed
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

/* Add local variable to current compiler
*   Arguments:
*   - Token* name: of the variable to add
*/
static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

// Declare local variable in current compiler, with just scanned identifier as a name
static void declareVariable() {
    if (current->scopeDepth == 0) return; // Global variable will be declared in runtime

    Token* name = &parser.previous; // Get variable name

    // Check for the same variable name in the current scope
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        
        // We've reached variable outside of current scope, escape
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        // Report error if found
        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }
    addLocal(*name);
}

/* Parse variable identifier
*   Arguments:
*   - const char* errorMessage: when no identifier found
*
*   Return index of name in constants table for globals
*/
static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);
    declareVariable(); // Declare local variable, global variable will be declared on runtime
    if (current->scopeDepth > 0) return 0; // Local will have name in compiler's locals table
    return identifierConstant(&parser.previous);
}

// Set the current scope depth to the top local variable
static void markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

/* Define global variable
*   Arguments:
*   - uint8_t global: index of variable name in globals table
*/
static void defineVariable(uint8_t global) {
    if (current->scopeDepth > 0) {
        /*If it is local there is no special things to do.
        It is already the last temporary remaining on stack.
        */
        markInitialized(); // Variable ready for use
        return;
    }
    // If reached then define global
    emitBytes(OP_DEFINE_GLOBAL, global);
}

/*Parse arguments list
*
*   Return number of arguments parsed
*/
static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (argCount == 255) {
                error("Can't have more than 255 arguments");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

/* Parse "and" infix token
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void and_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_IF_FALSE); // Short-circuit handling

    emitByte(OP_POP); // Pop 
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}


/* Parse "or" infix token
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void or_(bool canAssign) {
    // Elaborate jumps required for short-circuit behaviour
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);
    patchJump(elseJump);
    emitByte(OP_POP);
    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

/* Parse binary infix tokens
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence+1)); // Parse higher precedences

    switch (operatorType)
    {
    case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
    case TOKEN_GREATER: emitByte(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS: emitByte(OP_LESS); break;
    case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
    case TOKEN_PLUS: emitByte(OP_ADD); break;
    case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
    case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
    case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
    default: return;
    }
}

/* Parse "(" infix token
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

/* Parse dot infix token
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void dot(bool canAssign) {
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL)) {
        // Property setter
        expression();
        emitBytes(OP_SET_PROPERTY, name);
    } else if (match(TOKEN_LEFT_PAREN)) {
        // Method call
        uint8_t argCount = argumentList();
        emitBytes(OP_INVOKE, name);
        emitByte(argCount);
    } else {
        // Property getter
        emitBytes(OP_GET_PROPERTY, name);
    }
}

/* Parse literal tokens
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void literal(bool canAssign) {
    switch (parser.previous.type)
    {
    case TOKEN_FALSE: emitByte(OP_FALSE); break;
    case TOKEN_NIL: emitByte(OP_NIL); break;
    case TOKEN_TRUE: emitByte(OP_TRUE); break;
    default: break; //Unreachable
    }
}

/* Parse expression within (...)
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

/* Parse number token and add it to constants table
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

/* Parse string token and add it to constants table by copying from source to heap
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void string(bool canAssign) {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,parser.previous.length-2)));
}


/* Parse variable with a given name generating OP_GET_* or OP_SET_*
*   Arguments:
*   - Token name: token with the varaible name identifier
*   - bool canAssign: whether token used in contex where assignment available
*/
static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name); // Try finding in current compiler
    if (arg != -1) { // Found in local compiler
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) { // Try finding in enclosing functions
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        // Try finding in global scope
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    // If can assign and next token equal then set operation, get operation otherwise
    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else {
        emitBytes(getOp, (uint8_t)arg);
    }
}

/* Parse identifier token
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

/* Create token from given string
*   Arguments:
*   - const char* text: string to create token from
*
*   Return Token
*/
static Token syntheticToken(const char* text) {
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

/* Parse "super" prefix token
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void super_(bool canAssign) {
    if (currentClass == NULL) {
        error("Can't use 'super' outside of a class.");
    } else if (!currentClass->hasSuperclass) {
        error("can't use 'super' in a class with no superclass.");
    }
    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifierConstant(&parser.previous);

    namedVariable(syntheticToken("this"), false); // Create "this" token to add as a variable to point to instance
    if (match(TOKEN_LEFT_PAREN)) {
        // Super call right away
        uint8_t argCount = argumentList();
        namedVariable(syntheticToken("super"), false); // Create "super" token to add as a variable to point to superclass
        emitBytes(OP_SUPER_INVOKE, name);
        emitByte(argCount);
    } else {
        // Super get and allocate on heap
        namedVariable(syntheticToken("super"), false); // Create "super" token to add as a variable to point to superclass
        emitBytes(OP_GET_SUPER, name);
    }

}

/* Parse "this" prefix token
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void this_(bool canAssign) {
    if (currentClass == NULL) {
        error("Can't use 'this' outside of a class.");
        return;
    }
    /* Cannot assign to "this".
    Will be handled as normal variable as "this" synthetic token pointing to instance was created during instance creation
    */
    variable(false);
}

/* Parse unary prefix token
*   Arguments:
*   - bool canAssign: whether token used in contex where assignment available
*/
static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    parsePrecedence(PREC_UNARY); // Parse tokens with the same or higher precedence

    switch (operatorType) {
        case TOKEN_BANG: emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return;
    }
}

//Parse rules indicating how to treat every token when found in prefix or infix context
ParseRule rules[] = { // When parse rule not specified comment is added what functon parses it
    [TOKEN_LEFT_PAREN]      = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN]     = {NULL, NULL, PREC_NONE}, // Consumed when necessary
    [TOKEN_LEFT_BRACE]      = {NULL, NULL, PREC_NONE}, // Parsed with statement()
    [TOKEN_RIGHT_BRACE]     = {NULL, NULL, PREC_NONE}, // Consumed when necessary
    [TOKEN_COMMA]           = {NULL, NULL, PREC_NONE}, // Consumed when necessary
    [TOKEN_DOT]             = {NULL, dot, PREC_CALL},
    [TOKEN_MINUS]           = {unary, binary, PREC_TERM},
    [TOKEN_PLUS]            = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON]       = {NULL, NULL, PREC_NONE}, // Consumed when necessary
    [TOKEN_SLASH]           = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR]            = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG]            = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL]      = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL]           = {NULL, NULL, PREC_NONE}, // Consumed to indicate assignment
    [TOKEN_EQUAL_EQUAL]     = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER]         = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS]            = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]      = {variable, NULL, PREC_NONE},
    [TOKEN_STRING]          = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER]          = {number, NULL, PREC_NONE},
    [TOKEN_AND]             = {NULL, and_, PREC_AND},
    [TOKEN_CLASS]           = {NULL, NULL, PREC_NONE}, // Parsed with declaration()
    [TOKEN_ELSE]            = {NULL, NULL, PREC_NONE}, // Parsed in ifStatement()
    [TOKEN_FALSE]           = {literal, NULL, PREC_NONE},
    [TOKEN_FOR]             = {NULL, NULL, PREC_NONE}, // Parsed with statement()
    [TOKEN_FUN]             = {NULL, NULL, PREC_NONE}, // Parsed with declaration()
    [TOKEN_IF]              = {NULL, NULL, PREC_NONE}, // Parsed with statement()
    [TOKEN_NIL]             = {literal, NULL, PREC_NONE},
    [TOKEN_OR]              = {NULL, or_, PREC_OR},
    [TOKEN_PRINT]           = {NULL, NULL, PREC_NONE}, // Parsed with statement()
    [TOKEN_RETURN]          = {NULL, NULL, PREC_NONE}, // Parsed with statement()
    [TOKEN_SUPER]           = {super_, NULL, PREC_NONE},
    [TOKEN_THIS]            = {this_, NULL, PREC_NONE},
    [TOKEN_TRUE]            = {literal, NULL, PREC_NONE},
    [TOKEN_VAR]             = {NULL, NULL, PREC_NONE}, // Parsed with declaration()
    [TOKEN_WHILE]           = {NULL, NULL, PREC_NONE}, // Parsed with statement()
    [TOKEN_ERROR]           = {NULL, NULL, PREC_NONE}, // Token returned by prser when error happened
    [TOKEN_EOF]             = {NULL, NULL, PREC_NONE}, // End of file token
};

/* Parse tokens until token with the lower precedence found
*   Arguments:
*   - Precedence precedence: to compare with
*/
static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    /* Since assignment is the lowest-precedence expression,
        the only time we allow an assignment is when parsing
        an assignment expression or top-level expression like
        in an expression statement.
    */
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign); // Parse token according to prefix rule

    // Parse infix rules while next token precedence is higher or equal than given
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign); // Parse token according to infix rule
    }

    // If "=" was not consumed by expression then nothing will consume it
    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

/* Get parse rule of token from rules table
*   Arguments:
*   - Precedence precedence: to compare with
*/
static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

// Parse expression until token with PREC_ASSIGNMENT or PREC_NONE found
static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

// Parse declarations within {...}
static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

/* Parse function, including script
*   Arguments:
*   - FunctionType type: of function to parse
*/
static void function(FunctionType type) {
    // Define nand initialize new compiler. Add scope
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    
    // Parse argument list if passed
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant); // Define parameter
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block(); // Compile function body

    ObjFunction* function = endCompiler();

    // Handle upvalues
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    // Pass information gathered during variable resolving to the VM
    // Upvalues indicated here are upvalues from enclosings and needs to be saved onto the heap
    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0); // Whether upvalue is in direct enclosing scope or higher
        emitByte(compiler.upvalues[i].index); // Local or upvalue index to capture
    }
}

// Parse class method
static void method() {
    consume(TOKEN_IDENTIFIER, "Expect method name");
    uint8_t constant = identifierConstant(&parser.previous);

    FunctionType type = TYPE_METHOD;

    // Set type to initializer if name equals to "init"
    if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }
    function(type);

    emitBytes(OP_METHOD, constant);
}

// Parse class declaration statement
static void classDeclaration() {
    // Declare and define class variable
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint8_t nameConstant = identifierConstant(&parser.previous);
    declareVariable();
    emitBytes(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    // Create class compiler
    ClassCompiler classCompiler;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    // Handling superclass
    if (match(TOKEN_LESS)) {
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(false); // Parse superclass name

        if (identifiersEqual(&className, &parser.previous)) {
            error("A class can't inherit from itself.");
        }

        // Create local "super" variable
        beginScope(); // Ensuring that each class has it's own "super"
        addLocal(syntheticToken("super"));
        defineVariable(0);

        namedVariable(className, false); // Get superclass onto the stack
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(className, false); // Get class onto the stack

    // Parse methods
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        method();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body");
    emitByte(OP_POP); // Pop class from the stack as no longer needed

    // Closing scope that we've opened for "super"
    if (classCompiler.hasSuperclass) {
        endScope();
    }
    currentClass = currentClass->enclosing; // Go out of class compiler
}

// Parse function declaration statement
static void funDeclaration() {
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

// Parse variable declaration statement
static void varDeclaration() {
    uint8_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL); // Default varaible value
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
}

// Parse expression statement
static void expressionStatement() {
    expression();

    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");

    emitByte(OP_POP); // Statements don't leave anything on stack
}

// Parse "for" loop statement
static void forStatement() {
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after for.");

    // Parse initializer if exist
    if (match(TOKEN_SEMICOLON)) {
        //No initializer
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        expressionStatement();
    }

    int loopStart = currentChunk()->count;

    // Parse looping condition if exist
    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression(); // Condition expression
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        //Jump out of the loop if codition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Pop condition result
    }
    
    // Parse increment if exist
    // Increment is parsed before body, but executes after
    if(!match(TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(OP_JUMP); // Jump over increment clause
        int incrementStart = currentChunk()->count; // Remember where increment start
        expression(); // Evaluate expression
        emitByte(OP_POP); // Pop expression result
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clause.");

        emitLoop(loopStart); // Loop to the start (right before condition if exist)
        loopStart = incrementStart; // Mark this point to jump back agter loop body
        patchJump(bodyJump); // Patch jump over increment
    }

    statement(); // Statement within loop
    emitLoop(loopStart); // Loop to the start or to the increment if exist

    if (exitJump != -1) { // Patch looping condition if exist
        patchJump(exitJump);
        emitByte(OP_POP); // Pop condition expression result
    }

    endScope();
}

// Parse if statement
static void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(); // Condition
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Pop condition if true
    statement(); // Statement to evaluate when condition true

    int elseJump = emitJump(OP_JUMP); // Remember else position to patch later

    patchJump(thenJump); 
    emitByte(OP_POP); // Pop condition if false

    if (match(TOKEN_ELSE)) statement(); // Evaluate else statements if exist
    patchJump(elseJump);
}

// Parse print statement
static void printStatement() {
    expression();

    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

// Parse return statement
static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }
    if (match(TOKEN_SEMICOLON)) {
        emitReturn(); // Return without expression
    } else {
        if (current->type == TYPE_INITIALIZER) {
            error("Can't return a value from an initializer.");
        }
        expression(); // Evaluate what should be returned and leave it on the stack
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

// Parse while statement
static void whileStatement() {
    int loopStart = currentChunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after a while.");
    expression(); // Parse condition
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE); // Jump over loop body if condition false
    emitByte(OP_POP); // Pop condition expression result within loop

    statement();
    emitLoop(loopStart); // Jump to start of the loop

    patchJump(exitJump); // Patch where execution should resume after loop

    emitByte(OP_POP); // Pop condition expression result at the end of the loop
}

// Synchronize parser after error happened by skipping without errors to the next synchronization token
static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type)
        {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return; // Synchronization point reached, start parsing normally
        default:
            // Keep advancing
            break;
        }
        advance();
    }
}

// Parse declaration
static void declaration() {
    if (match(TOKEN_CLASS)) {
        classDeclaration();
    } else if (match(TOKEN_FUN)) {
        funDeclaration();
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }
    if (parser.panicMode) synchronize();
}

// Parse statement
static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if(match(TOKEN_RETURN)) {
        returnStatement();    
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

/* Compile source code to opcodes
*   Arguments:
*   - const char* source: pointer to the source code
*
*   Return script function that can be run by VM
*/
ObjFunction* compile(const char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }
    
    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}

// Mark functions on compile stack to not be sweeped by garbage collector
void markCompilerRoots() {
    Compiler* compiler = current; // Start with outermost compiler
    // Loop through linked list of enclosing compilers
    while (compiler != NULL) {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}