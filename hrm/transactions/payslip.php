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
 * Backward-compatible entry point for payslip transaction screen.
 *
 * Phase 1.19 migrates payslip processing into `hrm/transactions/`.
 * The full processing implementation currently resides in `hrm/payslip.php`.
 */
include_once(__DIR__.'/../payslip.php');

