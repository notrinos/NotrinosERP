<?php
/**
 * S39 staged standalone native producer V1, default-disabled.
 * Every authority entry owns a top-level transaction. Member result/progress
 * and final execution/members/replay/PAY-AUD custody commit atomically.
 * No legacy preparation, CORE-009 relabelling, business approval or GL effect.
 */
require_once dirname(dirname(dirname(dirname(__FILE__)))).'/includes/pay_glb_001_native_execution_admission.inc';
require_once dirname(__FILE__).'/pay_core_005_governance_db.inc';
require_once dirname(__FILE__).'/pay_core_010_governance_db.inc';
require_once dirname(__FILE__).'/pay_glb_001_native_schema_db.inc';
require_once dirname(dirname(__FILE__)).'/payroll/pay_glb_001_native_calculation_adapter.inc';

/** Explicit identity of new native production, separate from all old producers. */
function pay_glb_001_native_producer_version() { return 'PAY_GLB_001_STAGED_NATIVE_V1'; }

/** Preserve existing SQL text-scalar evidence across supported drivers; typed JSON and floats are not normalized. */
function pay_glb_001_native_producer_sql_strings(array $row,&$error=null)
{
    foreach($row as $field=>$value){
        if($value===null || is_string($value))continue;
        if(!is_int($value)){$error='native_producer_sql_scalar_invalid';return false;}
        $row[$field]=(string)$value;
    }
    return $row;
}

/** Rebuild only a recognized server-owned producer version under the real caller purpose. */
function pay_glb_001_native_producer_rebuild(array $context,$owner,$purpose,&$error=null)
{
    $input=array();foreach($context['sources'] as $source){
        $entry=array('snapshot_id'=>$source['snapshot_id'],'plan'=>$source['plan']);
        if(isset($context['replacement']))$entry['retro_plan_id']=isset($source['retro']) && $source['retro']!==null?$source['retro']['retro_plan_id']:null;
        $input[]=$entry;
    }
    if($context['producer_contract_version']===pay_glb_001_native_producer_version() && !isset($context['replacement']))
        return pay_glb_001_native_producer_source_context($context['scope']['payroll_period_id'],$context['command_key_sha256'],$input,$owner,$error,$purpose);
    if(function_exists('pay_glb_001_native_replacement_version') && $context['producer_contract_version']===pay_glb_001_native_replacement_version() && isset($context['replacement']))
        return pay_glb_001_native_replacement_prepare($context['scope']['payroll_period_id'],$context['command_key_sha256'],$input,
            $context['replacement']['predecessor_selection_id'],$error,$owner,in_array($purpose,array('native_select','native_retro_review'),true)?$purpose:'native_replace');
    $error='native_producer_unknown_version';return false;
}

/** Dispatch only actual versioned calculation contracts; no caller-built bundle is accepted. */
function pay_glb_001_native_producer_bundle(array $source,array $context,&$error=null)
{
    if($context['producer_contract_version']===pay_glb_001_native_producer_version() && !isset($context['replacement']))
        return pay_glb_001_native_calculation_execute_v1($source['plan'],$error);
    if(!function_exists('pay_glb_001_native_replacement_version') || $context['producer_contract_version']!==pay_glb_001_native_replacement_version()
        || !array_key_exists('retro',$source)){ $error='native_producer_calculation_version_invalid';return false; }
    return $source['retro']===null?pay_glb_001_native_calculation_execute_v1($source['plan'],$error):
        pay_glb_001_native_retro_recalculate_v1($source['retro']['historical_context'],$source['plan'],$error);
}

/** Begin only new replacement transactions at current-read isolation; preserve V1 defaults. */
function pay_glb_001_native_producer_begin($operation_id=null,$replacement=false,&$error=null)
{
    if($operation_id!==null){
        if(pay_glb_001_native_helper_id($operation_id,$error)===false)return false;
        $row=hrm_pay_core_004_read_one('SELECT request_context_json FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)user_company()
            .' AND command_domain=\'native.start\' AND command_receipt_id='.(int)$operation_id.' AND OCTET_LENGTH(request_context_json)<=16785408 LIMIT 2');
        $data=$row?json_decode($row['request_context_json'],true):false;
        $replacement=is_array($data) && isset($data['operation']['replacement']);
    }
    if($replacement){if(!function_exists('pay_glb_001_native_selection_begin')){$error='native_replacement_transaction_support_missing';return false;}return pay_glb_001_native_selection_begin($error);}
    if (begin_transaction()===false) { $error='native_producer_transaction_begin_failed';return false; }return true;
}

/** Validate a sealed historical proof without claiming it is current integrity or current selection. */
function pay_glb_001_native_producer_historical_proof($result_id,$executor,$proof,&$error=null)
{
    if(!is_array($proof) || !isset($proof['result_id'],$proof['integrity_evidence_id'],$proof['integrity_sha256'])
        || $proof['result_id']!==$result_id || pay_glb_001_native_helper_id($proof['integrity_evidence_id'],$error)===false){$error='native_producer_historical_proof_invalid';return false;}
    $row=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_004_integrity_evidence WHERE result_id='.(int)$result_id
        .' AND integrity_evidence_id='.(int)$proof['integrity_evidence_id'].' LIMIT 2 FOR UPDATE');
    if(!$row || $row['verification_status']!=='verified' || (int)$row['verified_by']<=0 || (int)$row['verified_by']===$executor
        || $row['integrity_sha256']!==$proof['integrity_sha256']){$error='native_producer_historical_proof_source_invalid';return false;}
    $d=array('result_id'=>(int)$row['result_id'],'snapshot_sha256'=>$row['snapshot_sha256'],'result_sha256'=>$row['result_sha256'],
        'line_set_sha256'=>$row['line_set_sha256'],'trace_set_sha256'=>$row['trace_set_sha256'],'verification_status'=>$row['verification_status'],
        'evidence_sha256'=>$row['evidence_sha256'],'verified_by'=>(int)$row['verified_by']);
    if(hrm_pay_core_004_hash($d)!==$row['integrity_sha256']){$error='native_producer_historical_proof_hash_invalid';return false;}return $row;
}

/** Hash source/operation material in an explicit forward-only domain. */
function pay_glb_001_native_producer_hash($kind,array $material)
{
    $json=hrm_pay_core_004_canonical_json($material);
    return $json===false?false:hash('sha256',"PAY-GLB-001|native-producer|".$kind."|v1\n".$json);
}

/** Capture the existing exact metadata guard's actual diagnostics. */
class PayGlb001NativeProducerSchemaState
{
    public $preflight_state='';public $preflight_detail='';public $errors=array();
    /** Preserve a genuine schema error rather than ignoring drift. */
    function log_error($message) { $this->errors[]=$message;return false; }
}

/** Admit the real live actor, complete metadata, audit gateway and top-level boundary. */
function pay_glb_001_native_producer_boundary(&$error=null)
{
    $error=null;$principal=pay_glb_001_native_execution_principal();
    if ($principal===false) { $error='native_producer_disabled_or_access_denied';return false; }
    if (!empty($GLOBALS['transaction_level'])) { $error='native_producer_top_level_transaction_required';return false; }
    if (!function_exists('append_domain_audit_event')) { $error='native_producer_audit_gateway_required';return false; }
    $state=new PayGlb001NativeProducerSchemaState();
    if (!pay_glb_001_native_require_schema($state,false)) { $error='native_producer_schema_invalid:'.$state->preflight_detail;return false; }
    return $principal;
}

