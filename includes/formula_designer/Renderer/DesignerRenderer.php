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
 * DesignerRenderer — base renderer scaffold for the Visual Formula Designer.
 *
 * Phase 1 intentionally returns an empty string. Concrete editor rendering is
 * introduced in Phase 2.
 *
 * @package FormulaDesigner\Renderer
 * @since   2.0.0
 */
class FormulaDesigner_Renderer_DesignerRenderer
{
    /**
     * Render the base designer output.
     *
     * @return string
     */
    public function render()
    {
        return '';
    }
}