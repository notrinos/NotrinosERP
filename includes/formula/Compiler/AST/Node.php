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
 * Node — Abstract base class for all AST (Abstract Syntax Tree) nodes.
 *
 * Every formula, after parsing, is represented as an immutable tree of
 * Node subclasses. The AST is the canonical internal representation of
 * a formula — it can be validated, optimized, serialized, cached, and
 * executed repeatedly.
 *
 * Node properties:
 * - Immutable after construction (optimization produces new nodes)
 * - Traversable via the Visitor pattern (accept())
 * - Serializable for cache storage (serialize())
 * - Source-mapped for error reporting (getSourceLocation())
 * - Metadata-carrying for diagnostics (getMetadata())
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
abstract class Formula_Compiler_AST_Node
{
    /** @var int Source line number (1-based) */
    protected $sourceLine;

    /** @var int Source column number (1-based) */
    protected $sourceColumn;

    /**
     * Construct a node with source location.
     *
     * @param int $line   Source line (1-based, 0 if N/A)
     * @param int $column Source column (1-based, 0 if N/A)
     */
    public function __construct($line = 0, $column = 0)
    {
        $this->sourceLine   = (int)$line;
        $this->sourceColumn = (int)$column;
    }

    /**
     * Accept a visitor for traversal (Visitor Pattern entry point).
     *
     * Each concrete node type overrides this to call the appropriate
     * visit* method on the visitor. This enables all AST operations
     * (validation, optimization, evaluation, serialization, explanation)
     * to be implemented as visitors.
     *
     * @param Formula_Compiler_AST_NodeVisitor $visitor
     * @return mixed
     */
    abstract public function accept(Formula_Compiler_AST_NodeVisitor $visitor);

    /**
     * Get all direct child nodes.
     *
     * Used by visitors that need generic tree traversal.
     *
     * @return Formula_Compiler_AST_Node[]
     */
    abstract public function getChildren();

    /**
     * Get the source line number.
     *
     * @return int
     */
    public function getSourceLine()
    {
        return $this->sourceLine;
    }

    /**
     * Get the source column number.
     *
     * @return int
     */
    public function getSourceColumn()
    {
        return $this->sourceColumn;
    }

    /**
     * Serialize the node to an array for cache storage.
     *
     * Subclasses MUST override this to include their specific data.
     * The base implementation returns structural metadata.
     *
     * @return array
     */
    public function serialize()
    {
        return array(
            'type'         => get_class($this),
            'source_line'  => $this->sourceLine,
            'source_column'=> $this->sourceColumn,
        );
    }

    /**
     * Get a human-readable representation for debugging.
     *
     * @return string
     */
    public function __toString()
    {
        return get_class($this);
    }
}
