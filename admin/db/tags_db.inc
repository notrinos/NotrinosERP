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

function add_tag($type, $name, $description)
{
	$sql = "INSERT INTO ".TB_PREF."tags (type, name, description)
		VALUES (".db_escape($type).", ".db_escape($name).", ".db_escape($description).")";

	return db_query($sql);
}

//--------------------------------------------------------------------------------------

function update_tag($id, $name, $description, $type=null)
{
	$sql = "UPDATE ".TB_PREF."tags SET name=".db_escape($name).", 
                                       description=".db_escape($description);
	if ($type != null)
		$sql .= ", type=".db_escape($type);

	$sql .= " WHERE id = ".db_escape($id);

	return db_query($sql);
}

//--------------------------------------------------------------------------------------

function get_tags($type, $all=false)
{
	$sql = "SELECT * FROM ".TB_PREF."tags WHERE type=".db_escape($type);
	
	if (!$all) $sql .= " AND !inactive";
	
	$sql .= " ORDER BY name";

	return db_query($sql, "could not get tags");
}

//--------------------------------------------------------------------------------------

function get_tag($id)
{
	$sql = "SELECT * FROM ".TB_PREF."tags WHERE id = ".db_escape($id);

	$result = db_query($sql, "could not get tag");

	return db_fetch($result);
}

//--------------------------------------------------------------------------------------

function get_tag_type($id)
{
	$sql = "SELECT type FROM ".TB_PREF."tags WHERE id = ".db_escape($id);

	$result = db_query($sql, "could not get tag type");

	$row = db_fetch_row($result);
	return $row[0];
}

//--------------------------------------------------------------------------------------

function get_tag_name($id)
{
	$sql = "SELECT name FROM ".TB_PREF."tags WHERE id = ".db_escape($id);

	$result = db_query($sql, "could not get tag name");

	$row = db_fetch_row($result);
	return $row[0];
}

//----------------------------------------------------------------------------------------------------
function get_tag_names($tags)
{
	$str = "";
	if ($tags == -1)
		return $str;
	foreach($tags as $id)
	{
		$tag = get_tag_name($id);
		if ($str == "")
			$str .= $tag;
		else	
			$str .= ", ".$tag;
	}
	return $str;
}
//--------------------------------------------------------------------------------------

function get_tag_description($id)
{
	$sql = "SELECT description FROM ".TB_PREF."tags WHERE id = ".db_escape($id);

	$result = db_query($sql, "could not get tag description");

	$row = db_fetch_row($result);
	return $row[0];
}

//--------------------------------------------------------------------------------------

function delete_tag($id)
{
	$sql = "DELETE FROM ".TB_PREF."tags WHERE id = ".db_escape($id);

	db_query($sql, "could not delete tag");
}

//--------------------------------------------------------------------------------------

function add_tag_associations($recordid, $tagids)
{
	foreach($tagids as $tagid) {
		if (!$tagid) continue;
		$sql = "INSERT INTO ".TB_PREF."tag_associations (record_id, tag_id)
			VALUES (".db_escape($recordid).", ".db_escape($tagid).")";

		db_query($sql, "could not add tag association");
	}
}

//--------------------------------------------------------------------------------------

function update_tag_associations($type, $recordid, $tagids)
{
	// Delete the old associations
	delete_tag_associations($type, $recordid, false);
	// Add the new associations
	add_tag_associations($recordid, $tagids);
}

//--------------------------------------------------------------------------------------
// To delete tag associations, we need to specify the tag type.
// Otherwise we may inadvertantly delete records for another type of tag
//
function delete_tag_associations($type, $recordid, $all=false)
{
/* multiply table DELETE syntax available since MySQL 4.0.0:
	$sql = "DELETE ta FROM ".TB_PREF."tag_associations ta 
				INNER JOIN ".TB_PREF."tags tags ON tags.id = ta.tag_id 
				WHERE tags.type = ".db_escape($type)." AND ta.record_id = ".db_escape($recordid);
*/
	// To support MySQL 3.xx we have to use multiply queries
	$sql = "SELECT * FROM ".TB_PREF."tag_associations ta 
			INNER JOIN ".TB_PREF."tags tags ON tags.id = ta.tag_id 
			WHERE tags.type = ".db_escape($type)." AND ta.record_id = ".db_escape($recordid);
	if (!$all)
		$sql .= " AND tags.inactive = 0";
	$result = db_query($sql, "could not select tag associations");

	while($ta = db_fetch($result)) {
		$sql2 = "DELETE FROM ".TB_PREF."tag_associations WHERE 
			record_id = '".$ta['record_id']."' AND tag_id=".$ta['tag_id'];
		db_query($sql2, "could not delete tag associations");
	}
}

//--------------------------------------------------------------------------------------

function get_records_associated_with_tag($id)
{
	// Which table we query is based on the tag type
	$type = get_tag_type($id);
	
	$table = $key = '';
	switch ($type) {
		case TAG_ACCOUNT:
			$table = TB_PREF."chart_master";
			$key = "account_code";
			break;
		case TAG_DIMENSION:
			$table = TB_PREF."dimensions";
			$key = "id";
			break;
	}
	
	$sql = "SELECT $table.* FROM $table 
		INNER JOIN ".TB_PREF."tag_associations AS ta ON ta.record_id = $table.$key
		INNER JOIN ".TB_PREF."tags AS tags ON ta.tag_id = tags.id
	        WHERE tags.id = ".db_escape($id);

	return db_query($sql, "could not get tag associations for tag");
}

//--------------------------------------------------------------------------------------

function get_tags_associated_with_record($type, $recordid)
{
	$sql = "SELECT tags.* FROM ".TB_PREF."tag_associations AS ta 
				INNER JOIN ".TB_PREF."tags AS tags ON tags.id = ta.tag_id 
				WHERE tags.type = $type	AND ta.record_id = ".db_escape($recordid);

	return db_query($sql, "could not get tags associations for record");
}

//--------------------------------------------------------------------------------------

function is_record_in_tags($tags, $type, $recordid)
{
	foreach($tags as $id)
	{
		$sql = "SELECT ta.record_id FROM ".TB_PREF."tag_associations AS ta 
				INNER JOIN ".TB_PREF."tags AS tags ON tags.id = ta.tag_id 
				WHERE tags.type = $type AND tags.id = $id AND ta.record_id = ".db_escape($recordid);
		$res = db_query($sql, "could not get tags associations for record");
		if (db_num_rows($res) == 0)
			return false;
	}
	return true;
}

