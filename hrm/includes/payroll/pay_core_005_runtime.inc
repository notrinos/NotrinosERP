<?php
/** PAY-CORE-005 deterministic, non-authoritative calculation planner/runtime. */
include_once(dirname(__FILE__).'/pay_core_005_contract.inc');
include_once(dirname(__FILE__).'/pay_core_004_contract.inc');
include_once(dirname(__FILE__).'/../payroll_formula_safe_profile.inc');

function hrm_pay_core_005_allowed_operations()
{
    return array('literal','identity','sum','subtract','multiply','divide','prorate','formula');
}

function hrm_pay_core_005_validate_snapshot_material(array $material,&$error=null)
{
    $error=null;
    foreach(array('descriptor','identity_sets','payload_json','snapshot_sha256') as $k)
        if(!array_key_exists($k,$material)){$error='pc5_snapshot_material_incomplete';return false;}
    if(!hrm_pay_core_004_validate_snapshot_descriptor($material['descriptor'])){$error='pc5_snapshot_descriptor_invalid';return false;}
    if(!hrm_pay_core_005_is_sha256($material['snapshot_sha256'])){$error='pc5_snapshot_sha_invalid';return false;}
    $expected=hrm_pay_core_004_snapshot_sha256($material['descriptor']);
    if($expected===false||!hash_equals($expected,$material['snapshot_sha256'])){$error='pc5_snapshot_sha_drift';return false;}
    $sets=$material['identity_sets'];
    if(!is_array($sets)){$error='pc5_snapshot_identity_sets_invalid';return false;}
    $required=array('approved_inputs','element_versions','eligibility','rule_artifacts','source_events','configuration');
    foreach($required as $name){
        if(!isset($sets[$name])||!is_array($sets[$name])){$error='pc5_snapshot_identity_set_missing';return false;}
        $copy=$sets[$name];foreach($copy as $h)if(!hrm_pay_core_005_is_sha256($h)){$error='pc5_snapshot_identity_hash_invalid';return false;}
        $unique=array_values(array_unique($copy));sort($unique,SORT_STRING);
        if(count($unique)!==count($copy)){$error='pc5_snapshot_identity_set_duplicate';return false;}
        if($unique!==array_values($copy)){$error='pc5_snapshot_identity_set_noncanonical';return false;}
        $descriptor_key=$name.'_sha256';
        $set_hash=hrm_pay_core_004_hash_set($copy);
        if($set_hash===false||!isset($material['descriptor'][$descriptor_key])||!hash_equals((string)$material['descriptor'][$descriptor_key],$set_hash)){$error='pc5_snapshot_identity_set_drift';return false;}
    }
    $payload=hrm_pay_core_004_canonical_json(array('descriptor'=>$material['descriptor'],'identity_sets'=>array(
        'approved_inputs'=>array_values($sets['approved_inputs']),
        'element_versions'=>array_values($sets['element_versions']),
        'eligibility'=>array_values($sets['eligibility']),
        'rule_artifacts'=>array_values($sets['rule_artifacts']),
        'source_events'=>array_values($sets['source_events']),
        'configuration'=>array_values($sets['configuration']),
    )));
    if($payload===false||!hash_equals($payload,(string)$material['payload_json'])){$error='pc5_snapshot_payload_drift';return false;}
    return true;
}

function hrm_pay_core_005_validate_formula_context(array $ctx,array $known_nodes,&$error=null)
{
    $error=null;$contract=hrm_payroll_formula_safe_variable_contract();
    if(array_keys($ctx)!==array_keys($contract)){$error='pc5_formula_context_exact_variables_required';return false;}
    foreach($contract as $name=>$spec){
        $value=$ctx[$name];
        if($spec['type']==='date'){
            if(!is_string($value)){ $error='pc5_formula_date_context_invalid'; return false; }
            try{hrm_payroll_formula_safe_normalize_date($value);}catch(Exception $e){$error='pc5_formula_date_context_invalid';return false;}
            continue;
        }
        if(is_array($value)){
            if(array_keys($value)!==array('node')||hrm_pay_core_005_token($value['node'])===false||!isset($known_nodes[$value['node']])){$error='pc5_formula_node_binding_invalid';return false;}
        } else {
            try{hrm_pay_core_005_decimal($value);}catch(Exception $e){$error='pc5_formula_decimal_context_invalid';return false;}
        }
    }
    return true;
}

