<?php
/** HRM-FND-007 distinct-checker immutable Typed Attribute definition lifecycle application. */
if (!function_exists('hrm_typed_attribute_definition_lifecycle_request'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_definition_lifecycle_writer_db.inc');

function hrm_typed_attribute_definition_lifecycle_applied_successor_matches($candidate,$pending,$checker_id,$final_status)
{
    return is_array($candidate)&&is_array($pending)
        &&(int)$candidate['created_by']===(int)$checker_id
        &&(int)$candidate['predecessor_id']===(int)$pending['definition_version_id']
        &&(string)$candidate['definition_status']===$final_status
        &&(string)$candidate['effective_from']===(string)$pending['effective_from']
        &&hrm_typed_attribute_definition_lifecycle_copy_fields_match($candidate,$pending);
}

/**
 * Apply one maker request with a distinct lifecycle checker. Retired is terminal;
 * only deprecated definitions can later enter an explicit reactivation request.
 */
function hrm_typed_attribute_definition_lifecycle_apply($pending_version_id)
{
    $pending_version_id=hrm_typed_attribute_definition_positive_id($pending_version_id,false);
    $checker_id=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if($pending_version_id===false||$checker_id<=0||!hrm_typed_attribute_definition_lifecycle_user_can_approve())return false;
    begin_transaction();$ok=true;$return_id=false;$definition_id=0;$rows=array();$successor=array();$leaf_ids=array();$max_version=0;
    $lookup=db_query('SELECT definition_id FROM '.TB_PREF.'hrm_attribute_definition_versions WHERE definition_version_id='.(int)$pending_version_id.' LIMIT 2','could not locate Typed Attribute lifecycle request');
    if(!$lookup||db_num_rows($lookup)!==1)$ok=false;else{$z=db_fetch_assoc($lookup);$definition_id=(int)$z['definition_id'];}
    if($ok){$d=db_query('SELECT definition_id,attribute_code FROM '.TB_PREF.'hrm_attribute_definitions WHERE definition_id='.(int)$definition_id.' LIMIT 1 FOR UPDATE','could not lock Typed Attribute lifecycle approval definition');if(!$d||db_num_rows($d)!==1)$ok=false;}
    if($ok){$r=db_query('SELECT definition_version_id,definition_id,version_no,predecessor_id,value_type,allowed_subject,sensitivity,owner_scope,validation_json,definition_status,effective_from,effective_to,created_by FROM '.TB_PREF.'hrm_attribute_definition_versions WHERE definition_id='.(int)$definition_id.' ORDER BY version_no,definition_version_id FOR UPDATE','could not lock Typed Attribute lifecycle approval chain');if(!$r)$ok=false;while($ok&&($row=db_fetch_assoc($r))){$id=(int)$row['definition_version_id'];$rows[$id]=$row;$max_version=max($max_version,(int)$row['version_no']);if($row['predecessor_id']!==null){$pred=(int)$row['predecessor_id'];if(isset($successor[$pred])){$ok=false;break;}$successor[$pred]=$id;}}foreach($rows as $id=>$row)if(!isset($successor[$id]))$leaf_ids[]=$id;if(count($leaf_ids)!==1||!isset($rows[$pending_version_id]))$ok=false;}
    if($ok){$pending=$rows[$pending_version_id];$action=hrm_typed_attribute_definition_lifecycle_action_from_pending_status((string)$pending['definition_status']);$final=$action===false?false:hrm_typed_attribute_definition_lifecycle_final_status($action);if($action===false||$final===false||(int)$pending['created_by']===(int)$checker_id)$ok=false;}
    if($ok){
        if(isset($successor[$pending_version_id])){$candidate=$rows[$successor[$pending_version_id]];if((int)$leaf_ids[0]===(int)$candidate['definition_version_id']&&hrm_typed_attribute_definition_lifecycle_applied_successor_matches($candidate,$pending,$checker_id,$final))$return_id=(int)$candidate['definition_version_id'];else$ok=false;}
        elseif((int)$leaf_ids[0]!==$pending_version_id)$ok=false;
    }
    if($ok&&$return_id===false){
        $version_no=$max_version+1;$sql='INSERT INTO '.TB_PREF.'hrm_attribute_definition_versions (definition_id,version_no,predecessor_id,value_type,allowed_subject,sensitivity,owner_scope,validation_json,definition_status,effective_from,effective_to,created_by) VALUES ('
            .(int)$definition_id.','.(int)$version_no.','.(int)$pending_version_id.','.db_escape($pending['value_type']).','.db_escape($pending['allowed_subject']).','.db_escape($pending['sensitivity']).','.db_escape($pending['owner_scope']).','.db_escape($pending['validation_json']).','.db_escape($final).','.db_escape($pending['effective_from']).','.($pending['effective_to']===null?'NULL':db_escape($pending['effective_to'])).','.(int)$checker_id.')';
        $ok=db_query($sql,'could not append Typed Attribute lifecycle applied successor')!==false;
        if($ok){$return_id=(int)db_insert_id();$receipt=append_domain_audit_event(array(
            'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,'entity_type'=>'hrm_typed_attribute_definition',
            'entity_key'=>array('definition_id'=>$definition_id,'definition_version_id'=>$return_id,'predecessor_id'=>(int)$pending_version_id,'version_no'=>$version_no),
            'action'=>'hrm.typed_attribute.definition_lifecycle_applied','reason_code'=>'hrm_fnd_007_typed_attribute_definition_lifecycle_apply',
            'before'=>array('definition_version_id'=>(int)$pending_version_id,'definition_status'=>(string)$pending['definition_status']),
            'after'=>array('definition_version_id'=>$return_id,'definition_status'=>$final,'effective_from'=>(string)$pending['effective_from']),
            'restricted_diff'=>array('immutable_successor'=>true,'applied_action'=>$action,'checker_distinct_from_maker'=>true,'metadata_copied'=>true,'retirement_terminal'=>$final==='retired','attribute_value_rows_mutated'=>0,'legacy_custom_data_mapped'=>false,'payroll_consumer_activated'=>false)));
            if($receipt===false){$ok=false;$return_id=false;}
        }
    }
    if($ok)commit_transaction();else cancel_transaction();return $ok?$return_id:false;
}
