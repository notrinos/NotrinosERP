<?php
/** PAY-RULE-003 deterministic in-memory decision/rate simulation and explain foundation. */
include_once(__DIR__.'/decision_rate_plan.inc');
if (!defined('HRM_PAY_RULE_003_SIMULATOR_VERSION'))
    define('HRM_PAY_RULE_003_SIMULATOR_VERSION', '1.0.476');

function hrm_payroll_rule_sim_fail(&$error, $code)
{
    $error = $code;
    return false;
}

function hrm_payroll_rule_date_in_interval($date, $from, $to)
{
    return hrm_payroll_decision_table_valid_date($date) && strcmp($date,$from)>=0 && ($to===null || strcmp($date,$to)<=0);
}

function hrm_payroll_decision_predicate_matches($predicate, $facts, $type)
{
    $name=$predicate['input']; $op=$predicate['op'];
    $exists=array_key_exists($name,$facts) && $facts[$name]!==null;
    if ($op==='exists') return $exists;
    if (!$exists) return false;
    $left=$facts[$name]; $value=$predicate['value'];
    if ($op==='in') { foreach($value as $candidate) if(hrm_payroll_decision_table_compare_scalar($type,$left,$candidate)===0) return true; return false; }
    if ($op==='between') return hrm_payroll_decision_table_compare_scalar($type,$left,$value[0])>=0 && hrm_payroll_decision_table_compare_scalar($type,$left,$value[1])<=0;
    if ($type==='boolean') { $cmp=($left===$value)?0:1; }
    else $cmp=hrm_payroll_decision_table_compare_scalar($type,$left,$value);
    if($op==='eq')return $cmp===0; if($op==='neq')return $cmp!==0; if($op==='lt')return $cmp<0; if($op==='lte')return $cmp<=0; if($op==='gt')return $cmp>0; if($op==='gte')return $cmp>=0;
    return false;
}

function hrm_payroll_decimal_add($left, $right)
{
    $parse=function($v){$v=(string)$v;$neg=isset($v[0])&&$v[0]==='-';if($neg)$v=substr($v,1);$p=explode('.',$v,2);return array($neg,$p[0],$p[1]??'');};
    $a=$parse($left);$b=$parse($right);$scale=max(strlen($a[2]),strlen($b[2]));
    $ai=ltrim($a[1].str_pad($a[2],$scale,'0'),'0'); if($ai==='')$ai='0';
    $bi=ltrim($b[1].str_pad($b[2],$scale,'0'),'0'); if($bi==='')$bi='0';
    $cmp=function($x,$y){if(strlen($x)!==strlen($y))return strlen($x)<strlen($y)?-1:1;return strcmp($x,$y)<=>0;};
    $add=function($x,$y){$i=strlen($x)-1;$j=strlen($y)-1;$c=0;$o='';while($i>=0||$j>=0||$c){$s=$c+($i>=0?(int)$x[$i--]:0)+($j>=0?(int)$y[$j--]:0);$o=($s%10).$o;$c=intdiv($s,10);}return ltrim($o,'0')?:'0';};
    $sub=function($x,$y){$i=strlen($x)-1;$j=strlen($y)-1;$brr=0;$o='';while($i>=0){$d=(int)$x[$i--]-$brr-($j>=0?(int)$y[$j--]:0);if($d<0){$d+=10;$brr=1;}else$brr=0;$o=$d.$o;}return ltrim($o,'0')?:'0';};
    if($a[0]===$b[0]){$digits=$add($ai,$bi);$neg=$a[0];}else{$c=$cmp($ai,$bi);if($c===0){$digits='0';$neg=false;}elseif($c>0){$digits=$sub($ai,$bi);$neg=$a[0];}else{$digits=$sub($bi,$ai);$neg=$b[0];}}
    if($scale>0){$digits=str_pad($digits,$scale+1,'0',STR_PAD_LEFT);$whole=substr($digits,0,-$scale);$frac=rtrim(substr($digits,-$scale),'0');$out=$frac===''?$whole:$whole.'.'.$frac;}else$out=$digits;
    if($neg && $out!=='0')$out='-'.$out; return $out;
}

