<?php
/** HRM-FND-007 immutable Typed Attribute definition lifecycle maker-request writer. */
if (!function_exists('hrm_typed_attribute_definition_lifecycle_canonical_command'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_definition_lifecycle_command_db.inc');
if (!function_exists('hrm_person_worker_actor_id'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_worker_db.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

function hrm_typed_attribute_definition_lifecycle_transition_allowed($status,$action)
{
    if ($action==='deprecate') return $status==='active';
    if ($action==='retire') return $status==='active' || $status==='deprecated';
    if ($action==='reactivate') return $status==='deprecated';
    return false;
}

function hrm_typed_attribute_definition_lifecycle_copy_fields_match($candidate,$target)
{
    if (!is_array($candidate)||!is_array($target)) return false;
    foreach(array('definition_id','value_type','allowed_subject','sensitivity','owner_scope','validation_json') as $field)
        if ((string)$candidate[$field] !== (string)$target[$field]) return false;
    return ($candidate['effective_to']===null?null:(string)$candidate['effective_to'])
        === ($target['effective_to']===null?null:(string)$target['effective_to']);
}

function hrm_typed_attribute_definition_lifecycle_request_matches($candidate,$target,$command,$maker_id)
{
    $pending=hrm_typed_attribute_definition_lifecycle_pending_status($command['action']);
    return $pending!==false && is_array($candidate) && is_array($target)
        && (int)$candidate['created_by']===(int)$maker_id
        && (int)$candidate['predecessor_id']===(int)$target['definition_version_id']
        && (string)$candidate['definition_status']===$pending
        && (string)$candidate['effective_from']===$command['effective_from']
        && hrm_typed_attribute_definition_lifecycle_copy_fields_match($candidate,$target);
}

/** Append one pending immutable lifecycle successor; exact maker retry is idempotent. */
function hrm_typed_attribute_definition_lifecycle_request($input)
{
    $command=hrm_typed_attribute_definition_lifecycle_canonical_command($input);
    $maker_id=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if($command===false||$maker_id<=0||!hrm_typed_attribute_definition_lifecycle_user_can_manage())return false;
    $digest=hrm_typed_attribute_definition_lifecycle_command_digest($command);if($digest===false)return false;

    begin_transaction();$ok=true;$return_id=false;$definition_id=0;$rows=array();$successor=array();$leaf_ids=array();$max_version=0;
    $lookup=db_query('SELECT definition_id FROM '.TB_PREF.'hrm_attribute_definition_versions WHERE definition_version_id='.(int)$command['definition_version_id'].' LIMIT 2', 'could not locate Typed Attribute lifecycle target');
    if(!$lookup||db_num_rows($lookup)!==1)$ok=false;else{$z=db_fetch_assoc($lookup);$definition_id=(int)$z['definition_id'];}
    if($ok){$d=db_query('SELECT definition_id,attribute_code FROM '.TB_PREF.'hrm_attribute_definitions WHERE definition_id='.(int)$definition_id.' LIMIT 1 FOR UPDATE','could not lock Typed Attribute lifecycle definition');if(!$d||db_num_rows($d)!==1)$ok=false;}
    if($ok){
        $r=db_query('SELECT definition_version_id,definition_id,version_no,predecessor_id,value_type,allowed_subject,sensitivity,owner_scope,validation_json,definition_status,effective_from,effective_to,created_by FROM '.TB_PREF.'hrm_attribute_definition_versions WHERE definition_id='.(int)$definition_id.' ORDER BY version_no,definition_version_id FOR UPDATE','could not lock Typed Attribute lifecycle chain');
        if(!$r)$ok=false;
        while($ok&&($row=db_fetch_assoc($r))){$id=(int)$row['definition_version_id'];$rows[$id]=$row;$max_version=max($max_version,(int)$row['version_no']);if($row['predecessor_id']!==null){$pred=(int)$row['predecessor_id'];if(isset($successor[$pred])){$ok=false;break;}$successor[$pred]=$id;}}
        foreach($rows as $id=>$row)if(!isset($successor[$id]))$leaf_ids[]=$id;
        if(count($leaf_ids)!==1||!isset($rows[$command['definition_version_id']]))$ok=false;
    }
    if($ok){
        $target=$rows[$command['definition_version_id']];
        if(isset($successor[$command['definition_version_id']])){
            $candidate=$rows[$successor[$command['definition_version_id']]];
            if((int)$leaf_ids[0]===(int)$candidate['definition_version_id']&&hrm_typed_attribute_definition_lifecycle_request_matches($candidate,$target,$command,$maker_id))$return_id=(int)$candidate['definition_version_id'];else$ok=false;
        }elseif((int)$leaf_ids[0]!==$command['definition_version_id']
            ||!hrm_typed_attribute_definition_lifecycle_transition_allowed((string)$target['definition_status'],$command['action'])
            ||strcmp($command['effective_from'],(string)$target['effective_from'])<=0
            ||($target['effective_to']!==null&&strcmp($command['effective_from'],(string)$target['effective_to'])>0)){$ok=false;}
    }
    if($ok&&$return_id===false){
        $target=$rows[$command['definition_version_id']];$version_no=$max_version+1;$pending=hrm_typed_attribute_definition_lifecycle_pending_status($command['action']);
        $sql='INSERT INTO '.TB_PREF.'hrm_attribute_definition_versions (definition_id,version_no,predecessor_id,value_type,allowed_subject,sensitivity,owner_scope,validation_json,definition_status,effective_from,effective_to,created_by) VALUES ('
            .(int)$definition_id.','.(int)$version_no.','.(int)$command['definition_version_id'].','.db_escape($target['value_type']).','.db_escape($target['allowed_subject']).','.db_escape($target['sensitivity']).','.db_escape($target['owner_scope']).','.db_escape($target['validation_json']).','.db_escape($pending).','.db_escape($command['effective_from']).','.($target['effective_to']===null?'NULL':db_escape($target['effective_to'])).','.(int)$maker_id.')';
        $ok=db_query($sql,'could not append Typed Attribute lifecycle request')!==false;
        if($ok){$return_id=(int)db_insert_id();$receipt=append_domain_audit_event(array(
            'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,'entity_type'=>'hrm_typed_attribute_definition',
            'entity_key'=>array('definition_id'=>$definition_id,'definition_version_id'=>$return_id,'predecessor_id'=>(int)$command['definition_version_id'],'version_no'=>$version_no,'command_digest'=>$digest),
            'action'=>'hrm.typed_attribute.definition_lifecycle_requested','reason_code'=>'hrm_fnd_007_typed_attribute_definition_lifecycle_writer',
            'before'=>array('definition_version_id'=>(int)$command['definition_version_id'],'definition_status'=>(string)$target['definition_status']),
            'after'=>array('definition_version_id'=>$return_id,'definition_status'=>$pending,'effective_from'=>$command['effective_from']),
            'restricted_diff'=>array('immutable_successor'=>true,'requested_action'=>$command['action'],'metadata_copied'=>true,'attribute_value_rows_mutated'=>0,'legacy_custom_data_mapped'=>false,'payroll_consumer_activated'=>false)));
            if($receipt===false){$ok=false;$return_id=false;}
        }
    }
    if($ok)commit_transaction();else cancel_transaction();return $ok?$return_id:false;
}
