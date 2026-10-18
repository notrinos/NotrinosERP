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
$page_security = 'SA_DIMENSIONREP';
$path_to_root='..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/gl/includes/gl_db.inc');

//----------------------------------------------------------------------------------------------------

print_dimension_summary();

function getTransactions($from, $to) {
	$sql = "SELECT *
		FROM
			".TB_PREF."dimensions
		WHERE id >= ".db_escape($from)."
		AND id <= ".db_escape($to)."
		ORDER BY
			reference";

	return db_query($sql, 'No transactions were returned');
}

/** Return the as-posted net GL balance from the selected fiscal start through as-of. */
function getYTD($dim, $as_of = null) {
	if ($as_of === null)
		$as_of = Today();
	if ((!is_int($dim) && !is_string($dim))
		|| !preg_match('/^[1-9][0-9]*$/D', (string)$dim) || (float)$dim > 2147483647
		|| !is_string($as_of) || !is_date($as_of))
		return false;
	$date = begin_fiscalyear();
	if (!is_string($date) || !is_date($date))
		return false;
	$date = date2sql($date);
	$as_of = date2sql($as_of);

	$sql = "SELECT COALESCE(SUM(amount),0) AS Balance
		FROM
			".TB_PREF."gl_trans
		WHERE (dimension_id = ".db_escape($dim)." OR dimension2_id = ".db_escape($dim).")
		AND tran_date >= ".db_escape($date)."
		AND tran_date <= ".db_escape($as_of);

	$TransResult = db_query($sql, 'No transactions were returned');
	if ($TransResult && db_num_rows($TransResult) == 1) {
		$DemandRow = db_fetch_row($TransResult);
		$balance = $DemandRow[0];
	}
	else
		return false;

	return $balance;
}

//----------------------------------------------------------------------------------------------------

function print_dimension_summary() {
	global $path_to_root;
	check_page_security('SA_DIMENSIONREP');

	$fromdim = isset($_POST['PARAM_0']) ? $_POST['PARAM_0'] : null;
	$todim = isset($_POST['PARAM_1']) ? $_POST['PARAM_1'] : null;
	foreach (array($fromdim, $todim) as $dimension) {
		if ((!is_int($dimension) && !is_string($dimension))
			|| !preg_match('/^(?:0|[1-9][0-9]*)$/D', (string)$dimension)
			|| (float)$dimension > 2147483647) {
			display_error(_('Invalid dimension range.'));
			return;
		}
	}
	$showbal = isset($_POST['PARAM_2']) ? $_POST['PARAM_2'] : 0;
	$comments = isset($_POST['PARAM_3']) ? $_POST['PARAM_3'] : '';
	$orientation = isset($_POST['PARAM_4']) ? $_POST['PARAM_4'] : 0;
	$destination = isset($_POST['PARAM_5']) ? $_POST['PARAM_5'] : 0;
	foreach (array($showbal, $orientation, $destination) as $option) {
		if ((!is_int($option) && !is_string($option)) || !preg_match('/^[01]?$/D', (string)$option)) {
			display_error(_('Invalid report option.'));
			return;
		}
	}
	if (!is_string($comments)) {
		display_error(_('Invalid report comments.'));
		return;
	}
	$as_of = Today();
	$fiscal_start = begin_fiscalyear();
	if ((int)$fromdim > (int)$todim || !is_string($as_of) || !is_date($as_of)
		|| !is_string($fiscal_start) || !is_date($fiscal_start)) {
		display_error(_('Invalid dimension range or fiscal dates.'));
		return;
	}
	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$cols = array(0, 50, 195, 230, 295, 360, 405, 515);

	$headers = array(_('Reference'), _('Name'), _('Type'), _('Date'), _('Due Date'), _('Closed'), _('Net GL balance'));

	$aligns = array('left',	'left', 'left',	'left', 'left', 'left', 'right');

	$params =   array( 	0 => $comments,
						1 => array('text' => _('Dimension'), 'from' => get_dimension_string($fromdim), 'to' => get_dimension_string($todim)),
						2 => array('text' => _('Year to date'), 'from' => $fiscal_start, 'to' => $as_of));

	$rep = new FrontReport(_('Dimension Summary'), 'DimensionSummary', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);

	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$res = getTransactions($fromdim, $todim);
	while ($trans=db_fetch($res)) {
		$rep->TextCol(0, 1, $trans['reference']);
		$rep->TextCol(1, 2, $trans['name']);
		$rep->TextCol(2, 3, $trans['type_']);
		$rep->DateCol(3, 4, $trans['date_'], true);
		$rep->DateCol(4, 5, $trans['due_date'], true);
		if ($trans['closed'])
			$str = _('Yes');
		else
			$str = _('No');
		$rep->TextCol(5, 6, $str);
		if ($showbal) {
			$balance = getYTD($trans['id'], $as_of);
			if ($balance === false) {
				display_error(_('The net GL balance could not be calculated.'));
				return;
			}
			$rep->AmountCol(6, 7, $balance, 0);
		}	
		$rep->NewLine(1, 2);
	}
	$rep->Line($rep->row);
	$rep->End();
}
