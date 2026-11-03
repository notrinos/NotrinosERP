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
require_once __DIR__.'/../../../dimensions/includes/dimension_master_metadata_writer.inc';

function add_account_type($id, $name, $class_id, $parent) {
	$sql = "INSERT INTO ".TB_PREF."chart_types (id, name, class_id, parent) VALUES (".db_escape($id).", ".db_escape($name).", ".db_escape($class_id).", ".db_escape($parent).")";

    return dimension_master_metadata_write('chart_types',null,array('id'=>$id),function($admitted)use($sql){return db_query($sql);},false,$id);
}

function update_account_type($id, $name, $class_id, $parent, $old_id) {
    $id=dimension_writer_master_id('chart_types',$id);$old_id=dimension_writer_master_id('chart_types',$old_id);if($id===false || $old_id===false)return false;
    $context=dimension_financial_reference_context();if(!$context || !begin_transaction())return false;
    try{
        $root=dimension_master_metadata_row('chart_types',$old_id);if(!$root){cancel_transaction();return false;}
        // Lock the target/gap before any rename; its current occupant cannot be overwritten.
        $target=db_query('SELECT id FROM '.TB_PREF.'chart_types WHERE id='.db_escape($id).' FOR UPDATE');if(!$target){cancel_transaction();return false;}
        $children=array();$accounts=array();$all=array($root);
        if($old_id!==$id){
            foreach(array('chart_types'=>'parent','chart_master'=>'account_type') as $owner=>$field){
                $primary=$owner==='chart_types'?'id':'account_code';$rows=db_query('SELECT * FROM '.TB_PREF.$owner.' WHERE '.$field.'='.db_escape($old_id).' ORDER BY '.$primary.' FOR UPDATE');if(!$rows){cancel_transaction();return false;}
                while($row=db_fetch_assoc($rows)){if(!array_key_exists('custom_data',$row)){cancel_transaction();return false;}$all[]=$row;if($owner==='chart_types')$children[]=$row;else $accounts[]=$row;}
            }
        }
        if(!dimension_master_metadata_lock_rows($all)){cancel_transaction();return false;}
        foreach(array('chart_types'=>$children,'chart_master'=>$accounts) as $owner=>$rows)foreach($rows as $row){$primary=$owner==='chart_types'?'id':'account_code';if(!dimension_ordinary_master_write($owner,$row[$primary],array(),function($admitted){return true;},true)){cancel_transaction();return false;}}
        $result=dimension_master_metadata_write('chart_types',$old_id,array(),function($admitted)use($id,$name,$class_id,$parent,$old_id,$children,$accounts){
            foreach($children as $row)if(!db_query('UPDATE '.TB_PREF.'chart_types SET parent='.db_escape($id).' WHERE id='.db_escape($row['id'])))return false;
            foreach($accounts as $row)if(!db_query('UPDATE '.TB_PREF.'chart_master SET account_type='.db_escape($id).' WHERE account_code='.db_escape($row['account_code'])))return false;
            if(!db_query('UPDATE '.TB_PREF.'chart_types SET id='.db_escape($id).',name='.db_escape($name).',class_id='.db_escape($class_id).',parent='.db_escape($parent).' WHERE id='.db_escape($old_id)))return false;
            foreach(array('chart_types'=>$children,'chart_master'=>$accounts) as $owner=>$rows)foreach($rows as $row){$primary=$owner==='chart_types'?'id':'account_code';$after=dimension_master_metadata_row($owner,$row[$primary]);if(!$after || $after['custom_data']!==$row['custom_data'])return false;}
            return true;
        },false,$id);
        if(!$result || dimension_financial_reference_context()!==$context || !commit_transaction()){cancel_transaction();return false;}return $result;
    }catch(Exception $e){cancel_transaction();return false;}catch(Throwable $e){cancel_transaction();return false;}
}

function get_account_types($all=false, $class_id=false, $parent=false) {
	$sql = "SELECT * FROM ".TB_PREF."chart_types";

	$where = array();
	
	if (!$all)
		$where[] = "!inactive";
	if ($class_id != false)  
		$where[] = "class_id=".db_escape($class_id);
	if ($parent == -1) 
		$where[] ="(parent = '' OR parent = '-1')";
	elseif ($parent !== false) 
		$where[] = "parent=".db_escape($parent);

	if (count($where))
		$sql .= ' WHERE '.implode(' AND ', $where);
	$sql .= " ORDER BY class_id, id, parent";

	return db_query($sql, 'could not get account types');
}

function get_account_type($id) {
	$sql = "SELECT * FROM ".TB_PREF."chart_types WHERE id = ".db_escape($id);

	$result = db_query($sql, 'could not get account type');

	return db_fetch($result);
}

function get_account_type_name($id) {
	$sql = "SELECT name FROM ".TB_PREF."chart_types WHERE id = ".db_escape($id);

	$result = db_query($sql, 'could not get account type');

	$row = db_fetch_row($result);
	return empty($row) ? '' : $row[0];
}

function delete_account_type($id) {
	$sql = "DELETE FROM ".TB_PREF."chart_types WHERE id = ".db_escape($id);

    return dimension_master_metadata_write('chart_types',$id,array(),function($admitted)use($sql){return db_query($sql,'could not delete account type');},true);
}

function add_account_class($id, $name, $ctype) {
	$sql = "INSERT INTO ".TB_PREF."chart_class (cid, class_name, ctype) VALUES (".db_escape($id).", ".db_escape($name).", ".db_escape($ctype).")";

    return dimension_master_metadata_write('chart_class',null,array('cid'=>$id),function($admitted)use($sql){return db_query($sql,'could not insert chart_class record');},false,$id);
}

function update_account_class($id, $name, $ctype) {
	$sql = "UPDATE ".TB_PREF."chart_class SET class_name=".db_escape($name).", ctype=".db_escape($ctype)." WHERE cid = ".db_escape($id);

    return dimension_master_metadata_write('chart_class',$id,array(),function($admitted)use($sql){return db_query($sql,'could not update account class');});
}

function get_account_classes($all=false, $balance=-1) {
	$sql = "SELECT * FROM ".TB_PREF."chart_class";

	if (!$all) 
		$sql .= " WHERE !inactive";
	if ($balance == 0) 
		$sql .= " AND ctype>".CL_EQUITY." OR ctype=0";
	elseif ($balance == 1) 
		$sql .= " AND ctype>0 AND ctype<".CL_INCOME; 
	$sql .= " ORDER BY ctype, cid";

	return db_query($sql, 'could not get account classes');
}

function get_account_class($id) {
	$sql = "SELECT * FROM ".TB_PREF."chart_class WHERE cid = ".db_escape($id);

	$result = db_query($sql, 'could not get account type');

	return db_fetch($result);
}

function get_account_class_name($id) {
	$sql = "SELECT class_name FROM ".TB_PREF."chart_class WHERE cid =".db_escape($id);

	$result = db_query($sql, 'could not get account type');

	$row = db_fetch_row($result);
	return $row[0];
}

function delete_account_class($id) {
	$sql = "DELETE FROM ".TB_PREF."chart_class WHERE cid = ".db_escape($id);

    return dimension_master_metadata_write('chart_class',$id,array(),function($admitted)use($sql){return db_query($sql,'could not delete account type');},true);
}
