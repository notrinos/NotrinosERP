<?php
/**
 * HRM-FND-007 one-mapping-at-a-time legacy custom_data conversion executor.
 *
 * The executor is deliberately narrow: one current Employee/Worker binding,
 * one exact approved mapping version, one scalar legacy key, and one initial
 * encrypted Typed Attribute value. The retained legacy key is not removed or
 * rewritten by this boundary. Exact retries are receipt-based and any failure
 * rolls back the value, receipt, and audit evidence together.
 */
if (!function_exists('hrm_typed_attribute_legacy_mapping_positive_id'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_mapping_command_db.inc');
if (!function_exists('hrm_typed_attribute_legacy_custom_data_parity'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_custom_data_inventory_db.inc');
if (!function_exists('hrm_typed_attribute_value_canonical_command'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_value_command_db.inc');
if (!function_exists('hrm_person_sensitive_active_key'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_sensitive_db.inc');
if (!function_exists('hrm_employment_lock_binding_for_interval_policy'))
    include_once($path_to_root.'/hrm/includes/db/employee_employment_db.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

function hrm_typed_attribute_legacy_conversion_user_can_execute($definition)
{
    return is_array($definition)
        && function_exists('user_check_access')
        && user_check_access('SA_HRM_EXECUTE_TYPED_ATTRIBUTE_LEGACY_CONVERSION')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_MAPPING')
        && hrm_typed_attribute_value_user_can_manage((string)$definition['sensitivity'])
        && hrm_typed_attribute_value_user_can_access_object_scope((string)$definition['owner_scope']);
}

function hrm_typed_attribute_legacy_conversion_canonical_command($input)
{
    if (!is_array($input) || array_diff(array_keys($input), array('employee_number','mapping_version_id'))
        || !array_key_exists('employee_number',$input) || !array_key_exists('mapping_version_id',$input)) return false;
    $employee_number = hrm_typed_attribute_legacy_mapping_positive_id($input['employee_number'], false);
    $mapping_version_id = hrm_typed_attribute_legacy_mapping_positive_id($input['mapping_version_id'], false);
    return $employee_number === false || $mapping_version_id === false ? false : array(
        'employee_number'=>$employee_number,
        'mapping_version_id'=>$mapping_version_id,
    );
}

/** Decode only the selected scalar after the caller has proved FND-006 parity. */
function hrm_typed_attribute_legacy_conversion_extract_scalar($raw, $legacy_key)
{
    if (!is_string($raw) || strlen($raw) > 1048576
        || hrm_typed_attribute_legacy_mapping_key($legacy_key) === false) return false;
    $trim=ltrim($raw); if($trim==='' || $trim[0]!=='{') return false;
    $decoded=json_decode($raw,true);
    if(!is_array($decoded)||json_last_error()!==JSON_ERROR_NONE||count($decoded)>100
        || !array_key_exists($legacy_key,$decoded)) return false;
    foreach($decoded as $key=>$value) if(hrm_typed_attribute_legacy_mapping_key((string)$key)===false) return false;
    $value=$decoded[$legacy_key];
    return is_scalar($value) ? array('value'=>$value) : false;
}

function hrm_typed_attribute_legacy_conversion_receipt_digest($mapping_version_id,$worker_id,$profile_version_id,$definition_version_id)
{
    foreach(array($mapping_version_id,$worker_id,$profile_version_id,$definition_version_id) as $id)
        if((int)$id<=0) return false;
    return hash('sha256','hrm-fnd-007-legacy-conversion-v1|mapping='.(int)$mapping_version_id
        .'|worker='.(int)$worker_id.'|profile='.(int)$profile_version_id.'|definition='.(int)$definition_version_id);
}

function hrm_typed_attribute_legacy_conversion_existing_receipt($mapping_version_id,$worker_id)
{
    $r=db_query('SELECT conversion_id,mapping_version_id,worker_id,profile_version_id,definition_version_id,'
        .'attribute_value_id,legacy_key,execution_digest,converted_by FROM '.TB_PREF.'hrm_attribute_legacy_conversion_receipts '
        .'WHERE mapping_version_id='.(int)$mapping_version_id.' AND worker_id='.(int)$worker_id.' LIMIT 2 FOR UPDATE',
        'could not lock legacy Typed Attribute conversion receipt');
    if(!$r||db_num_rows($r)>1) return false;
    return db_num_rows($r)===0 ? null : db_fetch_assoc($r);
}

function hrm_typed_attribute_legacy_conversion_receipt_matches($row,$mapping,$worker_id,$profile,$definition)
{
    if(!is_array($row)||!is_array($mapping)||!is_array($profile)||!is_array($definition)) return false;
    $digest=hrm_typed_attribute_legacy_conversion_receipt_digest((int)$mapping['mapping_version_id'],(int)$worker_id,
        (int)$profile['profile_version_id'],(int)$definition['definition_version_id']);
    if($digest===false || !function_exists('domain_audit_hash_equals')
        || !domain_audit_hash_equals((string)$row['execution_digest'],$digest)
        || (int)$row['mapping_version_id']!==(int)$mapping['mapping_version_id']
        || (int)$row['worker_id']!==(int)$worker_id
        || (int)$row['profile_version_id']!==(int)$profile['profile_version_id']
        || (int)$row['definition_version_id']!==(int)$definition['definition_version_id']
        || (string)$row['legacy_key']!==(string)$mapping['legacy_key']
        || (int)$row['attribute_value_id']<=0) return false;
    $q=db_query('SELECT attribute_value_id,definition_version_id,subject_type,subject_id,value_version_no,predecessor_id,source '
        .'FROM '.TB_PREF.'hrm_attribute_values WHERE attribute_value_id='.(int)$row['attribute_value_id'].' LIMIT 2',
        'could not validate legacy conversion value receipt');
    $value=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    return $value && (int)$value['definition_version_id']===(int)$definition['definition_version_id']
        && (string)$value['subject_type']==='worker' && (int)$value['subject_id']===(int)$worker_id
        && (int)$value['value_version_no']===1 && $value['predecessor_id']===null
        && (string)$value['source']==='legacy_convert';
}

/**
 * Convert one current retained scalar into one initial encrypted Typed Attribute value.
 * Returns conversion receipt id on success/idempotent retry; false on any mismatch.
 */
function hrm_typed_attribute_legacy_convert_one($input)
{
    $command=hrm_typed_attribute_legacy_conversion_canonical_command($input);
    $actor=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if($command===false||$actor<=0) return false;

    begin_transaction();
    $ok=true;$receipt_id=false;$mapping=false;$definition=false;$profile=false;$binding=false;
    $now=hrm_person_worker_utc_now(); if($now===false)$ok=false; $today=$ok?substr($now,0,10):false;

    if($ok){$binding=hrm_employment_lock_binding_for_interval_policy($command['employee_number']);
        if(!$binding||(int)$binding['worker_id']<=0)$ok=false;}
    if($ok){$q=db_query('SELECT employee_id,custom_data FROM '.TB_PREF.'employees WHERE employee_number='.(int)$command['employee_number'].' LIMIT 2 FOR UPDATE',
        'could not lock legacy Employee custom_data for conversion');$employee=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
        if(!$employee||(string)$employee['employee_id']!==(string)$binding['employee_id'])$ok=false;}
    if($ok){$profile=hrm_compatibility_profile_current((int)$binding['worker_id'],true);$retained=$profile?hrm_compatibility_profile_open($profile):false;
        if($retained===false||!array_key_exists('custom_data',$retained)
            ||!hrm_typed_attribute_legacy_custom_data_parity($employee['custom_data'],$retained['custom_data']))$ok=false;}

    if($ok){$q=db_query('SELECT mapping_version_id,legacy_key,version_no,predecessor_id,attribute_code,subject_type,mapping_status,'
        .'effective_from,effective_to,qualified_review_ref,approved_at,approved_by FROM '.TB_PREF.'hrm_attribute_legacy_mapping_versions '
        .'WHERE mapping_version_id='.(int)$command['mapping_version_id'].' LIMIT 2 FOR UPDATE','could not lock approved legacy mapping for conversion');
        $mapping=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
        if(!$mapping||!hrm_typed_attribute_legacy_mapping_validate_metadata($mapping)||(string)$mapping['mapping_status']!=='approved'
            ||(string)$mapping['subject_type']!=='worker'||$today<(string)$mapping['effective_from']
            ||($mapping['effective_to']!==null&&$today>(string)$mapping['effective_to']))$ok=false;}
    if($ok){$resolved=get_hrm_typed_attribute_legacy_mapping_as_of((string)$mapping['legacy_key'],$today);
        if(!is_array($resolved)||(int)$resolved['mapping_version_id']!==(int)$mapping['mapping_version_id'])$ok=false;}
    if($ok){$definition=hrm_typed_attribute_value_resolve_definition((string)$mapping['attribute_code'],$now,true,'write');
        if(!is_array($definition)||(string)$definition['allowed_subject']!=='worker'
            ||(int)$resolved['definition_version_id']!==(int)$definition['definition_version_id']
            ||!hrm_typed_attribute_legacy_conversion_user_can_execute($definition))$ok=false;}

    $receipt=$ok?hrm_typed_attribute_legacy_conversion_existing_receipt((int)$mapping['mapping_version_id'],(int)$binding['worker_id']):false;
    if($ok&&$receipt===false)$ok=false;
    if($ok&&is_array($receipt)){
        if(hrm_typed_attribute_legacy_conversion_receipt_matches($receipt,$mapping,(int)$binding['worker_id'],$profile,$definition))
            $receipt_id=(int)$receipt['conversion_id']; else $ok=false;
    }

    if($ok&&$receipt_id===false){
        $extracted=hrm_typed_attribute_legacy_conversion_extract_scalar($retained['custom_data'],(string)$mapping['legacy_key']);
        $canonical=$extracted===false?false:hrm_typed_attribute_value_apply_validation($definition,$extracted['value']);
        if($canonical===false)$ok=false;
        $q=$ok?db_query('SELECT a.attribute_value_id FROM '.TB_PREF.'hrm_attribute_values a INNER JOIN '
            .TB_PREF.'hrm_attribute_definition_versions v ON v.definition_version_id=a.definition_version_id '
            .'WHERE v.definition_id='.(int)$definition['definition_id']." AND a.subject_type='worker' AND a.subject_id=".(int)$binding['worker_id']
            .' ORDER BY a.value_version_no,a.attribute_value_id FOR UPDATE','could not lock legacy conversion Typed Attribute value chain'):false;
        if(!$q||db_num_rows($q)!==0)$ok=false;
        $active_key=$ok?hrm_person_sensitive_active_key(null,false):false;
        if($ok&&(!is_array($active_key)||empty($active_key['key_id'])||empty($active_key['key'])))$ok=false;
        if($ok){$context=hrm_typed_attribute_crypto_context((int)$definition['definition_version_id'],'worker',(int)$binding['worker_id'],1,null);
            $cipher=$context===false?false:hrm_person_sensitive_encrypt($canonical,$context,$active_key['key']); if($cipher===false)$ok=false;}
        if($ok){$hash=hash('sha256',$cipher);$ok=db_query('INSERT INTO '.TB_PREF.'hrm_attribute_values '
            .'(definition_version_id,subject_type,subject_id,value_version_no,predecessor_id,effective_from,effective_to,key_id,value_ciphertext,ciphertext_sha256,source,created_by) VALUES ('
            .(int)$definition['definition_version_id'].",'worker',".(int)$binding['worker_id'].',1,NULL,'.db_escape($now).',NULL,'
            .db_escape($active_key['key_id']).','.db_escape($cipher).','.db_escape($hash).",'legacy_convert',".(int)$actor.')',
            'could not append legacy-converted encrypted Typed Attribute value')!==false;}
        if($ok){$attribute_value_id=(int)db_insert_id();$digest=hrm_typed_attribute_legacy_conversion_receipt_digest((int)$mapping['mapping_version_id'],
            (int)$binding['worker_id'],(int)$profile['profile_version_id'],(int)$definition['definition_version_id']);
            $ok=$attribute_value_id>0&&$digest!==false&&db_query('INSERT INTO '.TB_PREF.'hrm_attribute_legacy_conversion_receipts '
                .'(mapping_version_id,worker_id,profile_version_id,definition_version_id,attribute_value_id,legacy_key,execution_digest,converted_by) VALUES ('
                .(int)$mapping['mapping_version_id'].','.(int)$binding['worker_id'].','.(int)$profile['profile_version_id'].','
                .(int)$definition['definition_version_id'].','.$attribute_value_id.','.db_escape($mapping['legacy_key']).','
                .db_escape($digest).','.(int)$actor.')','could not append legacy Typed Attribute conversion receipt')!==false;
            if($ok)$receipt_id=(int)db_insert_id();}
        if($ok){$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,
            'legal_entity_id'=>0,'entity_type'=>'hrm_typed_attribute_legacy_conversion',
            'entity_key'=>array('conversion_id'=>$receipt_id,'mapping_version_id'=>(int)$mapping['mapping_version_id'],
                'worker_id'=>(int)$binding['worker_id'],'attribute_value_id'=>$attribute_value_id),
            'action'=>'hrm.typed_attribute.legacy_value_converted','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_conversion',
            'before'=>array('typed_attribute_value_present'=>false),
            'after'=>array('typed_attribute_value_present'=>true,'source'=>'legacy_convert'),
            'restricted_diff'=>array('exact_fnd_006_profile_parity'=>true,'approved_mapping_exact'=>true,
                'active_definition_exact'=>true,'scalar_key_only'=>true,'plaintext_logged'=>false,'encrypted_value'=>true,
                'receipt_idempotency'=>true,'legacy_key_retained'=>true,'legacy_custom_data_rewritten'=>false,
                'bulk_route_activated'=>false,'payroll_consumer_activated'=>false)));
            if($audit===false){$ok=false;$receipt_id=false;}}
    }
    if($ok)commit_transaction();else cancel_transaction();
    return $ok?$receipt_id:false;
}
