<?php
/** HRM-FND-007 distinct-checker approval of one immutable legacy custom_data mapping draft. */
if(!function_exists('hrm_typed_attribute_legacy_mapping_append_draft'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_mapping_writer_db.inc');

function hrm_typed_attribute_legacy_mapping_approved_successor_matches($candidate,$target,$checker_id)
{
    if(!is_array($candidate)||!is_array($target)||(int)$candidate['approved_by']!==(int)$checker_id
        ||(int)$candidate['created_by']!==(int)$checker_id||(int)$candidate['predecessor_id']!==(int)$target['mapping_version_id']
        ||(string)$candidate['mapping_status']!=='approved')return false;
    foreach(array('legacy_key','attribute_code','subject_type','effective_from','qualified_review_ref') as $f)
        if((string)$candidate[$f]!== (string)$target[$f])return false;
    return ($candidate['effective_to']===null?null:(string)$candidate['effective_to'])===($target['effective_to']===null?null:(string)$target['effective_to']);
}

function hrm_typed_attribute_legacy_mapping_approve($mapping_version_id)
{
    $mapping_version_id=hrm_typed_attribute_legacy_mapping_positive_id($mapping_version_id,false);
    $checker_id=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if($mapping_version_id===false||$checker_id<=0||!hrm_typed_attribute_legacy_mapping_user_can_approve())return false;
    begin_transaction();$ok=true;$return_id=false;$target=false;$rows=array();$successor=array();$leaf_ids=array();$max_version=0;
    $q=db_query('SELECT legacy_key FROM '.TB_PREF.'hrm_attribute_legacy_mapping_versions WHERE mapping_version_id='.(int)$mapping_version_id.' LIMIT 2','could not locate legacy mapping draft');
    if(!$q||db_num_rows($q)!==1)$ok=false;else{$z=db_fetch_assoc($q);$legacy_key=(string)$z['legacy_key'];}
    if($ok){$r=db_query('SELECT mapping_version_id,legacy_key,version_no,predecessor_id,attribute_code,subject_type,mapping_status,effective_from,effective_to,qualified_review_ref,approved_at,approved_by,created_by FROM '.TB_PREF.'hrm_attribute_legacy_mapping_versions WHERE legacy_key='.db_escape($legacy_key).' ORDER BY version_no,mapping_version_id FOR UPDATE','could not lock legacy mapping approval chain');if(!$r)$ok=false;}
    while($ok&&($row=db_fetch_assoc($r))){$id=(int)$row['mapping_version_id'];$rows[$id]=$row;$max_version=max($max_version,(int)$row['version_no']);if($row['predecessor_id']!==null){$pred=(int)$row['predecessor_id'];if(isset($successor[$pred])||!isset($rows[$pred])){$ok=false;break;}$successor[$pred]=$id;}}
    if($ok){foreach($rows as $id=>$row)if(!isset($successor[$id]))$leaf_ids[]=$id;if(count($leaf_ids)>1||!isset($rows[$mapping_version_id]))$ok=false;}
    if($ok){$target=$rows[$mapping_version_id];$definition=hrm_typed_attribute_definition_resolve_state((string)$target['attribute_code'],(string)$target['effective_from'].' 00:00:00',true);
        if(!is_array($definition)||$definition['definition_status']!=='active'||$definition['allowed_subject']!=='worker'
            ||($definition['effective_to']!==null&&(string)$target['effective_from']>(string)$definition['effective_to']))$ok=false;}
    if($ok){if(isset($successor[$mapping_version_id])){$candidate=$rows[$successor[$mapping_version_id]];
            if(count($leaf_ids)===1&&(int)$leaf_ids[0]===(int)$candidate['mapping_version_id']&&hrm_typed_attribute_legacy_mapping_approved_successor_matches($candidate,$target,$checker_id))$return_id=(int)$candidate['mapping_version_id'];else$ok=false;
        }elseif(count($leaf_ids)!==1||(int)$leaf_ids[0]!==$mapping_version_id||(string)$target['mapping_status']!=='draft'||(int)$target['created_by']===$checker_id)$ok=false;}
    if($ok&&$return_id===false){$version_no=$max_version+1;
        $sql='INSERT INTO '.TB_PREF.'hrm_attribute_legacy_mapping_versions (legacy_key,version_no,predecessor_id,attribute_code,subject_type,mapping_status,effective_from,effective_to,qualified_review_ref,approved_at,approved_by,created_by) VALUES ('
            .db_escape($target['legacy_key']).','.(int)$version_no.','.(int)$mapping_version_id.','.db_escape($target['attribute_code']).",'worker','approved',".db_escape($target['effective_from']).','
            .($target['effective_to']===null?'NULL':db_escape($target['effective_to'])).','.db_escape($target['qualified_review_ref']).',NOW(),'.(int)$checker_id.','.(int)$checker_id.')';
        $ok=db_query($sql,'could not append approved legacy Typed Attribute mapping successor')!==false;
        if($ok){$return_id=(int)db_insert_id();$receipt=append_domain_audit_event(array(
            'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,'entity_type'=>'hrm_typed_attribute_legacy_mapping',
            'entity_key'=>array('mapping_version_id'=>$return_id,'legacy_key'=>(string)$target['legacy_key'],'predecessor_id'=>(int)$mapping_version_id,'version_no'=>$version_no),
            'action'=>'hrm.typed_attribute.legacy_mapping_approved','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_mapping_approval',
            'before'=>array('mapping_version_id'=>(int)$mapping_version_id,'mapping_status'=>'draft'),'after'=>array('mapping_version_id'=>$return_id,'mapping_status'=>'approved'),
            'restricted_diff'=>array('checker_distinct_from_maker'=>true,'metadata_copied_exactly'=>true,'raw_legacy_value_read'=>false,
                'typed_attribute_value_rows_written'=>0,'conversion_executor_activated'=>false,'immutable_successor'=>true)));
            if($receipt===false){$ok=false;$return_id=false;}}
    }
    if($ok)commit_transaction();else cancel_transaction();return $ok?$return_id:false;
}
