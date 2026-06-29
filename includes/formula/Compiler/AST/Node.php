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

    /** @var Formula_Compiler_AST_NodeMetadata|null Lazily computed per-node metadata */
    protected $nodeMetadata = null;

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

    // -----------------------------------------------------------------------
    //  Abstract contract
    // -----------------------------------------------------------------------

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
     * Get the node type identifier from NodeType constants.
     *
     * This short string (e.g., 'literal', 'binary', 'variable') is used
     * as the type discriminator in serialized output. It is always one
     * of the Formula_Compiler_AST_NodeType constants.
     *
     * @return string
     */
    abstract public function getNodeType();

    // -----------------------------------------------------------------------
    //  Source location
    // -----------------------------------------------------------------------

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

    // -----------------------------------------------------------------------
    //  Metadata
    // -----------------------------------------------------------------------

    /**
     * Get the per-node compile-time metadata.
     *
     * Returns lazily-initialized metadata. Subclasses override
     * computeMetadata() to supply type-specific metadata.
     *
     * @return Formula_Compiler_AST_NodeMetadata
     */
    public function getMetadata()
    {
        if ($this->nodeMetadata === null) {
            $this->nodeMetadata = $this->computeMetadata();
        }
        return $this->nodeMetadata;
    }

    /**
     * Compute type-specific metadata for this node.
     *
     * Subclasses override this to provide accurate type inference,
     * complexity scoring, and constant-foldability information.
     *
     * @return Formula_Compiler_AST_NodeMetadata
     */
    protected function computeMetadata()
    {
        return Formula_Compiler_AST_NodeMetadata::leaf('unknown', 1, false, true);
    }

    // -----------------------------------------------------------------------
    //  Serialization
    // -----------------------------------------------------------------------

    /**
     * Serialize the node to an array for cache storage.
     *
     * The serialized form uses short NodeType constants (e.g., 'literal',
     * 'binary') as the type discriminator — NOT fully qualified class names.
     * This makes the serialized output compact, language-agnostic, and
     * immune to class renaming.
     *
     * Subclasses MUST override this to include their specific data.
     * The base implementation returns structural metadata.
     *
     * @return array
     */
    public function serialize()
    {
        return array(
            'type'          => $this->getNodeType(),
            'source_line'   => $this->sourceLine,
            'source_column' => $this->sourceColumn,
        );
    }

    // -----------------------------------------------------------------------
    //  Helpers
    // -----------------------------------------------------------------------

    /**
     * Get a human-readable representation for debugging.
     *
     * @return string
     */
    public function __toString()
    {
        return get_class($this) . ' @ ' . $this->sourceLine . ':' . $this->sourceColumn;
    }

    /**
     * Check whether this node is a leaf (has no children).
     *
     * @return bool
     */
    public function isLeaf()
    {
        return count($this->getChildren()) === 0;
    }

    /**
     * Check whether this node type can be constant-folded.
     *
     * @return bool
     */
    public function isConstant()
    {
        return $this->getMetadata()->isConstant;
    }

    /**
     * Get the number of nodes in the subtree rooted at this node.
     *
     * @return int
     */
    public function getSubtreeNodeCount()
    {
        return $this->getMetadata()->subtreeNodeCount;
    }

    /**
     * Get the maximum depth of the subtree rooted at this node.
     *
     * @return int
     */
    public function getSubtreeDepth()
    {
        return $this->getMetadata()->subtreeDepth;
    }
}
