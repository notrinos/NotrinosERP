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
include_once($path_to_root . '/includes/ui.inc');

//--------------------------------------------------------------------------------------

/**
 * DIM-LFC-002: name the historical state of a Dimension in its read-only view.
 *
 * A closed master and a master whose identity was legitimately deleted both remain explainable from
 * their stored GL/budget IDs. This only renders state; it never reopens, edits or authorizes posting.
 *
 * @param array|false $row Master row, or false for a retained/deleted identity
 * @param int|string $id Exact native Dimension identity
 * @return void
 */
function display_dimension_history_state($row, $id) {
	if (!is_array($row))
		display_note(sprintf(_('Unknown Dimension #%d (deleted). Its historical GL and budget rows remain readable by the original ID.'), (int)$id));
	elseif ((int)$row['closed'] === 1)
		display_note(_('This dimension is closed. Its historical classifications remain readable; no new posting accepts it.'));
}

//--------------------------------------------------------------------------------------

function display_dimension_balance($id, $from, $to) {
	global $path_to_root;

	$result = get_dimension_balance_all($id, $from, $to);

	if (db_num_rows($result) == 0)
		display_note(_('There are no transactions for this dimension for the selected period.'));
	else {
		display_heading2(_('Balance for this Dimension'));
		br();
		start_table(TABLESTYLE);
		$th = array(_('Account'), _('Debit'), _('Credit'));
		table_header($th);

		$total = $k = 0;
		while ($myrow = db_fetch($result)) {
			alt_table_row_color($k);

			label_cell($myrow['account'].' '.$myrow['account_name']);
			display_debit_or_credit_cells($myrow['amt']);
			$total += $myrow['amt'];
			end_row();
		}
		start_row();
		label_cell('<b>'._('Balance').'</b>');
		if ($total >= 0) {
			amount_cell($total, true);
			label_cell('');
		}
		else {
			label_cell('');
			amount_cell(abs($total), true);
		}
		end_row();

		end_table();
	}
}