function hrm_pay_core_005_validate_plan(array $plan,&$error=null)
{
    $error=null;$limits=hrm_pay_core_005_resource_limits();
    $required=array('contract_version','scenario','snapshot_sha256','snapshot_material','currency','currency_minor_units','nodes','metadata');
    foreach($required as $k)if(!array_key_exists($k,$plan)){$error='pc5_plan_missing_'.$k;return false;}
    if($plan['contract_version']!==HRM_PAY_CORE_005_CONTRACT_VERSION){$error='pc5_plan_contract_version_invalid';return false;}
    if(!in_array($plan['scenario'],hrm_pay_core_005_scenarios(),true)){$error='pc5_plan_scenario_invalid';return false;}
    if(!hrm_pay_core_005_is_sha256($plan['snapshot_sha256'])){$error='pc5_plan_snapshot_sha_invalid';return false;}
    if(!is_array($plan['snapshot_material'])||!hrm_pay_core_005_validate_snapshot_material($plan['snapshot_material'],$error)||!hash_equals($plan['snapshot_sha256'],$plan['snapshot_material']['snapshot_sha256'])){$error=$error?:'pc5_plan_snapshot_material_invalid';return false;}
    if(!is_string($plan['currency'])||preg_match('/^[A-Z]{3}$/D',$plan['currency'])!==1){$error='pc5_plan_currency_invalid';return false;}
    if(!is_int($plan['currency_minor_units'])||(int)$plan['currency_minor_units']<0||(int)$plan['currency_minor_units']>6){$error='pc5_plan_minor_units_invalid';return false;}
    if(!is_array($plan['metadata'])||!is_array($plan['nodes'])){$error='pc5_plan_structure_invalid';return false;}
    $hashable_plan=$plan;unset($hashable_plan['plan_sha256']);
    $json=hrm_pay_core_005_canonical_json($hashable_plan);if($json===false||strlen($json)>$limits['max_plan_bytes']){$error='pc5_plan_size_limit';return false;}
    $order=hrm_pay_core_005_topological_order($plan['nodes'],$error);if($order===false)return false;
    $by=array();foreach($plan['nodes'] as $n)$by[$n['id']]=$n;
    foreach($by as $id=>$node){
        foreach(array('operation','arguments','emit_line') as $k)if(!array_key_exists($k,$node)){$error='pc5_node_missing_'.$k;return false;}
        if(!in_array($node['operation'],hrm_pay_core_005_allowed_operations(),true)||!is_array($node['arguments'])||!is_bool($node['emit_line'])){$error='pc5_node_operation_invalid';return false;}
        $args=$node['arguments'];foreach($args as $arg)if(hrm_pay_core_005_token($arg)===false||!isset($by[$arg])){$error='pc5_node_argument_invalid';return false;}
        $dep=$node['dependencies'];$a=$args;$d=$dep;sort($a,SORT_STRING);sort($d,SORT_STRING);if($a!==$d){$error='pc5_node_argument_dependency_mismatch';return false;}
        $op=$node['operation'];
        if($op==='literal'){
            if(count($args)!==0||!array_key_exists('value',$node)){$error='pc5_literal_node_invalid';return false;}
            try{hrm_pay_core_005_decimal($node['value']);}catch(Exception $e){$error='pc5_literal_value_invalid';return false;}
        } elseif($op==='identity') { if(count($args)!==1){$error='pc5_identity_arity';return false;} }
        elseif(in_array($op,array('subtract','multiply','divide'),true)) { if(count($args)!==2){$error='pc5_binary_arity';return false;} }
        elseif($op==='sum') { if(count($args)<1){$error='pc5_sum_arity';return false;} }
        elseif($op==='prorate') { if(count($args)!==3){$error='pc5_prorate_arity';return false;} }
        elseif($op==='formula') {
            foreach(array('formula_text','formula_sha256','artifact_sha256','profile_sha256','formula_context') as $k)if(!array_key_exists($k,$node)){$error='pc5_formula_node_incomplete';return false;}
            if(!is_string($node['formula_text'])||!hrm_pay_core_005_is_sha256($node['formula_sha256'])||!hash_equals(hash('sha256',trim($node['formula_text'])),$node['formula_sha256'])||!hrm_pay_core_005_is_sha256($node['artifact_sha256'])||!in_array($node['artifact_sha256'],$plan['snapshot_material']['identity_sets']['rule_artifacts'],true)||!hrm_pay_core_005_is_sha256($node['profile_sha256'])||!in_array($node['profile_sha256'],$plan['snapshot_material']['identity_sets']['configuration'],true)||!is_array($node['formula_context'])){$error='pc5_formula_identity_invalid';return false;}
            if(!hrm_pay_core_005_validate_formula_context($node['formula_context'],$by,$error))return false;
            foreach($node['formula_context'] as $binding)if(is_array($binding)&&!in_array($binding['node'],$node['dependencies'],true)){$error='pc5_formula_binding_not_dependency';return false;}
        }
        if(isset($node['round_currency'])&&!is_bool($node['round_currency'])){$error='pc5_round_flag_invalid';return false;}
        if($node['emit_line']){
            foreach(array('element_version_id','element_version_sha256','line_kind','source_set','source_sha256','quantity','rate','line_order') as $k)if(!array_key_exists($k,$node)){$error='pc5_line_node_incomplete';return false;}
            $allowed_source_sets=array('approved_inputs','eligibility','rule_artifacts','source_events','configuration');
            if((int)$node['element_version_id']<=0||!hrm_pay_core_005_is_sha256($node['element_version_sha256'])||!in_array($node['element_version_sha256'],$plan['snapshot_material']['identity_sets']['element_versions'],true)||!in_array($node['line_kind'],array('earning','deduction','employer_contribution','information'),true)||!in_array($node['source_set'],$allowed_source_sets,true)||!hrm_pay_core_005_is_sha256($node['source_sha256'])||!in_array($node['source_sha256'],$plan['snapshot_material']['identity_sets'][$node['source_set']],true)||!is_int($node['line_order'])){$error='pc5_line_metadata_invalid';return false;}
            try{hrm_pay_core_005_decimal($node['quantity']);hrm_pay_core_005_decimal($node['rate']);}catch(Exception $e){$error='pc5_line_numeric_metadata_invalid';return false;}
        }
    }
    $scenario=$plan['scenario'];$meta=$plan['metadata'];
    if(in_array($scenario,array('correction','reversal'),true)){
        if(!isset($meta['predecessor_result_sha256'],$meta['lineage_evidence_sha256'])||!hrm_pay_core_005_is_sha256($meta['predecessor_result_sha256'])||!hrm_pay_core_005_is_sha256($meta['lineage_evidence_sha256'])){$error='pc5_lineage_intent_required';return false;}
    }
    if($scenario==='multi_assignment'){
        if(!isset($meta['assignment_ids'])||!is_array($meta['assignment_ids'])||count($meta['assignment_ids'])<2||count($meta['assignment_ids'])>$limits['max_scenario_assignments']){$error='pc5_multi_assignment_scope_invalid';return false;}
        $ids=array_map('intval',$meta['assignment_ids']);if(min($ids)<=0||count(array_unique($ids))!==count($ids)){$error='pc5_multi_assignment_scope_invalid';return false;}
    }
    if($scenario==='multi_relationship'){
        if(!isset($meta['relationship_ids'])||!is_array($meta['relationship_ids'])||count($meta['relationship_ids'])<2||count($meta['relationship_ids'])>$limits['max_scenario_assignments']){$error='pc5_multi_relationship_scope_invalid';return false;}
        $ids=array_map('intval',$meta['relationship_ids']);if(min($ids)<=0||count(array_unique($ids))!==count($ids)){$error='pc5_multi_relationship_scope_invalid';return false;}
    }
    return array('order'=>$order,'nodes'=>$by,'plan_sha256'=>hrm_pay_core_005_hash($hashable_plan));
}

