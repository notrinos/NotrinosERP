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
 * DesignerFieldInterface — contract for fields exposed in the designer palette.
 *
 * @package FormulaDesigner\Contracts
 * @since   2.0.0
 */
interface FormulaDesigner_Contracts_DesignerFieldInterface
{
    /**
     * Convert the field metadata into a serializable array.
     *
     * @return array
     */
    public function toArray();

    /**
     * Get the fully qualified field name.
     *
     * @return string
     */
    public function getQualifiedName();

    /**
     * Get the human-readable field label.
     *
     * @return string
     */
    public function getLabel();
}