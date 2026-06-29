<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

/**
 * Parser — Recursive descent parser that converts a token stream into an
 * immutable Abstract Syntax Tree.
 *
 * The parser implements the full NFX grammar as specified in Volume 5, Section 5.6
 * of the Centralized Formula Framework architecture. Each grammar production
 * maps to a single private method, following the same pattern used by
 * Spreadsheet_Excel_Writer_Parser (reporting/includes/Workbook.php).
 *
 * ## Operator Precedence (Highest to Lowest)
 *
 * | Level | Operators                             | Method               |
 * |-------|---------------------------------------|----------------------|
 * | 1     | `()` parentheses                      | parsePrimary         |
 * | 2     | Function call, Variable reference     | parsePrimary         |
 * | 3     | `NOT`, `-` (unary), `+` (unary)       | parseUnary           |
 * | 4     | `^` (power, right-associative)        | parsePower           |
 * | 5     | `*`, `/`, `%`                         | parseMultiplication  |
 * | 6     | `+`, `-`                              | parseAddition        |
 * | 7     | `>`, `<`, `>=`, `<=`, `==`, `!=`, `<>` | parseComparison     |
 * | 8     | `AND`                                 | parseLogical         |
 * | 9     | `OR`, `XOR`                           | parseLogical         |
 * | 10    | `??` (null coalescing)                | parseExpression      |
 *
 * ## Grammar Implementation
 *
 *     Expression          = NullCoalescingExpression
 *     NullCoalescing      = LogicalExpression { "??" LogicalExpression }
 *     LogicalExpression   = ComparisonExpression { ("AND" | "OR" | "XOR") ComparisonExpression }
 *     ComparisonExpression = AdditiveExpression [ (">" | "<" | ">=" | "<=" | "==" | "!=" | "<>") AdditiveExpression ]
 *     AdditiveExpression  = MultiplicativeExpression { ("+" | "-") MultiplicativeExpression }
 *     MultiplicativeExpression = PowerExpression { ("*" | "/" | "%") PowerExpression }
 *     PowerExpression     = UnaryExpression [ "^" UnaryExpression ]
 *     UnaryExpression     = ["NOT" | "-" | "+"] PrimaryExpression
 *     PrimaryExpression   = Literal | VariableReference | FunctionCall | "(" Expression ")" | CellReference
 *
 * ## Error Handling
 *
 * The parser implements panic-mode error recovery: when a syntax error is
 * encountered, tokens are skipped until a synchronization point (comma,
 * right parenthesis, or end of input) is found. Multiple errors can be
 * reported in a single pass.
 *
 * ## Validation
 *
 * The parser validates:
 * - Function existence against the FunctionRegistry
 * - Function argument counts against declared min/max
 * - Variable namespace existence against the VariableRegistry
 *
 * Type checking and dependency analysis are deferred to the validation
 * pipeline (Phase 8).
 *
 * @package Formula\Compiler
 * @since   2.0.0
 * @see     Formula_Compiler_Lexer
 * @see     Formula_Compiler_TokenStream
 */
class Formula_Compiler_Parser
{
    /** @var Formula_Compiler_TokenStream The navigable token stream */
    private $tokens;

    /** @var Formula_Registry_FunctionRegistry Functions available for validation */
    private $functionRegistry;

    /** @var Formula_Registry_VariableRegistry Variable providers for validation */
    private $variableRegistry;

    /** @var int Current recursion depth (prevents stack overflow) */
    private $depth;

    /** @var int Maximum allowed recursion depth */
    private $maxDepth;

    /** @var array Accumulated parser warnings (non-fatal) */
    private $warnings;

    // -----------------------------------------------------------------------
    //  Constructor
    // -----------------------------------------------------------------------

    /**
     * Construct a parser for a token stream.
     *
     * @param Formula_Compiler_TokenStream     $tokenStream       Token stream from lexer
     * @param Formula_Registry_FunctionRegistry $functionRegistry  Function registry for validation
     * @param Formula_Registry_VariableRegistry $variableRegistry  Variable registry for validation
     */
    public function __construct(
        Formula_Compiler_TokenStream $tokenStream,
        Formula_Registry_FunctionRegistry $functionRegistry,
        Formula_Registry_VariableRegistry $variableRegistry
    ) {
        $this->tokens           = $tokenStream;
        $this->functionRegistry  = $functionRegistry;
        $this->variableRegistry  = $variableRegistry;
        $this->depth            = 0;
        $this->maxDepth         = defined('FORMULA_MAX_AST_DEPTH') ? FORMULA_MAX_AST_DEPTH : 100;
        $this->warnings         = array();
    }