function hrm_pay_core_005_build_plan(array $snapshot_material,$scenario,$currency_minor_units,array $nodes,array $metadata=array(),&$error=null)
{
    $error=null;if(!hrm_pay_core_005_validate_snapshot_material($snapshot_material,$error))return false;
    usort($nodes,function($a,$b){return strcmp(isset($a['id'])?(string)$a['id']:'',isset($b['id'])?(string)$b['id']:'');});
    foreach(array('assignment_ids','relationship_ids') as $scope_list)if(isset($metadata[$scope_list])&&is_array($metadata[$scope_list])){ $metadata[$scope_list]=array_values(array_map('intval',$metadata[$scope_list])); sort($metadata[$scope_list],SORT_NUMERIC); }
    $plan=array(
        'contract_version'=>HRM_PAY_CORE_005_CONTRACT_VERSION,
        'scenario'=>(string)$scenario,
        'snapshot_sha256'=>$snapshot_material['snapshot_sha256'],
        'snapshot_material'=>$snapshot_material,
        'currency'=>$snapshot_material['descriptor']['currency'],
        'currency_minor_units'=>(int)$currency_minor_units,
        'nodes'=>array_values($nodes),
        'metadata'=>$metadata,
    );
    $v=hrm_pay_core_005_validate_plan($plan,$error);if($v===false)return false;
    $plan['plan_sha256']=$v['plan_sha256'];return $plan;
}

