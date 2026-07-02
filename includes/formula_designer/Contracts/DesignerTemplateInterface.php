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
 * DesignerTemplateInterface — contract for formula templates.
 *
 * @package FormulaDesigner\Contracts
 * @since   2.0.0
 */
interface FormulaDesigner_Contracts_DesignerTemplateInterface
{
    /**
     * Get the unique template identifier.
     *
     * @return string
     */
    public function getId();

    /**
     * Get the template formula text.
     *
     * @return string
     */
    public function getFormula();

    /**
     * Get template metadata for later rendering and filtering.
     *
     * @return array
     */
    public function getMetadata();
}