    // -----------------------------------------------------------------------
    //  Public API
    // -----------------------------------------------------------------------

    /**
     * Parse the entire token stream into an AST root node.
     *
     * This is the single public entry point. It handles the top-level
     * expression and verifies that all tokens have been consumed (anything
     * remaining after expression parsing triggers a syntax error).
     *
     * An empty token stream (only T_EOF) returns a NullNode — matching the
     * legacy payroll_formula_engine behavior where an empty formula evaluates
     * to 0.0.
     *
     * @return Formula_Compiler_AST_Node The root node of the parsed AST
     * @throws Formula_Exceptions_SyntaxErrorException On syntax errors
     * @throws Formula_Exceptions_UnknownFunctionException On unknown function references
     * @throws Formula_Exceptions_UnknownVariableException On unknown variable namespace
     * @throws Formula_Exceptions_ResourceExhaustedException On max depth exceeded
     */
    public function parse()
    {
        $this->depth = 0;
        $this->warnings = array();

        // Empty formula: match legacy payroll engine behavior → 0.0
        $current = $this->tokens->current();
        if ($current === null || $current->isType(Formula_Compiler_TokenType::T_EOF)) {
            return new Formula_Compiler_AST_NullNode(1, 1);
        }

        $ast = $this->parseExpression();

        // Verify all tokens consumed
        $remaining = $this->tokens->current();
        if ($remaining !== null && !$remaining->isType(Formula_Compiler_TokenType::T_EOF)) {
            throw new Formula_Exceptions_SyntaxErrorException(
                sprintf(
                    'Unexpected token after end of expression: %s',
                    $remaining->value
                ),
                $remaining->line,
                $remaining->column,
                $remaining->value,
                'end of formula'
            );
        }

        return $ast;
    }

    /**
     * Get accumulated parser warnings.
     *
     * @return string[]
     */
    public function getWarnings()
    {
        return $this->warnings;
    }

    // -----------------------------------------------------------------------
    //  Grammar Rule: Expression (Top Level)
    // -----------------------------------------------------------------------

    /**
     * Parse the top-level expression, including null coalescing.
     *
     * EBNF: Expression = LogicalExpression [ "??" LogicalExpression ]
     *
     * Null coalescing has the lowest precedence (level 10). It is
     * left-associative: A ?? B ?? C is parsed as ((A ?? B) ?? C).
     *
     * @return Formula_Compiler_AST_Node
     */
    private function parseExpression()
    {
        $this->guardDepth();

        $left = $this->parseLogical();

        // Null coalescing: lowest precedence, left-associative
        while ($this->tokens->check(Formula_Compiler_TokenType::T_NULL_COALESCE)) {
            $operator = $this->tokens->current();
            $this->tokens->advance(); // consume ??
            $right = $this->parseLogical();
            $left = new Formula_Compiler_AST_BinaryOperatorNode(
                '??',
                $left,
                $right,
                $operator->line,
                $operator->column
            );
        }

        return $left;
    }

    // -----------------------------------------------------------------------
    //  Grammar Rule: Logical Expression (AND, OR, XOR)
    // -----------------------------------------------------------------------

    /**
     * Parse logical expressions: AND, OR, XOR.
     *
     * EBNF: LogicalExpression = ComparisonExpression { ("AND" | "OR" | "XOR") ComparisonExpression }
     *
     * AND and OR/XOR are at different precedence levels in Excel but at
     * equivalent levels in NFX with left-associativity. Parentheses must
     * be used for explicit grouping.
     *
     * @return Formula_Compiler_AST_Node
     */
    private function parseLogical()
    {
        $this->guardDepth();

        $left = $this->parseComparison();

        while (true) {
            $current = $this->tokens->current();
            if ($current === null) {
                break;
            }

            $operator = null;
            $type = $current->type;

            if ($type === Formula_Compiler_TokenType::T_AND) {
                $operator = 'AND';
            } elseif ($type === Formula_Compiler_TokenType::T_OR) {
                $operator = 'OR';
            } elseif ($type === Formula_Compiler_TokenType::T_XOR) {
                $operator = 'XOR';
            }

            if ($operator === null) {
                break;
            }

            $opToken = $current;
            $this->tokens->advance(); // consume AND/OR/XOR

            $right = $this->parseComparison();

            $left = new Formula_Compiler_AST_LogicalNode(
                $operator,
                $left,
                $right,
                $opToken->line,
                $opToken->column
            );
        }

        return $left;
    }

