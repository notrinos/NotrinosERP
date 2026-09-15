<?php
/** HRM-FND-007 immutable maker draft for explicit legacy custom_data mapping change-control. */
if(!function_exists('hrm_typed_attribute_legacy_mapping_canonical_command'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_mapping_command_db.inc');
if(!function_exists('hrm_person_worker_actor_id'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_worker_db.inc');
if(!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

function hrm_typed_attribute_legacy_mapping_draft_matches($row,$command,$actor_id)
{
    return is_array($row)&&is_array($command)&&(int)$row['created_by']===(int)$actor_id
        &&(string)$row['legacy_key']===$command['legacy_key']&&(string)$row['attribute_code']===$command['attribute_code']
        &&(string)$row['subject_type']==='worker'&&(string)$row['mapping_status']==='draft'
        &&(string)$row['effective_from']===$command['effective_from']
        &&(($row['effective_to']===null?null:(string)$row['effective_to'])===$command['effective_to'])
        &&(string)$row['qualified_review_ref']===$command['qualified_review_ref']
        &&(($row['predecessor_id']===null?null:(int)$row['predecessor_id'])===$command['predecessor_id'])
        &&$row['approved_at']===null&&$row['approved_by']===null;
}

function hrm_typed_attribute_legacy_mapping_append_draft($input)
{
    $command=hrm_typed_attribute_legacy_mapping_canonical_command($input);
    $actor_id=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if($command===false||$actor_id<=0||!hrm_typed_attribute_legacy_mapping_user_can_manage())return false;
    begin_transaction();$ok=true;$return_id=false;$rows=array();$successor=array();$leaf_ids=array();$max_version=0;
    $definition=hrm_typed_attribute_definition_resolve_state($command['attribute_code'],$command['effective_from'].' 00:00:00',true);
    if(!is_array($definition)||$definition['definition_status']!=='active'||$definition['allowed_subject']!=='worker'
        ||($definition['effective_to']!==null&&$command['effective_from']>(string)$definition['effective_to']))$ok=false;
    $r=db_query('SELECT mapping_version_id,legacy_key,version_no,predecessor_id,attribute_code,subject_type,mapping_status,'
        .'effective_from,effective_to,qualified_review_ref,approved_at,approved_by,created_by FROM '.TB_PREF
        .'hrm_attribute_legacy_mapping_versions WHERE legacy_key='.db_escape($command['legacy_key'])
        .' ORDER BY version_no,mapping_version_id FOR UPDATE','could not lock legacy Typed Attribute mapping chain');
    if(!$r)$ok=false;
    while($ok&&($row=db_fetch_assoc($r))){$id=(int)$row['mapping_version_id'];$rows[$id]=$row;$max_version=max($max_version,(int)$row['version_no']);
        if($row['predecessor_id']!==null){$pred=(int)$row['predecessor_id'];if(isset($successor[$pred])||!isset($rows[$pred])){$ok=false;break;}$successor[$pred]=$id;}}
    if($ok){foreach($rows as $id=>$row)if(!isset($successor[$id]))$leaf_ids[]=$id;if(count($leaf_ids)>1)$ok=false;}
    if($ok&&count($leaf_ids)===1){$leaf=$rows[$leaf_ids[0]];
        if(hrm_typed_attribute_legacy_mapping_draft_matches($leaf,$command,$actor_id))$return_id=(int)$leaf['mapping_version_id'];
        elseif($command['predecessor_id']!==(int)$leaf['mapping_version_id']||(string)$leaf['mapping_status']!=='approved'
            ||$leaf['effective_to']===null||strcmp($command['effective_from'],(string)$leaf['effective_to'])<=0)$ok=false;
    }elseif($ok&&count($leaf_ids)===0&&$command['predecessor_id']!==null)$ok=false;
    if($ok&&$return_id===false){$version_no=$max_version+1;
        $sql='INSERT INTO '.TB_PREF.'hrm_attribute_legacy_mapping_versions '
            .'(legacy_key,version_no,predecessor_id,attribute_code,subject_type,mapping_status,effective_from,effective_to,qualified_review_ref,approved_at,approved_by,created_by) VALUES ('
            .db_escape($command['legacy_key']).','.(int)$version_no.','.($command['predecessor_id']===null?'NULL':(int)$command['predecessor_id']).','
            .db_escape($command['attribute_code']).",'worker','draft',".db_escape($command['effective_from']).','
            .($command['effective_to']===null?'NULL':db_escape($command['effective_to'])).','.db_escape($command['qualified_review_ref']).',NULL,NULL,'.(int)$actor_id.')';
        $ok=db_query($sql,'could not append legacy Typed Attribute mapping draft')!==false;
        if($ok){$return_id=(int)db_insert_id();$receipt=append_domain_audit_event(array(
            'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,'entity_type'=>'hrm_typed_attribute_legacy_mapping',
            'entity_key'=>array('mapping_version_id'=>$return_id,'legacy_key'=>$command['legacy_key'],'version_no'=>$version_no),
            'action'=>'hrm.typed_attribute.legacy_mapping_draft_created','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_mapping_draft',
            'before'=>array('predecessor_id'=>$command['predecessor_id']),'after'=>array('mapping_version_id'=>$return_id,'mapping_status'=>'draft'),
            'restricted_diff'=>array('worker_subject_only'=>true,'qualified_review_ref_present'=>true,'raw_legacy_value_read'=>false,
                'typed_attribute_value_rows_written'=>0,'checker_approval_required'=>true,'immutable_successor'=>true)));
            if($receipt===false){$ok=false;$return_id=false;}}
    }
    if($ok)commit_transaction();else cancel_transaction();return $ok?$return_id:false;
}
