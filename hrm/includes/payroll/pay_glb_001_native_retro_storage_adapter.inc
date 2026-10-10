<?php
/** Pure forward CORE-007/storage bridge; no source authentication, approval, DB write or lineage authority. */
require_once dirname(__FILE__).'/pay_core_007_contract.inc';
require_once dirname(__FILE__).'/pay_glb_001_native_calculation_adapter.inc';

/** Return the new forward identity without changing any old CORE-007 hash domain. */
function pay_glb_001_native_retro_storage_version() { return 'PAY_GLB_001_CORE007_STORAGE_V1'; }

/** Admit only losslessly representable actual DECIMAL(24,8) text to old scale-six arithmetic. */
function pay_glb_001_native_retro_decimal_6($value,&$error=null,$delta_storage=false)
{
    $error=null;
    $digits=$delta_storage===true?21:15;
    if (!is_bool($delta_storage) || !is_string($value) || preg_match('/^-?(?:0|[1-9][0-9]{0,'.$digits.'})\.[0-9]{6}00$/D',$value)!==1 || $value==='-0.00000000') {
        $error='native_retro_scale_six_lossless_required';return false;
    }
    return substr($value,0,-2);
}

/** Validate old helper IDs before casts; stored source identity is not an approval. */
function pay_glb_001_native_retro_context(array $context,array $plan,&$error=null)
{
    foreach (array('affected_result_id','original_result_id') as $field)
        if (!isset($context[$field]) || (!is_int($context[$field]) && !is_string($context[$field]))
            || pay_glb_001_native_helper_id((string)$context[$field],$error)===false) { $error='native_retro_context_id_invalid';return false; }
    $c=hrm_pay_core_007_historical_context($context,$error);
    if ($c===false || $c['provenance_state']!=='complete') { $error='native_retro_complete_historical_context_required';return false; }
    if (isset($context['context_sha256']) && $context['context_sha256']!==$c['context_sha256']) { $error='native_retro_context_identity_drift';return false; }
    if (!isset($plan['scenario'],$plan['metadata']['predecessor_result_sha256'],$plan['metadata']['lineage_evidence_sha256'])
        || !in_array($plan['scenario'],array('correction','reversal'),true)
        || $plan['metadata']['predecessor_result_sha256']!==$c['original_result_sha256']
        || !hrm_pay_core_007_is_sha256($plan['metadata']['lineage_evidence_sha256'])) { $error='native_retro_plan_predecessor_binding_invalid';return false; }
    $v=hrm_pay_core_005_validate_plan($plan,$error);
    if ($v===false || (isset($plan['plan_sha256']) && $plan['plan_sha256']!==$v['plan_sha256'])) { $error='native_retro_plan_identity_invalid';return false; }
    return $c;
}

/** Recalculate through actual CORE-007 and bind its historical identity to new storage hashes. */
function pay_glb_001_native_retro_recalculate_v1(array $context,array $plan,&$error=null)
{
    $error=null;$c=pay_glb_001_native_retro_context($context,$plan,$error);if ($c===false) return false;
    $original=hrm_pay_core_007_recalculate($c,$plan,$error);if ($original===false) return false;
    $bundle=pay_glb_001_native_calculation_execute_v1($plan,$error);
    if ($bundle===false || $bundle['core005_bundle_sha256']!==$original['bundle_sha256'] || $bundle['plan_sha256']!==$original['plan_sha256']) {
        $error='native_retro_calculation_identity_mismatch';return false;
    }
    $binding=array('contract_version'=>pay_glb_001_native_retro_storage_version(),
        'affected_result_id'=>(string)$c['affected_result_id'],'original_result_id'=>(string)$c['original_result_id'],
        'original_result_sha256'=>$c['original_result_sha256'],'original_snapshot_sha256'=>$c['original_snapshot_sha256'],
        'recompute_snapshot_sha256'=>$c['recompute_snapshot_material']['snapshot_sha256'],'context_sha256'=>$c['context_sha256'],
        'core007_recalculation_sha256'=>$original['recalculation_sha256'],'core005_plan_sha256'=>$original['plan_sha256'],
        'core005_bundle_sha256'=>$original['bundle_sha256'],'native_calculation_bundle_sha256'=>$bundle['bundle_sha256'],
        'native_calculation_storage_binding_sha256'=>$bundle['storage_binding_sha256'],
        'lineage_evidence_sha256'=>$plan['metadata']['lineage_evidence_sha256']);
    $json=hrm_pay_core_004_canonical_json($binding);if ($json===false) { $error='native_retro_binding_invalid';return false; }
    $sha=hash('sha256',"PAY-GLB-001|retro-storage|v1\n".$json);$last=0;
    foreach($bundle['traces'] as $trace)$last=max($last,(int)$trace['trace_seq']);
    $limits=hrm_pay_core_005_resource_limits();
    if (count($bundle['traces'])>=$limits['max_trace_rows'] || $last>=4294967295) { $error='native_retro_storage_trace_limit';return false; }
    $bundle['traces'][]=array('trace_seq'=>$last+1,'trace_kind'=>'pay_glb_001.retro_storage_v1',
        'artifact_sha256'=>hash('sha256',pay_glb_001_native_retro_storage_version()),'input_sha256'=>$original['recalculation_sha256'],
        'output_sha256'=>$sha,'value_mode'=>'hash_only','value_digest'=>$sha);
    $hashes=array();foreach($bundle['traces'] as $trace){$h=hrm_pay_core_004_trace_sha256($trace);if($h===false){$error='native_retro_trace_invalid';return false;}$hashes[]=$h;}
    $bundle['result']['trace_set_sha256']=hrm_pay_core_004_hash_set($hashes);unset($bundle['result']['result_sha256']);
    $bundle['result']['result_sha256']=hrm_pay_core_004_result_sha256($bundle['result']);
    if ($bundle['result']['result_sha256']===false) { $error='native_retro_result_invalid';return false; }
    $bundle['calculation_storage_contract_version']=$bundle['storage_contract_version'];$bundle['storage_contract_version']=pay_glb_001_native_retro_storage_version();
    $bundle['retro_storage_binding']=$binding;$bundle['retro_storage_binding_sha256']=$sha;unset($bundle['bundle_sha256']);
    $json=hrm_pay_core_004_canonical_json($bundle);if($json===false){$error='native_retro_bundle_invalid';return false;}
    $bundle['bundle_sha256']=hash('sha256',"PAY-GLB-001|retro-bundle|v1\n".$json);return $bundle;
}