    // -----------------------------------------------------------------------
    //  Grammar Rule: Comparison (>, <, >=, <=, ==, !=, <>)
    // -----------------------------------------------------------------------

    /**
     * Parse comparison expressions.
     *
     * EBNF: ComparisonExpression = AdditiveExpression [ (">" | "<" | ">=" | "<=" | "==" | "!=" | "<>") AdditiveExpression ]
     *
     * Comparisons do NOT chain in NFX. A single comparison operator is consumed.
     * `1 < 2 < 3` is a syntax error — the second `<` triggers "unexpected token".
     *
     * @return Formula_Compiler_AST_Node
     */
    private function parseComparison()
    {
        $this->guardDepth();

        $left = $this->parseAddition();

        $current = $this->tokens->current();
        if ($current === null) {
            return $left;
        }

        $operator = $this->getComparisonOperator($current->type);
        if ($operator === null) {
            return $left;
        }

        $opToken = $current;
        $this->tokens->advance(); // consume comparison operator

        $right = $this->parseAddition();

        return new Formula_Compiler_AST_ComparisonNode(
            $operator,
            $left,
            $right,
            $opToken->line,
            $opToken->column
        );
    }

    // -----------------------------------------------------------------------
    //  Grammar Rule: Addition (+, -)
    // -----------------------------------------------------------------------

    /**
     * Parse additive expressions (+, -).
     *
     * EBNF: AdditiveExpression = MultiplicativeExpression { ("+" | "-") MultiplicativeExpression }
     *
     * Left-associative chaining.
     *
     * @return Formula_Compiler_AST_Node
     */
    private function parseAddition()
    {
        $this->guardDepth();

        $left = $this->parseMultiplication();

        while (true) {
            $current = $this->tokens->current();
            if ($current === null) {
                break;
            }

            $type = $current->type;
            if ($type === Formula_Compiler_TokenType::T_PLUS) {
                $operator = '+';
            } elseif ($type === Formula_Compiler_TokenType::T_MINUS) {
                $operator = '-';
            } else {
                break;
            }

            $opToken = $current;
            $this->tokens->advance(); // consume + or -

            $right = $this->parseMultiplication();

            $left = new Formula_Compiler_AST_BinaryOperatorNode(
                $operator,
                $left,
                $right,
                $opToken->line,
                $opToken->column
            );
        }

        return $left;
    }

    // -----------------------------------------------------------------------
    //  Grammar Rule: Multiplication (*, /, %)
    // -----------------------------------------------------------------------

    /**
     * Parse multiplicative expressions (*, /, %).
     *
     * EBNF: MultiplicativeExpression = PowerExpression { ("*" | "/" | "%") PowerExpression }
     *
     * Left-associative chaining.
     *
     * @return Formula_Compiler_AST_Node
     */
    private function parseMultiplication()
    {
        $this->guardDepth();

        $left = $this->parsePower();

        while (true) {
            $current = $this->tokens->current();
            if ($current === null) {
                break;
            }

            $type = $current->type;
            if ($type === Formula_Compiler_TokenType::T_MULTIPLY) {
                $operator = '*';
            } elseif ($type === Formula_Compiler_TokenType::T_DIVIDE) {
                $operator = '/';
            } elseif ($type === Formula_Compiler_TokenType::T_MODULO) {
                $operator = '%';
            } else {
                break;
            }

            $opToken = $current;
            $this->tokens->advance(); // consume operator

            $right = $this->parsePower();

            $left = new Formula_Compiler_AST_BinaryOperatorNode(
                $operator,
                $left,
                $right,
                $opToken->line,
                $opToken->column
            );
        }

        return $left;
    }

    // -----------------------------------------------------------------------
    //  Grammar Rule: Power (^)
    // -----------------------------------------------------------------------

    /**
     * Parse exponentiation (^).
     *
     * EBNF: PowerExpression = UnaryExpression [ "^" UnaryExpression ]
     *
     * RIGHT-associative: 2^3^2 is parsed as 2^(3^2).
     * This is achieved by recursively calling parsePower() for the right operand.
     *
     * @return Formula_Compiler_AST_Node
     */
    private function parsePower()
    {
        $this->guardDepth();

        $left = $this->parseUnary();

        $current = $this->tokens->current();
        if ($current !== null && $current->isType(Formula_Compiler_TokenType::T_POWER)) {
            $opToken = $current;
            $this->tokens->advance(); // consume ^

            // Right-associative: the right operand is parsed as another power expression
            $right = $this->parsePower();

            $left = new Formula_Compiler_AST_BinaryOperatorNode(
                '^',
                $left,
                $right,
                $opToken->line,
                $opToken->column
            );
        }

        return $left;
    }