/** Resolve actual reconciled period mapping, current entity/currency and group version. */
function pay_glb_001_native_producer_scope($period_id,$minor_units,&$error=null,$purpose='native_execute')
{
    $error=null;
    $id=pay_glb_001_native_helper_id($period_id,$error);
    if ($id===false || !is_int($minor_units) || $minor_units<0 || $minor_units>6) { $error='native_producer_period_or_currency_invalid';return false; }
    $period=hrm_pay_core_010_period_scope($id,!empty($GLOBALS['transaction_level']),$error);
    if ($period===false) return false;
    foreach (array('legal_entity_id','payroll_group_id','group_version_id','calendar_period_id') as $field)
        if (pay_glb_001_native_helper_id((string)$period[$field],$error)===false) return false;
    $entity=hrm_pay_core_004_read_one('SELECT legal_entity_id,entity_code,country_code,base_currency,entity_status,source FROM '.TB_PREF.'hrm_legal_entities WHERE legal_entity_id='.(int)$period['legal_entity_id'].' LIMIT 2'.(!empty($GLOBALS['transaction_level'])?' FOR UPDATE':''));
    if($entity!==false)$entity=pay_glb_001_native_producer_sql_strings($entity,$error);
    $group=hrm_pay_core_004_read_one('SELECT gv.version_sha256,gv.effective_from,gv.effective_to,g.group_sha256 FROM '.TB_PREF.'hrm_payroll_group_versions gv INNER JOIN '.TB_PREF.'hrm_payroll_groups g ON g.payroll_group_id=gv.payroll_group_id WHERE gv.group_version_id='.(int)$period['group_version_id'].' LIMIT 2'.(!empty($GLOBALS['transaction_level'])?' FOR UPDATE':''));
    $calendar=hrm_pay_core_004_read_one('SELECT cp.period_start,cp.period_end,mp.period_start mapped_start,mp.period_end mapped_end,mp.currency_code FROM '.TB_PREF.'hrm_payroll_calendar_periods cp INNER JOIN '.TB_PREF.'hrm_payroll_legacy_period_mapping_plans mp ON mp.calendar_version_id=cp.calendar_version_id WHERE cp.calendar_period_id='.(int)$period['calendar_period_id'].' AND mp.legacy_period_id='.$id.' LIMIT 2'.(!empty($GLOBALS['transaction_level'])?' FOR UPDATE':''));
    if (!$entity || !$group || $entity['entity_status']!=='active' || !pay_glb_001_code($entity['entity_code'])
        || !is_string($entity['country_code']) || preg_match('/^[A-Z]{2}$/D',$entity['country_code'])!==1
        || !pay_glb_001_native_field_valid('execution','currency_code',$entity['base_currency'])
        || !is_string($entity['source']) || preg_match('/^[a-z][a-z0-9_]{0,31}$/D',$entity['source'])!==1
        || in_array($entity['source'],array('fixture','synthetic','mock','test'),true)
        || $group['effective_from']>$period['from_date']
        || ($group['effective_to']!==null && $group['effective_to']<$period['to_date'])) {
        $error='native_producer_mapped_entity_or_group_invalid';return false;
    }
    if (!$calendar || $calendar['period_start']!==$period['from_date'] || $calendar['period_end']!==$period['to_date']
        || $calendar['mapped_start']!==$period['from_date'] || $calendar['mapped_end']!==$period['to_date']
        || $calendar['currency_code']!==$entity['base_currency']) { $error='native_producer_calendar_or_currency_mapping_drift';return false; }
    $currency=hrm_pay_core_004_read_one('SELECT curr_abrev FROM '.TB_PREF.'currencies WHERE curr_abrev='.db_escape($entity['base_currency']).' AND inactive=0 LIMIT 2');
    if (!$currency) { $error='native_producer_currency_unavailable';return false; }
    $principal=$purpose==='native_execute'?pay_glb_001_native_execution_principal():
        ($purpose==='native_select' && function_exists('pay_glb_001_native_selection_principal')?pay_glb_001_native_selection_principal():
        ($purpose==='native_replace' && function_exists('pay_glb_001_native_replacement_principal')?pay_glb_001_native_replacement_principal():
        ($purpose==='native_retro_review' && function_exists('pay_glb_001_native_correction_review_principal')?pay_glb_001_native_correction_review_principal():false)));
    if ($principal===false) { $error='native_producer_principal_revoked';return false; }
    $scope=array('company_id'=>(string)$principal['company_id'],'legal_entity_id'=>(string)$period['legal_entity_id'],
        'payroll_group_id'=>(string)$period['payroll_group_id'],'group_version_id'=>(string)$period['group_version_id'],
        'payroll_period_id'=>$period_id,'calendar_period_id'=>(string)$period['calendar_period_id'],
        'entity_code'=>$entity['entity_code'],'country_code'=>$entity['country_code'],
        'currency_code'=>$entity['base_currency'],'currency_minor_units'=>$minor_units);
    $evidence=array('from_date'=>$period['from_date'],'to_date'=>$period['to_date'],
        'period_sha256'=>$period['period_sha256'],'mapping_plan_sha256'=>$period['mapping_plan_sha256'],
        'conversion_sha256'=>$period['conversion_sha256'],'group_sha256'=>$group['group_sha256'],
        'group_version_sha256'=>$group['version_sha256'],'entity'=>$entity);
    foreach (array('period_sha256','mapping_plan_sha256','conversion_sha256','group_sha256','group_version_sha256') as $field)
        if (!pay_glb_001_sha256($evidence[$field])) { $error='native_producer_mapping_hash_invalid';return false; }
    return array('scope'=>$scope,'evidence'=>$evidence);
}

/** Read every overlapping enrolled relationship and active assignment, rejecting ambiguity. */
function pay_glb_001_native_producer_population(array $mapped,&$error=null)
{
    $error=null;$scope=$mapped['scope'];$dates=$mapped['evidence'];
    $q=db_query('SELECT r.payroll_relationship_id,r.legal_entity_id,r.relationship_sha256,r.effective_from relationship_from,r.effective_to relationship_to,ge.enrollment_id,ge.enrollment_sha256,ge.effective_from enrollment_from,ge.effective_to enrollment_to FROM '.TB_PREF.'hrm_payroll_group_enrollments ge INNER JOIN '.TB_PREF.'hrm_payroll_relationships r ON r.payroll_relationship_id=ge.payroll_relationship_id WHERE ge.group_version_id='.(int)$scope['group_version_id']
        .' AND ge.effective_from<='.db_escape($dates['to_date']).' AND (ge.effective_to IS NULL OR ge.effective_to>='.db_escape($dates['from_date']).')'
        .' AND r.effective_from<='.db_escape($dates['to_date']).' AND (r.effective_to IS NULL OR r.effective_to>='.db_escape($dates['from_date']).')'
        .' ORDER BY r.payroll_relationship_id,ge.enrollment_id LIMIT 4097'.(!empty($GLOBALS['transaction_level'])?' FOR UPDATE':''));
    if (!$q) { $error='native_producer_population_read_failed';return false; }
    $rows=array();$seen=array();
    while ($row=db_fetch_assoc($q)) {
        $row=pay_glb_001_native_producer_sql_strings($row,$error);if($row===false)return false;
        $id=(string)$row['payroll_relationship_id'];
        if (count($rows)>=4096 || isset($seen[$id]) || pay_glb_001_native_helper_id($id,$error)===false
            || (string)$row['legal_entity_id']!==$scope['legal_entity_id']
            || !pay_glb_001_sha256($row['relationship_sha256']) || !pay_glb_001_sha256($row['enrollment_sha256'])) {
            $error='native_producer_population_ambiguous_or_out_of_scope';return false;
        }
        $seen[$id]=true;
        $overlap_from=max($dates['from_date'],$row['relationship_from'],$row['enrollment_from']);
        $overlap_to=min($dates['to_date'],$row['relationship_to']===null?$dates['to_date']:$row['relationship_to'],
            $row['enrollment_to']===null?$dates['to_date']:$row['enrollment_to']);
        if ($overlap_from>$overlap_to) { $error='native_producer_disjoint_effective_population';return false; }
        $mq=db_query('SELECT assignment_id,member_sha256,effective_from,effective_to FROM '.TB_PREF.'hrm_payroll_relationship_members WHERE payroll_relationship_id='.(int)$id
            .' AND effective_from<='.db_escape($overlap_to).' AND (effective_to IS NULL OR effective_to>='.db_escape($overlap_from).') ORDER BY assignment_id,relationship_member_id LIMIT 4097'.(!empty($GLOBALS['transaction_level'])?' FOR UPDATE':''));
        if (!$mq) { $error='native_producer_assignment_read_failed';return false; }
        $members=array();$assignments=array();
        while ($member=db_fetch_assoc($mq)) {
            $member=pay_glb_001_native_producer_sql_strings($member,$error);if($member===false)return false;
            $aid=(string)$member['assignment_id'];
            if (count($members)>=4096 || isset($assignments[$aid]) || pay_glb_001_native_helper_id($aid,$error)===false || !pay_glb_001_sha256($member['member_sha256'])) {
                $error='native_producer_assignment_ambiguous';return false;
            }
            $assignments[$aid]=true;$members[]=$member;
        }
        if (!$members) { $error='native_producer_assignment_population_missing';return false; }
        $row['assignments']=$members;$rows[]=$row;
    }
    if (!$rows) { $error='native_producer_expected_population_empty';return false; }
    return $rows;
}

/**
 * Build the actual source-owned complete request before any result write.
 * Input rows contain only a sealed snapshot ID and its deterministic plan.
 * The resulting exact digest must separately exist in protected native admission.
 */
function pay_glb_001_native_producer_prepare($period_id,$command_key_sha,array $input,&$error=null)
{
    $error=null;$principal=pay_glb_001_native_execution_principal();
    if ($principal===false) { $error='native_producer_principal_denied';return false; }
    return pay_glb_001_native_producer_source_context($period_id,$command_key_sha,$input,$principal['actor_user_id'],$error);
}

