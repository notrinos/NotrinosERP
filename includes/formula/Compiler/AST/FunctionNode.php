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
 * FunctionNode — AST node for function calls.
 *
 * Represents a function invocation with evaluated arguments.
 * The function is resolved through the FunctionRegistry at
 * compile time and invoked at runtime.
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
class Formula_Compiler_AST_FunctionNode extends Formula_Compiler_AST_Node
{
    /** @var string Function name (case-insensitive, uppercase) */
    public $functionName;

    /** @var Formula_Compiler_AST_Node[] Argument expressions */
    public $arguments;

    /**
     * @param string                        $functionName
     * @param Formula_Compiler_AST_Node[]    $arguments
     * @param int                           $line
     * @param int                           $column
     */
    public function __construct($functionName, array $arguments = array(), $line = 0, $column = 0)
    {
        parent::__construct($line, $column);
        $this->functionName = (string)$functionName;
        $this->arguments    = $arguments;
    }

    /** @return string */
    public function getNodeType()
    {
        return Formula_Compiler_AST_NodeType::FUNCTION;
    }

    /**
     * @param Formula_Compiler_AST_NodeVisitor $visitor
     * @return mixed
     */
    public function accept(Formula_Compiler_AST_NodeVisitor $visitor)
    {
        return $visitor->visitFunction($this);
    }

    /**
     * @return Formula_Compiler_AST_Node[]
     */
    public function getChildren()
    {
        return $this->arguments;
    }

    /**
     * Serialize for cache storage.
     *
     * @return array
     */
    public function serialize()
    {
        $data = parent::serialize();
        $data['functionName'] = $this->functionName;
        $data['arguments'] = array();
        foreach ($this->arguments as $arg) {
            $data['arguments'][] = $arg->serialize();
        }
        return $data;
    }

    /**
     * Compute per-node metadata: functions reference themselves, merge child metadata.
     *
     * @return Formula_Compiler_AST_NodeMetadata
     */
    protected function computeMetadata()
    {
        $metadata = Formula_Compiler_AST_NodeMetadata::leaf('mixed', 5, false, false);
        $metadata->referencedFunctions = array($this->functionName);
        foreach ($this->arguments as $arg) {
            $metadata->mergeChild($arg->getMetadata());
        }
        return $metadata;
    }
}