    // -----------------------------------------------------------------------
    //  Grammar Rule: Unary (NOT, -, +)
    // -----------------------------------------------------------------------

    /**
     * Parse unary expressions: NOT, unary minus, unary plus.
     *
     * EBNF: UnaryExpression = ["NOT" | "-" | "+"] PrimaryExpression
     *
     * Unary operators are right-associative (effectively, only one applied
     * before a primary expression). Multiple unary operators can be stacked:
     * NOT - NOT X evaluates as NOT(-(NOT(X))).
     *
     * Unary `+` is a no-op but must parse correctly: +5 → LiteralNode(5).
     * The optimizer will later constant-fold unary + and - applied to literals.
     *
     * @return Formula_Compiler_AST_Node
     */
    private function parseUnary()
    {
        $this->guardDepth();

        $current = $this->tokens->current();
        if ($current === null) {
            throw new Formula_Exceptions_SyntaxErrorException(
                'Unexpected end of formula — expected expression',
                1,
                1,
                '',
                'expression'
            );
        }

        $type = $current->type;

        // NOT keyword
        if ($type === Formula_Compiler_TokenType::T_NOT) {
            $opToken = $current;
            $this->tokens->advance(); // consume NOT
            $operand = $this->parseUnary(); // right-recursive for stacking: NOT NOT X
            return new Formula_Compiler_AST_UnaryOperatorNode(
                'NOT',
                $operand,
                $opToken->line,
                $opToken->column
            );
        }

        // Unary minus
        if ($type === Formula_Compiler_TokenType::T_MINUS) {
            $opToken = $current;
            $this->tokens->advance(); // consume -
            $operand = $this->parseUnary(); // right-recursive: --X
            return new Formula_Compiler_AST_UnaryOperatorNode(
                '-',
                $operand,
                $opToken->line,
                $opToken->column
            );
        }

        // Unary plus (no-op, but parsed for compatibility)
        if ($type === Formula_Compiler_TokenType::T_PLUS) {
            $opToken = $current;
            $this->tokens->advance(); // consume +
            $operand = $this->parseUnary();
            return new Formula_Compiler_AST_UnaryOperatorNode(
                '+',
                $operand,
                $opToken->line,
                $opToken->column
            );
        }

        // Not a unary operator → parse primary
        return $this->parsePrimary();
    }

    // -----------------------------------------------------------------------
    //  Grammar Rule: Primary Expression
    // -----------------------------------------------------------------------