function hrm_pay_core_005_formula_adapter(array $node,array $values,&$receipt=null,&$error=null)
{
    $receipt=null;$error=null;$context=array();
    foreach($node['formula_context'] as $name=>$binding){$context[$name]=is_array($binding)?$values[$binding['node']]:$binding;}
    $safe_receipt=null;$value=hrm_payroll_formula_safe_evaluate($node['formula_text'],$context,$safe_receipt,$error);if($value===false)return false;
    if(!is_array($safe_receipt)||!isset($safe_receipt['formula_sha256'],$safe_receipt['profile_sha256'])||!hash_equals($node['formula_sha256'],$safe_receipt['formula_sha256'])||!hash_equals($node['profile_sha256'],$safe_receipt['profile_sha256'])){$error='pc5_formula_artifact_drift';return false;}
    $receipt=array(
        'artifact_sha256'=>$node['artifact_sha256'],
        'formula_sha256'=>$safe_receipt['formula_sha256'],
        'profile_sha256'=>$safe_receipt['profile_sha256'],
        'runtime_version'=>$safe_receipt['runtime_version'],
        'result_unit'=>$safe_receipt['result_unit'],
    );
    return $value->decimalString();
}

function hrm_pay_core_005_execute_plan(array $plan,&$error=null)
{
    $error=null;$v=hrm_pay_core_005_validate_plan($plan,$error);if($v===false)return false;
    if(isset($plan['plan_sha256'])&&!hash_equals($v['plan_sha256'],(string)$plan['plan_sha256'])){$error='pc5_plan_sha_drift';return false;}
    $values=array();$traces=array();$line_candidates=array();$trace_seq=1;$limits=hrm_pay_core_005_resource_limits();
    foreach($v['order'] as $id){
        $node=$v['nodes'][$id];$op=$node['operation'];$args=array();foreach($node['arguments'] as $a)$args[]=$values[$a];
        try{
            if($op==='literal')$out=hrm_pay_core_005_decimal_string($node['value']);
            elseif($op==='identity')$out=$args[0];
            elseif($op==='sum'){$out='0.000000';foreach($args as $a)$out=hrm_pay_core_005_decimal_add($out,$a);}
            elseif($op==='subtract')$out=hrm_pay_core_005_decimal_sub($args[0],$args[1]);
            elseif($op==='multiply')$out=hrm_pay_core_005_decimal_mul($args[0],$args[1]);
            elseif($op==='divide')$out=hrm_pay_core_005_decimal_div($args[0],$args[1]);
            elseif($op==='prorate')$out=hrm_pay_core_005_prorate($args[0],$args[1],$args[2],null);
            elseif($op==='formula'){$formula_receipt=null;$out=hrm_pay_core_005_formula_adapter($node,$values,$formula_receipt,$error);if($out===false)return false;}
            else{$error='pc5_operation_denied';return false;}
            $out=hrm_pay_core_005_decimal_string($out); // re-normalize every intermediate to enforce magnitude/scale bounds
            if(!empty($node['round_currency']))$out=hrm_pay_core_005_round_currency($out,$plan['currency_minor_units']);
        }catch(Exception $e){$error=$e->getMessage();return false;}
        $values[$id]=$out;
        if($node['emit_line'])$line_candidates[]=array('id'=>$id,'value'=>$out,'node'=>$node);
        if($op!=='literal'){
            if(count($traces)>=$limits['max_trace_rows']){$error='pc5_trace_limit';return false;}
            $artifact=$op==='formula'?$node['artifact_sha256']:hrm_pay_core_005_hash(array('contract'=>HRM_PAY_CORE_005_CONTRACT_VERSION,'operation'=>$op));
            $input_hash=hrm_pay_core_005_hash(array('node_id'=>$id,'arguments'=>$args,'formula_context'=>$op==='formula'?$node['formula_context']:null));
            $output_hash=hrm_pay_core_005_hash(array('node_id'=>$id,'value'=>$out));
            $traces[]=array('trace_seq'=>$trace_seq++,'trace_kind'=>'pay_core_005.'.$op,'artifact_sha256'=>$artifact,'input_sha256'=>$input_hash,'output_sha256'=>$output_hash,'value_mode'=>'hash_only','value_digest'=>$output_hash);
        }
    }
    usort($line_candidates,function($a,$b){$x=$a['node']['line_order']<=>$b['node']['line_order'];return $x!==0?$x:strcmp($a['id'],$b['id']);});
    $lines=array();$gross='0.000000';$ded='0.000000';$line_no=1;
    foreach($line_candidates as $row){$n=$row['node'];$amount=hrm_pay_core_005_round_currency($row['value'],$plan['currency_minor_units']);
        $lines[]=array('line_no'=>$line_no++,'element_version_id'=>(int)$n['element_version_id'],'line_kind'=>$n['line_kind'],'currency'=>$plan['currency'],'amount'=>$amount,'quantity'=>hrm_pay_core_005_decimal_string($n['quantity']),'rate'=>hrm_pay_core_005_decimal_string($n['rate']),'source_sha256'=>$n['source_sha256']);
        if($n['line_kind']==='earning')$gross=hrm_pay_core_005_decimal_add($gross,$amount);
        elseif($n['line_kind']==='deduction')$ded=hrm_pay_core_005_decimal_add($ded,$amount);
    }
    $gross=hrm_pay_core_005_round_currency($gross,$plan['currency_minor_units']);$ded=hrm_pay_core_005_round_currency($ded,$plan['currency_minor_units']);$net=hrm_pay_core_005_round_currency(hrm_pay_core_005_decimal_sub($gross,$ded),$plan['currency_minor_units']);
    if($plan['scenario']==='zero_pay'&&(hrm_pay_core_005_decimal_compare($gross,'0')!==0||hrm_pay_core_005_decimal_compare($ded,'0')!==0||hrm_pay_core_005_decimal_compare($net,'0')!==0)){$error='pc5_zero_pay_nonzero_result';return false;}
    $line_hashes=array();foreach($lines as $line){$h=hrm_pay_core_004_result_line_sha256($line);if($h===false){$error='pc5_core004_line_projection_invalid';return false;}$line_hashes[]=$h;}
    $trace_hashes=array();foreach($traces as $trace){$h=hrm_pay_core_004_trace_sha256($trace);if($h===false){$error='pc5_core004_trace_projection_invalid';return false;}$trace_hashes[]=$h;}
    $result=array('snapshot_sha256'=>$plan['snapshot_sha256'],'currency'=>$plan['currency'],'gross_amount'=>$gross,'deduction_amount'=>$ded,'net_amount'=>$net,'line_set_sha256'=>hrm_pay_core_004_hash_set($line_hashes),'trace_set_sha256'=>hrm_pay_core_004_hash_set($trace_hashes));
    $result_sha=hrm_pay_core_004_result_sha256($result);if($result_sha===false){$error='pc5_core004_result_projection_invalid';return false;}$result['result_sha256']=$result_sha;
    $bundle_hash=hrm_pay_core_005_hash(array('plan_sha256'=>$v['plan_sha256'],'result'=>$result,'lines'=>$lines,'traces'=>$traces));
    return array('plan_sha256'=>$v['plan_sha256'],'result'=>$result,'lines'=>$lines,'traces'=>$traces,'node_values'=>$values,'bundle_sha256'=>$bundle_hash);
}

