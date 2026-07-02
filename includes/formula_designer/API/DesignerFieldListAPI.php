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

define('FORMULA_DESIGNER_API_NO_AUTO_RUN', true);
require_once dirname(__FILE__) . '/DesignerAPI.php';

call_user_func(array('FormulaDesigner_API_DesignerAPI', 'renderFields'));