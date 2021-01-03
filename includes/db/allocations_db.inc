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
//
// Functions below are unified interface to allocations (should supersede currently separate cust/supp allocations in a future)
//
//----------------------------------------------------------------------------------------
//
//	Returns table of payments currently allocated to selected transaction.
//
function get_payments_for($trans_no, $trans_type, $person_id)
{
	$allocations = array();

	$sql = "(SELECT alloc.*, trans.tran_date FROM ".TB_PREF."cust_allocations alloc
				LEFT JOIN ".TB_PREF."debtor_trans trans ON trans.type=alloc.trans_type_from AND trans.trans_no=alloc.trans_no_from
			WHERE (trans_type_to=".db_escape($trans_type)." AND trans_no_to=".db_escape($trans_no)." AND person_id=".db_escape($person_id)."))
		UNION
			(SELECT alloc.*, trans.tran_date FROM ".TB_PREF."supp_allocations alloc
				LEFT JOIN ".TB_PREF."supp_trans trans ON trans.type=alloc.trans_type_from AND trans.trans_no=alloc.trans_no_from
			WHERE (trans_type_to=".db_escape($trans_type)." AND trans_no_to=".db_escape($trans_no)." AND person_id=".db_escape($person_id)."))";
	$result = db_query($sql, "error reading current allocations");

	while($dat = db_fetch($result))
	{
		$allocations[] = $dat;
	}

	return $allocations;
}

//
//	Unified inteface to (re)allocate payments to supp/cust/transaction
//
function allocate_payment($type_from, $no_from, $type_to, $no_to, $person_id, $amount, $date)
{
	$date = date2sql($date); // FIXME types
	if (in_array($type_to, array(ST_SALESORDER, ST_CUSTCREDIT, ST_CUSTDELIVERY, ST_SALESINVOICE)))
		$allocations = 'cust_allocations';
	else
		$allocations = 'supp_allocations';

	$sql = "DELETE FROM ".TB_PREF.$allocations."
		WHERE trans_type_from=".db_escape($type_from)." AND trans_no_from=".db_escape($no_from)
	   	." AND trans_type_to=".db_escape($type_to)." AND trans_no_to=".db_escape($no_to)." AND person_id=".db_escape($person_id);
	db_query($sql, "The existing allocations could not be updated");
	if (floatcmp($amount, 0) != 0)
	{
		$sql = "INSERT INTO ".TB_PREF.$allocations." (amt, date_alloc, trans_type_from, trans_no_from, trans_type_to, trans_no_to, person_id)
		VALUES (". db_escape($amount).",'$date',".db_escape($type_from).",".db_escape($no_from)
		   	.",".db_escape($type_to).",".db_escape($no_to).",".db_escape($person_id).")";
		db_query($sql, "The existing allocations could not be updated");
	}
	if ($allocations == 'cust_allocations') {
		update_debtor_trans_allocation($type_from, $no_from, $person_id);
		update_debtor_trans_allocation($type_to, $no_to, $person_id);
	} else {
		update_supp_trans_allocation($type_from, $no_from, $person_id);
		update_supp_trans_allocation($type_to, $no_to, $person_id);
	}
}

//----------------------------------------------------------------------------------------
//
//	Reallocates allocations to selected transaction.
//  $allocs is table of payments which should be reallocated
//  $free_remainder should be true if old allacations should be freed when not allocated to new transaction.
//
function reallocate_payments($trans_no, $trans_type, $alloc_date, $max_alloc, $allocs, $person_id, $free_remainder=true)
{

	foreach($allocs as $alloc)
	{
		$amount = min($alloc['amt'], $max_alloc);
		$remainder = $alloc['amt'] - $amount;

		$max_alloc = floatcmp($max_alloc, $amount) > 0 ? $max_alloc-$amount : 0;

		$same_to =  $trans_type == $alloc['trans_type_to'] && $trans_no == $alloc['trans_no_to'];

		allocate_payment($alloc['trans_type_from'], $alloc['trans_no_from'], $trans_type, $trans_no, $person_id, $amount, $alloc_date);

		if (!$same_to && ($remainder > 0 || $free_remainder))
		{
			allocate_payment($alloc['trans_type_from'], $alloc['trans_no_from'], 
				$alloc['trans_type_to'], $alloc['trans_no_to'], $person_id, $free_remainder ? 0 : $remainder, $alloc_date);
		}
		if (!$free_remainder && $max_alloc==0)
			break;
	}
}
