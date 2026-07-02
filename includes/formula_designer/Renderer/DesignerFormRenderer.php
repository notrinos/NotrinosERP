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
 * DesignerFormRenderer — server-rendered no-JS formula form builder.
 *
 * This is the progressive enhancement base layer defined.
 * When JavaScript is enabled, the Visual Formula Designer 
 * enhances this base layer. When JavaScript is disabled, this form-based
 * builder provides an accessible, functional formula editor that posts
 * valid NFX formula text through standard form submission.
 *
 * @package FormulaDesigner\Renderer
 * @since   2.0.0
 */
class FormulaDesigner_Renderer_DesignerFormRenderer
{
    /** @var string */
    private $formula;

    /** @var string */
    private $module;

    /** @var array */
    private $options;

    /** @var array */
    private $fieldSections;

    /** @var array */
    private $functionSections;

    /**
     * @param string $formula  Current NFX formula text.
     * @param string $module   Module identifier (e.g. 'hrm').
     * @param array  $options  Configuration options.
     */
    public function __construct($formula = '', $module = '', array $options = array())
    {
        $this->formula = (string)$formula;
        $this->module = (string)$module;
        $this->options = $options;
        $this->fieldSections = array();
        $this->functionSections = array();
    }

    /**
     * Set the field palette sections for the no-JS builder.
     *
     * @param array $sections
     * @return void
     */
    public function setFieldSections(array $sections)
    {
        $this->fieldSections = $sections;
    }

    /**
     * Set the function palette sections for the no-JS builder.
     *
     * @param array $sections
     * @return void
     */
    public function setFunctionSections(array $sections)
    {
        $this->functionSections = $sections;
    }

