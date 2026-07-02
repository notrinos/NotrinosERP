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
 * DesignerEditor — in-memory editor state for the Phase 2 canvas.
 *
 * The editor stores the visual expression model and a dirty flag while the
 * canonical textarea synchronization remains deferred to Phase 4.
 *
 * @package FormulaDesigner\Editor
 * @since   2.0.0
 */
class FormulaDesigner_Editor_DesignerEditor
{
    /** @var array */
    private $expression = array();

    /** @var bool */
    private $dirty = false;

    /**
     * @param array $expression
     */
    public function __construct(array $expression = array())
    {
        if (empty($expression)) {
            $expression = self::createPhaseTwoTestExpression();
        }

        $this->expression = $this->normalizeExpression($expression);
    }

    /**
     * Get the normalized expression payload.
     *
     * @return array
     */
    public function getExpression()
    {
        return $this->expression;
    }

    /**
     * Replace the active expression.
     *
     * @param array $expression
     * @return void
     */
    public function setExpression(array $expression)
    {
        $this->expression = $this->normalizeExpression($expression);
        $this->dirty = true;
    }

    /**
     * Get the ordered token list.
     *
     * @return array
     */
    public function getTokens()
    {
        return isset($this->expression['tokens']) ? $this->expression['tokens'] : array();
    }

    /**
     * Replace the token list while keeping the current expression id.
     *
     * @param array $tokens
     * @return void
     */
    public function setTokens(array $tokens)
    {
        $expression = $this->expression;
        $expression['tokens'] = $tokens;

        $this->setExpression($expression);
    }

    /**
     * Mark the editor as dirty.
     *
     * @return void
     */
    public function markDirty()
    {
        $this->dirty = true;
    }

    /**
     * Mark the editor as clean.
     *
     * @return void
     */
    public function markClean()
    {
        $this->dirty = false;
    }

    /**
     * Check whether the expression changed since last clean mark.
     *
     * @return bool
     */
    public function isDirty()
    {
        return $this->dirty;
    }

    /**
     * Convert the editor state into a serializable array.
     *
     * @return array
     */
    public function toArray()
    {
        return array(
            'expression' => $this->expression,
            'dirty' => $this->dirty,
        );
    }

    /**
     * Create the roadmap-defined Phase 2 verification expression.
     *
     * @return array
     */
    public static function createPhaseTwoTestExpression()
    {
        return array(
            'id' => 'expr-root',
            'type' => 'expression',
            'tokens' => array(
                array(
                    'id' => 't1',
                    'type' => 'variable',
                    'value' => 'Employee.BasicSalary',
                    'label' => 'Basic Salary',
                    'metadata' => array(
                        'namespace' => 'Employee',
                        'name' => 'BasicSalary',
                        'dataType' => 'number',
                    ),
                ),
                array(
                    'id' => 't2',
                    'type' => 'operator',
                    'value' => '*',
                    'label' => '×',
                ),
                array(
                    'id' => 't3',
                    'type' => 'literal',
                    'value' => '0.12',
                    'label' => '0.12',
                    'metadata' => array(
                        'dataType' => 'number',
                        'rawValue' => 0.12,
                    ),
                ),
                array(
                    'id' => 't4',
                    'type' => 'operator',
                    'value' => '+',
                    'label' => '+',
                ),
                array(
                    'id' => 't5',
                    'type' => 'function',
                    'value' => 'ROUND(',
                    'label' => 'ROUND',
                    'metadata' => array(
                        'name' => 'ROUND',
                        'minArgs' => 1,
                        'maxArgs' => 2,
                    ),
                ),
                array(
                    'id' => 't6',
                    'type' => 'variable',
                    'value' => 'Employee.Overtime',
                    'label' => 'Overtime',
                    'metadata' => array(
                        'namespace' => 'Employee',
                        'name' => 'Overtime',
                        'dataType' => 'number',
                    ),
                ),
                array(
                    'id' => 't7',
                    'type' => 'literal',
                    'value' => '2',
                    'label' => '2',
                    'metadata' => array(
                        'dataType' => 'number',
                        'rawValue' => 2,
                    ),
                ),
                array(
                    'id' => 't8',
                    'type' => 'group',
                    'value' => ')',
                    'label' => '',
                ),
            ),
        );
    }

    /**
     * Normalize the expression payload for predictable rendering.
     *
     * @param array $expression
     * @return array
     */
    private function normalizeExpression(array $expression)
    {
        $normalized = array(
            'id' => isset($expression['id']) ? (string)$expression['id'] : 'expr-root',
            'type' => isset($expression['type']) ? (string)$expression['type'] : 'expression',
            'tokens' => array(),
        );

        $tokens = isset($expression['tokens']) && is_array($expression['tokens'])
            ? $expression['tokens']
            : array();

        foreach ($tokens as $index => $token) {
            if (!is_array($token)) {
                continue;
            }

            $normalized['tokens'][] = $this->normalizeToken($token, $index);
        }

        return $normalized;
    }

    /**
     * Normalize one token entry.
     *
     * @param array $token
     * @param int   $index
     * @return array
     */
    private function normalizeToken(array $token, $index)
    {
        $metadata = isset($token['metadata']) && is_array($token['metadata'])
            ? $token['metadata']
            : array();

        return array(
            'id' => isset($token['id']) ? (string)$token['id'] : 'tok-' . (int)$index,
            'type' => isset($token['type']) ? (string)$token['type'] : 'literal',
            'value' => isset($token['value']) ? (string)$token['value'] : '',
            'label' => isset($token['label']) ? (string)$token['label'] : '',
            'metadata' => $metadata,
        );
    }
}