function hrm_payroll_decision_table_simulate($artifact, $facts, $as_of, &$receipt, &$error)
{
    $receipt=null; $plan=hrm_payroll_decision_table_compile($artifact,$error); if($plan===false)return false;
    if(!is_array($facts))return hrm_payroll_rule_sim_fail($error,'decision_facts_invalid');
    $columns=array(); foreach($plan['inputs'] as $c)$columns[$c['name']]=$c;
    foreach($facts as $name=>$value)if(!isset($columns[$name]))return hrm_payroll_rule_sim_fail($error,'decision_fact_unknown');
    foreach($columns as $name=>$column){$exists=array_key_exists($name,$facts)&&$facts[$name]!==null;if(!$exists){if($column['null_policy']==='reject')return hrm_payroll_rule_sim_fail($error,'decision_fact_missing_'.$name);continue;}if(!hrm_payroll_decision_table_validate_scalar($column['type'],$facts[$name]))return hrm_payroll_rule_sim_fail($error,'decision_fact_type_'.$name);}
    if(!hrm_payroll_decision_table_valid_date($as_of))return hrm_payroll_rule_sim_fail($error,'decision_as_of_invalid');
    $matches=array(); foreach($plan['rows'] as $row){if(!hrm_payroll_rule_date_in_interval($as_of,$row['effective_from'],$row['effective_to']))continue;$ok=true;foreach($row['predicates'] as $p)if(!hrm_payroll_decision_predicate_matches($p,$facts,$columns[$p['input']]['type'])){$ok=false;break;}if($ok)$matches[]=$row;}
    if(count($matches)===0 && $plan['gap_policy']==='reject')return hrm_payroll_rule_sim_fail($error,'decision_no_hit');
    if(count($matches)>1 && $plan['overlap_policy']==='reject')return hrm_payroll_rule_sim_fail($error,'decision_multi_hit');
    if($plan['hit_policy']==='unique' && count($matches)>1)return hrm_payroll_rule_sim_fail($error,'decision_unique_multi_hit');
    $selected=$matches; if($plan['hit_policy']==='first_by_priority' && count($matches)>1)$selected=array($matches[0]);
    $outputs=null;
    if(count($selected)>0){
        if($plan['hit_policy']==='collect'){$outputs=array();foreach($selected as $row)$outputs[]=array('row_id'=>$row['row_id'],'values'=>$row['outputs']);}
        elseif($plan['hit_policy']==='sum'){$outputs=array();foreach($plan['outputs'] as $col)$outputs[$col['name']]=$col['type']==='integer'?'0':'0';foreach($selected as $row)foreach($row['outputs'] as $name=>$value)$outputs[$name]=hrm_payroll_decimal_add($outputs[$name],(string)$value);}
        else{$row=$selected[0];$outputs=$row['outputs'];foreach($plan['outputs'] as $col)if(!array_key_exists($col['name'],$outputs)&&array_key_exists('default',$col))$outputs[$col['name']]=$col['default'];}
    }
    $receipt=array('simulator_version'=>HRM_PAY_RULE_003_SIMULATOR_VERSION,'plan_hash'=>$plan['plan_hash'],'artifact_hash'=>$plan['artifact_hash'],'table_id'=>$plan['table_id'],'as_of'=>$as_of,'matched_row_ids'=>array_map(function($r){return $r['row_id'];},$selected),'outputs'=>$outputs,'explanations'=>array_map(function($r){return array('row_id'=>$r['row_id'],'label'=>$r['explanation_label'],'source_ids'=>$r['source_ids']);},$selected));
    $receipt['receipt_hash']=hash('sha256',json_encode(hrm_payroll_decision_table_canonicalize($receipt),JSON_UNESCAPED_SLASHES)); $error=null; return true;
}

function hrm_payroll_rate_table_match_bracket($plan, $input)
{
    foreach($plan['brackets'] as $b){$lower=true;$upper=true;if($b['lower_bound']!==null){$c=hrm_payroll_decision_table_decimal_compare($input,$b['lower_bound']);$lower=$c>0||($c===0&&$b['lower_inclusive']);}if($b['upper_bound']!==null){$c=hrm_payroll_decision_table_decimal_compare($input,$b['upper_bound']);$upper=$c<0||($c===0&&$b['upper_inclusive']);}if($lower&&$upper)return $b;}return null;
}

function hrm_payroll_rate_table_simulate($artifact, $input, $as_of, &$receipt, &$error)
{
    $receipt=null;$plan=hrm_payroll_rate_table_compile($artifact,$error);if($plan===false)return false;
    if(!hrm_payroll_rate_table_decimal($input))return hrm_payroll_rule_sim_fail($error,'rate_input_invalid');
    if(!hrm_payroll_rule_date_in_interval($as_of,$plan['legal_interval']['from'],$plan['legal_interval']['to']))return hrm_payroll_rule_sim_fail($error,'rate_as_of_outside_legal_interval');
    $b=hrm_payroll_rate_table_match_bracket($plan,$input);if($b===null&&$plan['gap_policy']==='reject')return hrm_payroll_rule_sim_fail($error,'rate_no_hit');
    $receipt=array('simulator_version'=>HRM_PAY_RULE_003_SIMULATOR_VERSION,'plan_hash'=>$plan['plan_hash'],'artifact_hash'=>$plan['artifact_hash'],'table_id'=>$plan['table_id'],'as_of'=>$as_of,'input'=>$input,'matched_bracket_id'=>$b?$b['bracket_id']:null,'values'=>$b?array('base_amount'=>$b['base_amount'],'fixed_amount'=>$b['fixed_amount'],'marginal_rate'=>$b['marginal_rate'],'floor'=>$b['floor'],'cap'=>$b['cap'],'employee_rate'=>$b['employee_rate'],'employer_rate'=>$b['employer_rate']):null,'explanation'=>$b?array('label'=>$b['explanation_label'],'source_ids'=>$b['source_ids']):null,'rounding'=>$plan['rounding']);
    $receipt['receipt_hash']=hash('sha256',json_encode(hrm_payroll_decision_table_canonicalize($receipt),JSON_UNESCAPED_SLASHES));$error=null;return true;
}

function hrm_payroll_rate_table_verify_vectors($artifact, &$error)
{
    $plan=hrm_payroll_rate_table_compile($artifact,$error);if($plan===false)return false;
    $date=$plan['legal_interval']['from'];
    foreach($plan['test_vectors'] as $v){$receipt=null;if(!hrm_payroll_rate_table_simulate($artifact,$v['input'],$date,$receipt,$error))return false;if($receipt['matched_bracket_id']!==$v['expected_bracket_id'])return hrm_payroll_rule_sim_fail($error,'rate_test_vector_expected_mismatch_'.$v['vector_id']);}
    return true;
}
