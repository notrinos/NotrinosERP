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

require_once dirname(__FILE__) . '/DesignerEditorRenderer.php';
require_once dirname(__FILE__) . '/DesignerPreviewRenderer.php';
require_once dirname(__DIR__) . '/Validator/DesignerPreSubmitValidator.php';

/**
 * DesignerRenderer — base renderer for the Visual Formula Designer shell.
 *
 *
 * @package FormulaDesigner\Renderer
 * @since   2.0.0
 */
class FormulaDesigner_Renderer_DesignerRenderer
{
    /** @var string */
    private $formula;

    /** @var string */
    private $module;

    /** @var array */
    private $options;

    /**
     * @param string $formula
     * @param string $module
     * @param array  $options
     */
    public function __construct($formula = '', $module = '', array $options = array())
    {
        $this->formula = (string)$formula;
        $this->module = (string)$module;
        $this->options = $options;
    }

    /**
     * Render the base designer output.
     *
     * @return string
     */
    public function render()
    {
        $instance_id = isset($this->options['instanceId']) && $this->options['instanceId'] !== ''
            ? (string)$this->options['instanceId']
            : 'fd-' . substr(md5($this->module . '|' . $this->formula), 0, 8);
        $base_url = isset($this->options['baseUrl']) ? rtrim((string)$this->options['baseUrl'], '/') : '';
        $field_api_url = $base_url . '/includes/formula_designer/API/DesignerFieldListAPI.php?module=' . rawurlencode($this->module);
        $function_api_url = $base_url . '/includes/formula_designer/API/DesignerFunctionListAPI.php?module=' . rawurlencode($this->module);
        $validate_api_url = $base_url . '/includes/formula_designer/API/DesignerValidateAPI.php';
        $textarea_name = isset($this->options['textareaName']) && $this->options['textareaName'] !== ''
            ? (string)$this->options['textareaName']
            : 'formula_designer_formula';
        $textarea_id = isset($this->options['textareaId']) && $this->options['textareaId'] !== ''
            ? (string)$this->options['textareaId']
            : $instance_id . '-source';
        $field_sections = DesignerFacade::getAvailableFields($this->module);
        $function_sections = DesignerFacade::getAvailableFunctions($this->module);
        $submit_block_message = FormulaDesigner_Validator_DesignerPreSubmitValidator::getSubmitBlockMessage();

        $editor = $this->createEditor();
        $editor_renderer = new FormulaDesigner_Renderer_DesignerEditorRenderer($editor, $this->options);
        $preview_renderer = new FormulaDesigner_Renderer_DesignerPreviewRenderer();

        $parts = array();
        $parts[] = '<div class="fd-container" role="application" data-designer="root" data-instance-id="'
            . $this->escape($instance_id) . '" data-module="' . $this->escape($this->module)
            . '" data-field-api-url="' . $this->escape($field_api_url)
            . '" data-function-api-url="' . $this->escape($function_api_url)
            . '" data-validate-api-url="' . $this->escape($validate_api_url)
            . '" data-textarea-id="' . $this->escape($textarea_id)
            . '" data-submit-block-message="' . $this->escape($submit_block_message) . '">';
        $parts[] = $this->renderToolbar();
        $parts[] = '<div class="fd-workspace">';
        $parts[] = $this->renderFieldPalette($field_sections, $field_api_url);
        $parts[] = '<div class="fd-main-area">';
        $parts[] = $this->renderCanvas($instance_id, $editor_renderer);
        $parts[] = $editor_renderer->renderErrorPanel();
        $parts[] = $preview_renderer->renderPreviewPanel();
        $parts[] = $preview_renderer->renderExplainPanel();
        $parts[] = '</div>';
        $parts[] = $this->renderFunctionPalette($function_sections, $function_api_url);
        $parts[] = $this->renderPropertyPanel();
        $parts[] = $this->renderTemplateBrowser();
        $parts[] = $this->renderAIPanel();
        $parts[] = '</div>';
        $parts[] = $editor_renderer->renderSourceTextarea(
            $textarea_name,
            $editor->getFormula(),
            $textarea_id
        );
        $parts[] = '<script type="application/json" class="fd-expression-data">'
            . $this->encodeJson($editor->getExpression())
            . '</script>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Build the editor state.
     *
     * @return FormulaDesigner_Editor_DesignerEditor
     */
    private function createEditor()
    {
        if (isset($this->options['expression']) && is_array($this->options['expression'])) {
            return new FormulaDesigner_Editor_DesignerEditor($this->options['expression']);
        }

        return new FormulaDesigner_Editor_DesignerEditor($this->formula);
    }

    /**
     * Render the compact toolbar.
     *
     * @return string
     */
    private function renderToolbar()
    {
        $parts = array();
        $parts[] = '<div class="fd-toolbar" role="toolbar" aria-label="Formula editor toolbar">';
        $parts[] = '<div class="fd-toolbar-group fd-toolbar-group--title">';
        $parts[] = '<span class="fd-toolbar-title">Visual Formula Designer</span>';
        $parts[] = '<span class="fd-toolbar-subtitle">Phase 15 AI features</span>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-toolbar-group fd-toolbar-group--undo">';
        $parts[] = '<button type="button" class="fd-toolbar-action fd-toolbar-action--undo" data-action="undo" aria-label="Undo" disabled="disabled" title="Undo">↩</button>';
        $parts[] = '<button type="button" class="fd-toolbar-action fd-toolbar-action--redo" data-action="redo" aria-label="Redo" disabled="disabled" title="Redo">↪</button>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-toolbar-group fd-toolbar-group--actions">';
        $parts[] = '<button type="button" class="fd-toolbar-action fd-toolbar-action--validate" data-action="validate" aria-label="Validate formula">';
        $parts[] = 'Validate <span class="fd-toolbar-badge" data-role="validation-count">0</span>';
        $parts[] = '</button>';
        $parts[] = '<button type="button" class="fd-toolbar-action fd-toolbar-action--preview" data-action="toggle-preview" aria-label="Preview formula result">Preview</button>';
        $parts[] = '<button type="button" class="fd-toolbar-action fd-toolbar-action--explain" data-action="toggle-explain" aria-label="Explain formula step-by-step">Explain</button>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-toolbar-group fd-toolbar-group--templates">';
        $parts[] = '<button type="button" class="fd-toolbar-action fd-toolbar-action--template" data-action="toggle-template-browser" aria-label="Browse templates and favorites" title="Templates &amp; Favorites">📋</button>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-toolbar-group fd-toolbar-group--ai">';
        $parts[] = '<button type="button" class="fd-toolbar-action fd-toolbar-action--ai" data-action="toggle-ai-panel" aria-label="Open AI assistant" title="AI Formula Assistant">🤖 AI</button>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-toolbar-group fd-toolbar-group--zoom">';
        $parts[] = '<button type="button" class="fd-toolbar-action" data-action="zoom-out" aria-label="Zoom out">-</button>';
        $parts[] = '<button type="button" class="fd-toolbar-action fd-toolbar-action--ghost" data-action="reset-zoom" aria-label="Reset zoom">';
        $parts[] = '<span class="fd-zoom-value">100%</span>';
        $parts[] = '</button>';
        $parts[] = '<button type="button" class="fd-toolbar-action" data-action="zoom-in" aria-label="Zoom in">+</button>';
        $parts[] = '</div>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render the canvas shell.
     *
     * @param string                                            $instance_id
     * @param FormulaDesigner_Renderer_DesignerEditorRenderer $editor_renderer
     * @return string
     */
    private function renderCanvas($instance_id, FormulaDesigner_Renderer_DesignerEditorRenderer $editor_renderer)
    {
        $parts = array();
        $parts[] = '<div class="fd-canvas" id="fd-canvas-' . $this->escape($instance_id) . '"';
        $parts[] = ' data-designer="canvas" data-grid-size="20" data-zoom-level="1"';
        $parts[] = ' role="group" aria-label="Formula expression editor" tabindex="0">';
        $parts[] = '<svg class="fd-grid-background" aria-hidden="true"></svg>';
        $parts[] = '<div class="fd-expression" data-designer="expression" data-expression-id="root">';
        $parts[] = $editor_renderer->render();
        $parts[] = '</div>';
        $parts[] = '<div class="fd-drop-indicator" aria-hidden="true"></div>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render the field palette shell and initial namespace sections.
     *
     * @param array  $sections
     * @param string $api_url
     * @return string
     */
    private function renderFieldPalette(array $sections, $api_url)
    {
        $parts = array();
        $parts[] = '<div class="fd-palette fd-palette-left" data-designer="field-palette" data-api-url="'
            . $this->escape($api_url) . '" role="region" aria-label="Available fields">';
        $parts[] = '<div class="fd-palette-header">';
        $parts[] = '<span class="fd-palette-title">Fields</span>';
        $parts[] = '<input class="fd-palette-search" type="search" placeholder="Search fields..." aria-label="Search fields">';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-palette-body">';

        foreach ($sections as $section) {
            $parts[] = $this->renderFieldSection($section);
        }

        $parts[] = '</div>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render the function palette shell and initial category sections.
     *
     * @param array  $sections
     * @param string $api_url
     * @return string
     */
    private function renderFunctionPalette(array $sections, $api_url)
    {
        $parts = array();
        $parts[] = '<div class="fd-palette fd-palette-right" data-designer="function-palette" data-api-url="'
            . $this->escape($api_url) . '" role="region" aria-label="Available functions">';
        $parts[] = '<div class="fd-palette-header">';
        $parts[] = '<span class="fd-palette-title">Functions</span>';
        $parts[] = '<input class="fd-palette-search" type="search" placeholder="Search functions..." aria-label="Search functions">';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-palette-body">';

        // ---- Built-in operator tokens ---- //
        $parts[] = '<div class="fd-category-section" data-category="operators">';
        $parts[] = '<div class="fd-category-header" role="button" aria-expanded="true" tabindex="0">';
        $parts[] = '<span class="fd-category-icon">±</span>';
        $parts[] = '<span class="fd-category-label">Operators</span>';
        $parts[] = '<span class="fd-category-count">4</span>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-category-items">';
        $operators = array(
            array('label' => '+', 'value' => '+'),
            array('label' => '−', 'value' => '-'),
            array('label' => '×', 'value' => '*'),
            array('label' => '÷', 'value' => '/'),
        );
        foreach ($operators as $op) {
            $parts[] = '<div class="fd-palette-item fd-palette-function" draggable="true"'
                . ' data-token-type="operator"'
                . ' data-token-value="' . $this->escape($op['value']) . '"'
                . ' data-display-label="' . $this->escape($op['value']) . '"'
                . ' data-metadata="{}"'
                . ' data-function-name="' . $this->escape($op['value']) . '"'
                . ' data-function-signature="' . $this->escape($op['value']) . '"'
                . ' data-function-description="' . $this->escape($op['value']) . '"'
                . ' role="listitem">';
            $parts[] = '<span class="fd-palette-item-icon fd-icon-function">±</span>';
            $parts[] = '<span class="fd-palette-item-label">' . $this->escape($op['label']) . '</span>';
            $parts[] = '<span class="fd-palette-item-signature"></span>';
            $parts[] = '</div>';
        }
        $parts[] = '</div>';
        $parts[] = '</div>';

        // ---- Built-in literal tokens ---- //
        $parts[] = '<div class="fd-category-section" data-category="literals">';
        $parts[] = '<div class="fd-category-header" role="button" aria-expanded="true" tabindex="0">';
        $parts[] = '<span class="fd-category-icon">#</span>';
        $parts[] = '<span class="fd-category-label">Literals</span>';
        $parts[] = '<span class="fd-category-count">4</span>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-category-items">';
        $literals = array(
            array('label' => 'Number', 'value' => '0', 'hint' => 'Click → type a number (e.g. 5, 0.1, 3000)'),
            array('label' => 'TRUE', 'value' => 'TRUE', 'hint' => 'Boolean True'),
            array('label' => 'FALSE', 'value' => 'FALSE', 'hint' => 'Boolean False'),
            array('label' => 'NULL', 'value' => 'NULL', 'hint' => 'Null / Empty'),
        );
        foreach ($literals as $lit) {
            $meta = array('dataType' => ($lit['value'] === 'TRUE' || $lit['value'] === 'FALSE') ? 'boolean' : ($lit['value'] === 'NULL' ? 'null' : 'number'));
            if ($lit['value'] === 'TRUE') $meta['rawValue'] = true;
            if ($lit['value'] === 'FALSE') $meta['rawValue'] = false;
            if ($lit['value'] === 'NULL') $meta['rawValue'] = null;
            $metaJson = json_encode($meta);
            $parts[] = '<div class="fd-palette-item fd-palette-function" draggable="true"'
                . ' data-token-type="literal"'
                . ' data-token-value="' . $this->escape($lit['value']) . '"'
                . ' data-display-label="' . $this->escape($lit['label']) . '"'
                . ' data-metadata="' . $this->escape($metaJson) . '"'
                . ' data-function-name="' . $this->escape($lit['value']) . '"'
                . ' data-function-signature="' . $this->escape($lit['hint']) . '"'
                . ' data-function-description="' . $this->escape($lit['hint']) . '"'
                . ' role="listitem">';
            $parts[] = '<span class="fd-palette-item-icon fd-icon-function">#</span>';
            $parts[] = '<span class="fd-palette-item-label">' . $this->escape($lit['label']) . '</span>';
            $parts[] = '<span class="fd-palette-item-signature"></span>';
            $parts[] = '</div>';
        }
        $parts[] = '</div>';
        $parts[] = '</div>';

        foreach ($sections as $section) {
            $parts[] = $this->renderFunctionSection($section);
        }

        $parts[] = '</div>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render one namespace section.
     *
     * @param array $section
     * @return string
     */
    private function renderFieldSection(array $section)
    {
        $namespace = isset($section['namespace']) ? $section['namespace'] : 'General';
        $items = isset($section['items']) && is_array($section['items']) ? $section['items'] : array();
        $parts = array();

        $parts[] = '<div class="fd-namespace-section" data-namespace="' . $this->escape($namespace) . '">';
        $parts[] = '<div class="fd-namespace-header" role="button" aria-expanded="true" tabindex="0">';
        $parts[] = '<span class="fd-namespace-icon">' . $this->escape(substr($namespace, 0, 3)) . '</span>';
        $parts[] = '<span class="fd-namespace-label">' . $this->escape(isset($section['label']) ? $section['label'] : $namespace) . '</span>';
        $parts[] = '<span class="fd-namespace-count">' . (int)count($items) . '</span>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-namespace-items">';

        foreach ($items as $item) {
            $parts[] = $this->renderFieldItem($item);
        }

        $parts[] = '</div>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render one function category section.
     *
     * @param array $section
     * @return string
     */
    private function renderFunctionSection(array $section)
    {
        $category = isset($section['category']) ? $section['category'] : 'General';
        $items = isset($section['items']) && is_array($section['items']) ? $section['items'] : array();
        $parts = array();

        $parts[] = '<div class="fd-category-section" data-category="' . $this->escape($category) . '">';
        $parts[] = '<div class="fd-category-header" role="button" aria-expanded="true" tabindex="0">';
        $parts[] = '<span class="fd-category-icon">fx</span>';
        $parts[] = '<span class="fd-category-label">' . $this->escape(isset($section['label']) ? $section['label'] : $category) . '</span>';
        $parts[] = '<span class="fd-category-count">' . (int)count($items) . '</span>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-category-items">';

        foreach ($items as $item) {
            $parts[] = $this->renderFunctionItem($item);
        }

        $parts[] = '</div>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render one draggable field palette item.
     *
     * @param array $item
     * @return string
     */
    private function renderFieldItem(array $item)
    {
        $qualified = isset($item['qualifiedName']) ? $item['qualifiedName'] : ((isset($item['namespace']) ? $item['namespace'] . '.' : '') . (isset($item['name']) ? $item['name'] : ''));
        $metadata = array(
            'namespace' => isset($item['namespace']) ? $item['namespace'] : '',
            'name' => isset($item['name']) ? $item['name'] : '',
            'dataType' => isset($item['type']) ? $item['type'] : 'mixed',
            'description' => isset($item['description']) ? $item['description'] : '',
        );

        $parts = array();
        $parts[] = '<div class="fd-palette-item fd-palette-field" draggable="true"';
        $parts[] = ' data-token-type="variable"';
        $parts[] = ' data-token-value="' . $this->escape($qualified) . '"';
        $parts[] = ' data-display-label="' . $this->escape(isset($item['label']) ? $item['label'] : $qualified) . '"';
        $parts[] = ' data-metadata="' . $this->escape($this->encodeAttributeJson($metadata)) . '"';
        $parts[] = ' data-field-type="' . $this->escape(isset($item['type']) ? $item['type'] : 'mixed') . '"';
        $parts[] = ' data-field-qualified="' . $this->escape($qualified) . '"';
        $parts[] = ' data-field-label="' . $this->escape(isset($item['label']) ? $item['label'] : $qualified) . '"';
        $parts[] = ' role="listitem">';
        $parts[] = '<span class="fd-palette-item-icon fd-icon-variable">' . $this->escape($this->typeAbbreviation(isset($item['type']) ? $item['type'] : 'mixed')) . '</span>';
        $parts[] = '<span class="fd-palette-item-label">' . $this->escape(isset($item['label']) ? $item['label'] : $qualified) . '</span>';
        $parts[] = '<span class="fd-palette-item-type">' . $this->escape(isset($item['type']) ? $item['type'] : 'mixed') . '</span>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render one function palette item.
     *
     * @param array $item
     * @return string
     */
    private function renderFunctionItem(array $item)
    {
        $enabled = !isset($item['enabled']) || $item['enabled'];
        $metadata = array(
            'name' => isset($item['name']) ? $item['name'] : '',
            'minArgs' => isset($item['minArgs']) ? (int)$item['minArgs'] : 0,
            'maxArgs' => isset($item['maxArgs']) ? (int)$item['maxArgs'] : 0,
            'returnType' => isset($item['returnType']) ? $item['returnType'] : 'mixed',
            'description' => isset($item['description']) ? $item['description'] : '',
        );
        $classes = 'fd-palette-item fd-palette-function';

        if (!$enabled) {
            $classes .= ' fd-palette-item--disabled';
        }

        $parts = array();
        $parts[] = '<div class="' . $classes . '" draggable="' . ($enabled ? 'true' : 'false') . '"';
        $parts[] = ' data-token-type="function"';
        $parts[] = ' data-token-value="' . $this->escape(isset($item['tokenValue']) ? $item['tokenValue'] : ((isset($item['name']) ? $item['name'] : '') . '(')) . '"';
        $parts[] = ' data-display-label="' . $this->escape(isset($item['label']) ? $item['label'] : '') . '"';
        $parts[] = ' data-metadata="' . $this->escape($this->encodeAttributeJson($metadata)) . '"';
        $parts[] = ' data-function-name="' . $this->escape(isset($item['name']) ? $item['name'] : '') . '"';
        $parts[] = ' data-function-signature="' . $this->escape(isset($item['signature']) ? $item['signature'] : '') . '"';
        $parts[] = ' data-function-description="' . $this->escape(isset($item['description']) ? $item['description'] : '') . '"';
        $parts[] = ' role="listitem">';
        $parts[] = '<span class="fd-palette-item-icon fd-icon-function">fx</span>';
        $parts[] = '<span class="fd-palette-item-label">' . $this->escape(isset($item['label']) ? $item['label'] : '') . '</span>';
        $parts[] = '<span class="fd-palette-item-signature">' . $this->escape(isset($item['signature']) ? $item['signature'] : '') . '</span>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Encode a JSON payload for a data attribute.
     *
     * @param array $payload
     * @return string
     */
    private function encodeAttributeJson(array $payload)
    {
        $json = json_encode($payload);

        return $json === false ? '{}' : $json;
    }

    /**
     * Convert a field type to a compact badge label.
     *
     * @param string $type
     * @return string
     */
    private function typeAbbreviation($type)
    {
        switch (strtolower((string)$type)) {
            case 'number':
            case 'decimal':
            case 'integer':
                return 'N';

            case 'text':
            case 'string':
                return 'T';

            case 'date':
                return 'D';

            case 'boolean':
                return 'B';

            default:
                return 'M';
        }
    }

    /**
     * Render the Property panel shell.
     *
     * The panel is hidden by default and shown when a token is selected.
     *
     * @return string
     */
    private function renderPropertyPanel()
    {
        $parts = array();
        $parts[] = '<div class="fd-property-panel" data-designer="property-panel" hidden aria-label="Token properties">';
        $parts[] = '<div class="fd-property-panel-header">';
        $parts[] = '<span class="fd-property-panel-title">Token Properties</span>';
        $parts[] = '<button type="button" class="fd-property-panel-close" data-action="close-property-panel" aria-label="Close property panel">×</button>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-property-panel-body" data-designer="property-panel-body">';
        $parts[] = '<div class="fd-property-panel-empty">Select a token to view its properties</div>';
        $parts[] = '</div>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render the template browser and favorites panel.
     *
     * The panel is hidden by default and shown via the 📋 toolbar toggle.
     *
     * @return string
     */
    private function renderTemplateBrowser()
    {
        $template_api_url = isset($this->options['baseUrl'])
            ? rtrim((string)$this->options['baseUrl'], '/') . '/includes/formula_designer/API/DesignerTemplateAPI.php?module=' . rawurlencode($this->module)
            : '';

        $parts = array();
        $parts[] = '<div class="fd-template-browser" data-designer="template-browser" hidden aria-label="Templates and favorites browser">';
        $parts[] = '<div class="fd-template-browser-header">';
        $parts[] = '<span class="fd-template-browser-title">Templates &amp; Favorites</span>';
        $parts[] = '<button type="button" class="fd-template-browser-close" data-action="close-template-browser" aria-label="Close template browser">×</button>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-template-browser-body">';

        // Template browser section
        $parts[] = '<div class="fd-template-section">';
        $parts[] = '<div class="fd-template-section-header">';
        $parts[] = '<span class="fd-template-section-title">Built-in Templates</span>';
        $parts[] = '<span class="fd-template-section-count" data-designer="template-count">34</span>';
        $parts[] = '</div>';
        $parts[] = '<input class="fd-template-search" type="search" placeholder="Search templates..." aria-label="Search templates" data-designer="template-search">';
        $parts[] = '<div class="fd-template-list" data-designer="template-list" role="list">';
        $parts[] = '<div class="fd-template-loading">Loading templates...</div>';
        $parts[] = '</div>';
        $parts[] = '</div>';

        // Favorites section
        $parts[] = '<div class="fd-template-section fd-favorites-section">';
        $parts[] = '<div class="fd-template-section-header">';
        $parts[] = '<span class="fd-template-section-title">Favorites</span>';
        $parts[] = '<span class="fd-template-section-count" data-designer="favorite-count">0</span>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-favorite-list" data-designer="favorite-list" role="list">';
        $parts[] = '<div class="fd-favorites-empty" data-designer="favorites-empty">No favorites saved yet. Use the star ⭐ to save formulas.</div>';
        $parts[] = '</div>';
        $parts[] = '</div>';

        $parts[] = '</div>';
        // Hidden template API URL for JS
        $parts[] = '<input type="hidden" data-designer="template-api-url" value="' . $this->escape($template_api_url) . '">';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render the AI assistant panel.
     *
     * @return string
     */
    private function renderAIPanel()
    {
        $ai_api_url = isset($this->options['baseUrl'])
            ? rtrim((string)$this->options['baseUrl'], '/') . '/includes/formula_designer/API/DesignerAIApi.php'
            : '';

        $parts = array();
        $parts[] = '<div class="fd-ai-panel" data-designer="ai-panel" hidden aria-label="AI Formula Assistant">';
        $parts[] = '<div class="fd-ai-panel-header">';
        $parts[] = '<span class="fd-ai-panel-title">🤖 AI Formula Assistant</span>';
        $parts[] = '<span class="fd-ai-provider-badge" data-designer="ai-provider-name">RuleBased</span>';
        $parts[] = '<button type="button" class="fd-ai-panel-close" data-action="close-ai-panel" aria-label="Close AI assistant">×</button>';
        $parts[] = '</div>';

        // Chat body
        $parts[] = '<div class="fd-ai-chat-body">';
        $parts[] = '<div class="fd-ai-bubble fd-ai-bubble--assistant">';
        $parts[] = '<div class="fd-ai-bubble-content">';
        $parts[] = 'Hello! I can help you create formulas. Describe what you want to calculate in plain language, or paste an Excel formula using the <strong>Import Excel</strong> button below.';
        $parts[] = '</div>';
        $parts[] = '</div>';
        $parts[] = '</div>';

        // Chat input area
        $parts[] = '<div class="fd-ai-chat-input-area">';
        $parts[] = '<textarea class="fd-ai-chat-input" placeholder="Describe the formula you need..." rows="2" aria-label="Describe your formula"></textarea>';
        $parts[] = '<div class="fd-ai-chat-actions">';
        $parts[] = '<button type="button" class="fd-ai-action-btn fd-ai-import-btn" title="Import Excel formula">📎 Excel</button>';
        $parts[] = '<button type="button" class="fd-ai-action-btn fd-ai-clear-btn" title="Clear chat">🗑 Clear</button>';
        $parts[] = '<button type="button" class="fd-ai-action-btn fd-ai-send-btn fd-ai-send-btn--primary" title="Send message">Send ✉</button>';
        $parts[] = '</div>';
        $parts[] = '</div>';

        // Hidden API URL for JS
        $parts[] = '<input type="hidden" data-designer="ai-api-url" value="' . $this->escape($ai_api_url) . '">';

        $parts[] = '</div>';

        return implode('', $parts);
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

    /**
     * Safely embed JSON inside a script tag.
     *
     * @param array $payload
     * @return string
     */
    private function encodeJson(array $payload)
    {
        $json = json_encode($payload);
        if ($json === false) {
            return '{}';
        }

        return str_replace('</', '<\/', $json);
    }
}