    /**
     * Render the complete no-JS formula builder.
     *
     * @return string
     */
    public function render()
    {
        $textarea_name = isset($this->options['textareaName']) && $this->options['textareaName'] !== ''
            ? (string)$this->options['textareaName']
            : 'formula_designer_formula';
        $textarea_id = isset($this->options['textareaId']) && $this->options['textareaId'] !== ''
            ? (string)$this->options['textareaId']
            : 'fd-form-source';

        $tokens = $this->parseFormulaToTokens($this->formula);

        $parts = array();
        $parts[] = '<div class="fd-form-builder" data-designer="form-builder">';

        // Header
        $parts[] = '<div class="fd-form-header">';
        $parts[] = '<h3 class="fd-form-title">' . _('Formula Builder') . '</h3>';
        $parts[] = '<p class="fd-form-subtitle">' . _('Build your formula without JavaScript.') . '</p>';
        $parts[] = '</div>';

        // Expression area — current tokens
        $parts[] = $this->renderExpressionArea($tokens);

        // Add token section
        $parts[] = $this->renderAddSection();

        // Preview notice (no-JS)
        $parts[] = $this->renderPreviewNotice();

        // Action buttons
        $parts[] = $this->renderActions();

        // Hidden textarea — canonical source of truth
        $parts[] = '<textarea name="' . $this->escapeAttr($textarea_name) . '"'
            . ' id="' . $this->escapeAttr($textarea_id) . '"'
            . ' class="fd-source"'
            . ' data-designer="source-textarea"'
            . ' aria-hidden="true">'
            . $this->escape($this->formula)
            . '</textarea>';

        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render the expression area showing current tokens.
     *
     * @param array $tokens
     * @return string
     */
    private function renderExpressionArea(array $tokens)
    {
        $parts = array();
        $parts[] = '<div class="fd-form-expression" data-designer="expression-area">';
        $parts[] = '<div class="fd-form-expression-header">';
        $parts[] = '<span class="fd-form-expression-label">' . _('Expression:') . '</span>';
        $parts[] = '</div>';

        $parts[] = '<div class="fd-form-token-list">';

        if (empty($tokens)) {
            $parts[] = '<div class="fd-form-empty">';
            $parts[] = _('No tokens yet. Add a token below to start building your formula.');
            $parts[] = '</div>';
        } else {
            foreach ($tokens as $index => $token) {
                $parts[] = $this->renderTokenRow($token, $index);
            }
        }

        // Add token trigger — submits form to add a new empty row
        $parts[] = '<div class="fd-form-add-row">';
        $parts[] = '<button type="submit" name="fd_add_token" value="1" class="fd-form-btn fd-form-btn--add">'
            . _('+ Add Token') . '</button>';
        $parts[] = '</div>';

        $parts[] = '</div>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render a single token row in the expression area.
     *
     * @param array $token
     * @param int   $index
     * @return string
     */
    private function renderTokenRow(array $token, $index)
    {
        $type = isset($token['type']) ? $token['type'] : 'literal';
        $value = isset($token['value']) ? $token['value'] : '';

        $parts = array();
        $parts[] = '<div class="fd-form-token-row" data-token-index="' . (int)$index . '">';

        // Type badge
        $parts[] = '<span class="fd-form-token-badge fd-form-token-badge--' . $this->escapeAttr($type) . '">';
        $parts[] = $this->escape($this->getTokenTypeLabel($type));
        $parts[] = '</span>';

        // Token value display
        $parts[] = '<span class="fd-form-token-value">' . $this->escape($this->getTokenDisplayValue($type, $value)) . '</span>';

        // Hidden inputs for form POST
        $parts[] = '<input type="hidden" name="fd_token_type[' . (int)$index . ']" value="' . $this->escapeAttr($type) . '">';
        $parts[] = '<input type="hidden" name="fd_token_value[' . (int)$index . ']" value="' . $this->escapeAttr($value) . '">';

        // Edit token link
        $parts[] = '<div class="fd-form-token-actions">';
        $parts[] = '<button type="submit" name="fd_edit_token" value="' . (int)$index . '"'
            . ' class="fd-form-btn fd-form-btn--small"'
            . ' title="' . _('Edit this token') . '">'
            . _('Edit') . '</button>';

        // Remove token link
        $parts[] = '<button type="submit" name="fd_remove_token" value="' . (int)$index . '"'
            . ' class="fd-form-btn fd-form-btn--small fd-form-btn--danger"'
            . ' title="' . _('Remove this token') . '">'
            . _('Remove') . '</button>';
        $parts[] = '</div>';

        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render the add-token section with type selector and value input.
     *
     * @return string
     */
    private function renderAddSection()
    {
        $editing_token = $this->getEditingToken();

        $parts = array();
        $parts[] = '<div class="fd-form-add-section">';
        $parts[] = '<div class="fd-form-add-header">';
        $parts[] = '<span class="fd-form-add-label">' . ($editing_token !== null ? _('Edit Token:') : _('Add Token:')) . '</span>';
        $parts[] = '</div>';

        $parts[] = '<div class="fd-form-add-controls">';

        // Token type selector
        $parts[] = '<div class="fd-form-add-field">';
        $parts[] = '<label class="fd-form-add-field-label">' . _('Type') . '</label>';
        $parts[] = '<select name="fd_new_token_type" class="fd-form-select">';
        $parts[] = '<option value="variable"' . $this->selected($editing_token, 'type', 'variable') . '>'
            . _('Variable') . '</option>';
        $parts[] = '<option value="operator"' . $this->selected($editing_token, 'type', 'operator') . '>'
            . _('Operator') . '</option>';
        $parts[] = '<option value="literal"' . $this->selected($editing_token, 'type', 'literal') . '>'
            . _('Literal') . '</option>';
        $parts[] = '<option value="function"' . $this->selected($editing_token, 'type', 'function') . '>'
            . _('Function') . '</option>';
        $parts[] = '<option value="group"' . $this->selected($editing_token, 'type', 'group') . '>'
            . _('Parenthesis') . '</option>';
        $parts[] = '</select>';
        $parts[] = '</div>';

        // Value input (contextual) — variable selector or text input
        $parts[] = '<div class="fd-form-add-field">';
        $parts[] = '<label class="fd-form-add-field-label">' . _('Value') . '</label>';
        $parts[] = $this->renderValueInput($editing_token);
        $parts[] = '</div>';

        // Add/Update button
        $parts[] = '<div class="fd-form-add-field fd-form-add-field--action">';
        $button_label = $editing_token !== null ? _('Update Token') : _('Add Token');
        $parts[] = '<button type="submit" name="fd_submit_token" value="1" class="fd-form-btn fd-form-btn--primary">'
            . $this->escape($button_label) . '</button>';

        if ($editing_token !== null) {
            $parts[] = '<button type="submit" name="fd_cancel_edit" value="1" class="fd-form-btn">'
                . _('Cancel') . '</button>';
        }
        $parts[] = '</div>';

        $parts[] = '</div>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render the value input field depending on token type context.
     *
     * @param array|null $editing_token
     * @return string
     */
    private function renderValueInput($editing_token)
    {
        $selected_type = $this->getSelectedTokenType($editing_token);
        $current_value = $editing_token !== null && isset($editing_token['value'])
            ? $editing_token['value']
            : '';

        switch ($selected_type) {
            case 'variable':
                return $this->renderVariableSelector($current_value);

            case 'operator':
                return $this->renderOperatorSelector($current_value);

            case 'function':
                return $this->renderFunctionSelector($current_value);

            case 'group':
                return $this->renderGroupSelector($current_value);

            case 'literal':
            default:
                return '<input type="text" name="fd_new_token_value"'
                    . ' class="fd-form-input"'
                    . ' value="' . $this->escapeAttr($current_value) . '"'
                    . ' placeholder="' . _('e.g. 0.12 or "text"') . '">';
        }
    }

    /**
     * Render a variable selector with available fields.
     *
     * @param string $current_value
     * @return string
     */
    private function renderVariableSelector($current_value)
    {
        $parts = array();
        $parts[] = '<select name="fd_new_token_value" class="fd-form-select">';
        $parts[] = '<option value="">-- ' . _('Select variable') . ' --</option>';

        foreach ($this->fieldSections as $section) {
            $namespace = isset($section['namespace']) ? $section['namespace'] : '';
            $items = isset($section['items']) ? $section['items'] : array();

            if (empty($items)) {
                continue;
            }

            $parts[] = '<optgroup label="' . $this->escapeAttr($namespace) . '">';
            foreach ($items as $item) {
                $qualified = isset($item['qualifiedName']) ? $item['qualifiedName'] : '';
                $label = isset($item['label']) ? $item['label'] : $qualified;
                $selected = ($qualified === $current_value) ? ' selected' : '';
                $parts[] = '<option value="' . $this->escapeAttr($qualified) . '"' . $selected . '>'
                    . $this->escape($label) . '</option>';
            }
            $parts[] = '</optgroup>';
        }

        $parts[] = '</select>';

        return implode('', $parts);
    }

    /**
     * Render an operator selector.
     *
     * @param string $current_value
     * @return string
     */
    private function renderOperatorSelector($current_value)
    {
        $operators = array(
            '+' => _('+ (Addition)'),
            '-' => _('- (Subtraction)'),
            '*' => _('* (Multiplication)'),
            '/' => _('/ (Division)'),
            '%' => _('% (Modulus)'),
            '^' => _('^ (Power)'),
            '(' => _('( (Opening Parenthesis)'),
            ')' => _(') (Closing Parenthesis)'),
            ',' => _(', (Argument Separator)'),
        );

        $parts = array();
        $parts[] = '<select name="fd_new_token_value" class="fd-form-select">';
        $parts[] = '<option value="">-- ' . _('Select operator') . ' --</option>';

        foreach ($operators as $sym => $label) {
            $selected = ($sym === $current_value) ? ' selected' : '';
            $parts[] = '<option value="' . $this->escapeAttr($sym) . '"' . $selected . '>'
                . $this->escape($label) . '</option>';
        }

        $parts[] = '</select>';

        return implode('', $parts);
    }

    /**
     * Render a function selector with available functions.
     *
     * @param string $current_value
     * @return string
     */
    private function renderFunctionSelector($current_value)
    {
        $parts = array();
        $parts[] = '<select name="fd_new_token_value" class="fd-form-select">';
        $parts[] = '<option value="">-- ' . _('Select function') . ' --</option>';

        foreach ($this->functionSections as $section) {
            $category = isset($section['category']) ? $section['category'] : '';
            $items = isset($section['items']) ? $section['items'] : array();

            if (empty($items)) {
                continue;
            }

            $parts[] = '<optgroup label="' . $this->escapeAttr($category) . '">';
            foreach ($items as $item) {
                $name = isset($item['name']) ? $item['name'] : '';
                $label = isset($item['label']) ? $item['label'] : $name;
                $signature = isset($item['signature']) ? $item['signature'] : '';
                $display = $name;
                if ($signature !== '') {
                    $display .= ' ' . $signature;
                }

                $selected = ($name === $current_value) ? ' selected' : '';
                $parts[] = '<option value="' . $this->escapeAttr($name) . '"' . $selected . '>'
                    . $this->escape($display) . '</option>';
            }
            $parts[] = '</optgroup>';
        }

        $parts[] = '</select>';

        return implode('', $parts);
    }

    /**
     * Render a group (parenthesis) selector.
     *
     * @param string $current_value
     * @return string
     */
    private function renderGroupSelector($current_value)
    {
        $selected_open = ($current_value === '(' || $current_value === '') ? ' selected' : '';
        $selected_close = ($current_value === ')') ? ' selected' : '';

        $parts = array();
        $parts[] = '<select name="fd_new_token_value" class="fd-form-select">';
        $parts[] = '<option value="("' . $selected_open . '>' . _('( (Opening)') . '</option>';
        $parts[] = '<option value=")"' . $selected_close . '>' . _(') (Closing)') . '</option>';
        $parts[] = '</select>';

        return implode('', $parts);
    }

    /**
     * Render the preview notice for no-JS mode.
     *
     * @return string
     */
    private function renderPreviewNotice()
    {
        $parts = array();
        $parts[] = '<div class="fd-form-preview-notice">';
        $parts[] = '<span class="fd-form-preview-icon">&#9888;</span>';
        $parts[] = '<span class="fd-form-preview-text">';
        $parts[] = _('Preview and live validation require JavaScript. Enable JavaScript for an enhanced formula editing experience with drag-and-drop, live preview, and step-by-step formula explanation.');
        $parts[] = '</span>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render action buttons (Validate, Reset).
     *
     * @return string
     */
    private function renderActions()
    {
        $parts = array();
        $parts[] = '<div class="fd-form-actions">';

        // Validate button — triggers server-side validation via form POST
        $parts[] = '<button type="submit" name="fd_validate" value="1" class="fd-form-btn fd-form-btn--validate">'
            . _('Validate Formula') . '</button>';

        // Reset button — clears the formula
        $parts[] = '<button type="submit" name="fd_reset" value="1" class="fd-form-btn fd-form-btn--reset">'
            . _('Reset') . '</button>';

        $parts[] = '</div>';

        return implode('', $parts);
    }

    // -----------------------------------------------------------------------
    // Token parsing / serialization
    // -----------------------------------------------------------------------

    /**
     * Parse an NFX formula string into a flat array of tokens.
     *
     * The tokenizer uses a simple regex-based approach that is deliberately
     * aligned with the designer token model. It handles:
     *   - Qualified variables:  Namespace.Identifier
     *   - Simple variables:     UPPER_CASE identifiers
     *   - Numbers:              123, 45.67
     *   - String literals:      "text"
     *   - Operators:            + - * / % ^
     *   - Parentheses:          ( )
     *   - Commas:               ,
     *   - Functions:            IF(..., ..., ...)
     *
     * @param string $formula
     * @return array
     */
    public function parseFormulaToTokens($formula)
    {
        $formula = trim((string)$formula);
        if ($formula === '') {
            return array();
        }

        $tokens = array();
        $length = strlen($formula);
        $pos = 0;

        while ($pos < $length) {
            $char = $formula[$pos];

            // Skip whitespace
            if ($char === ' ' || $char === "\t" || $char === "\n" || $char === "\r") {
                $pos++;
                continue;
            }

            // Parentheses
            if ($char === '(') {
                $tokens[] = array('type' => 'group', 'value' => '(');
                $pos++;
                continue;
            }
            if ($char === ')') {
                $tokens[] = array('type' => 'group', 'value' => ')');
                $pos++;
                continue;
            }

            // Comma
            if ($char === ',') {
                $tokens[] = array('type' => 'operator', 'value' => ',');
                $pos++;
                continue;
            }

            // Operators
            if (strpos('+-*/%^', $char) !== false) {
                $tokens[] = array('type' => 'operator', 'value' => $char);
                $pos++;
                continue;
            }

            // String literal
            if ($char === '"' || $char === "'") {
                $quote = $char;
                $pos++;
                $start = $pos;
                while ($pos < $length && $formula[$pos] !== $quote) {
                    if ($formula[$pos] === '\\' && $pos + 1 < $length) {
                        $pos++;
                    }
                    $pos++;
                }
                $str_val = substr($formula, $start, $pos - $start);
                $pos++; // skip closing quote

                $tokens[] = array('type' => 'literal', 'value' => $quote . $str_val . $quote);
                continue;
            }

            // Number literal
            if (ctype_digit($char) || ($char === '.' && $pos + 1 < $length && ctype_digit($formula[$pos + 1]))) {
                $start = $pos;
                while ($pos < $length && (ctype_digit($formula[$pos]) || $formula[$pos] === '.')) {
                    $pos++;
                }
                $num_val = substr($formula, $start, $pos - $start);

                $tokens[] = array('type' => 'literal', 'value' => $num_val);
                continue;
            }

            // Identifier: function, variable, or constant
            if (ctype_alpha($char) || $char === '_') {
                $start = $pos;
                while ($pos < $length && (ctype_alnum($formula[$pos]) || $formula[$pos] === '_' || $formula[$pos] === '.')) {
                    $pos++;
                }
                $identifier = substr($formula, $start, $pos - $start);

                // Check if followed by '('  => function call
                $next_pos = $pos;
                while ($next_pos < $length && ($formula[$next_pos] === ' ' || $formula[$next_pos] === "\t")) {
                    $next_pos++;
                }
                if ($next_pos < $length && $formula[$next_pos] === '(') {
                    $tokens[] = array('type' => 'function', 'value' => strtoupper($identifier));
                } elseif (strpos($identifier, '.') !== false || ctype_upper($identifier)) {
                    // Qualified variable (Namespace.Field) or UPPER_CASE constant
                    $tokens[] = array('type' => 'variable', 'value' => $identifier);
                } else {
                    // Unqualified identifier — treat as variable
                    $tokens[] = array('type' => 'variable', 'value' => $identifier);
                }
                continue;
            }

            // Unknown character — skip
            $pos++;
        }

        return $tokens;
    }

    /**
     * Serialize an array of tokens back into an NFX formula string.
     *
     * @param array $tokens
     * @return string
     */
    public function serializeTokens(array $tokens)
    {
        if (empty($tokens)) {
            return '';
        }

        $parts = array();

        for ($i = 0, $count = count($tokens); $i < $count; $i++) {
            $token = $tokens[$i];
            $type = isset($token['type']) ? $token['type'] : 'literal';
            $value = isset($token['value']) ? $token['value'] : '';

            // Determine if we need a space before this token
            $need_space_before = ($i > 0);
            $need_space_after = false;

            switch ($type) {
                case 'variable':
                    $parts[] = ($need_space_before ? ' ' : '') . $value;
                    break;

                case 'function':
                    $parts[] = ($need_space_before ? ' ' : '') . $value;
                    break;

                case 'literal':
                    $parts[] = ($need_space_before ? ' ' : '') . $value;
                    break;

                case 'operator':
                    if ($value === ',' || $value === '(' || $value === ')') {
                        // No space before comma or closing paren
                        if ($value === '(') {
                            $need_space_before = true;
                        } else {
                            $need_space_before = false;
                        }
                    }
                    $parts[] = ($need_space_before ? ' ' : '') . $value;
                    break;

                case 'group':
                    if ($value === ')') {
                        $need_space_before = false;
                    } elseif ($value === '(' && $i > 0) {
                        $prev = $tokens[$i - 1];
                        $prev_type = isset($prev['type']) ? $prev['type'] : '';
                        if ($prev_type === 'variable' || $prev_type === 'literal') {
                            // Implicit multiplication — add operator
                            $parts[] = ' * ';
                        }
                    }
                    $parts[] = ($need_space_before ? ' ' : '') . $value;
                    break;

                default:
                    $parts[] = ($need_space_before ? ' ' : '') . $value;
                    break;
            }
        }

        return trim(implode('', $parts));
    }

    // -----------------------------------------------------------------------
    // Form state helpers
    // -----------------------------------------------------------------------

    /**
     * Get the token currently being edited (if any).
     *
     * @return array|null
     */
    private function getEditingToken()
    {
        $edit_index = $this->getPostedInt('fd_edit_token');
        if ($edit_index === null) {
            return null;
        }

        $tokens = $this->parseFormulaToTokens($this->formula);
        if (!isset($tokens[$edit_index])) {
            return null;
        }

        $token = $tokens[$edit_index];
        $token['_index'] = $edit_index;

        return $token;
    }

    /**
     * Get the currently selected token type (from POST or from an editing token).
     *
     * @param array|null $editing_token
     * @return string
     */
    private function getSelectedTokenType($editing_token)
    {
        $posted = $this->getPostedString('fd_new_token_type');
        if ($posted !== null && $posted !== '') {
            return $posted;
        }

        if ($editing_token !== null && isset($editing_token['type'])) {
            return $editing_token['type'];
        }

        return 'literal';
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /**
     * Get a human-readable label for a token type.
     *
     * @param string $type
     * @return string
     */
    private function getTokenTypeLabel($type)
    {
        $labels = array(
            'variable' => _('Var'),
            'operator' => _('Op'),
            'literal' => _('Lit'),
            'function' => _('Fn'),
            'group' => _('()'),
        );

        return isset($labels[$type]) ? $labels[$type] : $type;
    }

    /**
     * Get a display value for a token.
     *
     * @param string $type
     * @param string $value
     * @return string
     */
    private function getTokenDisplayValue($type, $value)
    {
        if ($type === 'function') {
            return strtoupper($value) . '(...)';
        }

        if ($type === 'group') {
            return $value === '(' ? _('( open') : _(') close');
        }

        return $value;
    }

    /**
     * Generate a "selected" attribute string when values match.
     *
     * @param array|null $token
     * @param string     $key
     * @param string     $expected
     * @return string
     */
    private function selected($token, $key, $expected)
    {
        if ($token === null) {
            return '';
        }

        $value = isset($token[$key]) ? $token[$key] : '';

        return ((string)$value === (string)$expected) ? ' selected' : '';
    }

    /**
     * Safely read a string value from $_POST.
     *
     * @param string $key
     * @return string|null
     */
    private function getPostedString($key)
    {
        return isset($_POST[$key]) && is_string($_POST[$key]) ? $_POST[$key] : null;
    }

    /**
     * Safely read an integer value from $_POST.
     *
     * @param string $key
     * @return int|null
     */
    private function getPostedInt($key)
    {
        return isset($_POST[$key]) && is_numeric($_POST[$key]) ? (int)$_POST[$key] : null;
    }

    /**
     * Escape a string for safe HTML output.
     *
     * @param string $str
     * @return string
     */
    private function escape($str)
    {
        return htmlspecialchars((string)$str, ENT_QUOTES, 'UTF-8');
    }

    /**
     * Escape a string for use in an HTML attribute value.
     *
     * @param string $str
     * @return string
     */
    private function escapeAttr($str)
    {
        return htmlspecialchars((string)$str, ENT_QUOTES, 'UTF-8');
    }

    // -----------------------------------------------------------------------
    // Static helpers for the parent page
    // -----------------------------------------------------------------------

    /**
     * Process a form POST from the no-JS form builder and return the
     * serialized formula text. The caller should use the result as the
     * canonical formula value for database storage.
     *
     * Usage in a module page:
     *   $formula = FormulaDesigner_Renderer_DesignerFormRenderer::processPost($defaultFormula);
     *
     * @param string $current_formula  The current formula from the textarea (fallback).
     * @return string
     */
    public static function processPost($current_formula = '')
    {
        if (!isset($_POST['fd_submit_token']) && !isset($_POST['fd_remove_token'])
            && !isset($_POST['fd_reset']) && !isset($_POST['fd_add_token'])
        ) {
            return (string)$current_formula;
        }

        // Get the current serialized formula from the hidden textarea
        $serialized = isset($_POST['formula_designer_formula'])
            ? (string)$_POST['formula_designer_formula']
            : (string)$current_formula;

        $renderer = new self($serialized, '');

        // Parse current tokens from the hidden textarea
        $tokens = $renderer->parseFormulaToTokens($serialized);

        // Handle reset
        if (isset($_POST['fd_reset'])) {
            return '';
        }

        // Handle remove token
        if (isset($_POST['fd_remove_token']) && is_numeric($_POST['fd_remove_token'])) {
            $remove_index = (int)$_POST['fd_remove_token'];
            if (isset($tokens[$remove_index])) {
                array_splice($tokens, $remove_index, 1);
            }
            return $renderer->serializeTokens($tokens);
        }

        // Handle add/edit token
        if (isset($_POST['fd_submit_token'])) {
            $new_type = isset($_POST['fd_new_token_type']) ? (string)$_POST['fd_new_token_type'] : 'literal';
            $new_value = isset($_POST['fd_new_token_value']) ? (string)$_POST['fd_new_token_value'] : '';

            // Validate token type
            $valid_types = array('variable', 'operator', 'literal', 'function', 'group');
            if (!in_array($new_type, $valid_types, true)) {
                $new_type = 'literal';
            }

            // Validate value is not empty
            if ($new_value === '') {
                return $serialized;
            }

            // Check if editing an existing token
            $edit_index = $renderer->getPostedInt('fd_edit_token');
            if ($edit_index !== null) {
                if (isset($tokens[$edit_index])) {
                    $tokens[$edit_index] = array('type' => $new_type, 'value' => $new_value);
                }
            } else {
                // Adding a new token at the end
                $tokens[] = array('type' => $new_type, 'value' => $new_value);
            }

            return $renderer->serializeTokens($tokens);
        }

        // Handle add token trigger (just returns current formula unchanged)
        return $serialized;
    }
}