/** Derive exact old-domain deltas from new stored originals, preserving a new versioned byte binding. */
function pay_glb_001_native_retro_compute_deltas_v1(array $context,array $plan,array $stored_original_lines,&$error=null)
{
    $error=null;$c=pay_glb_001_native_retro_context($context,$plan,$error);if($c===false)return false;
    if(count($stored_original_lines)<1 || count($stored_original_lines)>4096 || array_keys($stored_original_lines)!==range(0,count($stored_original_lines)-1)) {
        $error='native_retro_original_line_bound';return false;
    }
    $raw_json=hrm_pay_core_004_canonical_json($stored_original_lines);
    if($raw_json===false || strlen($raw_json)>16777216){$error='native_retro_original_line_payload_invalid';return false;}
    $lines=$stored_original_lines;
    foreach($lines as &$line){
        if(!is_array($line) || !isset($line['element_version_id'],$line['line_kind'],$line['currency'],$line['amount'])
            || (!is_int($line['element_version_id']) && !is_string($line['element_version_id']))
            || pay_glb_001_native_helper_id((string)$line['element_version_id'],$error)===false
            || !pay_glb_001_native_field_valid('execution','currency_code',$line['currency'])
            || $line['currency']!==$plan['currency']){unset($line);$error='native_retro_original_line_identity_invalid';return false;}
        $line['amount']=pay_glb_001_native_retro_decimal_6($line['amount'],$error);if($line['amount']===false){unset($line);return false;}
    }unset($line);
    $recomputed=hrm_pay_core_007_recalculate($c,$plan,$error);if($recomputed===false)return false;
    $d=hrm_pay_core_007_compute_deltas($lines,$recomputed['lines'],$error);if($d===false)return false;
    $rows=$d['rows'];foreach($rows as &$row){
        foreach(array('original_amount','recomputed_amount','delta_amount') as $field){
            if(!is_string($row[$field]) || preg_match('/^-?(?:0|[1-9][0-9]{0,21})\.[0-9]{6}$/D',$row[$field])!==1 || $row[$field]==='-0.000000'){
                unset($row);$error='native_retro_delta_decimal_storage_invalid';return false;
            }
            $row[$field].='00';
        }
        $row['core007_delta_sha256']=$row['delta_sha256'];unset($row['delta_sha256']);
        $json=hrm_pay_core_004_canonical_json($row);if($json===false){unset($row);$error='native_retro_stored_delta_invalid';return false;}
        $row['storage_delta_sha256']=hash('sha256',"PAY-GLB-001|retro-delta|v1\n".$json);
    }unset($row);
    $binding=array('contract_version'=>pay_glb_001_native_retro_storage_version(),'context_sha256'=>$c['context_sha256'],
        'plan_sha256'=>$recomputed['plan_sha256'],'recalculation_sha256'=>$recomputed['recalculation_sha256'],
        'original_stored_line_sha256'=>hash('sha256',$raw_json),
        'core007_delta_set_sha256'=>$d['delta_set_sha256'],'stored_rows'=>$rows);
    $json=hrm_pay_core_004_canonical_json($binding);if($json===false){$error='native_retro_delta_binding_invalid';return false;}
    return array('core007_deltas'=>$d,'stored_rows'=>$rows,'storage_binding'=>$binding,
        'storage_binding_sha256'=>hash('sha256',"PAY-GLB-001|retro-delta-storage|v1\n".$json));
}