/** Rebuild source identity for a recorded producer owner; this read helper confers no actor authority. */
function pay_glb_001_native_producer_source_context($period_id,$command_key_sha,array $input,$producer_actor,&$error=null,$purpose='native_execute')
{
    $error=null;
    if (!is_int($producer_actor) || $producer_actor<=0 || !pay_glb_001_sha256($command_key_sha) || count($input)<1 || count($input)>4096
        || array_keys($input)!==range(0,count($input)-1)) { $error='native_producer_request_invalid';return false; }
    $sources=array();$minor=null;$seen=array();$source_bytes=0;
    foreach ($input as $entry) {
        if (!is_array($entry) || array_keys($entry)!==array('snapshot_id','plan') || !is_array($entry['plan'])
            || pay_glb_001_native_helper_id($entry['snapshot_id'],$error)===false) { $error='native_producer_source_entry_invalid';return false; }
        $entry_json=hrm_pay_core_004_canonical_json($entry);
        if ($entry_json===false || ($source_bytes+=strlen($entry_json))>16777216) { $error='native_producer_source_memory_limit';return false; }
        $row=hrm_pay_core_004_read_one('SELECT snapshot_id,payroll_relationship_id,assignment_id,calendar_period_id,legal_entity_id,currency,run_identity,snapshot_sha256,sealed_by,OCTET_LENGTH(snapshot_payload_json) payload_bytes FROM '.TB_PREF.'hrm_pay_core_004_snapshots WHERE snapshot_id='.(int)$entry['snapshot_id'].' LIMIT 2'.(!empty($GLOBALS['transaction_level'])?' FOR UPDATE':''));
        if (!$row || (int)$row['sealed_by']===$producer_actor) { $error='native_producer_independently_sealed_snapshot_required';return false; }
        if ((int)$row['payload_bytes']>1048576) { $error='native_producer_snapshot_payload_limit';return false; }
        $payload=hrm_pay_core_004_read_one('SELECT snapshot_payload_json FROM '.TB_PREF.'hrm_pay_core_004_snapshots WHERE snapshot_id='.(int)$entry['snapshot_id'].' LIMIT 2');
        if (!$payload) { $error='native_producer_snapshot_payload_missing';return false; }
        $row['snapshot_payload_json']=$payload['snapshot_payload_json'];
        $material=hrm_pay_core_005_snapshot_from_row($row,$error);
        if ($material===false || hrm_pay_core_005_validate_plan($entry['plan'],$error)===false
            || hrm_pay_core_005_canonical_json($entry['plan']['snapshot_material'])!==hrm_pay_core_005_canonical_json($material)) {
            $error='native_producer_plan_snapshot_binding_invalid';return false;
        }
        $descriptor=$material['descriptor'];
        foreach (array('payroll_relationship_id','assignment_id','calendar_period_id','legal_entity_id','currency','run_identity') as $field) {
            if (!array_key_exists($field,$descriptor) || (!is_int($descriptor[$field]) && !is_string($descriptor[$field]) && $descriptor[$field]!==null)
                || ($descriptor[$field]===null?null:(string)$descriptor[$field])!==($row[$field]===null?null:(string)$row[$field])) {
                $error='native_producer_snapshot_relational_scope_mismatch';return false;
            }
        }
        if (in_array($entry['plan']['scenario'],array('retroactive_adjustment','multi_relationship'),true)
            || (in_array($entry['plan']['scenario'],array('correction','reversal'),true) && !in_array($purpose,array('native_replace','native_select','native_retro_review'),true))) {
            $error='native_producer_governed_correction_protocol_required';return false;
        }
        $validated=hrm_pay_core_005_validate_plan($entry['plan'],$error);
        if (isset($entry['plan']['plan_sha256']) && $entry['plan']['plan_sha256']!==$validated['plan_sha256']) {
            $error='native_producer_plan_identity_drift';return false;
        }
        $plan=$entry['plan'];$plan['plan_sha256']=$validated['plan_sha256'];
        if ($minor===null) $minor=$plan['currency_minor_units'];
        if ($minor!==$plan['currency_minor_units']) { $error='native_producer_currency_policy_ambiguous';return false; }
        $d=$material['descriptor'];$relationship=(string)$d['payroll_relationship_id'];
        if (isset($seen[$relationship])) { $error='native_producer_duplicate_relationship';return false; }
        $seen[$relationship]=true;
        $sources[]=array('snapshot_id'=>$entry['snapshot_id'],'snapshot_sha256'=>$material['snapshot_sha256'],
            'payroll_relationship_id'=>$relationship,'assignment_id'=>$d['assignment_id']===null?null:(string)$d['assignment_id'],
            'sealed_by'=>(string)$row['sealed_by'],'plan'=>$plan);
    }
    usort($sources,function($a,$b) { return pay_glb_001_native_unsigned_compare($a['payroll_relationship_id'],$b['payroll_relationship_id']); });
    $mapped=pay_glb_001_native_producer_scope($period_id,$minor,$error,$purpose);if ($mapped===false) return false;
    $population=pay_glb_001_native_producer_population($mapped,$error);if ($population===false) return false;
    if (count($sources)!==count($population)) { $error='native_producer_complete_population_required';return false; }
    foreach ($sources as $index=>$source) {
        $expected=$population[$index];$d=$source['plan']['snapshot_material']['descriptor'];$scope=$mapped['scope'];
        if ($source['payroll_relationship_id']!==(string)$expected['payroll_relationship_id']
            || (string)$d['legal_entity_id']!==$scope['legal_entity_id'] || (string)$d['calendar_period_id']!==$scope['calendar_period_id']
            || $d['currency']!==$scope['currency_code'] || $source['plan']['currency']!==$scope['currency_code']) {
            $error='native_producer_source_scope_mismatch';return false;
        }
        $assignments=array();foreach ($expected['assignments'] as $member) $assignments[]=(string)$member['assignment_id'];
        if ($source['assignment_id']!==null && (!in_array($source['assignment_id'],$assignments,true) || count($assignments)!==1)) {
            $error='native_producer_complete_assignment_scope_required';return false;
        }
        if ($source['assignment_id']===null && count($assignments)>1) {
            if ($source['plan']['scenario']!=='multi_assignment' || !isset($source['plan']['metadata']['assignment_ids'])) {
                $error='native_producer_complete_assignment_plan_required';return false;
            }
            $planned=array_map('strval',$source['plan']['metadata']['assignment_ids']);
            if ($planned!==$assignments) { $error='native_producer_assignment_plan_mismatch';return false; }
        }
    }
    $context=array('schema'=>'PAY_GLB_001_NATIVE_OPERATION_V1','producer_contract_version'=>pay_glb_001_native_producer_version(),
        'scope'=>$mapped['scope'],'scope_evidence'=>$mapped['evidence'],'population'=>$population,
        'population_sha256'=>pay_glb_001_native_producer_hash('population',$population),'sources'=>$sources,
        'source_manifest_sha256'=>pay_glb_001_native_producer_hash('source-manifest',$sources),
        'command_key_sha256'=>$command_key_sha);
    $json=hrm_pay_core_004_canonical_json($context);
    if ($json===false || strlen($json)>16777216) { $error='native_producer_operation_context_limit';return false; }
    $context['request_sha256']=pay_glb_001_native_producer_hash('request',$context);
    return $context;
}

/** Stable replay scope includes company and actual mapped period/group/entity. */
function pay_glb_001_native_producer_scope_hash(array $context)
{
    return pay_glb_001_native_producer_hash('scope',$context['scope']);
}

/** Load one exact command with SQL ownership fencing; ambiguity denies. */
function pay_glb_001_native_producer_command($domain,$scope_sha,$key,&$error=null)
{
    $error=null;
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)user_company()
        .' AND command_domain='.db_escape($domain).' AND scope_sha256='.db_escape($scope_sha).' AND command_key_sha256='.db_escape($key).' LIMIT 2 FOR UPDATE');
    if (!$q || db_num_rows($q)>1) { $error='native_producer_command_lookup_failed';return false; }
    return db_num_rows($q)===1?db_fetch_assoc($q):null;
}

/** Verify typed persisted command bytes and immutable actor/request/effect identity. */
function pay_glb_001_native_producer_command_valid(array $row,$request_sha,$actor,&$error=null)
{
    $d=pay_glb_001_native_receipt_decode_v1('command',$row['receipt_json'],$error);
    if ($d===false || pay_glb_001_native_helper_id($d['effect_id'],$error)===false || $d['request_sha256']!==$request_sha || $d['actor_id']!==(string)$actor
        || $d['purpose_code']!=='PAY_GLB_001_NATIVE_EXECUTE') { $error='native_producer_command_replay_mismatch';return false; }
    foreach ($d as $field=>$value) if ($value!==($row[$field]===null?null:(string)$row[$field])) {
        $error='native_producer_command_column_mismatch';return false;
    }
    if (pay_glb_001_native_receipt_sha256_v1('command',$d,$error)!==$row['command_sha256']) {
        $error='native_producer_command_integrity_mismatch';return false;
    }
    return $d;
}