/** Deterministic bounded bisection over one explicit gross input node. */
function hrm_pay_core_005_solve_net_to_gross(array $plan,$gross_input_node,$target_net,$lower,$upper,$tolerance='0.000001',$max_iterations=64,&$error=null)
{
    $error=null;$gross_input_node=hrm_pay_core_005_token($gross_input_node);if($gross_input_node===false){$error='pc5_ntg_input_node_invalid';return false;}
    $v=hrm_pay_core_005_validate_plan($plan,$error);if($v===false||!isset($v['nodes'][$gross_input_node])||$v['nodes'][$gross_input_node]['operation']!=='literal'){$error='pc5_ntg_literal_gross_input_required';return false;}
    try{hrm_pay_core_005_decimal($target_net);hrm_pay_core_005_decimal($lower);hrm_pay_core_005_decimal($upper);hrm_pay_core_005_decimal($tolerance);}catch(Exception $e){$error='pc5_ntg_numeric_invalid';return false;}
    if(hrm_pay_core_005_decimal_compare($lower,$upper)>0||hrm_pay_core_005_decimal_compare($tolerance,'0')<=0){$error='pc5_ntg_bounds_invalid';return false;}
    $limit=min((int)$max_iterations,hrm_pay_core_005_resource_limits()['max_solver_iterations']);if($limit<1){$error='pc5_ntg_iteration_limit_invalid';return false;}
    $ids=array();foreach($plan['nodes'] as $i=>$n)$ids[$n['id']]=$i;$idx=$ids[$gross_input_node];$last=null;
    for($i=1;$i<=$limit;$i++){
        $mid=hrm_pay_core_005_decimal_div(hrm_pay_core_005_decimal_add($lower,$upper),'2');$candidate=$plan;$candidate['nodes'][$idx]['value']=$mid;unset($candidate['plan_sha256']);$bundle=hrm_pay_core_005_execute_plan($candidate,$exec_error);if($bundle===false){$error='pc5_ntg_execution_failed:'.$exec_error;return false;}
        $delta=hrm_pay_core_005_decimal_abs(hrm_pay_core_005_decimal_sub($bundle['result']['net_amount'],$target_net));$last=array('gross_amount'=>$mid,'bundle'=>$bundle,'iterations'=>$i,'delta'=>$delta);
        if(hrm_pay_core_005_decimal_compare($delta,$tolerance)<=0)return $last;
        if(hrm_pay_core_005_decimal_compare($bundle['result']['net_amount'],$target_net)<0)$lower=$mid;else$upper=$mid;
    }
    $error='pc5_ntg_non_convergence';return false;
}

function hrm_pay_core_005_replay_equal(array $a,array $b)
{
    foreach(array('plan_sha256','bundle_sha256') as $k)if(!isset($a[$k],$b[$k])||!hash_equals((string)$a[$k],(string)$b[$k]))return false;
    return hrm_pay_core_005_canonical_json($a['result'])===hrm_pay_core_005_canonical_json($b['result'])
        && hrm_pay_core_005_canonical_json($a['lines'])===hrm_pay_core_005_canonical_json($b['lines'])
        && hrm_pay_core_005_canonical_json($a['traces'])===hrm_pay_core_005_canonical_json($b['traces']);
}
