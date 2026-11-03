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
 * Entity class for sales types.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * ::remove() overrides the base to also delete from the prices table (multi-table transaction).
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class sales_types_entity extends Entity {
	function __construct() {
		parent::__construct(
			'sales_types',
			array('sales_type', 'tax_included', 'factor', 'id', 'inactive'),
			'id'
		);
	}

	/**
	 * Delete a sales type and its associated prices.
	 *
	 * @param mixed $key Primary key value (id)
	 * @return bool True on success
	 */
	public static function remove($key) {
        require_once __DIR__.'/../../../dimensions/includes/dimension_master_metadata_writer.inc';
        $context=dimension_financial_reference_context();if(!$context || !begin_transaction())return false;
        try{
            $parent=dimension_master_metadata_row('sales_types',$key);if(!$parent){cancel_transaction();return false;}$key=$parent['id'];
            $result=db_query('SELECT id,custom_data FROM '.TB_PREF.'prices WHERE sales_type_id='.db_escape($key).' ORDER BY id FOR UPDATE');if(!$result){cancel_transaction();return false;}
            $prices=array();$all=array($parent);while($row=db_fetch_assoc($result)){$prices[]=$row;$all[]=$row;}
            if(!dimension_master_metadata_lock_rows($all)){cancel_transaction();return false;}
            foreach($prices as $row)if(!dimension_ordinary_master_write('prices',$row['id'],array(),function($admitted){return true;},true,true)){cancel_transaction();return false;}
            $entity=new static();$saved=$entity->delete($key);
            if($saved)$saved=db_query('DELETE FROM '.TB_PREF.'prices WHERE sales_type_id='.db_escape($key),'The Sales type prices could not be deleted')!==false;
            if($saved){$remaining=db_query('SELECT id FROM '.TB_PREF.'prices WHERE sales_type_id='.db_escape($key).' FOR UPDATE');$saved=$remaining && db_num_rows($remaining)===0;}
            if($saved && dimension_financial_reference_context()===$context && commit_transaction())return true;
            cancel_transaction();return false;
        }catch(Exception $e){cancel_transaction();return false;}catch(Throwable $e){cancel_transaction();return false;}
    }
}
