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
 * DesignerEditorRenderer — token renderer for the Phase 2 visual canvas.
 *
 * @package FormulaDesigner\Renderer
 * @since   2.0.0
 */
class FormulaDesigner_Renderer_DesignerEditorRenderer
{
    /** @var FormulaDesigner_Editor_DesignerEditor */
    private $editor;

    /** @var array */
    private $options;

    /**
     * @param FormulaDesigner_Editor_DesignerEditor $editor
     * @param array                                 $options
     */
    public function __construct(FormulaDesigner_Editor_DesignerEditor $editor, array $options = array())
    {
        $this->editor = $editor;
        $this->options = $options;
    }

    /**
     * Render all tokens in order.
     *
     * @return string
     */
    public function render()
    {
        $parts = array();
        $tokens = $this->editor->getTokens();

        $parts[] = $this->renderConnector(0);

        foreach ($tokens as $index => $token) {
            $parts[] = $this->renderToken($token, $index);
            $parts[] = $this->renderConnector($index + 1);
        }

        return implode('', $parts);
    }

    /**
     * Render the canonical hidden textarea.
     *
     * @param string $name
     * @param string $value
     * @param string $id
     * @return string
     */
    public function renderSourceTextarea($name, $value, $id)
    {
        return '<textarea class="fd-source" data-designer="source" name="'
            . $this->escape($name) . '" id="' . $this->escape($id)
            . '" spellcheck="false" aria-hidden="true">'
            . $this->escape($value) . '</textarea>';
    }

    /**
     * Render the collapsible validation error panel shell.
     *
     * @return string
     */
    public function renderErrorPanel()
    {
        $parts = array();
        $parts[] = '<div class="fd-error-panel" data-designer="error-panel" hidden="hidden">';
        $parts[] = '<div class="fd-error-panel-header">';
        $parts[] = '<span class="fd-error-panel-title">Validation Errors (<span data-role="validation-count">0</span>)</span>';
        $parts[] = '<button type="button" class="fd-error-panel-close" data-action="dismiss-errors" aria-label="Dismiss validation errors">x</button>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-error-panel-body">';
        $parts[] = '<ul class="fd-error-list"></ul>';
        $parts[] = '</div>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render one connector drop zone.
     *
     * @param int $position
     * @return string
     */
    private function renderConnector($position)
    {
        return '<span class="fd-connector" data-connector-position="' . (int)$position
            . '" data-designer="connector" aria-hidden="true"></span>';
    }

    /**
     * Render a single visual token.
     *
     * @param array $token
     * @param int   $index
     * @return string
     */
    private function renderToken(array $token, $index)
    {
        $type = isset($token['type']) ? (string)$token['type'] : 'literal';
        $value = isset($token['value']) ? (string)$token['value'] : '';
        $label = isset($token['label']) ? (string)$token['label'] : $value;
        $metadata = isset($token['metadata']) && is_array($token['metadata']) ? $token['metadata'] : array();

        $attributes = array(
            'class="fd-token fd-token--' . $this->escape($type) . ' fd-token-' . $this->escape($type) . '"',
            'draggable="true"',
            'data-token-id="' . $this->escape(isset($token['id']) ? $token['id'] : 'tok-' . $index) . '"',
            'data-token-index="' . (int)$index . '"',
            'data-token-type="' . $this->escape($type) . '"',
            'data-token-value="' . $this->escape($value) . '"',
            'tabindex="-1"',
            'role="button"',
            'aria-label="' . $this->escape($this->buildAriaLabel($type, $label, $value)) . '"',
        );

        return '<span ' . implode(' ', $attributes) . '>' . $this->renderTokenInner($type, $label, $value, $metadata) . '</span>';
    }

    /**
     * Render the type-specific inner markup.
     *
     * @param string $type
     * @param string $label
     * @param string $value
     * @param array  $metadata
     * @return string
     */
    private function renderTokenInner($type, $label, $value, array $metadata)
    {
        if ($type === 'variable') {
            $namespace = isset($metadata['namespace']) ? (string)$metadata['namespace'] : '';

            return '<span class="fd-token-badge fd-token-badge--namespace fd-token-badge-namespace">'
                . $this->escape($this->namespaceAbbreviation($namespace))
                . '</span>'
                . '<span class="fd-token-label">' . $this->escape($label) . '</span>';
        }

        if ($type === 'function') {
            return '<span class="fd-token-prefix">fx</span>'
                . '<span class="fd-token-label">' . $this->escape($label) . '</span>'
                . '<span class="fd-token-suffix">(</span>';
        }

        if ($type === 'operator') {
            return '<span class="fd-token-symbol">' . $this->escape($label !== '' ? $label : $value) . '</span>';
        }

        if ($type === 'group') {
            return '<span class="fd-token-symbol">' . $this->escape($value !== '' ? $value : ')') . '</span>';
        }

        return '<span class="fd-token-value">' . $this->escape($label !== '' ? $label : $value) . '</span>';
    }

    /**
     * Build the token aria-label.
     *
     * @param string $type
     * @param string $label
     * @param string $value
     * @return string
     */
    private function buildAriaLabel($type, $label, $value)
    {
        $display = $label !== '' ? $label : $value;

        switch ($type) {
            case 'variable':
                return 'Variable: ' . $display;

            case 'function':
                return 'Function: ' . $display;

            case 'operator':
                return 'Operator: ' . $display;

            case 'group':
                return 'Group: ' . $display;

            default:
                return 'Literal: ' . $display;
        }
    }

    /**
     * Convert a namespace into a short badge label.
     *
     * @param string $namespace
     * @return string
     */
    private function namespaceAbbreviation($namespace)
    {
        $namespace = preg_replace('/[^A-Za-z]/', '', (string)$namespace);
        if ($namespace === '') {
            return 'Var';
        }

        return substr($namespace, 0, 3);
    }

    /**
     * Escape plain text for HTML output.
     *
     * @param string $value
     * @return string
     */
    private function escape($value)
    {
        return htmlspecialchars((string)$value, ENT_QUOTES, 'UTF-8');
    }
}