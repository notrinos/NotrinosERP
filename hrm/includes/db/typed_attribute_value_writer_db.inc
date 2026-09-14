<?php
/** HRM-FND-007 atomic immutable encrypted Typed Attribute value writer. */
if (!function_exists('hrm_typed_attribute_value_canonical_command'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_value_command_db.inc');
if (!function_exists('hrm_person_sensitive_active_key'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_sensitive_db.inc');
if (!function_exists('hrm_person_worker_actor_id'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_worker_db.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

function hrm_typed_attribute_value_writer_row_matches($row, $command, $actor_id)
{
    if (!is_array($row) || !is_array($command)
        || (int)$row['definition_version_id'] !== (int)$command['definition_version_id']
        || (string)$row['subject_type'] !== $command['subject_type']
        || (int)$row['subject_id'] !== (int)$command['subject_id']
        || (string)$row['effective_from'] !== $command['effective_from']
        || (($row['effective_to'] === null ? null : (string)$row['effective_to']) !== $command['effective_to'])
        || (($row['predecessor_id'] === null ? null : (int)$row['predecessor_id']) !== $command['predecessor_id'])
        || (string)$row['source'] !== 'manual' || (int)$row['created_by'] !== (int)$actor_id
        || !preg_match('/^[a-f0-9]{64}$/D',(string)$row['ciphertext_sha256'])
        || !function_exists('domain_audit_hash_equals')
        || !domain_audit_hash_equals(hash('sha256',$row['value_ciphertext']),(string)$row['ciphertext_sha256']))
        return false;
    $key = hrm_person_sensitive_key((string)$row['key_id']);
    $context = hrm_typed_attribute_crypto_context((int)$row['definition_version_id'],(string)$row['subject_type'],
        (int)$row['subject_id'],(int)$row['value_version_no'],$row['predecessor_id']===null?null:(int)$row['predecessor_id']);
    $plain = $key===false || $context===false ? false : hrm_person_sensitive_decrypt($row['value_ciphertext'],$context,$key);
    return is_string($plain) && $plain === $command['value'];
}

function hrm_typed_attribute_value_append_version($input)
{
    if (!is_array($input) || !isset($input['attribute_code'],$input['effective_from'])) return false;
    $actor_id = function_exists('hrm_person_worker_actor_id') ? hrm_person_worker_actor_id() : 0;
    if ($actor_id <= 0) return false;
    $active_key = hrm_person_sensitive_active_key(null,false);
    if (!is_array($active_key) || empty($active_key['key_id']) || empty($active_key['key'])) return false;

    begin_transaction();
    $ok=true; $return_id=false; $definition=false;
    $code=hrm_typed_attribute_value_attribute_code($input['attribute_code']);
    if ($code===false) $ok=false;
    if ($ok) {
        $lock=db_query('SELECT definition_id FROM '.TB_PREF.'hrm_attribute_definitions WHERE attribute_code='
            .db_escape($code).' LIMIT 2 FOR UPDATE','could not lock Typed Attribute value definition');
        if (!$lock || db_num_rows($lock)!==1) $ok=false;
    }
    if ($ok) $definition=hrm_typed_attribute_value_resolve_definition($code,$input['effective_from'],true);
    $command=$ok?hrm_typed_attribute_value_canonical_command($input,$definition):false;
    if ($command===false) $ok=false;
    if ($ok && !hrm_typed_attribute_value_subject_exists($command['subject_type'],$command['subject_id'],true)) $ok=false;

    $rows=array();$successor=array();$max_version=0;$leaf_ids=array();
    if ($ok) {
        $sql='SELECT a.attribute_value_id,a.definition_version_id,a.subject_type,a.subject_id,a.value_version_no,'
            .'a.predecessor_id,a.effective_from,a.effective_to,a.key_id,a.value_ciphertext,a.ciphertext_sha256,a.source,a.created_by '
            .'FROM '.TB_PREF.'hrm_attribute_values a INNER JOIN '.TB_PREF.'hrm_attribute_definition_versions v '
            .'ON v.definition_version_id=a.definition_version_id WHERE v.definition_id='.(int)$command['definition_id']
            .' AND a.subject_type='.db_escape($command['subject_type']).' AND a.subject_id='.(int)$command['subject_id']
            .' ORDER BY a.value_version_no,a.attribute_value_id FOR UPDATE';
        $r=db_query($sql,'could not lock Typed Attribute value chain'); if(!$r)$ok=false;
        while($ok&&($row=db_fetch_assoc($r))){$id=(int)$row['attribute_value_id'];$rows[$id]=$row;
            $max_version=max($max_version,(int)$row['value_version_no']);if($row['predecessor_id']!==null){$pred=(int)$row['predecessor_id'];
                if(isset($successor[$pred])){$ok=false;break;}$successor[$pred]=$id;}}
        foreach($rows as $id=>$row)if(!isset($successor[$id]))$leaf_ids[]=$id;
        if(count($leaf_ids)>1)$ok=false;
    }
    $predecessor=$ok?$command['predecessor_id']:null;
    if($ok&&$predecessor!==null){
        if(!isset($rows[$predecessor]))$ok=false;
        elseif(isset($successor[$predecessor])){$candidate=$rows[$successor[$predecessor]];
            if(count($leaf_ids)===1&&(int)$leaf_ids[0]===(int)$candidate['attribute_value_id']
                &&hrm_typed_attribute_value_writer_row_matches($candidate,$command,$actor_id))$return_id=(int)$candidate['attribute_value_id'];else$ok=false;}
        elseif(count($leaf_ids)!==1||(int)$leaf_ids[0]!==$predecessor)$ok=false;
        elseif(strcmp($command['effective_from'],(string)$rows[$predecessor]['effective_from'])<=0)$ok=false;
    }elseif($ok&&count($rows)>0){
        if(count($leaf_ids)===1&&hrm_typed_attribute_value_writer_row_matches($rows[$leaf_ids[0]],$command,$actor_id))
            $return_id=(int)$leaf_ids[0];else$ok=false;
    }

    if($ok&&$return_id===false){
        $version_no=$max_version+1;
        $context=hrm_typed_attribute_crypto_context($command['definition_version_id'],$command['subject_type'],
            $command['subject_id'],$version_no,$predecessor);
        $cipher=$context===false?false:hrm_person_sensitive_encrypt($command['value'],$context,$active_key['key']);
        if($cipher===false)$ok=false;
        if($ok){$hash=hash('sha256',$cipher);
            $sql='INSERT INTO '.TB_PREF.'hrm_attribute_values (definition_version_id,subject_type,subject_id,value_version_no,'
                .'predecessor_id,effective_from,effective_to,key_id,value_ciphertext,ciphertext_sha256,source,created_by) VALUES ('
                .(int)$command['definition_version_id'].','.db_escape($command['subject_type']).','.(int)$command['subject_id'].','
                .(int)$version_no.','.($predecessor===null?'NULL':(int)$predecessor).','.db_escape($command['effective_from']).','
                .($command['effective_to']===null?'NULL':db_escape($command['effective_to'])).','.db_escape($active_key['key_id']).','
                .db_escape($cipher).','.db_escape($hash).",'manual',".(int)$actor_id.')';
            $ok=db_query($sql,'could not append encrypted Typed Attribute value version')!==false;
        }
        if($ok){$return_id=(int)db_insert_id();$receipt=append_domain_audit_event(array(
            'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
            'entity_type'=>'hrm_typed_attribute_value','entity_key'=>array('attribute_value_id'=>$return_id,
                'definition_id'=>(int)$command['definition_id'],'definition_version_id'=>(int)$command['definition_version_id'],
                'subject_type'=>$command['subject_type'],'subject_id'=>(int)$command['subject_id'],'value_version_no'=>$version_no),
            'action'=>'hrm.typed_attribute.value_version_created','reason_code'=>'hrm_fnd_007_typed_attribute_value_writer',
            'before'=>$predecessor===null?null:array('predecessor_id'=>(int)$predecessor),
            'after'=>array('attribute_value_id'=>$return_id,'source'=>'manual'),
            'restricted_diff'=>array('encrypted_value'=>true,'plaintext_in_audit'=>false,'immutable_successor'=>true,
                'sensitivity'=>$command['sensitivity'],'legacy_custom_data_mapped'=>false,'payroll_consumer_activated'=>false)));
            if($receipt===false){$ok=false;$return_id=false;}}
    }
    if($ok)commit_transaction();else cancel_transaction();
    return $ok?$return_id:false;
}
