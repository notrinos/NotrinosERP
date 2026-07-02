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
 * DesignerPreviewRenderer — renders the Phase 7 preview and explain panels.
 *
 * @package FormulaDesigner\Renderer
 * @since   2.0.0
 */
class FormulaDesigner_Renderer_DesignerPreviewRenderer
{
    /**
     * Render the live preview panel (hidden by default).
     *
     * @return string
     */
    public function renderPreviewPanel()
    {
        $parts = array();
        $parts[] = '<div class="fd-preview-panel" data-designer="preview-panel" aria-hidden="true">';
        $parts[] = '<div class="fd-preview-header">';
        $parts[] = '<h3>Preview Result</h3>';
        $parts[] = '<div class="fd-preview-header-actions">';
        $parts[] = '<button type="button" data-action="preview-refresh" aria-label="Refresh preview">Refresh</button>';
        $parts[] = '<button type="button" data-action="sample-reset" aria-label="Reset sample values">Reset</button>';
        $parts[] = '</div>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-preview-body">';
        $parts[] = '<div class="fd-preview-note">Open the Preview panel to evaluate your formula with sample values.</div>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-preview-samples">';
        $parts[] = '<div class="fd-preview-samples-header"><h4>Sample Values</h4></div>';
        $parts[] = '<div class="fd-preview-samples-body">';
        $parts[] = '<div class="fd-preview-samples-empty">Sample values appear when variables are detected.</div>';
        $parts[] = '</div>';
        $parts[] = '</div>';
        $parts[] = '</div>';

        return implode('', $parts);
    }

    /**
     * Render the explain step-by-step panel (hidden by default).
     *
     * @return string
     */
    public function renderExplainPanel()
    {
        $parts = array();
        $parts[] = '<div class="fd-explain-panel" data-designer="explain-panel" aria-hidden="true">';
        $parts[] = '<div class="fd-explain-header">';
        $parts[] = '<h3>Step-by-Step Explain</h3>';
        $parts[] = '</div>';
        $parts[] = '<div class="fd-explain-body">';
        $parts[] = '<div class="fd-explain-note">Open the Explain panel to see a step-by-step evaluation trace.</div>';
        $parts[] = '</div>';
        $parts[] = '</div>';

        return implode('', $parts);
    }
}
