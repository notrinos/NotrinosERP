<?php
/** HRM-FND-007 distinct-checker immutable Typed Attribute definition publication. */
if (!function_exists('hrm_typed_attribute_definition_append_draft'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_definition_writer_db.inc');

function hrm_typed_attribute_definition_user_can_approve()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_APPROVE_TYPED_ATTRIBUTE_DEFINITION');
}

function hrm_typed_attribute_definition_publish_target_is_canonical($definition, $target)
{
    if (!is_array($definition) || !is_array($target)) return false;
    $metadata = array(
        'attribute_code'=>(string)$definition['attribute_code'],
        'value_type'=>(string)$target['value_type'],
        'allowed_subject'=>(string)$target['allowed_subject'],
        'sensitivity'=>(string)$target['sensitivity'],
        'owner_scope'=>(string)$target['owner_scope'],
        'validation_json'=>(string)$target['validation_json'],
    );
    $from = hrm_typed_attribute_definition_date((string)$target['effective_from'], false);
    $to = hrm_typed_attribute_definition_date(
        $target['effective_to'] === null ? null : (string)$target['effective_to'], true);
    $validation = hrm_typed_attribute_definition_validation_json(
        (string)$target['value_type'], (string)$target['validation_json']);
    return hrm_typed_attribute_validate_definition_metadata($metadata)
        && $validation !== false && $validation === (string)$target['validation_json']
        && $from !== false && $to !== false && ($to === null || strcmp($to, $from) >= 0);
}

function hrm_typed_attribute_definition_publish_successor_matches($candidate, $target, $checker_id)
{
    if (!is_array($candidate) || !is_array($target) || (int)$candidate['created_by'] !== (int)$checker_id
        || (int)$candidate['definition_id'] !== (int)$target['definition_id']
        || (int)$candidate['predecessor_id'] !== (int)$target['definition_version_id']
        || (string)$candidate['definition_status'] !== 'active') return false;
    foreach (array('value_type','allowed_subject','sensitivity','owner_scope','validation_json','effective_from') as $field)
        if ((string)$candidate[$field] !== (string)$target[$field]) return false;
    return ($candidate['effective_to'] === null ? null : (string)$candidate['effective_to'])
        === ($target['effective_to'] === null ? null : (string)$target['effective_to']);
}

function hrm_typed_attribute_definition_publish($definition_version_id)
{
    $definition_version_id = hrm_typed_attribute_definition_positive_id($definition_version_id, false);
    $checker_id = function_exists('hrm_person_worker_actor_id') ? hrm_person_worker_actor_id() : 0;
    if ($definition_version_id === false || $checker_id <= 0 || !hrm_typed_attribute_definition_user_can_approve())
        return false;

    begin_transaction();
    $ok = true; $return_id = false;
    $lookup = db_query('SELECT definition_id FROM '.TB_PREF.'hrm_attribute_definition_versions '
        .'WHERE definition_version_id='.(int)$definition_version_id.' LIMIT 2', 'could not locate Typed Attribute draft');
    if (!$lookup || db_num_rows($lookup) !== 1) $ok = false;
    $definition_id = 0;
    if ($ok) { $z = db_fetch_assoc($lookup); $definition_id = (int)$z['definition_id']; }
    if ($ok) {
        $d = db_query('SELECT definition_id,attribute_code FROM '.TB_PREF.'hrm_attribute_definitions '
            .'WHERE definition_id='.(int)$definition_id.' LIMIT 1 FOR UPDATE', 'could not lock Typed Attribute definition');
        if (!$d || db_num_rows($d) !== 1) $ok = false;
        else $definition = db_fetch_assoc($d);
    }

    $rows=array(); $successor=array(); $max_version=0; $leaf_ids=array();
    if ($ok) {
        $r=db_query('SELECT definition_version_id,definition_id,version_no,predecessor_id,value_type,allowed_subject,'
            .'sensitivity,owner_scope,validation_json,definition_status,effective_from,effective_to,created_by '
            .'FROM '.TB_PREF.'hrm_attribute_definition_versions WHERE definition_id='.(int)$definition_id
            .' ORDER BY version_no,definition_version_id FOR UPDATE', 'could not lock Typed Attribute publication chain');
        if (!$r) $ok=false;
        while ($ok && ($row=db_fetch_assoc($r))) {
            $id=(int)$row['definition_version_id']; $rows[$id]=$row; $max_version=max($max_version,(int)$row['version_no']);
            if ($row['predecessor_id']!==null) {
                $pred=(int)$row['predecessor_id']; if(isset($successor[$pred])){$ok=false;break;} $successor[$pred]=$id;
            }
        }
        foreach($rows as $id=>$row) if(!isset($successor[$id])) $leaf_ids[]=$id;
        if(count($leaf_ids)>1 || !isset($rows[$definition_version_id])) $ok=false;
    }
    if ($ok) {
        $target=$rows[$definition_version_id];
        if (!hrm_typed_attribute_definition_publish_target_is_canonical($definition, $target)) $ok=false;
    }
    if ($ok) {
        if (isset($successor[$definition_version_id])) {
            $candidate=$rows[$successor[$definition_version_id]];
            if (count($leaf_ids)===1 && (int)$leaf_ids[0]===(int)$candidate['definition_version_id']
                && hrm_typed_attribute_definition_publish_successor_matches($candidate,$target,$checker_id))
                $return_id=(int)$candidate['definition_version_id'];
            else $ok=false;
        } elseif (count($leaf_ids)!==1 || (int)$leaf_ids[0]!==$definition_version_id
            || (string)$target['definition_status']!=='draft'
            || (int)$target['created_by']===(int)$checker_id) {
            $ok=false;
        }
    }

    if ($ok && $return_id===false) {
        $version_no=$max_version+1;
        $sql='INSERT INTO '.TB_PREF.'hrm_attribute_definition_versions '
            .'(definition_id,version_no,predecessor_id,value_type,allowed_subject,sensitivity,owner_scope,validation_json,'
            .'definition_status,effective_from,effective_to,created_by) VALUES ('
            .(int)$definition_id.','.(int)$version_no.','.(int)$definition_version_id.','
            .db_escape($target['value_type']).','.db_escape($target['allowed_subject']).','
            .db_escape($target['sensitivity']).','.db_escape($target['owner_scope']).','
            .db_escape($target['validation_json']).",'active',".db_escape($target['effective_from']).','
            .($target['effective_to']===null?'NULL':db_escape($target['effective_to'])).','.(int)$checker_id.')';
        $ok=db_query($sql,'could not append active Typed Attribute definition successor')!==false;
        if($ok){
            $return_id=(int)db_insert_id();
            $receipt=append_domain_audit_event(array(
                'company_id'=>function_exists('user_company')?(int)user_company():0,
                'legal_entity_id'=>0,'entity_type'=>'hrm_typed_attribute_definition',
                'entity_key'=>array('definition_id'=>$definition_id,'definition_version_id'=>$return_id,
                    'predecessor_id'=>(int)$definition_version_id,'version_no'=>$version_no),
                'action'=>'hrm.typed_attribute.definition_published',
                'reason_code'=>'hrm_fnd_007_typed_attribute_definition_publish',
                'before'=>array('definition_version_id'=>(int)$definition_version_id,'definition_status'=>'draft'),
                'after'=>array('definition_version_id'=>$return_id,'definition_status'=>'active'),
                'restricted_diff'=>array('immutable_successor'=>true,'checker_distinct_from_maker'=>true,
                    'metadata_copied_exactly'=>true,'attribute_value_rows_mutated'=>0,
                    'legacy_custom_data_mapped'=>false,'payroll_consumer_activated'=>false)
            ));
            if($receipt===false){$ok=false;$return_id=false;}
        }
    }
    if($ok) commit_transaction(); else cancel_transaction();
    return $ok?$return_id:false;
}
