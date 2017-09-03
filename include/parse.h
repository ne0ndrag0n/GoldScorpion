#ifndef GOLDSCORPION_PARSE
#define GOLDSCORPION_PARSE

#include <stddef.h>
#include <setjmp.h>
#include <stdbool.h>
#include "lex.h"

struct ASTNode;

typedef enum ASTNodeType {
  ASTLiteral,
  ASTIdentifier,
  ASTGetter,
  ASTArgumentList,
  ASTCall,
  ASTUnaryExpression,
  ASTBinaryExpression,
  ASTAssignment,
  ASTPackageStatement,
  ASTImportStatement,
  ASTVardecl,
  ASTBlock,
  ASTTypeSpecifier,
  ASTFunction,
  ASTReturnStatement
} ASTNodeType;

typedef struct ExprGet {
  struct ASTNode* source;
  char* field;
} ExprGet;

typedef struct ExprCall {
  struct ASTNode* source;
  struct ASTNode* arguments;
} ExprCall;

typedef struct UnaryExpression {
  struct ASTNode* rhs;
  Token op;
} UnaryExpression;

typedef struct BinaryExpression {
  struct ASTNode* lhs;
  struct ASTNode* rhs;
  Token op;
} BinaryExpression;

typedef struct AssignmentExpression {
  struct ASTNode* lhs;
  struct ASTNode* rhs;
  Token op;
  bool newQualifier;
  bool stackQualifier;
} AssignmentExpression;

typedef struct List_Node {
  struct ASTNode* data;
  struct List_Node* next;
} List_Node;

typedef struct ImportStatement {
  List_Node* imports;
  struct ASTNode* from;
} ImportStatement;

typedef struct TypeSpecifier {
  bool udt;
  union {
    struct ASTNode* udt;
    Token primitive;
  } type;
  bool array;
} TypeSpecifier;

typedef struct Vardecl {
  struct ASTNode* typeSpecifier;
  struct ASTNode* identifier;
  struct ASTNode* assignmentExpression;
} Vardecl;

typedef struct Fundecl {
  struct ASTNode* typeSpecifier;
  struct ASTNode* identifier;
  List_Node* arguments;
  struct ASTNode* body;
} Fundecl;

typedef struct ASTNode {
  ASTNodeType type;
  union {
    // ASTLiteral, ASTIdentifier
    Token token;
    // ASTGetter
    ExprGet get;
    // ASTArgumentList, ASTBlock
    List_Node* expressionList;
    // ASTCall
    ExprCall call;
    // ASTUnaryExpression
    UnaryExpression unaryExpression;
    // ASTBinaryExpression
    BinaryExpression binaryExpression;
    // ASTAssignment
    AssignmentExpression assignmentExpression;
    // ASTPackageStatement, ASTReturnStatement
    struct ASTNode* node;
    // ASTImportStatement
    ImportStatement import;
    // ASTVardecl
    Vardecl vardecl;
    // ASTTypeSpecifier
    TypeSpecifier specifier;
    // ASTFunction
    Fundecl function;
  } data;
} ASTNode;

typedef struct Parser {
  List_Token* prev;
  List_Token* current;
  jmp_buf exceptionHandler;
  char* error;
} Parser;

void gsParserThrow( Parser* self, char* error );

void gsParserIncrement( Parser* self );
List_Token* gsParserExpect( Parser* self, TokenType type );
List_Token* gsIndeterminateLookahead( List_Token* start, TokenType first, TokenType second );

ASTNode* gsCreatePrimaryNode( Token token );
ASTNode* gsCreateGetNode( ASTNode* source, char* field );
ASTNode* gsCreateCallNode( ASTNode* source, ASTNode* arguments );
ASTNode* gsCreateUnaryExpressionNode( Token op, ASTNode* rhs );
ASTNode* gsCreateBinaryExpressionNode( ASTNode* lhs, Token op, ASTNode* rhs );
ASTNode* gsCreateAssignmentExpressionNode( ASTNode* lhs, Token op, ASTNode* rhs, bool nw, bool stack );

ASTNode* gsGetArguments( Parser* self );

ASTNode* gsGetExpressionPrimary( Parser* self );
ASTNode* gsGetExpressionCall( Parser* self );
ASTNode* gsGetExpressionUnary( Parser* self );
ASTNode* gsGetExpressionMultiplication( Parser* self );
ASTNode* gsGetExpressionAddition( Parser* self );
ASTNode* gsGetExpressionComparison( Parser* self );
ASTNode* gsGetExpressionEquality( Parser* self );
ASTNode* gsGetExpressionLogicAnd( Parser* self );
ASTNode* gsGetExpressionLogicOr( Parser* self );
ASTNode* gsGetExpressionAssignment( Parser* self );
ASTNode* gsGetExpression( Parser* self );

ASTNode* gsGetPackageStatement( Parser* self );
ASTNode* gsGetImportStatement( Parser* self );
ASTNode* gsGetReturnStatement( Parser* self );
ASTNode* gsGetStatement( Parser* self );

ASTNode* gsGetVarDecl( Parser* self, bool independent );
ASTNode* gsGetFunDecl( Parser* self );

ASTNode* gsGetDeclaration( Parser* self );

ASTNode* gsGetBlock( Parser* self );
ASTNode* gsGetTypeSpecifier( Parser* self );

Parser* gsGetParser( List_Token* starterToken );

void gsDebugPrintAST( ASTNode* root );

#endif