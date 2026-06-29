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
 * DependencyValidator — Circular reference detection for formula variables.
 *
 * Detects circular dependencies in variable references within the AST.
 * A circular dependency occurs when variable A depends on variable B,
 * which depends on variable C, which depends back on variable A.
 *
 * Example of circular reference:
 *   Formula for A: B + C
 *   Formula for B: D + E
 *   Formula for C: A * 2    ← C depends on A, which depends on C indirectly
 *
 * The validator builds a directed graph of variable→dependency edges,
 * then runs depth-first search to detect cycles. When a cycle is found,
 * the full cycle path is reported for debugging.
 *
 * The validator needs external knowledge of which variables depend on
 * which other variables. This is supplied via the resolved dependency
 * map — a mapping from variable qualified names to the variables they
 * reference.
 *
 * Implements ValidatorInterface. Does NOT implement NodeVisitor
 * directly — instead uses internal graph-building logic.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 */
class Formula_Compiler_DependencyValidator implements Formula_Contracts_ValidatorInterface
{
    /** @var array<string, string[]> Variable name → list of variable names it depends on */
    private $dependencyMap;

    /** @var Formula_Compiler_ValidationResult Accumulated result */
    private $result;

    /** @var string[] Variables currently in the DFS recursion stack (for cycle detection) */
    private $stack;

    /** @var array<string, bool> Variables that have been fully processed */
    private $visited;

    /**
     * Construct a dependency validator.
     *
     * @param array<string, string[]> $dependencyMap Variable → array of variables it references.
     *                                                Keys and values are fully qualified variable
     *                                                names (e.g., "Employee.BasicSalary" or "SALARY").
     */
    public function __construct(array $dependencyMap = array())
    {
        $this->dependencyMap = $dependencyMap;
    }

    // -----------------------------------------------------------------------
    //  ValidatorInterface
    // -----------------------------------------------------------------------

    /**
     * Validate the AST for circular variable dependencies.
     *
     * Builds a dependency graph from all VariableNode instances in the AST,
     * then runs DFS cycle detection.
     *
     * @param Formula_Compiler_AST_Node $ast Root node of the AST
     * @return Formula_Compiler_ValidationResult
     */
    public function validate(Formula_Compiler_AST_Node $ast)
    {
        $this->result  = new Formula_Compiler_ValidationResult();
        $this->stack   = array();
        $this->visited = array();

        if ($ast === null) {
            return $this->result;
        }

        // Step 1: Collect all variables referenced in this AST.
        $rootMetadata = $ast->getMetadata();
        $allVariables = $this->getAllVariables($rootMetadata);

        // Step 2: Build graph from dependency map.
        // Only include edges where both ends are in the current formula's scope.
        $graph = array();
        foreach ($allVariables as $var) {
            $graph[$var] = array();
            if (isset($this->dependencyMap[$var])) {
                foreach ($this->dependencyMap[$var] as $dep) {
                    if (in_array($dep, $allVariables, true)) {
                        $graph[$var][] = $dep;
                    }
                }
            }
        }

        // Step 3: DFS cycle detection for each variable.
        foreach ($graph as $variable => $dependencies) {
            if (!isset($this->visited[$variable])) {
                $this->dfs($variable, $graph, array());
            }
        }

        return $this->result;
    }

    // -----------------------------------------------------------------------
    //  Setter for external dependency map
    // -----------------------------------------------------------------------

    /**
     * Set the dependency map used for cycle detection.
     *
     * Called before validate() when the dependency map is built
     * externally (e.g., by the FormulaCompiler orchestrator).
     *
     * @param array<string, string[]> $dependencyMap
     * @return void
     */
    public function setDependencyMap(array $dependencyMap)
    {
        $this->dependencyMap = $dependencyMap;
    }

    // -----------------------------------------------------------------------
    //  DFS cycle detection
    // -----------------------------------------------------------------------

    /**
     * Depth-first search for cycle detection.
     *
     * Tarjan-style DFS: track the recursion stack to detect back-edges.
     * When a node is encountered that is already on the stack, a cycle
     * has been found.
     *
     * @param string   $variable     The current variable being visited
     * @param array    $graph        Adjacency list of variable → dependencies
     * @param string[] $currentPath  The path taken to reach this variable
     * @return void
     */
    private function dfs($variable, array $graph, array $currentPath)
    {
        // If already fully processed, skip.
        if (isset($this->visited[$variable])) {
            return;
        }

        // If already on the current recursion stack, we found a cycle.
        if (isset($this->stack[$variable])) {
            // Extract the cycle: find where $variable appears in $currentPath,
            // then build the cycle from that point.
            $cycleStart = array_search($variable, $currentPath, true);
            if ($cycleStart !== false) {
                $cycle = array_slice($currentPath, $cycleStart);
                $cycle[] = $variable; // Close the loop
                $this->result->addError(
                    sprintf(
                        'Circular variable dependency detected: %s.',
                        implode(' → ', $cycle)
                    )
                );
            }
            return;
        }

        // Mark as being processed.
        $this->stack[$variable] = true;
        $currentPath[] = $variable;

        // Visit all dependencies.
        if (isset($graph[$variable])) {
            foreach ($graph[$variable] as $dependency) {
                $this->dfs($dependency, $graph, $currentPath);
            }
        }

        // Mark as fully processed.
        unset($this->stack[$variable]);
        $this->visited[$variable] = true;
    }

    // -----------------------------------------------------------------------
    //  Helpers
    // -----------------------------------------------------------------------

    /**
     * Extract all unique variable names from node metadata.
     *
     * Recursively collects referencedVariables from the metadata tree.
     *
     * @param Formula_Compiler_AST_NodeMetadata $metadata
     * @return string[]
     */
    private function getAllVariables(Formula_Compiler_AST_NodeMetadata $metadata)
    {
        return array_unique($metadata->referencedVariables);
    }
}