/** Internal insert into only the three owned immutable producer custody tables. */
/** Preserve raw typed JSON while retaining the normal SQL boundary for scalar custody fields. */
function pay_glb_001_native_custody_sql_value($column,$value,&$error=null)
{
    if ($value===null) return 'NULL';
    $literal=substr($column,-5)==='_json'?hrm_pay_core_004_json_sql_literal($value):db_escape((string)$value);
    if ($literal===false) $error='native_custody_json_storage_invalid';
    return $literal;
}

/** Insert the existing allowlisted immutable native columns in the owned transaction. */
function pay_glb_001_native_producer_insert($suffix,array $values,&$error=null)
{
    if (!in_array($suffix,array('native_command_receipts','native_executions','native_members'),true)
        || empty($GLOBALS['transaction_level']) || pay_glb_001_native_execution_principal()===false) { $error='native_producer_insert_table_or_transaction_denied';return false; }
    $allowed=array();
    foreach (pay_glb_001_native_schema_definitions() as $model) if ($model['name']==='hrm_pay_glb_001_'.$suffix)
        foreach ($model['columns'] as $column) if ($column['extra']!=='auto_increment' && $column['name']!=='created_at') $allowed[]=$column['name'];
    foreach ($values as $column=>$value) if (!in_array($column,$allowed,true)) { $error='native_producer_insert_column_denied';return false; }
    $columns=array();$literals=array();
    foreach ($values as $column=>$value) { $columns[]=$column;$literal=pay_glb_001_native_custody_sql_value($column,$value,$error);if($literal===false)return false;$literals[]=$literal; }
    if (!db_query('INSERT INTO '.TB_PREF.'hrm_pay_glb_001_'.$suffix.' ('.implode(',',$columns).') VALUES ('.implode(',',$literals).')')) {
        $error='native_producer_custody_insert_failed';return false;
    }
    return (string)db_insert_id();
}

/** Append actor-bound replay and source context only inside the caller's transaction. */
function pay_glb_001_native_producer_record_command($domain,array $context,$key,$effect_kind,$effect_id,$effect_sha,array $effect_context,&$error=null,$predecessor_sha=null)
{
    $principal=pay_glb_001_native_execution_principal();
    if ($principal===false || empty($GLOBALS['transaction_level'])) { $error='native_producer_command_transaction_required';return false; }
    $d=array('company_id'=>(string)$principal['company_id'],'command_domain'=>$domain,
        'scope_sha256'=>pay_glb_001_native_producer_scope_hash($context),'command_key_sha256'=>$key,
        'expected_predecessor_sha256'=>$predecessor_sha,'request_sha256'=>$context['request_sha256'],
        'effect_kind'=>$effect_kind,'effect_id'=>$effect_id,'effect_sha256'=>$effect_sha,
        'actor_id'=>(string)$principal['actor_user_id'],'purpose_code'=>'PAY_GLB_001_NATIVE_EXECUTE');
    $json=pay_glb_001_native_receipt_canonical_v1('command',$d,$error);if ($json===false) return false;
    $values=$d;$values['command_sha256']=pay_glb_001_native_receipt_sha256_v1('command',$d,$error);
    $values['request_context_json']=hrm_pay_core_004_canonical_json($effect_context);$values['receipt_json']=$json;
    if ($values['request_context_json']===false || strlen($values['request_context_json'])>16777216) { $error='native_producer_effect_context_invalid';return false; }
    return pay_glb_001_native_producer_insert('native_command_receipts',$values,$error);
}

/** Revalidate current source grant after work and before every immutable commit. */
function pay_glb_001_native_producer_admit(array $context,&$error=null)
{
    $fields=array('company_id','legal_entity_id','payroll_group_id','group_version_id',
        'payroll_period_id','calendar_period_id','entity_code','country_code','currency_code','currency_minor_units');
    if (!isset($context['scope']) || !is_array($context['scope']) || count($context['scope'])!==count($fields)
        || array_diff($fields,array_keys($context['scope']))) { $error='native_producer_admission_scope_invalid';return false; }
    // CORE-004 canonical JSON sorts object keys. Restore only the exact field
    // order required by the typed admission boundary; preserve every value/type.
    $scope=array();foreach ($fields as $field) $scope[$field]=$context['scope'][$field];
    return pay_glb_001_native_execution_admission($scope,$context['request_sha256'],
        $context['population_sha256'],$context['source_manifest_sha256'],$error);
}

/** Hold one native audit-key depth for reads or writes until actual outer SQL completion. */
function pay_glb_001_native_producer_hold_key_lock($company,&$error=null)
{
    $company=(int)$company;
    if (empty($GLOBALS['transaction_level'])) { $error='native_producer_audit_transaction_required';return false; }
    if (!function_exists('register_transaction_resource_release')) { $error='native_producer_outer_transaction_cleanup_required';return false; }
    if (isset($GLOBALS['pay_glb_001_native_key_lock_company']) && $GLOBALS['pay_glb_001_native_key_lock_company']!==$company) {
        $error='native_producer_key_mutex_scope_mismatch';return false;
    }
    if (!register_transaction_resource_release('pay_glb_001_native_audit_key','pay_glb_001_native_producer_release_key_lock')) {
        $error='native_producer_outer_transaction_cleanup_required';return false;
    }
    if (!isset($GLOBALS['pay_glb_001_native_key_lock_company'])) {
        if (!domain_audit_acquire_key_lock($company)) { $error='native_producer_key_mutex_required';return false; }
        $GLOBALS['pay_glb_001_native_key_lock_company']=$company;
    }
    return true;
}

/** Couple real protected PAY-AUD event/outbox/checkpoint with the outer business effect. */
function pay_glb_001_native_producer_audit(array $context,$action,$effect_sha,&$error=null)
{
    $company=(int)$context['scope']['company_id'];
    if (!pay_glb_001_native_producer_hold_key_lock($company,$error)) return false;
    $receipt=append_domain_audit_event(array('company_id'=>(int)$context['scope']['company_id'],
        'legal_entity_id'=>(int)$context['scope']['legal_entity_id'],'entity_type'=>'pay_glb_native_execution',
        'entity_key'=>array('request_sha256'=>$context['request_sha256'],'effect_sha256'=>$effect_sha),
        'action'=>'pay_glb.native.'.$action,'reason_code'=>'native_producer_v1','restricted_diff'=>array()));
    if (!is_array($receipt) || !isset($receipt['event_id']) || (int)$receipt['event_id']<=0) {
        $error='native_producer_atomic_audit_required';return false;
    }
    return $receipt;
}

/** Release the native owned key depth only after the actual outer SQL completion. */
function pay_glb_001_native_producer_release_key_lock()
{
    if (!empty($GLOBALS['transaction_level'])) return false;
    if (isset($GLOBALS['pay_glb_001_native_key_lock_company'])) {
        $company=$GLOBALS['pay_glb_001_native_key_lock_company'];
        if (!domain_audit_release_key_lock($company)) return false;
        unset($GLOBALS['pay_glb_001_native_key_lock_company']);
    }
    return true;
}

/** Finish the owned transaction; registered cleanup belongs to its outermost owner. */
function pay_glb_001_native_producer_finish($commit,&$error=null)
{
    $finished=$commit?commit_transaction():cancel_transaction();
    if ($finished===false) {
        $error=$commit?'native_producer_commit_or_key_release_uncertain':'native_producer_rollback_key_release_uncertain';return false;
    }
    return true;
}

