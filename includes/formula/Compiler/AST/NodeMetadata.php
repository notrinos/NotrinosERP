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
 * NodeMetadata — Per-node compile-time metadata.
 *
 * Every AST node carries metadata computed during the compilation phase
 * (specifically, during the TypeCheckVisitor and MetadataVisitor passes).
 * This metadata enables:
 *   - Type inference (what type will this node evaluate to?)
 *   - Complexity estimation (how expensive is evaluation?)
 *   - Cacheability decisions (can this subtree be memoized?)
 *   - Constant-foldability (can this subtree be pre-computed?)
 *
 * Metadata is IMMUTABLE after computation. It is serialized with the AST
 * for cache storage and deserialized back.
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
class Formula_Compiler_AST_NodeMetadata
{
    /** @var string Inferred return type: 'integer', 'decimal', 'boolean', 'string', 'date', 'null', 'mixed', 'unknown' */
    public $returnType;

    /** @var int Complexity contribution of this node (1-10 scale) */
    public $complexity;

    /** @var bool Whether this subtree can be constant-folded at compile time */
    public $isConstant;

    /** @var bool Whether this subtree is deterministic (same inputs → same output) */
    public $isDeterministic;

    /** @var string[] Variable names referenced in this subtree (namespace-qualified or simple) */
    public $referencedVariables;

    /** @var string[] Function names called in this subtree */
    public $referencedFunctions;

    /** @var string[] Namespace prefixes referenced in this subtree */
    public $referencedNamespaces;

    /** @var int Number of nodes in this subtree (including this node) */
    public $subtreeNodeCount;

    /** @var int Maximum depth of this subtree (1 = leaf) */
    public $subtreeDepth;

    /**
     * Construct node metadata.
     *
     * @param array $data Associative array of property values
     */
    public function __construct(array $data = array())
    {
        $this->returnType           = isset($data['returnType']) ? (string)$data['returnType'] : 'unknown';
        $this->complexity           = isset($data['complexity']) ? (int)$data['complexity'] : 0;
        $this->isConstant           = isset($data['isConstant']) ? (bool)$data['isConstant'] : false;
        $this->isDeterministic      = isset($data['isDeterministic']) ? (bool)$data['isDeterministic'] : true;
        $this->referencedVariables  = isset($data['referencedVariables']) ? (array)$data['referencedVariables'] : array();
        $this->referencedFunctions  = isset($data['referencedFunctions']) ? (array)$data['referencedFunctions'] : array();
        $this->referencedNamespaces = isset($data['referencedNamespaces']) ? (array)$data['referencedNamespaces'] : array();
        $this->subtreeNodeCount     = isset($data['subtreeNodeCount']) ? (int)$data['subtreeNodeCount'] : 1;
        $this->subtreeDepth         = isset($data['subtreeDepth']) ? (int)$data['subtreeDepth'] : 1;
    }

    /**
     * Create metadata for a leaf node (no children).
     *
     * @param string $returnType      Inferred return type
     * @param int    $complexity      Complexity score (1-10)
     * @param bool   $isConstant      Whether this is a compile-time constant
     * @param bool   $isDeterministic Whether evaluation is deterministic
     * @return self
     */
    public static function leaf($returnType = 'unknown', $complexity = 1, $isConstant = false, $isDeterministic = true)
    {
        return new self(array(
            'returnType'       => $returnType,
            'complexity'       => $complexity,
            'isConstant'       => $isConstant,
            'isDeterministic'  => $isDeterministic,
            'subtreeNodeCount' => 1,
            'subtreeDepth'     => 1,
        ));
    }

    /**
     * Merge child metadata into this node's metadata.
     *
     * Called by composite nodes (binary, conditional, function, etc.)
     * to aggregate metadata from all children.
     *
     * @param Formula_Compiler_AST_NodeMetadata $childMetadata
     * @return void
     */
    public function mergeChild(Formula_Compiler_AST_NodeMetadata $childMetadata)
    {
        // Variables: union all referenced variables
        $this->referencedVariables = array_unique(
            array_merge($this->referencedVariables, $childMetadata->referencedVariables)
        );

        // Functions: union all referenced functions
        $this->referencedFunctions = array_unique(
            array_merge($this->referencedFunctions, $childMetadata->referencedFunctions)
        );

        // Namespaces: union all referenced namespaces
        $this->referencedNamespaces = array_unique(
            array_merge($this->referencedNamespaces, $childMetadata->referencedNamespaces)
        );

        // Complexity: sum all contributions
        $this->complexity += $childMetadata->complexity;

        // Subtree node count: accumulate
        $this->subtreeNodeCount += $childMetadata->subtreeNodeCount;

        // Subtree depth: max of children + 1
        $this->subtreeDepth = max($this->subtreeDepth, $childMetadata->subtreeDepth + 1);

        // isConstant: false if any child is not constant
        if (!$childMetadata->isConstant) {
            $this->isConstant = false;
        }

        // isDeterministic: false if any child is not deterministic
        if (!$childMetadata->isDeterministic) {
            $this->isDeterministic = false;
        }
    }

    /**
     * Set the return type for this node.
     *
     * @param string $type
     * @return void
     */
    public function setReturnType($type)
    {
        $this->returnType = (string)$type;
    }

    /**
     * Convert metadata to array for serialization.
     *
     * @return array
     */
    public function toArray()
    {
        return array(
            'returnType'           => $this->returnType,
            'complexity'           => $this->complexity,
            'isConstant'           => $this->isConstant,
            'isDeterministic'      => $this->isDeterministic,
            'referencedVariables'  => array_values($this->referencedVariables),
            'referencedFunctions'  => array_values($this->referencedFunctions),
            'referencedNamespaces' => array_values($this->referencedNamespaces),
            'subtreeNodeCount'     => $this->subtreeNodeCount,
            'subtreeDepth'         => $this->subtreeDepth,
        );
    }

    /**
     * Create metadata from a serialized array.
     *
     * @param array $data
     * @return self
     */
    public static function fromArray(array $data)
    {
        return new self($data);
    }

    /**
     * Compute the estimated formula-level complexity score (1-100).
     *
     * Formula-level complexity is based on:
     *   - Total node count (weighted)
     *   - Maximum nesting depth (weighted)
     *   - Number of function calls (weighted)
     *   - Number of variable references (weighted)
     *
     * @return float Score from 0.0 (empty) to 100.0 (extremely complex)
     */
    public function computeFormulaComplexity()
    {
        // Base: node count contributes up to 40 points
        $nodeScore = min($this->subtreeNodeCount * 2.5, 40.0);

        // Depth: contributes up to 30 points
        $depthScore = min($this->subtreeDepth * 3.0, 30.0);

        // Function calls: contributes up to 20 points
        $fnScore = min(count($this->referencedFunctions) * 5.0, 20.0);

        // Variable references: contributes up to 10 points
        $varScore = min(count($this->referencedVariables) * 2.0, 10.0);

        return round($nodeScore + $depthScore + $fnScore + $varScore, 1);
    }
}
