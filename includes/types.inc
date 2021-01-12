<?php
/**********************************************************************
	Copyright (C) FrontAccounting, LLC.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
//----------------------------------------------------------------------------------
//	FrontAccounting system transaction types
//
define('ST_JOURNAL', 0);

define('ST_BANKPAYMENT', 1);
define('ST_BANKDEPOSIT', 2);
define('ST_BANKTRANSFER', 4);

define('ST_SALESINVOICE', 10);
define('ST_CUSTCREDIT', 11);
define('ST_CUSTPAYMENT', 12);
define('ST_CUSTDELIVERY', 13);

define('ST_LOCTRANSFER', 16);
define('ST_INVADJUST', 17);

define('ST_PURCHORDER', 18);
define('ST_SUPPINVOICE', 20);
define('ST_SUPPCREDIT', 21);
define('ST_SUPPAYMENT', 22);
define('ST_SUPPRECEIVE', 25);

define('ST_WORKORDER', 26);
define('ST_MANUISSUE', 28);
define('ST_MANURECEIVE', 29);

//
//	Depreciation period types
//
define('FA_MONTHLY', 0);
define('FA_YEARLY', 1);

define('ST_SALESORDER', 30);
define('ST_SALESQUOTE', 32);
define('ST_COSTUPDATE', 35);
define('ST_DIMENSION', 40);
define('ST_CUSTOMER', 41);
define('ST_SUPPLIER', 42);

// Don't include these defines in the $systypes_array.
// They are used for documents only.
define ('ST_STATEMENT', 91);
define ('ST_CHEQUE', 92);

// document inheritance
$document_child_types = array(
	ST_SALESQUOTE => ST_SALESORDER,
	ST_SALESORDER => ST_CUSTDELIVERY,
	ST_CUSTDELIVERY => ST_SALESINVOICE,
	ST_SALESINVOICE => ST_CUSTCREDIT,

	ST_PURCHORDER => ST_SUPPRECEIVE,
	ST_SUPPRECEIVE => ST_SUPPINVOICE,
	ST_SUPPINVOICE => ST_SUPPCREDIT,
);

function get_child_type($type) {
	global $document_child_types;
	return isset($document_child_types[$type]) ? $document_child_types[$type] : 0;
}

function get_parent_type($type) {
	global $document_child_types;
	$child = array_search($type, $document_child_types);
	return $child ? $child : 0;
}

//----------------------------------------------------------------------------------
//		Bank transaction types
//
define('BT_TRANSFER', 0);
define('BT_CHEQUE', 1);
define('BT_CREDIT', 2);
define('BT_CASH', 3);

include_once($path_to_root . '/manufacturing/includes/manufacturing_db.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');
include_once($path_to_root . '/sales/includes/sales_db.inc');
include_once($path_to_root . '/dimensions/includes/dimensions_db.inc');
//----------------------------------------------------------------------------------
//	Payment types
//
define('PT_MISC', 0);
define('PT_WORKORDER', 1);
define('PT_CUSTOMER', 2);
define('PT_SUPPLIER', 3);
define('PT_QUICKENTRY', 4);
define('PT_DIMESION', 5);

function payment_person_currency($type, $person_id)  {
	switch ($type) {
		case PT_MISC :
		case PT_QUICKENTRY :
		case PT_WORKORDER :
			return get_company_currency();

		case PT_CUSTOMER :
			return get_customer_currency($person_id);

		case PT_SUPPLIER :
			return get_supplier_currency($person_id);

		default :
			return get_company_currency();
	}
}

function payment_person_name($type, $person_id, $full=true) {
	global $payment_person_types;

	switch ($type) {
		case PT_MISC :
			return $person_id;
		case PT_QUICKENTRY :
			$qe = get_quick_entry($person_id);
			return ($full ? $payment_person_types[$type] . " ":"") . $qe["description"];
		case PT_WORKORDER :
			global $wo_cost_types;
			return (!$full ? _('Work Order').' '.$person_id : get_trans_view_str(ST_WORKORDER, $person_id, _('Work Order').' '.$person_id));
		case PT_CUSTOMER :
			return ($full ?$payment_person_types[$type] . " ":"") . sprintf("[%05s] %s", $person_id, get_customer_name($person_id));
		case PT_SUPPLIER :
			return ($full ? $payment_person_types[$type] . " ":"") . sprintf("[%05s] %s", $person_id, get_supplier_name($person_id));
		default :
			return '';
	}
}

function payment_person_has_items($type) {
	switch ($type) {
		case PT_MISC :
			return true;
		case PT_QUICKENTRY :
			return db_has_quick_entries();
		case PT_WORKORDER : // 070305 changed to open workorders JH
			return db_has_open_workorders();
		case PT_CUSTOMER :
			return db_has_customers();
		case PT_SUPPLIER :
			return db_has_suppliers();
		default :
			display_db_error("Invalid type sent to has_items", "");
			return false;
	}
}
//----------------------------------------------------------------------------------
//	Payment terms categories
//
define('PM_ANY', 0);
define('PM_CASH', 1);
define('PM_CREDIT', 2);

//----------------------------------------------------------------------------------
//	Manufacturing types
//
define('WO_ASSEMBLY', 0);
define('WO_UNASSEMBLY', 1);
define('WO_ADVANCED', 2);

define('WO_LABOUR', 0);
define('WO_OVERHEAD', 1);
define('WO_MATERIALS', 2);

//----------------------------------------------------------------------------------
//	GL account classes
//
define('CL_NONE', 0); // for backward compatibility
define('CL_ASSETS', 1);
define('CL_LIABILITIES', 2);
define('CL_EQUITY', 3);
define('CL_INCOME', 4);
define('CL_COGS', 5);
define('CL_EXPENSE', 6);

function get_class_type_convert($ctype) {
	global $SysPrefs;

	if (isset($SysPrefs->use_oldstyle_convert) && $SysPrefs->use_oldstyle_convert == 1)
		return (($ctype >= CL_INCOME || $ctype == CL_NONE) ? -1 : 1);
	else	
		return ((($ctype >= CL_LIABILITIES && $ctype <= CL_INCOME) || $ctype == CL_NONE) ? -1 : 1);
}
//----------------------------------------------------------------------------------
//	Quick entry types
//
define('QE_PAYMENT', '1');
define('QE_DEPOSIT', '2');
define('QE_JOURNAL', '3');
define('QE_SUPPINV', '4');

//----------------------------------------------------------------------------------
//	Special option values for various list selectors.
//
define('ANY_TEXT', '');
define('ANY_NUMERIC', -1);
define('ALL_TEXT', '');
define('ALL_NUMERIC', -1);

//----------------------------------------------------------------------------------
// Special class values for tables (start_table())
define('TABLESTYLE',  1);
define('TABLESTYLE2', 2);
define('TABLESTYLE_NOBORDER', 3);

//----------------------------------------------------------------------------------

define('TAG_ACCOUNT',   1);
define('TAG_DIMENSION', 2);

//----------------------------------------------------------------------------------
// Payment term types

define('PTT_PRE', 1);
define('PTT_CASH', 2);
define('PTT_DAYS', 3);
define('PTT_FOLLOWING', 4);

//----------------------------------------------------------------------------------
// Tax calculation algorithms used in als and purchase (depends on supplier's invoicing software)

define('TCA_TOTALS', 1); // taxes are calculated from respective net totals for all lines
define('TCA_LINES', 2); // taxes calculated for every line, then summed
//
//	Bank account owner types
//
define('BO_UNKNOWN', 0);
define('BO_COMPANY', 1);
define('BO_CUSTBRANCH', 2);
define('BO_SUPPLIER', 3);

include_once($path_to_root.'/includes/sysnames.inc');

// tax register type
define('TR_OUTPUT', 0); // sales
define('TR_INPUT', 1);	// purchase
//---------------------------------------------------------------------------------
// Constants optionally redefined locally
//
defined('ICON_EDIT') || define('ICON_EDIT', 'edit.svg');
defined('ICON_DELETE') || define('ICON_DELETE', 'delete.svg');
defined('ICON_ADD')	|| define('ICON_ADD', 'ok.svg');
defined('ICON_UPDATE') || define('ICON_UPDATE', 'ok.svg');
defined('ICON_OK') || define('ICON_OK', 'ok.svg');
defined('ICON_CANCEL') || define('ICON_CANCEL', 'cancel.png');
defined('ICON_GL') || define('ICON_GL', 'gl.svg');
defined('ICON_PRINT') || define('ICON_PRINT', 'print.png');
defined('ICON_PDF') || define('ICON_PDF', 'pdf.gif');
defined('ICON_DOC') || define('ICON_DOC', 'invoice.gif');
defined('ICON_CREDIT') || define('ICON_CREDIT', 'credit.gif');
defined('ICON_RECEIVE') || define('ICON_RECEIVE', 'receive.gif');
defined('ICON_DOWN') || define('ICON_DOWN', 'download.svg');
defined('ICON_MONEY') || define('ICON_MONEY', 'money.png');
defined('ICON_REMOVE') || define('ICON_REMOVE', 'remove.png');
defined('ICON_REPORT') || define('ICON_REPORT', 'report.png');
defined('ICON_VIEW') || define('ICON_VIEW', 'view.svg');
defined('ICON_SUBMIT') || define('ICON_SUBMIT', 'ok.svg');
defined('ICON_ESCAPE') || define('ICON_ESCAPE', 'escape.png');
defined('ICON_ALLOC') || define('ICON_ALLOC', 'alloc.png');
defined('ICON_CLOSED') || define('ICON_CLOSED', 'closed.png');