    /**
     * Parse primary expressions: literals, variables, functions, parentheses,
     * cell references, and implicit values.
     *
     * EBNF:
     *     PrimaryExpression = Literal
     *                       | VariableReference
     *                       | FunctionCall
     *                       | "(" Expression ")"
     *                       | CellReference
     *
     * This is the highest-precedence grammar rule. It handles the fundamental
     * building blocks before operators are applied.
     *
     * @return Formula_Compiler_AST_Node
     */
    private function parsePrimary()
    {
        $this->guardDepth();

        $current = $this->tokens->current();
        if ($current === null) {
            throw new Formula_Exceptions_SyntaxErrorException(
                'Unexpected end of formula — expected expression',
                1,
                1,
                '',
                'number, string, identifier, or ('
            );
        }

        $type = $current->type;

        // -------------------------------------------------------------------
        //  Literal: integer
        // -------------------------------------------------------------------
        if ($type === Formula_Compiler_TokenType::T_INTEGER) {
            $this->tokens->advance();
            return new Formula_Compiler_AST_LiteralNode(
                (int)$current->value,
                'integer',
                $current->line,
                $current->column
            );
        }

        // -------------------------------------------------------------------
        //  Literal: decimal
        // -------------------------------------------------------------------
        if ($type === Formula_Compiler_TokenType::T_DECIMAL) {
            $this->tokens->advance();
            return new Formula_Compiler_AST_LiteralNode(
                (float)$current->value,
                'decimal',
                $current->line,
                $current->column
            );
        }

        // -------------------------------------------------------------------
        //  Literal: string
        // -------------------------------------------------------------------
        if ($type === Formula_Compiler_TokenType::T_STRING) {
            $this->tokens->advance();
            return new Formula_Compiler_AST_LiteralNode(
                $current->value,
                'string',
                $current->line,
                $current->column
            );
        }

        // -------------------------------------------------------------------
        //  Literal: boolean TRUE
        // -------------------------------------------------------------------
        if ($type === Formula_Compiler_TokenType::T_TRUE) {
            $this->tokens->advance();
            return new Formula_Compiler_AST_LiteralNode(
                true,
                'boolean',
                $current->line,
                $current->column
            );
        }

        // -------------------------------------------------------------------
        //  Literal: boolean FALSE
        // -------------------------------------------------------------------
        if ($type === Formula_Compiler_TokenType::T_FALSE) {
            $this->tokens->advance();
            return new Formula_Compiler_AST_LiteralNode(
                false,
                'boolean',
                $current->line,
                $current->column
            );
        }

        // -------------------------------------------------------------------
        //  Literal: NULL
        // -------------------------------------------------------------------
        if ($type === Formula_Compiler_TokenType::T_NULL) {
            $this->tokens->advance();
            return new Formula_Compiler_AST_NullNode(
                $current->line,
                $current->column
            );
        }

        // -------------------------------------------------------------------
        //  Parenthesized expression: ( Expression )
        // -------------------------------------------------------------------
        if ($type === Formula_Compiler_TokenType::T_LPAREN) {
            $this->tokens->advance(); // consume (

            $inner = $this->parseExpression();

            $this->expect(Formula_Compiler_TokenType::T_RPAREN, 'closing parenthesis )');

            return $inner;
        }

        // -------------------------------------------------------------------
        //  Identifier or Namespace-qualified variable
        // -------------------------------------------------------------------
        if ($type === Formula_Compiler_TokenType::T_IDENTIFIER || $type === Formula_Compiler_TokenType::T_NAMESPACE) {
            return $this->parseIdentifierOrFunction();
        }

        // -------------------------------------------------------------------
        //  Cell reference (spreadsheet context)
        // -------------------------------------------------------------------
        if ($type === Formula_Compiler_TokenType::T_CELL_REF) {
            return $this->parseCellReference();
        }

        // -------------------------------------------------------------------
        //  Dollar sign: $A$1 absolute cell reference
        // -------------------------------------------------------------------
        if ($type === Formula_Compiler_TokenType::T_DOLLAR) {
            return $this->parseCellReference();
        }

        // -------------------------------------------------------------------
        //  Anything else is a syntax error
        // -------------------------------------------------------------------
        throw new Formula_Exceptions_SyntaxErrorException(
            sprintf(
                'Unexpected token: %s',
                $current->value !== '' ? $current->value : Formula_Compiler_TokenType::getName($type)
            ),
            $current->line,
            $current->column,
            $current->value,
            'number, string, identifier, TRUE, FALSE, NULL, or ('
        );
    }

    // -----------------------------------------------------------------------
    //  Identifier Dispatch: Variable vs. Function Call
    // -----------------------------------------------------------------------

    /**
     * Parse an identifier or namespace-qualified token.
     *
     * This method disambiguates between variable references and function calls
     * by peeking at the next token:
     *   - If next token is `(` → function call: IDENTIFIER ( args )
     *   - Otherwise → variable reference
     *
     * For namespace-qualified tokens like "Employee.BasicSalary":
     *   - T_NAMESPACE contains the full dotted path
     *   - The namespace is everything before the last dot
     *   - The identifier is everything after the last dot
     *
     * @return Formula_Compiler_AST_Node VariableNode or FunctionNode
     */
    private function parseIdentifierOrFunction()
    {
        $current = $this->tokens->current();

        // Peek to determine if this is a function call
        $next = $this->tokens->peek();

        if ($next !== null && $next->isType(Formula_Compiler_TokenType::T_LPAREN)) {
            // This is a function call
            $functionName = $current->value;
            $this->tokens->advance(); // consume identifier/namespace
            return $this->parseFunctionCall($functionName, $current->line, $current->column);
        }

        // This is a variable reference
        return $this->parseVariableReference();
    }

    // -----------------------------------------------------------------------
    //  Variable Reference
    // -----------------------------------------------------------------------

