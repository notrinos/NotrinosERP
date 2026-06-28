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
 * RuntimeExecutionException — Unexpected runtime error during AST evaluation.
 *
 * Catch-all for unexpected errors that occur during AST evaluation
 * that are not covered by more specific exception types.
 *
 * @package Formula\Exceptions
 * @since   2.0.0
 */
class Formula_Exceptions_RuntimeExecutionException extends Formula_Exceptions_FormulaException
{
}