/** Start a complete source-bound operation; never adopt any pre-existing result. */
function pay_glb_001_native_producer_start($period_id,$command_key_sha,array $input,&$error=null,$replacement_selection_id=null)
{
    $principal=pay_glb_001_native_producer_boundary($error);if ($principal===false) return false;
    if(!pay_glb_001_native_producer_begin(null,$replacement_selection_id!==null,$error))return false;
    $context=$replacement_selection_id===null?pay_glb_001_native_producer_prepare($period_id,$command_key_sha,$input,$error):
        (function_exists('pay_glb_001_native_replacement_prepare')?pay_glb_001_native_replacement_prepare($period_id,$command_key_sha,$input,$replacement_selection_id,$error):false);
    if ($context===false || pay_glb_001_native_producer_admit($context,$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $scope_sha=pay_glb_001_native_producer_scope_hash($context);
    $existing=pay_glb_001_native_producer_command('native.start',$scope_sha,$command_key_sha,$error);
    if ($existing===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    if ($existing!==null) {
        $d=pay_glb_001_native_producer_command_valid($existing,$context['request_sha256'],$principal['actor_user_id'],$error);
        $stored=json_decode($existing['request_context_json'],true);
        if ($d===false || !is_array($stored) || !isset($stored['operation']) || hrm_pay_core_004_canonical_json($stored['operation'])!==hrm_pay_core_004_canonical_json($context)) {
            pay_glb_001_native_producer_finish(false,$error);$error='native_producer_start_replay_context_mismatch';return false;
        }
        if (!pay_glb_001_native_producer_finish(true,$error)) return false;return (string)$existing['command_receipt_id'];
    }
    foreach ($context['sources'] as $source) {
        $q=db_query('SELECT result_id FROM '.TB_PREF.'hrm_pay_core_004_results WHERE snapshot_id='.(int)$source['snapshot_id'].' LIMIT 1 FOR UPDATE');
        if (!$q || db_num_rows($q)!==0) { pay_glb_001_native_producer_finish(false,$error);$error='native_producer_historical_result_adoption_denied';return false; }
    }
    $admission=pay_glb_001_native_producer_admit($context,$error);
    if ($admission===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $id=pay_glb_001_native_producer_record_command('native.start',$context,$command_key_sha,'native_operation_seed',
        $context['sources'][0]['snapshot_id'],$context['request_sha256'],array('operation'=>$context,'admission'=>$admission),$error);
    if ($id===false || pay_glb_001_native_producer_audit($context,'operation_started',$context['request_sha256'],$error)===false
        || pay_glb_001_native_producer_admit($context,$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    if (!pay_glb_001_native_producer_finish(true,$error)) return false;return $id;
}

/**
 * Cancel only an owned unsealed attempt under fresh scoped admission and the
 * existing recovery permission. Retain all results/progress/audit evidence.
 * Current population drift may prevent completion, but cannot prevent a
 * separately authorized negative cancellation. Sealed executions are excluded.
 */
function pay_glb_001_native_producer_abort($operation_id,$reason_code,&$error=null)
{
    global $security_areas;
    $principal=pay_glb_001_native_producer_boundary($error);if ($principal===false) return false;
    if (pay_glb_001_native_helper_id($operation_id,$error)===false
        || !pay_glb_001_native_field_valid('selection','reason_code',$reason_code)
        || !user_check_access('SA_HRM_RECOVER_PAY_CORE_RESULT')) { $error='native_producer_recovery_permission_or_reason_invalid';return false; }
    if(!pay_glb_001_native_producer_begin($operation_id,false,$error))return false;
    $role=hrm_pay_core_004_read_one('SELECT areas,sections,inactive FROM '.TB_PREF.'security_roles WHERE id='.(int)$_SESSION['wa_current_user']->access.' LIMIT 2 FOR UPDATE');
    $area=isset($security_areas['SA_HRM_RECOVER_PAY_CORE_RESULT'][0])?$security_areas['SA_HRM_RECOVER_PAY_CORE_RESULT'][0]:null;
    if (!$role || $area===null || (int)$role['inactive']!==0 || !in_array((string)$area,explode(';',$role['areas']),true)
        || !in_array((string)($area & ~0xff),explode(';',$role['sections']),true)) {
        pay_glb_001_native_producer_finish(false,$error);$error='native_producer_current_recovery_permission_required';return false;
    }
    $start=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE command_receipt_id='.(int)$operation_id.' AND command_domain=\'native.start\' LIMIT 2 FOR UPDATE');
    $stored=$start?json_decode($start['request_context_json'],true):false;
    if (!is_array($stored) || !isset($stored['operation']) || !is_array($stored['operation'])
        || !isset($stored['operation']['request_sha256'],$stored['operation']['scope'],$stored['operation']['command_key_sha256'])) {
        pay_glb_001_native_producer_finish(false,$error);$error='native_producer_abort_operation_invalid';return false;
    }
    $context=$stored['operation'];$body=$context;unset($body['request_sha256']);
    if (pay_glb_001_native_producer_command_valid($start,$context['request_sha256'],$principal['actor_user_id'],$error)===false
        || pay_glb_001_native_producer_hash('request',$body)!==$context['request_sha256']
        || pay_glb_001_native_producer_admit($context,$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $seal=pay_glb_001_native_producer_command('native.seal',pay_glb_001_native_producer_scope_hash($context),$context['command_key_sha256'],$error);
    if ($seal!==null) { pay_glb_001_native_producer_finish(false,$error);$error='native_producer_sealed_or_ambiguous_attempt_cannot_abort';return false; }
    if(isset($context['replacement']) && (!function_exists('pay_glb_001_native_replacement_abort_allowed')
        || !pay_glb_001_native_replacement_abort_allowed($start,$context,$error))){pay_glb_001_native_producer_finish(false,$error);return false;}
    $key=pay_glb_001_native_producer_hash('abort-key',array('operation_sha256'=>$start['command_sha256']));
    $effect=array('operation_id'=>$operation_id,'operation_sha256'=>$start['command_sha256'],'reason_code'=>$reason_code);
    $sha=pay_glb_001_native_producer_hash('abort-effect',$effect);
    $existing=pay_glb_001_native_producer_command('native.abort',pay_glb_001_native_producer_scope_hash($context),$key,$error);
    if ($existing===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    if ($existing!==null) {
        $d=pay_glb_001_native_producer_command_valid($existing,$context['request_sha256'],$principal['actor_user_id'],$error);
        if ($d===false || $d['effect_kind']!=='native_abort' || $d['effect_id']!==$operation_id || $d['effect_sha256']!==$sha
            || hrm_pay_core_004_canonical_json(json_decode($existing['request_context_json'],true))!==hrm_pay_core_004_canonical_json($effect)) {
            pay_glb_001_native_producer_finish(false,$error);$error='native_producer_abort_replay_conflict';return false;
        }
        if (!pay_glb_001_native_producer_finish(true,$error)) return false;return (string)$existing['command_receipt_id'];
    }
    $id=pay_glb_001_native_producer_record_command('native.abort',$context,$key,'native_abort',$operation_id,$sha,$effect,$error,$start['command_sha256']);
    if ($id===false || pay_glb_001_native_producer_audit($context,'operation_aborted',$sha,$error)===false
        || pay_glb_001_native_producer_admit($context,$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    if (!pay_glb_001_native_producer_finish(true,$error)) return false;return $id;
}

/** Reload and rederive the exact frozen operation under the current source/actor grant. */
function pay_glb_001_native_producer_load($operation_id,&$error=null,$allow_readmission=false)
{
    $id=pay_glb_001_native_helper_id($operation_id,$error);if ($id===false) return false;
    $row=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE command_receipt_id='.$id.' AND command_domain=\'native.start\' LIMIT 2 FOR UPDATE');
    $principal=pay_glb_001_native_execution_principal();
    if (!$row || $principal===false || (string)$row['company_id']!==(string)$principal['company_id']) { $error='native_producer_operation_missing_or_denied';return false; }
    $stored=json_decode($row['request_context_json'],true);
    if (!is_array($stored) || !isset($stored['operation'],$stored['admission']) || !is_array($stored['operation'])) { $error='native_producer_operation_context_invalid';return false; }
    $context=$stored['operation'];
    if (pay_glb_001_native_producer_command_valid($row,$context['request_sha256'],$principal['actor_user_id'],$error)===false) return false;
    $aborted=pay_glb_001_native_producer_command('native.abort',pay_glb_001_native_producer_scope_hash($context),
        pay_glb_001_native_producer_hash('abort-key',array('operation_sha256'=>$row['command_sha256'])),$error);
    if ($aborted!==null) { $error='native_producer_operation_aborted_or_abort_lookup_failed';return false; }
    $current=pay_glb_001_native_producer_rebuild($context,$principal['actor_user_id'],'native_execute',$error);
    $admission=$current===false?false:pay_glb_001_native_producer_admit($current,$error);
    if ($current===false || $admission===false || hrm_pay_core_004_canonical_json($current)!==hrm_pay_core_004_canonical_json($context)) {
        $error='native_producer_operation_source_or_admission_drift';return false;
    }
    $latest=pay_glb_001_native_producer_latest_readmission($operation_id,$row,$current,$principal['actor_user_id'],$error);
    if ($latest===false) return false;
    $previous=$latest===null?$stored['admission']:$latest['admission'];
    if (hrm_pay_core_004_canonical_json($admission)!==hrm_pay_core_004_canonical_json($previous)
        && (!$allow_readmission || $admission['revision']<=$previous['revision'])) {
        $error='native_producer_explicit_fresh_readmission_required';return false;
    }
    return array('id'=>$operation_id,'row'=>$row,'context'=>$current,'admission'=>$admission,
        'previous_admission'=>$previous,'previous_command'=>$latest===null?$row:$latest['row']);
}

/** Validate the latest immutable source reauthorization and its predecessor link. */
function pay_glb_001_native_producer_latest_readmission($operation_id,array $start,array $context,$actor,&$error=null)
{
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)$context['scope']['company_id']
        .' AND command_domain=\'native.readmit\' AND scope_sha256='.db_escape(pay_glb_001_native_producer_scope_hash($context))
        .' AND effect_kind=\'native_operation\' AND effect_id='.(int)$operation_id.' ORDER BY command_receipt_id DESC LIMIT 1 FOR UPDATE');
    if (!$q) { $error='native_producer_readmission_lookup_failed';return false; }
    if (db_num_rows($q)===0) return null;
    $row=db_fetch_assoc($q);$d=pay_glb_001_native_producer_command_valid($row,$context['request_sha256'],$actor,$error);
    $data=json_decode($row['request_context_json'],true);
    if ($d===false || !is_array($data) || !isset($data['admission'],$data['operation_id'],$data['operation_sha256'],$data['previous_command_receipt_id'],$data['previous_command_sha256']) || !is_array($data['admission'])
        || $data['operation_id']!==$operation_id || $data['operation_sha256']!==$start['command_sha256']
        || pay_glb_001_native_helper_id($data['previous_command_receipt_id'],$error)===false
        || pay_glb_001_native_unsigned_compare($data['previous_command_receipt_id'],(string)$row['command_receipt_id'])>=0
        || $d['expected_predecessor_sha256']!==$data['previous_command_sha256']
        || $d['effect_sha256']!==pay_glb_001_native_producer_hash('readmission',$data['admission'])) {
        $error='native_producer_readmission_receipt_invalid';return false;
    }
    $previous=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE command_receipt_id='.(int)$data['previous_command_receipt_id'].' LIMIT 2 FOR UPDATE');
    if (!$previous || pay_glb_001_native_producer_command_valid($previous,$context['request_sha256'],$actor,$error)===false
        || !in_array($previous['command_domain'],array('native.start','native.readmit'),true)
        || $previous['scope_sha256']!==$row['scope_sha256'] || $previous['command_sha256']!==$data['previous_command_sha256']
        || ($previous['command_domain']==='native.start' && (string)$previous['command_receipt_id']!==$operation_id)
        || ($previous['command_domain']==='native.readmit' && (string)$previous['effect_id']!==$operation_id)) {
        $error='native_producer_readmission_predecessor_invalid';return false;
    }
    return array('row'=>$row,'admission'=>$data['admission']);
}

/** Explicitly renew only an unchanged owned operation under a higher independently pinned source revision. */
function pay_glb_001_native_producer_readmit($operation_id,&$error=null)
{
    $principal=pay_glb_001_native_producer_boundary($error);if ($principal===false) return false;
    if(!pay_glb_001_native_producer_begin($operation_id,false,$error))return false;
    $operation=pay_glb_001_native_producer_load($operation_id,$error,true);
    if ($operation===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    if (hrm_pay_core_004_canonical_json($operation['admission'])===hrm_pay_core_004_canonical_json($operation['previous_admission'])) {
        $id=(string)$operation['previous_command']['command_receipt_id'];
        if (!pay_glb_001_native_producer_finish(true,$error)) return false;return $id;
    }
    $context=$operation['context'];$sha=pay_glb_001_native_producer_hash('readmission',$operation['admission']);
    $key=pay_glb_001_native_producer_hash('readmission-key',array('operation_sha256'=>$operation['row']['command_sha256'],'admission_sha256'=>$sha));
    $effect=array('operation_id'=>$operation_id,'operation_sha256'=>$operation['row']['command_sha256'],
        'previous_command_receipt_id'=>(string)$operation['previous_command']['command_receipt_id'],
        'previous_command_sha256'=>$operation['previous_command']['command_sha256'],'admission'=>$operation['admission']);
    $id=pay_glb_001_native_producer_record_command('native.readmit',$context,$key,'native_operation',$operation_id,$sha,$effect,$error,$operation['previous_command']['command_sha256']);
    $audit=$id===false?false:pay_glb_001_native_producer_audit($context,'operation_readmitted',$sha,$error);
    $current=pay_glb_001_native_producer_admit($context,$error);
    if ($id===false || $audit===false
        || $current===false || hrm_pay_core_004_canonical_json($current)!==hrm_pay_core_004_canonical_json($operation['admission'])) {
        pay_glb_001_native_producer_finish(false,$error);return false;
    }
    if (!pay_glb_001_native_producer_finish(true,$error)) return false;return $id;
}

/** Stable per-member effect key; callers cannot choose result identities. */
function pay_glb_001_native_producer_member_key(array $operation,$relationship_id)
{
    return pay_glb_001_native_producer_hash('member-key',array('operation_sha256'=>$operation['row']['command_sha256'],'payroll_relationship_id'=>$relationship_id));
}

/** Calculate one admitted member and commit its actual immutable result plus progress atomically. */
function pay_glb_001_native_producer_calculate($operation_id,$relationship_id,&$error=null)
{
    $principal=pay_glb_001_native_producer_boundary($error);if ($principal===false) return false;
    if (pay_glb_001_native_helper_id($relationship_id,$error)===false) return false;
    if(!pay_glb_001_native_producer_begin($operation_id,false,$error))return false;
    $operation=pay_glb_001_native_producer_load($operation_id,$error);
    if ($operation===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $context=$operation['context'];$source=null;
    foreach ($context['sources'] as $candidate) if ($candidate['payroll_relationship_id']===$relationship_id) $source=$candidate;
    if ($source===null) { pay_glb_001_native_producer_finish(false,$error);$error='native_producer_member_out_of_scope';return false; }
    $key=pay_glb_001_native_producer_member_key($operation,$relationship_id);
    $existing=pay_glb_001_native_producer_command('native.member',pay_glb_001_native_producer_scope_hash($context),$key,$error);
    if ($existing===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $bundle=pay_glb_001_native_producer_bundle($source,$context,$error);
    if ($bundle===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    if ($existing!==null) {
        $d=pay_glb_001_native_producer_command_valid($existing,$context['request_sha256'],$principal['actor_user_id'],$error);
        $integrity=$d===false?false:hrm_pay_core_004_recompute_integrity((int)$d['effect_id'],$error);
        $stored=json_decode($existing['request_context_json'],true);
        if ($d===false || $d['effect_kind']!=='core004_result' || $integrity===false
            || $d['effect_sha256']!==$bundle['result']['result_sha256']
            || $integrity['result_sha256']!==$d['effect_sha256'] || (string)$integrity['snapshot_id']!==$source['snapshot_id']
            || $integrity['executor_id']!==$principal['actor_user_id'] || !is_array($stored)
            || hrm_pay_core_004_canonical_json($stored)!==hrm_pay_core_004_canonical_json(array('operation_id'=>$operation_id,'operation_sha256'=>$operation['row']['command_sha256'],
                'payroll_relationship_id'=>$relationship_id,'snapshot_id'=>$source['snapshot_id'],'bundle_sha256'=>$bundle['bundle_sha256']))) {
            pay_glb_001_native_producer_finish(false,$error);$error='native_producer_member_replay_mismatch';return false;
        }
        if (!pay_glb_001_native_producer_finish(true,$error)) return false;return $d['effect_id'];
    }
    $seal=pay_glb_001_native_producer_command('native.seal',pay_glb_001_native_producer_scope_hash($context),$context['command_key_sha256'],$error);
    if ($seal!==null) { pay_glb_001_native_producer_finish(false,$error);$error='native_producer_operation_already_sealed';return false; }
    $q=db_query('SELECT result_id FROM '.TB_PREF.'hrm_pay_core_004_results WHERE snapshot_id='.(int)$source['snapshot_id'].' LIMIT 1 FOR UPDATE');
    if (!$q || db_num_rows($q)!==0) { pay_glb_001_native_producer_finish(false,$error);$error='native_producer_unowned_result_denied';return false; }
    $result_id=hrm_pay_core_004_append_result((int)$source['snapshot_id'],$bundle['result'],$bundle['lines'],$bundle['traces'],$error);
    $integrity=$result_id===false?false:hrm_pay_core_004_recompute_integrity($result_id,$error);
    if ($integrity===false || $integrity['result_sha256']!==$bundle['result']['result_sha256']) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $effect_context=array('operation_id'=>$operation_id,'operation_sha256'=>$operation['row']['command_sha256'],
        'payroll_relationship_id'=>$relationship_id,'snapshot_id'=>$source['snapshot_id'],'bundle_sha256'=>$bundle['bundle_sha256']);
    $receipt=pay_glb_001_native_producer_record_command('native.member',$context,$key,'core004_result',(string)$result_id,
        $bundle['result']['result_sha256'],$effect_context,$error);
    if ($receipt===false || pay_glb_001_native_producer_audit($context,'member_calculated',$bundle['result']['result_sha256'],$error)===false
        || pay_glb_001_native_producer_admit($context,$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    if (!pay_glb_001_native_producer_finish(true,$error)) return false;return (string)$result_id;
}

/** Validate the latest independent CORE-004 integrity receipt and its exact source hash. */
function pay_glb_001_native_producer_integrity($result_id,$executor,&$error=null)
{
    $row=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_004_integrity_evidence WHERE result_id='.(int)$result_id.' ORDER BY integrity_evidence_id DESC LIMIT 1 FOR UPDATE');
    if (!$row || $row['verification_status']!=='verified' || (int)$row['verified_by']===$executor
        || (int)$row['verified_by']<=0 || !pay_glb_001_sha256($row['evidence_sha256'])) {
        $error='native_producer_current_independent_integrity_required';return false;
    }
    $d=array('result_id'=>(int)$row['result_id'],'snapshot_sha256'=>$row['snapshot_sha256'],
        'result_sha256'=>$row['result_sha256'],'line_set_sha256'=>$row['line_set_sha256'],
        'trace_set_sha256'=>$row['trace_set_sha256'],'verification_status'=>$row['verification_status'],
        'evidence_sha256'=>$row['evidence_sha256'],'verified_by'=>(int)$row['verified_by']);
    if (hrm_pay_core_004_hash($d)!==$row['integrity_sha256']) { $error='native_producer_integrity_receipt_invalid';return false; }
    return $row;
}

/** Derive every member only from this operation's committed actual calculator effects. */
function pay_glb_001_native_producer_complete_material(array $operation,&$error=null,$producer_actor=null,$history=null,$verification_purpose='native_select')
{
    $context=$operation['context'];$principal=pay_glb_001_native_producer_verification_principal($producer_actor,$verification_purpose);
    if ($principal===false) { $error='native_producer_principal_revoked';return false; }
    $key='NATIVE-'.pay_glb_001_native_producer_hash('execution-key',array('operation_sha256'=>$operation['row']['command_sha256']));
    $members=array();$commands=array();$proofs=array();$items=array();
    $totals=array('gross_8'=>'0.00000000','deduction_8'=>'0.00000000','net_8'=>'0.00000000');
    foreach ($context['sources'] as $index=>$source) {
        $command=pay_glb_001_native_producer_command('native.member',pay_glb_001_native_producer_scope_hash($context),
            pay_glb_001_native_producer_member_key($operation,$source['payroll_relationship_id']),$error);
        if (!is_array($command)) { $error='native_producer_partial_operation_cannot_seal';return false; }
        $d=pay_glb_001_native_producer_command_valid($command,$context['request_sha256'],$principal['actor_user_id'],$error);
        $bundle=pay_glb_001_native_producer_bundle($source,$context,$error);
        $integrity=$d===false?false:hrm_pay_core_004_recompute_integrity((int)$d['effect_id'],$error);
        $proof=$integrity===false?false:($history===null?pay_glb_001_native_producer_integrity($d['effect_id'],$principal['actor_user_id'],$error):
            pay_glb_001_native_producer_historical_proof($d['effect_id'],$principal['actor_user_id'],isset($history[$index])?$history[$index]:null,$error));
        $effect=json_decode($command['request_context_json'],true);
        $expected_effect=$bundle===false?null:array('operation_id'=>$operation['id'],'operation_sha256'=>$operation['row']['command_sha256'],
            'payroll_relationship_id'=>$source['payroll_relationship_id'],'snapshot_id'=>$source['snapshot_id'],'bundle_sha256'=>$bundle['bundle_sha256']);
        if ($d===false || $bundle===false || $integrity===false || $proof===false || $d['effect_kind']!=='core004_result'
            || $integrity['executor_id']!==$principal['actor_user_id'] || (string)$integrity['snapshot_id']!==$source['snapshot_id']
            || $d['effect_sha256']!==$bundle['result']['result_sha256']
            || hrm_pay_core_004_canonical_json($effect)!==hrm_pay_core_004_canonical_json($expected_effect)) {
            $error=$error===null?'native_producer_owned_member_required':$error;return false;
        }
        foreach (array('snapshot_sha256','result_sha256','line_set_sha256','trace_set_sha256') as $field)
            if ($integrity[$field]!==$bundle['result'][$field] || $proof[$field]!==$integrity[$field]) { $error='native_producer_member_hash_mismatch';return false; }
        $superseded=db_query('SELECT lineage_id FROM '.TB_PREF.'hrm_pay_core_004_correction_lineage WHERE predecessor_result_id='.(int)$d['effect_id'].' LIMIT 1 FOR UPDATE');
        if (!$superseded || ($history===null && db_num_rows($superseded)>0)) { $error='native_producer_superseded_result_or_lineage_read_denied';return false; }
        if (isset($source['retro']) && $source['retro']!==null
            && (!function_exists('pay_glb_001_native_replacement_execution_proof')
            || pay_glb_001_native_replacement_execution_proof($operation,$source,$d['effect_id'],$error)===false)) return false;
        $member=array('native_execution_key'=>$key,'ordinal'=>(string)($index+1),'payroll_relationship_id'=>$source['payroll_relationship_id'],
            'assignment_id'=>$source['assignment_id'],'core004_snapshot_id'=>$source['snapshot_id'],'core004_result_id'=>$d['effect_id'],
            'snapshot_sha256'=>$integrity['snapshot_sha256'],'result_sha256'=>$integrity['result_sha256'],
            'line_set_sha256'=>$integrity['line_set_sha256'],'trace_set_sha256'=>$integrity['trace_set_sha256'],
            'joint_core009_run_item_id'=>null,'joint_core009_document_sha256'=>null,'cross_domain_parity_sha256'=>null);
        if (!pay_glb_001_native_receipt_validate('member',$member,$error)) return false;
        $members[]=$member;
        $commands[]=array('command_receipt_id'=>(string)$command['command_receipt_id'],'command_sha256'=>$command['command_sha256']);
        $proofs[]=array('result_id'=>$d['effect_id'],'integrity_evidence_id'=>(string)$proof['integrity_evidence_id'],'integrity_sha256'=>$proof['integrity_sha256']);
        $items[]=array('result_id'=>(int)$d['effect_id'],'result_sha256'=>$integrity['result_sha256']);
        foreach (array('gross_8'=>'gross_amount','deduction_8'=>'deduction_amount','net_8'=>'net_amount') as $field=>$amount) {
            $totals[$field]=pay_glb_001_native_decimal_add($totals[$field],$bundle['result'][$amount],$error);
            if ($totals[$field]===false) return false;
        }
    }
    if (!pay_glb_001_native_totals_match($totals['gross_8'],$totals['deduction_8'],$totals['net_8'])) { $error='native_producer_complete_totals_invalid';return false; }
    $set=pay_glb_001_native_member_set_v1($members,$error);if ($set===false) return false;
    usort($items,function($a,$b) { return $a['result_id']===$b['result_id']?0:($a['result_id']<$b['result_id']?-1:1); });
    // Same explicit ID/hash serialization as CORE-010; never use its float totals.
    return array('native_execution_key'=>$key,'members'=>$members,'member_set'=>$set,'member_commands'=>$commands,
        'integrity_proofs'=>$proofs,'core010_result_set_sha256'=>hrm_pay_core_010_hash($items),'totals'=>$totals);
}

/** Resolve only a verification owner; independent checking never grants execution rights. */
function pay_glb_001_native_producer_verification_principal($producer_actor=null,$purpose='native_select')
{
    if ($producer_actor===null) return pay_glb_001_native_execution_principal();
    if (!is_int($producer_actor) || $producer_actor<=0 || empty($GLOBALS['transaction_level'])
        || ($purpose==='native_select'?(!function_exists('pay_glb_001_native_selection_principal') || pay_glb_001_native_selection_principal()===false):
            ($purpose==='native_replace'?(!function_exists('pay_glb_001_native_replacement_principal') || pay_glb_001_native_replacement_principal()===false):
                ($purpose!=='native_retro_review' || !function_exists('pay_glb_001_native_correction_review_principal') || pay_glb_001_native_correction_review_principal()===false)))) return false;
    // The actual current checker was authenticated above. This value is only
    // the immutable historical owner used by existing result/receipt checks.
    return array('actor_user_id'=>$producer_actor);
}

/** Check a sealed immutable execution and exact member rows before returning replay. */
function pay_glb_001_native_producer_sealed_replay(array $operation,array $material,array $command,&$error=null,$producer_actor=null,$verification_purpose='native_select')
{
    $context=$operation['context'];$principal=pay_glb_001_native_producer_verification_principal($producer_actor,$verification_purpose);
    $d=$principal===false?false:pay_glb_001_native_producer_command_valid($command,$context['request_sha256'],$principal['actor_user_id'],$error);
    if ($d===false || $d['effect_kind']!=='native_execution') { $error='native_producer_seal_replay_denied';return false; }
    $row=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_executions WHERE native_execution_id='.(int)$d['effect_id'].' LIMIT 2 FOR UPDATE');
    $receipt=$row?pay_glb_001_native_receipt_decode_v1('execution',$row['receipt_json'],$error):false;
    $stored=json_decode($command['request_context_json'],true);
    if ($receipt===false || !is_array($stored) || !isset($stored['event'],$stored['operation_id'])
        || $stored['operation_id']!==$operation['id'] || $receipt['native_execution_key']!==$material['native_execution_key']
        || $receipt['source_kind']!=='GOVERNED_NATIVE_V1' || $receipt['revision_no']!==(isset($context['replacement'])?$context['replacement']['revision_no']:'1')
        || ($row['predecessor_execution_id']===null?null:(string)$row['predecessor_execution_id'])!==(isset($context['replacement'])?$context['replacement']['predecessor_execution_id']:null)
        || $receipt['request_sha256']!==$context['request_sha256']
        || $receipt['producer_contract_version']!==$context['producer_contract_version']
        || $receipt['producer_event_sha256']!==pay_glb_001_native_producer_hash('seal-event',$stored['event'])
        || $receipt['member_set_sha256']!==$material['member_set']['member_set_sha256']
        || $receipt['core010_result_set_sha256']!==$material['core010_result_set_sha256']) {
        $error='native_producer_sealed_execution_mismatch';return false;
    }
    foreach ($material['totals'] as $field=>$value) if ($receipt[$field]!==$value) { $error='native_producer_sealed_totals_mismatch';return false; }
    foreach (array_slice(pay_glb_001_native_receipt_fields()['execution'],0,6) as $field)
        if ($receipt[$field]!==$context['scope'][$field]) { $error='native_producer_sealed_scope_mismatch';return false; }
    foreach ($receipt as $field=>$value) {
        if ($field==='predecessor_execution_sha256') { if ($value!==(isset($context['replacement'])?$context['replacement']['predecessor_execution_sha256']:null)) return false;continue; }
        $actual=$row[$field]===null?null:(string)$row[$field];
        if ($field==='produced_at_utc') $actual=str_replace(' ','T',$actual).'Z';
        if ($value!==$actual) { $error='native_producer_sealed_column_mismatch';return false; }
    }
    if (pay_glb_001_native_receipt_sha256_v1('execution',$receipt,$error)!==$row['execution_sha256']
        || $row['execution_sha256']!==$d['effect_sha256']) { $error='native_producer_sealed_hash_mismatch';return false; }
    $expected_event=array('operation_sha256'=>$operation['row']['command_sha256'],'request_sha256'=>$context['request_sha256'],
        'population_sha256'=>$context['population_sha256'],'source_manifest_sha256'=>$context['source_manifest_sha256'],
        'member_commands'=>$material['member_commands'],'integrity_proofs'=>$stored['event']['integrity_proofs'],
        'produced_at_utc'=>$receipt['produced_at_utc']);
    if (hrm_pay_core_004_canonical_json($stored['event'])!==hrm_pay_core_004_canonical_json($expected_event)
        || count($stored['event']['integrity_proofs'])!==count($material['members'])) { $error='native_producer_sealed_event_mismatch';return false; }
    foreach ($stored['event']['integrity_proofs'] as $index=>$proof) {
        if (!is_array($proof) || !isset($proof['integrity_evidence_id'],$proof['result_id'],$proof['integrity_sha256'])
            || pay_glb_001_native_helper_id($proof['integrity_evidence_id'],$error)===false
            || $proof['result_id']!==$material['members'][$index]['core004_result_id']) { $error='native_producer_sealed_proof_invalid';return false; }
        $old=hrm_pay_core_004_read_one('SELECT integrity_sha256,verification_status,verified_by FROM '.TB_PREF.'hrm_pay_core_004_integrity_evidence WHERE integrity_evidence_id='.(int)$proof['integrity_evidence_id'].' AND result_id='.(int)$proof['result_id'].' LIMIT 2 FOR UPDATE');
        if (!$old || $old['integrity_sha256']!==$proof['integrity_sha256'] || $old['verification_status']!=='verified' || (int)$old['verified_by']===$principal['actor_user_id']) {
            $error='native_producer_sealed_proof_source_mismatch';return false;
        }
    }
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_members WHERE native_execution_id='.(int)$d['effect_id'].' ORDER BY ordinal_no FOR UPDATE');
    if (!$q || db_num_rows($q)!==count($material['members'])) { $error='native_producer_sealed_member_count_mismatch';return false; }
    $index=0;
    while ($member=db_fetch_assoc($q)) {
        $expected=$material['members'][$index++];
        if (pay_glb_001_native_receipt_decode_v1('member',$member['receipt_json'],$error)!==$expected
            || pay_glb_001_native_receipt_sha256_v1('member',$expected,$error)!==$member['member_sha256']) {
            $error='native_producer_sealed_member_hash_mismatch';return false;
        }
        foreach ($expected as $field=>$value) {
            if ($field==='native_execution_key') continue;
            $column=$field==='ordinal'?'ordinal_no':$field;
            if ($value!==($member[$column]===null?null:(string)$member[$column])) { $error='native_producer_sealed_member_column_mismatch';return false; }
        }
    }
    return $d['effect_id'];
}

/** Seal only the independently verified complete owned member set, never a partial run. */
function pay_glb_001_native_producer_seal($operation_id,&$error=null)
{
    $principal=pay_glb_001_native_producer_boundary($error);if ($principal===false) return false;
    if(!pay_glb_001_native_producer_begin($operation_id,false,$error))return false;
    $operation=pay_glb_001_native_producer_load($operation_id,$error);
    if ($operation===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $context=$operation['context'];$material=pay_glb_001_native_producer_complete_material($operation,$error);
    if ($material===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $existing=pay_glb_001_native_producer_command('native.seal',pay_glb_001_native_producer_scope_hash($context),$context['command_key_sha256'],$error);
    if ($existing===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    if ($existing!==null) {
        $id=pay_glb_001_native_producer_sealed_replay($operation,$material,$existing,$error);
        if ($id===false || pay_glb_001_native_producer_admit($context,$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
        if (!pay_glb_001_native_producer_finish(true,$error)) return false;return $id;
    }
    $event=array('operation_sha256'=>$operation['row']['command_sha256'],'request_sha256'=>$context['request_sha256'],
        'population_sha256'=>$context['population_sha256'],'source_manifest_sha256'=>$context['source_manifest_sha256'],
        'member_commands'=>$material['member_commands'],'integrity_proofs'=>$material['integrity_proofs'],
        'produced_at_utc'=>gmdate('Y-m-d\\TH:i:s\\Z'));
    $d=array();$fields=pay_glb_001_native_receipt_fields();
    foreach (array_slice($fields['execution'],0,6) as $field) $d[$field]=$context['scope'][$field];
    $d+=array('source_kind'=>'GOVERNED_NATIVE_V1','producer_contract_version'=>$context['producer_contract_version'],
        'producer_event_sha256'=>pay_glb_001_native_producer_hash('seal-event',$event),'request_sha256'=>$context['request_sha256'],
        'native_execution_key'=>$material['native_execution_key'],'revision_no'=>isset($context['replacement'])?$context['replacement']['revision_no']:'1',
        'predecessor_execution_sha256'=>isset($context['replacement'])?$context['replacement']['predecessor_execution_sha256']:null,
        'joint_core009_run_id'=>null,'joint_core009_state_version'=>null,'joint_core009_state_sha256'=>null,'joint_producer_sha256'=>null,
        'member_count'=>(string)count($material['members']),'member_set_sha256'=>$material['member_set']['member_set_sha256'],
        'core010_result_set_sha256'=>$material['core010_result_set_sha256'],
        'gross_8'=>$material['totals']['gross_8'],'deduction_8'=>$material['totals']['deduction_8'],'net_8'=>$material['totals']['net_8'],
        'currency_code'=>$context['scope']['currency_code'],'maker_id'=>(string)$principal['actor_user_id'],'produced_at_utc'=>$event['produced_at_utc']);
    $json=pay_glb_001_native_receipt_canonical_v1('execution',$d,$error);
    if ($json===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $values=$d;unset($values['predecessor_execution_sha256']);$values['predecessor_execution_id']=isset($context['replacement'])?$context['replacement']['predecessor_execution_id']:null;
    $values['execution_sha256']=pay_glb_001_native_receipt_sha256_v1('execution',$d,$error);$values['receipt_json']=$json;
    $values['produced_at_utc']=str_replace(array('T','Z'),array(' ',''),$d['produced_at_utc']);
    $id=pay_glb_001_native_producer_insert('native_executions',$values,$error);
    if ($id===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    foreach ($material['members'] as $member) {
        $values=$member;unset($values['native_execution_key'],$values['ordinal']);
        $values['native_execution_id']=$id;$values['ordinal_no']=$member['ordinal'];
        $values['member_sha256']=pay_glb_001_native_receipt_sha256_v1('member',$member,$error);
        $values['receipt_json']=pay_glb_001_native_receipt_canonical_v1('member',$member,$error);
        if (pay_glb_001_native_producer_insert('native_members',$values,$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    }
    $sha=pay_glb_001_native_receipt_sha256_v1('execution',$d,$error);
    $receipt=pay_glb_001_native_producer_record_command('native.seal',$context,$context['command_key_sha256'],'native_execution',$id,$sha,
        array('operation_id'=>$operation_id,'event'=>$event),$error);
    if ($receipt===false || pay_glb_001_native_producer_audit($context,'execution_sealed',$sha,$error)===false
        || pay_glb_001_native_producer_admit($context,$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    if (!pay_glb_001_native_producer_finish(true,$error)) return false;return $id;
}