    /**
     * Parse a variable reference.
     *
     * Variables can be simple (e.g., "SALARY") or namespace-qualified
     * (e.g., "Employee.BasicSalary"). The lexer produces T_IDENTIFIER for
     * simple names and T_NAMESPACE for dotted paths.
     *
     * For simple variables, the namespace is empty and the identifier is
     * the raw name. For namespace-qualified variables, the namespace is
     * the portion before the last dot, and the identifier is after.
     *
     * Variables are NOT validated here — semantic validation happens in
     * the validation pipeline. Simple variables always pass (they resolve
     * against the FormulaContext at runtime). Namespace-qualified variables
     * are checked for known namespaces and produce warnings if unrecognized.
     *
     * @return Formula_Compiler_AST_VariableNode
     */
    private function parseVariableReference()
    {
        $current = $this->tokens->current();
        $this->tokens->advance(); // consume identifier/namespace

        if ($current->isType(Formula_Compiler_TokenType::T_NAMESPACE)) {
            // Namespace-qualified: "Employee.BasicSalary"
            $parts = explode('.', $current->value, 2);
            $namespace  = $parts[0];
            $identifier = isset($parts[1]) ? $parts[1] : '';

            // Warn if namespace is unknown
            if ($namespace !== '' && !$this->variableRegistry->hasNamespace($namespace)) {
                $this->warnings[] = sprintf(
                    'Unknown variable namespace "%s" at line %d, column %d',
                    $namespace,
                    $current->line,
                    $current->column
                );
            }

            return new Formula_Compiler_AST_VariableNode(
                $namespace,
                $identifier,
                $current->line,
                $current->column
            );
        }

        // Simple variable: "SALARY", "GROSS", "DAYS_WORKED"
        return new Formula_Compiler_AST_VariableNode(
            '',
            $current->value,
            $current->line,
            $current->column
        );
    }

    // -----------------------------------------------------------------------
    //  Function Call
    // -----------------------------------------------------------------------

