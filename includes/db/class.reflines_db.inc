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
include_once 'class.data_set.inc';

/**
*	Reference lines.
*
**/

class reflines_db extends data_set {
	function __construct()
	{
		$this->set_validator('prefix:ui:_check_prefix', _("This prefix conflicts with another one already defined. Prefix have to be unambigous."));
		$this->set_validator('prefix:ui:_check_template', _("Invalid template format."));
		$this->set_validator('trans_type:ui:required', _("Transaction type cannot be empty."));
		$this->set_validator('pattern:ui:required', _("Next reference cannot be empty."));
		parent::__construct('reflines', 
			array('trans_type', 'prefix', 'description', 'default', 'pattern', 'id', 'inactive'), 
			'id');
	}

	/*
		Prefix cannot be ambigous.
	*/
	function _check_prefix($data, $dummy_opt, $key)
	{
		$cond = "`id`<>".db_escape($key)." AND `trans_type`=".db_escape($data['trans_type']);
		if ($data['prefix'] === '')
			$cond .= " AND `prefix`='".$data['prefix']."'";
		else
			$cond .= "AND ((LOCATE('".$data['prefix']."', CONCAT(`prefix`,`pattern`))=1 OR (`prefix`<>'' AND LOCATE(`prefix`, '".$data['prefix'].$data['pattern']."')=1)))";

		return db_num_rows($this->get_all($cond)) == 0;
	}

	function _check_template($data, $dummy_opt, $key)
	{
		global $refline_options, $refline_placeholders;

		if (strpbrk($data['prefix'], '{}') !== false)
			return $this->error(_("You cannot use placeholders in refline prefix."));

		if (substr_count($data['pattern'], '{') != substr_count($data['pattern'], '}'))
			return $this->error(_("Curly brackets does not balance."));

		if (preg_match_all('/\{([^\}]*)\}/', $data['pattern'], $match)) // placeholders defind in template
		{
			$numph = 0;
			foreach($match[1] as $ph) {
				if (is_numeric($ph))
					$numph++;
				elseif (!isset($refline_placeholders[$ph]) || !@in_array($refline_placeholders[$ph], $refline_options[$data['trans_type']])) {
					$allowed = array();
					foreach($refline_placeholders as $id => $dt)
						 if (in_array($dt, $refline_options[$data['trans_type']]))
						 	$allowed[] = $id;

					return $this->error(sprintf(_("Invalid placeholder '%s'. Placeholders allowed for this transaction type are: %s."),
						$ph, implode(',', $allowed)));
				}
			}

			if ($numph !== 1)
				return $this->error(_("Missing numeric placeholder. If you want to use template based references, you have to define numeric placeholder too."));
		}
		return true;
	}

	function is_used($prefix, $trans_type)
	{

		$sql = "SELECT *
			FROM (SELECT r.* FROM ".TB_PREF."refs r 
				LEFT JOIN ".TB_PREF."voided as v
					ON r.id=v.id AND r.type=v.type
				WHERE r.type=".db_escape($trans_type)." AND ISNULL(v.id)
				) ref
			LEFT JOIN ".TB_PREF."reflines line ON ref.type=line.trans_type AND substr(ref.reference,1, LENGTH(line.prefix))= line.prefix AND line.prefix<>''
			WHERE ".($prefix == '' ? "ISNULL(prefix)" : "prefix=".db_escape($prefix));

		$res = db_query($sql, "cannot check reference line");

		return db_num_rows($res);
	}

	function delete_check($ref_id)
	{
		$rec = $this->get($ref_id);
		if ($rec['default'])
			return $this->error(_("Reference line which is default for any transaction type cannot be deleted."));

		if ($this->is_used($rec['prefix'], $rec['trans_type']))
			return $this->error(_("Reference line cannot be deleted because it is already in use."));

		return true;
	}

	function _set_as_default($id, $type)
	{
		$sql  = "UPDATE ".TB_PREF."reflines SET `default`=(`id`=".db_escape($id).")
			 WHERE `trans_type`=".db_escape($type);
		return db_query($sql, "cannot update default refline");
	}

	function insert($data)
	{
		if (!parent::insert($data))
			return false;
		if (@$data['default'])
			return $this->_set_as_default(db_insert_id(), $data['trans_type']);
		return true;
	}

	function update($key, $data)
	{
		if (!parent::update($key, $data))
			return false;
		if (@$data['default'])
			return $this->_set_as_default($key , $data['trans_type']);
		return true;
	}

	function get_default($type)
	{
		$sql = "SELECT * FROM ".TB_PREF."reflines WHERE trans_type=".db_escape($type)." AND `default`";
		$result = db_query($sql, "cannot retreive default refline for trnasaction type $type");
		return db_fetch($result);
	}

	function count($type, $all=false)
	{
		$sql = "SELECT count(*) FROM ".TB_PREF."reflines WHERE trans_type=".db_escape($type);

		if (!$all)
			$sql .= " AND !inactive";
		$result = db_query($sql, "cannot retreive refline count for transaction type $type");
		$rec = db_fetch($result);
		return $rec ? $rec[0] : 0;
	}

	/*
		Recognize refline by reference prefix
	*/
	function find_refline_id($reference, $type, $fallback=true)
	{
		$sql = "SELECT * FROM ".TB_PREF."reflines WHERE trans_type=".db_escape($type)
			." AND CHAR_LENGTH(`prefix`) AND LEFT(".db_escape($reference).", CHAR_LENGTH(`prefix`)) = `prefix`";
		if ($fallback)	// if not found return refline with empty prefix
			$sql .= " UNION SELECT * FROM ".TB_PREF."reflines WHERE trans_type=".db_escape($type)." AND `prefix`=''";
		$ret = db_query($sql, "cannot check reference line id");
		$line = db_fetch($ret);

		if (!$fallback && (db_num_rows($fallback) != 1))	// more than one record means ambigous reference line
			return null;

		return $line ? $line['id'] : null;
	}

	function save_next($type, $reference, $line=null)
	{
	    $sql = "UPDATE ".TB_PREF."reflines SET pattern=SUBSTRING(" . db_escape(trim($reference)) .", LENGTH(`prefix`)+1)"
		. " WHERE trans_type = ".db_escape($type)." AND ";

		if (isset($line))
			$sql .= "`id`=".db_escape($line);
		else
			$sql .= "`default`";

		return db_query($sql, "The next transaction ref for $type could not be updated");
	}
}
