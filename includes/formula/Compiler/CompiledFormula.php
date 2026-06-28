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
 * CompiledFormula — Immutable compiled formula value object.
 *
 * Represents a formula that has been fully compiled: parsed, validated,
 * optimized, and prepared for execution. The compiled AST is immutable
 * and can be executed multiple times with different FormulaContexts.
 *
 * This is the primary output of the CompilerInterface::compile() method
 * and the primary input to the RuntimeInterface::execute() method.
 *
 * Key design principle: Compile once, execute many times.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 */
class Formula_Compiler_CompiledFormula
{
    /** @var Formula_Compiler_AST_Node The optimized, immutable AST root node */
    public $ast;

    /** @var Formula_Compiler_FormulaMetadata Pre-computed metadata */
    public $metadata;

    /** @var string SHA-256 checksum of the original formula source */
    public $sourceChecksum;

    /** @var float Compile time in milliseconds */
    public $compileTimeMs;

    /** @var array Compilation warnings (non-fatal) */
    public $warnings;

    /**
     * Construct a compiled formula.
     *
     * @param Formula_Compiler_AST_Node         $ast         The optimized AST root
     * @param Formula_Compiler_FormulaMetadata   $metadata    Pre-computed metadata
     * @param string                            $sourceChecksum SHA-256 of formula source
     * @param float                             $compileTimeMs Compilation duration in ms
     * @param array                             $warnings     Non-fatal compilation warnings
     */
    public function __construct(
        Formula_Compiler_AST_Node $ast,
        Formula_Compiler_FormulaMetadata $metadata,
        $sourceChecksum,
        $compileTimeMs = 0.0,
        array $warnings = array()
    ) {
        $this->ast            = $ast;
        $this->metadata       = $metadata;
        $this->sourceChecksum = (string)$sourceChecksum;
        $this->compileTimeMs  = (float)$compileTimeMs;
        $this->warnings       = $warnings;
    }

    /**
     * Get the AST root node.
     *
     * @return Formula_Compiler_AST_Node
     */
    public function getAST()
    {
        return $this->ast;
    }

    /**
     * Get the formula metadata.
     *
     * @return Formula_Compiler_FormulaMetadata
     */
    public function getMetadata()
    {
        return $this->metadata;
    }

    /**
     * Get the source checksum.
     *
     * @return string
     */
    public function getSourceChecksum()
    {
        return $this->sourceChecksum;
    }

    /**
     * Get the compile time in milliseconds.
     *
     * @return float
     */
    public function getCompileTimeMs()
    {
        return $this->compileTimeMs;
    }

    /**
     * Get compilation warnings.
     *
     * @return string[]
     */
    public function getWarnings()
    {
        return $this->warnings;
    }

    /**
     * Check whether this compiled formula references a specific variable.
     *
     * @param string $variableName Variable name (case-insensitive)
     * @return bool
     */
    public function referencesVariable($variableName)
    {
        return in_array(
            strtoupper((string)$variableName),
            $this->metadata->referencedVariables,
            true
        );
    }

    /**
     * Check whether this compiled formula calls a specific function.
     *
     * @param string $functionName Function name (case-insensitive)
     * @return bool
     */
    public function referencesFunction($functionName)
    {
        return in_array(
            strtoupper((string)$functionName),
            $this->metadata->referencedFunctions,
            true
        );
    }

    /**
     * Check whether this formula is safe to cache.
     *
     * A formula is cacheable if it contains no volatile functions
     * (like NOW(), RAND()) and all referenced functions are deterministic.
     *
     * @return bool
     */
    public function isCacheable()
    {
        return $this->metadata->isCacheable;
    }
}