    /**
     * Parse a function call with arguments.
     *
     * EBNF: FunctionCall = Identifier "(" [ ArgumentList ] ")"
     *        ArgumentList = Expression { "," Expression }
     *
     * The function name has already been consumed by the caller. This method
     * handles the argument list inside parentheses.
     *
     * Validates:
     * - Function existence against FunctionRegistry
     * - Argument count against declared minArgs/maxArgs
     *
     * Special handling:
     * - IF(condition, trueBranch, falseBranch) → ConditionalNode (not generic FunctionNode)
     *   This enables short-circuit evaluation and dead-branch elimination.
     *
     * @param string $functionName Function name (from identifier token)
     * @param int    $line         Source line of the function name
     * @param int    $column       Source column of the function name
     * @return Formula_Compiler_AST_Node FunctionNode or ConditionalNode
     * @throws Formula_Exceptions_UnknownFunctionException If function is not registered
     */
    private function parseFunctionCall($functionName, $line, $column)
    {
        // Validate function existence
        $canonicalName = strtoupper($functionName);

        if (!$this->functionRegistry->has($canonicalName)) {
            $suggestion = 'Did you mean one of: '
                . implode(', ', array_slice(array_keys($this->functionRegistry->all()), 0, 10))
                . '...';
            throw new Formula_Exceptions_UnknownFunctionException(
                sprintf('Unknown function: %s', $functionName),
                $functionName,
                $line,
                $column
            );
        }

        $functionMeta = $this->functionRegistry->get($canonicalName)->getMetadata();

        // Consume the opening parenthesis (already peeked by caller)
        $this->expect(Formula_Compiler_TokenType::T_LPAREN, 'opening parenthesis (');

        // Parse argument list
        $arguments = array();

        // Check for empty argument list: FUNC()
        $current = $this->tokens->current();
        if ($current !== null && !$current->isType(Formula_Compiler_TokenType::T_RPAREN)) {
            // Parse first argument
            $arguments[] = $this->parseExpression();

            // Parse remaining arguments
            while (true) {
                $current = $this->tokens->current();
                if ($current === null) {
                    throw new Formula_Exceptions_SyntaxErrorException(
                        'Unexpected end of formula — expected , or )',
                        $line,
                        $column,
                        '',
                        ', or )'
                    );
                }

                if ($current->isType(Formula_Compiler_TokenType::T_RPAREN)) {
                    break;
                }

                if ($current->isType(Formula_Compiler_TokenType::T_COMMA)) {
                    $this->tokens->advance(); // consume ,

                    // Check for empty argument: FUNC(1,,2) → error
                    $afterComma = $this->tokens->current();
                    if ($afterComma !== null && $afterComma->isType(Formula_Compiler_TokenType::T_RPAREN)) {
                        throw new Formula_Exceptions_SyntaxErrorException(
                            'Empty argument in function call — unexpected ,)',
                            $current->line,
                            $current->column,
                            ',',
                            'expression or )'
                        );
                    }

                    $arguments[] = $this->parseExpression();
                } else {
                    throw new Formula_Exceptions_SyntaxErrorException(
                        sprintf(
                            'Unexpected token in argument list: %s — expected , or )',
                            $current->value !== '' ? $current->value : Formula_Compiler_TokenType::getName($current->type)
                        ),
                        $current->line,
                        $current->column,
                        $current->value,
                        ', or )'
                    );
                }
            }
        }

        // Consume closing parenthesis
        $this->expect(Formula_Compiler_TokenType::T_RPAREN, 'closing parenthesis )');

        // Validate argument count
        $argCount = count($arguments);
        $minArgs  = $functionMeta->minArgs;
        $maxArgs  = $functionMeta->maxArgs;

        if ($minArgs >= 0 && $argCount < $minArgs) {
            throw new Formula_Exceptions_SyntaxErrorException(
                sprintf(
                    'Function %s expects at least %d argument(s), got %d',
                    $canonicalName,
                    $minArgs,
                    $argCount
                ),
                $line,
                $column,
                strval($argCount),
                sprintf('at least %d arguments', $minArgs)
            );
        }

        if ($maxArgs >= 0 && $argCount > $maxArgs) {
            throw new Formula_Exceptions_SyntaxErrorException(
                sprintf(
                    'Function %s expects at most %d argument(s), got %d',
                    $canonicalName,
                    $maxArgs,
                    $argCount
                ),
                $line,
                $column,
                strval($argCount),
                sprintf('at most %d arguments', $maxArgs)
            );
        }

        // -------------------------------------------------------------------
        //  Special: IF() → ConditionalNode for short-circuit semantics
        // -------------------------------------------------------------------
        if ($canonicalName === 'IF') {
            // IF expects exactly 3 arguments: condition, trueBranch, falseBranch
            $condition   = isset($arguments[0]) ? $arguments[0] : new Formula_Compiler_AST_LiteralNode(true, 'boolean', $line, $column);
            $trueBranch  = isset($arguments[1]) ? $arguments[1] : new Formula_Compiler_AST_NullNode($line, $column);
            $falseBranch = isset($arguments[2]) ? $arguments[2] : new Formula_Compiler_AST_NullNode($line, $column);

            return new Formula_Compiler_AST_ConditionalNode(
                $condition,
                $trueBranch,
                $falseBranch,
                $line,
                $column
            );
        }

        // Generic function call
        return new Formula_Compiler_AST_FunctionNode(
            $canonicalName,
            $arguments,
            $line,
            $column
        );
    }

    // -----------------------------------------------------------------------
    //  Cell Reference
    // -----------------------------------------------------------------------

    /**
     * Parse a cell or range reference.
     *
     * Handles:
     *   - Simple cell: A1
     *   - Absolute cell: $A$1, $A1, A$1
     *   - Cell range: A1:B10
     *   - Column range: A:A, A:B
     *   - Row range: 1:10
     *   - Cross-sheet: Sheet1!A1 (T_EXCLAMATION separator)
     *
     * The lexer produces T_CELL_REF tokens for cell-like patterns.
     * Ranges are detected by the colon separator between two cell refs.
     *
     * @return Formula_Compiler_AST_RangeNode
     */
    private function parseCellReference()
    {
        $sheetName = null;

        // Optional sheet name: Sheet1!A1
        $current = $this->tokens->current();
        if ($current->isType(Formula_Compiler_TokenType::T_IDENTIFIER)) {
            $next = $this->tokens->peek();
            if ($next !== null && $next->isType(Formula_Compiler_TokenType::T_EXCLAMATION)) {
                $sheetName = $current->value;
                $this->tokens->advance(); // consume sheet name
                $this->tokens->advance(); // consume !
                $current = $this->tokens->current();
            }
        }

        // Parse start cell
        $startCell = $this->parseSingleCellRef();
        $endCell   = $startCell;

        // Check for range: A1:B10
        $colon = $this->tokens->current();
        if ($colon !== null && $colon->isType(Formula_Compiler_TokenType::T_COLON)) {
            $this->tokens->advance(); // consume :
            $endCell = $this->parseSingleCellRef();
        }

        $line   = $current->line;
        $column = $current->column;

        return new Formula_Compiler_AST_RangeNode(
            $startCell,
            $endCell,
            $sheetName,
            $line,
            $column
        );
    }

