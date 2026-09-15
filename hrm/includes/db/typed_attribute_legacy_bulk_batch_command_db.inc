<?php
/**
 * HRM-FND-007 maker command for bounded legacy Typed Attribute conversion batches.
 *
 * The maker freezes only candidate metadata. Every candidate is revalidated
 * transactionally against current Employee/Worker/profile custody, the exact
 * approved mapping and active definition, and absence of conversion/value rows.
 * No legacy plaintext, encrypted value, receipt, or legacy rewrite is persisted.
 */
if (!function_exists('get_hrm_typed_attribute_legacy_bulk_conversion_candidates'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_bulk_candidate_db.inc');
if (!function_exists('hrm_employment_lock_binding_for_interval_policy'))
    include_once($path_to_root.'/hrm/includes/db/employee_employment_db.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

function hrm_typed_attribute_legacy_bulk_batch_user_can_manage()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_CANDIDATES')
        && user_check_access('SA_HRM_MANAGE_TYPED_ATTRIBUTE_LEGACY_BATCH');
}

function hrm_typed_attribute_legacy_bulk_batch_nonce($value)
{
    if (!is_string($value) || strlen($value) < 16 || strlen($value) > 128) return false;
    return preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:-]{15,127}$/D', $value) === 1 ? $value : false;
}

function hrm_typed_attribute_legacy_bulk_batch_digest($mapping_version_id, $candidate_as_of, $candidates)
{
    if ((int)$mapping_version_id <= 0 || hrm_typed_attribute_legacy_mapping_date($candidate_as_of, false) === false
        || !is_array($candidates) || count($candidates) < 1 || count($candidates) > 25) return false;
    $parts=array();
    foreach ($candidates as $row) {
        if (!is_array($row)) return false;
        $parts[]=(int)$row['employee_number'].':'.(int)$row['worker_id'].':'.(int)$row['profile_version_id'].':'
            .(int)$row['definition_version_id'].':'.(string)$row['candidate_digest'];
    }
    return hash('sha256', 'hrm-fnd-007-legacy-bulk-batch-set-v1|mapping='.(int)$mapping_version_id
        .'|as_of='.$candidate_as_of.'|'.implode('|',$parts));
}

/** Canonicalize one maker request without accepting any legacy value. */
function hrm_typed_attribute_legacy_bulk_batch_canonical_command($input)
{
    $allowed=array('mapping_version_id','candidate_as_of','request_nonce','candidates');
    if (!is_array($input) || array_diff(array_keys($input),$allowed)) return false;
    foreach (array('mapping_version_id','candidate_as_of','request_nonce','candidates') as $field)
        if (!array_key_exists($field,$input)) return false;
    $mapping=hrm_typed_attribute_legacy_mapping_positive_id($input['mapping_version_id'],false);
    $as_of=is_string($input['candidate_as_of'])?hrm_typed_attribute_legacy_mapping_date($input['candidate_as_of'],false):false;
    $nonce=hrm_typed_attribute_legacy_bulk_batch_nonce($input['request_nonce']);
    if ($mapping===false || $as_of===false || $nonce===false || !is_array($input['candidates'])
        || count($input['candidates'])<1 || count($input['candidates'])>25) return false;
    $rows=array();$employees=array();$workers=array();$digests=array();
    foreach ($input['candidates'] as $candidate) {
        if (!is_array($candidate) || array_diff(array_keys($candidate),array('employee_number','worker_id','profile_version_id','definition_version_id','candidate_digest')))
            return false;
        foreach (array('employee_number','worker_id','profile_version_id','definition_version_id','candidate_digest') as $field)
            if (!array_key_exists($field,$candidate)) return false;
        $employee=hrm_typed_attribute_legacy_mapping_positive_id($candidate['employee_number'],false);
        $worker=hrm_typed_attribute_legacy_mapping_positive_id($candidate['worker_id'],false);
        $profile=hrm_typed_attribute_legacy_mapping_positive_id($candidate['profile_version_id'],false);
        $definition=hrm_typed_attribute_legacy_mapping_positive_id($candidate['definition_version_id'],false);
        $digest=is_string($candidate['candidate_digest'])&&preg_match('/^[a-f0-9]{64}$/D',$candidate['candidate_digest'])===1
            ? $candidate['candidate_digest'] : false;
        if ($employee===false||$worker===false||$profile===false||$definition===false||$digest===false
            || isset($employees[$employee]) || isset($workers[$worker]) || isset($digests[$digest])) return false;
        $expected=hrm_typed_attribute_legacy_bulk_candidate_digest($mapping,$worker,$profile,$definition);
        if ($expected===false || !hash_equals($expected,$digest)) return false;
        $employees[$employee]=true;$workers[$worker]=true;$digests[$digest]=true;
        $rows[]=array('employee_number'=>$employee,'worker_id'=>$worker,'profile_version_id'=>$profile,
            'definition_version_id'=>$definition,'candidate_digest'=>$digest);
    }
    usort($rows,function($a,$b){return $a['employee_number']===$b['employee_number']?0:($a['employee_number']<$b['employee_number']?-1:1);});
    $set_digest=hrm_typed_attribute_legacy_bulk_batch_digest($mapping,$as_of,$rows);
    if ($set_digest===false) return false;
    return array('mapping_version_id'=>$mapping,'candidate_as_of'=>$as_of,
        'request_nonce_hash'=>hash('sha256','hrm-fnd-007-legacy-bulk-batch-nonce-v1|'.$nonce),
        'candidate_set_digest'=>$set_digest,'candidate_count'=>count($rows),'candidates'=>$rows);
}

/**
 * Lock and revalidate every candidate using the one-record conversion prerequisites.
 * Caller owns the transaction. Returns exact mapping/definition custody or false.
 */
function hrm_typed_attribute_legacy_bulk_batch_revalidate_candidates($command)
{
    if (!is_array($command) || empty($command['candidates'])) return false;
    $locked=array();
    foreach ($command['candidates'] as $candidate) {
        $binding=hrm_employment_lock_binding_for_interval_policy((int)$candidate['employee_number']);
        if (!$binding || (int)$binding['worker_id']!==(int)$candidate['worker_id']) return false;
        $q=db_query('SELECT employee_id,custom_data FROM '.TB_PREF.'employees WHERE employee_number='.(int)$candidate['employee_number'].' LIMIT 2 FOR UPDATE',
            'could not lock legacy Employee custom_data for bulk batch authorization');
        $employee=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
        if (!$employee || (string)$employee['employee_id']!==(string)$binding['employee_id']) return false;
        $profile=hrm_compatibility_profile_current((int)$candidate['worker_id'],true);
        $retained=$profile?hrm_compatibility_profile_open($profile):false;
        if (!$profile || (int)$profile['profile_version_id']!==(int)$candidate['profile_version_id'] || $retained===false
            || !array_key_exists('custom_data',$retained)
            || !hrm_typed_attribute_legacy_custom_data_parity($employee['custom_data'],$retained['custom_data'])) return false;
        $locked[] = array('candidate'=>$candidate,'retained_custom_data'=>$retained['custom_data']);
    }

    $q=db_query('SELECT mapping_version_id,legacy_key,version_no,predecessor_id,attribute_code,subject_type,mapping_status,'
        .'effective_from,effective_to,qualified_review_ref,approved_at,approved_by FROM '.TB_PREF.'hrm_attribute_legacy_mapping_versions '
        .'WHERE mapping_version_id='.(int)$command['mapping_version_id'].' LIMIT 2 FOR UPDATE',
        'could not lock approved legacy mapping for bulk batch authorization');
    $mapping=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if (!$mapping || !hrm_typed_attribute_legacy_mapping_validate_metadata($mapping) || (string)$mapping['mapping_status']!=='approved'
        || (string)$mapping['subject_type']!=='worker' || $command['candidate_as_of']<(string)$mapping['effective_from']
        || ($mapping['effective_to']!==null && $command['candidate_as_of']>(string)$mapping['effective_to'])) return false;
    $r=db_query('SELECT mapping_version_id FROM '.TB_PREF.'hrm_attribute_legacy_mapping_versions WHERE legacy_key='.db_escape($mapping['legacy_key'])
        ." AND mapping_status='approved' AND effective_from<=".db_escape($command['candidate_as_of'])
        .' AND (effective_to IS NULL OR effective_to>='.db_escape($command['candidate_as_of']).') FOR UPDATE',
        'could not serialize approved legacy mapping interval for bulk batch authorization');
    if (!$r || db_num_rows($r)!==1) return false;
    $resolved_mapping=db_fetch_assoc($r);
    if ((int)$resolved_mapping['mapping_version_id']!==(int)$mapping['mapping_version_id']) return false;

    $definition=hrm_typed_attribute_value_resolve_definition((string)$mapping['attribute_code'],$command['candidate_as_of'].' 00:00:00',true,'write');
    if (!is_array($definition) || (string)$definition['allowed_subject']!=='worker'
        || !hrm_typed_attribute_value_user_can_view((string)$definition['sensitivity'])
        || !hrm_typed_attribute_value_user_can_access_object_scope((string)$definition['owner_scope'])) return false;

    foreach ($locked as $row) {
        $candidate=$row['candidate'];
        if ((int)$candidate['definition_version_id']!==(int)$definition['definition_version_id']) return false;
        $extracted=hrm_typed_attribute_legacy_conversion_extract_scalar($row['retained_custom_data'],(string)$mapping['legacy_key']);
        if ($extracted===false || hrm_typed_attribute_value_apply_validation($definition,$extracted['value'])===false) return false;
        $receipt=db_query('SELECT conversion_id FROM '.TB_PREF.'hrm_attribute_legacy_conversion_receipts WHERE mapping_version_id='.(int)$mapping['mapping_version_id']
            .' AND worker_id='.(int)$candidate['worker_id'].' LIMIT 2 FOR UPDATE','could not lock legacy conversion receipt for bulk batch authorization');
        if (!$receipt || db_num_rows($receipt)!==0) return false;
        $values=db_query('SELECT a.attribute_value_id FROM '.TB_PREF.'hrm_attribute_values a INNER JOIN '
            .TB_PREF.'hrm_attribute_definition_versions v ON v.definition_version_id=a.definition_version_id '
            .'WHERE v.definition_id='.(int)$definition['definition_id']." AND a.subject_type='worker' AND a.subject_id=".(int)$candidate['worker_id']
            .' ORDER BY a.value_version_no,a.attribute_value_id FOR UPDATE','could not lock Typed Attribute value chain for bulk batch authorization');
        if (!$values || db_num_rows($values)!==0) return false;
        $digest=hrm_typed_attribute_legacy_bulk_candidate_digest((int)$mapping['mapping_version_id'],(int)$candidate['worker_id'],
            (int)$candidate['profile_version_id'],(int)$candidate['definition_version_id']);
        if ($digest===false || !hash_equals($digest,(string)$candidate['candidate_digest'])) return false;
    }
    return array('mapping'=>$mapping,'definition'=>$definition,'locked'=>$locked);
}

function hrm_typed_attribute_legacy_bulk_batch_existing_matches($batch,$command,$maker_id)
{
    if (!is_array($batch) || !is_array($command) || (int)$batch['maker_id']!==(int)$maker_id
        || (string)$batch['batch_status']!=='draft' || $batch['checker_id']!==null || $batch['approved_at']!==null
        || (int)$batch['mapping_version_id']!==(int)$command['mapping_version_id']
        || (string)$batch['request_nonce_hash']!==(string)$command['request_nonce_hash']
        || (string)$batch['candidate_set_digest']!==(string)$command['candidate_set_digest']
        || (string)$batch['candidate_as_of']!==(string)$command['candidate_as_of']
        || (int)$batch['candidate_count']!==(int)$command['candidate_count'] || (int)$batch['row_version']!==1) return false;
    $q=db_query('SELECT employee_number,worker_id,profile_version_id,definition_version_id,candidate_digest,item_state,conversion_id FROM '
        .TB_PREF.'hrm_attribute_legacy_conversion_batch_items WHERE batch_id='.(int)$batch['batch_id'].' ORDER BY employee_number,batch_item_id FOR UPDATE',
        'could not lock existing legacy bulk batch items');
    if (!$q || db_num_rows($q)!==count($command['candidates'])) return false;
    for ($index=0,$count=count($command['candidates']); $index<$count; ++$index) {
        $item=db_fetch_assoc($q);
        if (!is_array($item)) return false;
        $candidate=$command['candidates'][$index];
        if ((int)$item['employee_number']!==(int)$candidate['employee_number'] || (int)$item['worker_id']!==(int)$candidate['worker_id']
            || (int)$item['profile_version_id']!==(int)$candidate['profile_version_id'] || (int)$item['definition_version_id']!==(int)$candidate['definition_version_id']
            || (string)$item['candidate_digest']!==(string)$candidate['candidate_digest'] || (string)$item['item_state']!=='pending'
            || $item['conversion_id']!==null) return false;
    }
    return true;
}

/** Create one immutable draft batch. Exact same maker/nonce/set retry is idempotent. */
function hrm_typed_attribute_legacy_bulk_batch_create_draft($input)
{
    $command=hrm_typed_attribute_legacy_bulk_batch_canonical_command($input);
    $maker=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if ($command===false || $maker<=0 || !hrm_typed_attribute_legacy_bulk_batch_user_can_manage()) return false;
    begin_transaction();$ok=true;$batch_id=false;
    $existing=db_query('SELECT batch_id,mapping_version_id,request_nonce_hash,candidate_set_digest,candidate_as_of,candidate_count,batch_status,'
        .'maker_id,checker_id,approved_at,expires_at,row_version FROM '.TB_PREF.'hrm_attribute_legacy_conversion_batches WHERE request_nonce_hash='
        .db_escape($command['request_nonce_hash']).' OR (mapping_version_id='.(int)$command['mapping_version_id'].' AND candidate_set_digest='
        .db_escape($command['candidate_set_digest']).') ORDER BY batch_id FOR UPDATE','could not lock legacy bulk batch identity');
    if (!$existing || db_num_rows($existing)>1) $ok=false;
    if ($ok && db_num_rows($existing)===1) {
        $batch=db_fetch_assoc($existing);
        if (!hrm_typed_attribute_legacy_bulk_batch_existing_matches($batch,$command,$maker)) $ok=false;
        else {$check=db_query('SELECT UTC_TIMESTAMP() utc_now','could not read bulk batch retry time');$now=$check?db_fetch_assoc($check):false;
            if (!$now || (string)$batch['expires_at']<=(string)$now['utc_now']) $ok=false; else $batch_id=(int)$batch['batch_id'];}
    }
    if ($ok && hrm_typed_attribute_legacy_bulk_batch_revalidate_candidates($command)===false) $ok=false;
    if ($ok && $batch_id===false) {
        $ok=db_query('INSERT INTO '.TB_PREF.'hrm_attribute_legacy_conversion_batches '
            .'(mapping_version_id,request_nonce_hash,candidate_set_digest,candidate_as_of,candidate_count,batch_status,maker_id,checker_id,approved_at,expires_at,row_version) VALUES ('
            .(int)$command['mapping_version_id'].','.db_escape($command['request_nonce_hash']).','.db_escape($command['candidate_set_digest']).','
            .db_escape($command['candidate_as_of']).','.(int)$command['candidate_count'].','."'draft'".','.(int)$maker.',NULL,NULL,DATE_ADD(UTC_TIMESTAMP(),INTERVAL 24 HOUR),1)',
            'could not create legacy Typed Attribute bulk batch draft')!==false;
        if ($ok) {$batch_id=(int)db_insert_id(); if ($batch_id<=0)$ok=false;}
        foreach ($command['candidates'] as $candidate) if ($ok) {
            $ok=db_query('INSERT INTO '.TB_PREF.'hrm_attribute_legacy_conversion_batch_items '
                .'(batch_id,employee_number,worker_id,profile_version_id,definition_version_id,candidate_digest,item_state,conversion_id) VALUES ('
                .$batch_id.','.(int)$candidate['employee_number'].','.(int)$candidate['worker_id'].','.(int)$candidate['profile_version_id'].','
                .(int)$candidate['definition_version_id'].','.db_escape($candidate['candidate_digest']).','."'pending'".',NULL)',
                'could not freeze legacy Typed Attribute bulk batch candidate')!==false;
        }
        if ($ok) {
            $audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
                'entity_type'=>'hrm_typed_attribute_legacy_conversion_batch','entity_key'=>array('batch_id'=>$batch_id,'mapping_version_id'=>(int)$command['mapping_version_id']),
                'action'=>'hrm.typed_attribute.legacy_bulk_batch_drafted','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_batch_draft',
                'before'=>array('batch_present'=>false),'after'=>array('batch_present'=>true,'batch_status'=>'draft','candidate_count'=>(int)$command['candidate_count']),
                'restricted_diff'=>array('maker_only'=>true,'candidate_set_digest'=>(string)$command['candidate_set_digest'],'transactional_revalidation'=>true,
                    'plaintext_logged'=>false,'legacy_custom_data_rewritten'=>false,'typed_attribute_values_written'=>0,'conversion_receipts_written'=>0,
                    'checker_approval_required'=>true,'bulk_executor_activated'=>false,'payroll_consumer_activated'=>false)));
            if ($audit===false){$ok=false;$batch_id=false;}
        }
    }
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok?$batch_id:false;
}