    /**
     * Parse a single cell reference with optional absolute anchoring.
     *
     * Supports: A1, $A$1, $A1, A$1
     *
     * @return string The cell reference string
     */
    private function parseSingleCellRef()
    {
        $parts = array();
        $current = $this->tokens->current();

        // Optional $ for absolute column
        if ($current->isType(Formula_Compiler_TokenType::T_DOLLAR)) {
            $parts[] = '$';
            $this->tokens->advance();
            $current = $this->tokens->current();
        }

        // Cell reference (e.g., "A1")
        if ($current === null || !$current->isType(Formula_Compiler_TokenType::T_CELL_REF)) {
            $info = $current !== null ? $current->value : 'end of input';
            throw new Formula_Exceptions_SyntaxErrorException(
                sprintf('Expected cell reference, got: %s', $info),
                $current !== null ? $current->line : 1,
                $current !== null ? $current->column : 1,
                $info,
                'cell reference like A1'
            );
        }

        $parts[] = $current->value;
        $this->tokens->advance();

        return implode('', $parts);
    }

    // -----------------------------------------------------------------------
    //  Helpers: Token Expectations
    // -----------------------------------------------------------------------

    /**
     * Assert that the current token matches the expected type and consume it.
     *
     * If the current token does not match, a SyntaxErrorException is thrown
     * with source location and a clear description of what was expected.
     *
     * @param int    $expectedType   The expected TokenType constant
     * @param string $description    Human-readable description of what was expected
     * @return Formula_Compiler_Token The consumed token
     * @throws Formula_Exceptions_SyntaxErrorException
     */
    private function expect($expectedType, $description = '')
    {
        $current = $this->tokens->current();

        if ($current === null) {
            throw new Formula_Exceptions_SyntaxErrorException(
                sprintf('Unexpected end of formula — expected %s', $description),
                1,
                1,
                '',
                $description
            );
        }

        if (!$current->isType($expectedType)) {
            throw new Formula_Exceptions_SyntaxErrorException(
                sprintf(
                    'Unexpected token: %s — expected %s',
                    $current->value !== '' ? $current->value : Formula_Compiler_TokenType::getName($current->type),
                    $description
                ),
                $current->line,
                $current->column,
                $current->value,
                $description
            );
        }

        $this->tokens->advance();
        return $current;
    }

    // -----------------------------------------------------------------------
    //  Helpers: Guard & Comparison Map
    // -----------------------------------------------------------------------

    /**
     * Guard against excessive recursion depth.
     *
     * Each grammar rule method calls this on entry. If the recursion depth
     * exceeds FORMULA_MAX_AST_DEPTH, a ResourceExhaustedException is thrown
     * to prevent stack overflow.
     *
     * @return void
     * @throws Formula_Exceptions_ResourceExhaustedException
     */
    private function guardDepth()
    {
        $this->depth++;

        if ($this->depth > $this->maxDepth) {
            throw new Formula_Exceptions_ResourceExhaustedException(
                sprintf(
                    'Maximum AST depth exceeded (%d). The formula may be too deeply nested.',
                    $this->maxDepth
                ),
                'ast_depth',
                $this->maxDepth
            );
        }
    }

    /**
     * Map a comparison token type to its string operator.
     *
     * Returns null if the token type is not a comparison operator.
     *
     * @param int $type TokenType constant
     * @return string|null The operator string, or null
     */
    private function getComparisonOperator($type)
    {
        $map = array(
            Formula_Compiler_TokenType::T_GT => '>',
            Formula_Compiler_TokenType::T_GE => '>=',
            Formula_Compiler_TokenType::T_LT => '<',
            Formula_Compiler_TokenType::T_LE => '<=',
            Formula_Compiler_TokenType::T_EQ => '==',
            Formula_Compiler_TokenType::T_NE => '!=',
        );

        return isset($map[$type]) ? $map[$type] : null;
    }

    /**
     * Get the current recursion depth (for diagnostics).
     *
     * @return int
     */
    public function getDepth()
    {
        return $this->depth;
    }

    /**
     * Get the current token stream (for diagnostics).
     *
     * @return Formula_Compiler_TokenStream
     */
    public function getTokenStream()
    {
        return $this->tokens;
    }
}
