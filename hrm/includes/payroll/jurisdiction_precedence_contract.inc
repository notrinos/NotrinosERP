<?php
/**
 * PAY-RULE-005 pure jurisdiction / precedence contract.
 *
 * No database, session, file, network or payroll-routing side effects belong in
 * this module. Precedence ranks are explicit expert-owned metadata; jurisdiction
 * type or source row order never silently creates legal priority.
 */
if (!defined('HRM_PAY_RULE_005_CONTRACT_VERSION'))
    define('HRM_PAY_RULE_005_CONTRACT_VERSION', '1.0.498');
if (!defined('HRM_PAY_RULE_005_PRODUCTION_ACTIVATION_ALLOWED'))
    define('HRM_PAY_RULE_005_PRODUCTION_ACTIVATION_ALLOWED', false);

function hrm_pay_rule_005_ascii_token($value, $max=160)
{
    if (!is_string($value)) return false;
    $value = trim($value);
    if ($value === '' || strlen($value) > $max) return false;
    return preg_match('/^[A-Za-z0-9][A-Za-z0-9._:\/-]*$/D', $value) ? $value : false;
}

function hrm_pay_rule_005_text($value, $max=256)
{
    if (!is_string($value)) return false;
    $value = trim($value);
    if ($value === '' || strlen($value) > $max || preg_match('/[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/', $value))
        return false;
    return $value;
}

function hrm_pay_rule_005_date($value)
{
    if (!is_string($value) || !preg_match('/^(\d{4})-(\d{2})-(\d{2})$/D', $value, $m)) return false;
    return checkdate((int)$m[2], (int)$m[3], (int)$m[1]) ? $value : false;
}

function hrm_pay_rule_005_sha256($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) ? $value : false;
}

function hrm_pay_rule_005_jurisdiction_types()
{
    return array('country','subdivision','locality','authority','industry','collective','establishment','custom_governed');
}

function hrm_pay_rule_005_fact_roles()
{
    return array('employer','work','residence','tax_residence','remote_work','temporary_work','authority_industry','collective','establishment');
}

function hrm_pay_rule_005_subject_types()
{
    return array('legal_entity','work_location','employment','assignment');
}

function hrm_pay_rule_005_applicability_modes()
{
    return array('mandatory','permitted_optional','employer_selectable','worker_election');
}

function hrm_pay_rule_005_validate_interval($from, $to, &$error=null)
{
    $error=null;
    $from=hrm_pay_rule_005_date($from); $to=hrm_pay_rule_005_date($to);
    if ($from===false || $to===false || strcmp($from,$to)>0) { $error='effective_interval_invalid'; return false; }
    return array($from,$to);
}

function hrm_pay_rule_005_interval_contains($from, $to, $date)
{
    if (hrm_pay_rule_005_validate_interval($from,$to)===false || hrm_pay_rule_005_date($date)===false) return null;
    return strcmp($from,$date)<=0 && strcmp($date,$to)<=0;
}

function hrm_pay_rule_005_intervals_overlap($a_from,$a_to,$b_from,$b_to)
{
    if (hrm_pay_rule_005_validate_interval($a_from,$a_to)===false || hrm_pay_rule_005_validate_interval($b_from,$b_to)===false) return null;
    return strcmp($a_from,$b_to)<=0 && strcmp($b_from,$a_to)<=0;
}

function hrm_pay_rule_005_canonical_json($value)
{
    if (is_array($value)) {
        $is_list = array_keys($value) === range(0, count($value)-1);
        if ($is_list) {
            $out=array(); foreach($value as $v) $out[]=hrm_pay_rule_005_canonicalize($v); return json_encode($out, JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
        }
        $keys=array_keys($value); sort($keys,SORT_STRING); $out=array();
        foreach($keys as $k) $out[(string)$k]=hrm_pay_rule_005_canonicalize($value[$k]);
        return json_encode($out, JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
    }
    return json_encode(hrm_pay_rule_005_canonicalize($value), JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
}

function hrm_pay_rule_005_canonicalize($value)
{
    if (!is_array($value)) return $value;
    $is_list = array_keys($value) === range(0, count($value)-1);
    if ($is_list) { $out=array(); foreach($value as $v) $out[]=hrm_pay_rule_005_canonicalize($v); return $out; }
    $keys=array_keys($value); sort($keys,SORT_STRING); $out=array();
    foreach($keys as $k) $out[(string)$k]=hrm_pay_rule_005_canonicalize($value[$k]);
    return $out;
}

function hrm_pay_rule_005_hash_payload($value)
{
    return hash('sha256', hrm_pay_rule_005_canonical_json($value));
}

function hrm_pay_rule_005_validate_jurisdiction_version($row, &$error=null)
{
    $error=null;
    if (!is_array($row)) { $error='jurisdiction_version_not_array'; return false; }
    foreach(array('jurisdiction_key','jurisdiction_type','version_no','parent_jurisdiction_id','hierarchy_depth','effective_from','effective_to','authority_source_ref') as $k)
        if(!array_key_exists($k,$row)){ $error='jurisdiction_version_missing_'.$k; return false; }
    $key=hrm_pay_rule_005_ascii_token($row['jurisdiction_key'],96);
    $type=hrm_pay_rule_005_ascii_token($row['jurisdiction_type'],24);
    $interval=hrm_pay_rule_005_validate_interval($row['effective_from'],$row['effective_to'],$error);
    $source=hrm_pay_rule_005_text($row['authority_source_ref'],256);
    if($key===false || !in_array($type,hrm_pay_rule_005_jurisdiction_types(),true)){ $error='jurisdiction_identity_invalid'; return false; }
    if(!is_int($row['version_no']) || $row['version_no']<=0){ $error='jurisdiction_version_no_invalid'; return false; }
    if(!is_int($row['parent_jurisdiction_id']) || $row['parent_jurisdiction_id']<0){ $error='jurisdiction_parent_invalid'; return false; }
    if(!is_int($row['hierarchy_depth']) || $row['hierarchy_depth']<0 || $row['hierarchy_depth']>32){ $error='jurisdiction_depth_invalid'; return false; }
    if(($row['parent_jurisdiction_id']===0 && $row['hierarchy_depth']!==0) || ($row['parent_jurisdiction_id']>0 && $row['hierarchy_depth']===0)){ $error='jurisdiction_parent_depth_mismatch'; return false; }
    if($interval===false || $source===false) return false;
    return array('jurisdiction_key'=>$key,'jurisdiction_type'=>$type,'version_no'=>$row['version_no'],'parent_jurisdiction_id'=>$row['parent_jurisdiction_id'],
        'hierarchy_depth'=>$row['hierarchy_depth'],'effective_from'=>$interval[0],'effective_to'=>$interval[1],'authority_source_ref'=>$source);
}

function hrm_pay_rule_005_validate_binding($row, &$error=null)
{
    $error=null;
    if(!is_array($row)){ $error='binding_not_array'; return false; }
    foreach(array('subject_type','subject_id','subject_legal_entity_id','fact_role','jurisdiction_id','effective_from','effective_to','source_token','reason_token') as $k)
        if(!array_key_exists($k,$row)){ $error='binding_missing_'.$k; return false; }
    $subject=hrm_pay_rule_005_ascii_token($row['subject_type'],24); $role=hrm_pay_rule_005_ascii_token($row['fact_role'],32);
    $source=hrm_pay_rule_005_ascii_token($row['source_token'],64); $reason=hrm_pay_rule_005_ascii_token($row['reason_token'],96);
    $interval=hrm_pay_rule_005_validate_interval($row['effective_from'],$row['effective_to'],$error);
    if(!in_array($subject,hrm_pay_rule_005_subject_types(),true)){ $error='binding_subject_type_invalid'; return false; }
    if(!in_array($role,hrm_pay_rule_005_fact_roles(),true)){ $error='binding_fact_role_invalid'; return false; }
    if(!is_int($row['subject_id'])||$row['subject_id']<=0||!is_int($row['subject_legal_entity_id'])||$row['subject_legal_entity_id']<=0||!is_int($row['jurisdiction_id'])||$row['jurisdiction_id']<=0){ $error='binding_scope_invalid'; return false; }
    if($source===false||$reason===false||$interval===false) return false;
    return array('subject_type'=>$subject,'subject_id'=>$row['subject_id'],'subject_legal_entity_id'=>$row['subject_legal_entity_id'],'fact_role'=>$role,'jurisdiction_id'=>$row['jurisdiction_id'],
        'effective_from'=>$interval[0],'effective_to'=>$interval[1],'source_token'=>$source,'reason_token'=>$reason);
}

function hrm_pay_rule_005_validate_applicability($row, &$error=null)
{
    $error=null;
    if(!is_array($row)){ $error='applicability_not_array'; return false; }
    foreach(array('rule_version_id','publication_id','jurisdiction_id','fact_role','resolution_group_key','applicability_mode','precedence_rank','effective_from','effective_to') as $k)
        if(!array_key_exists($k,$row)){ $error='applicability_missing_'.$k; return false; }
    $role=hrm_pay_rule_005_ascii_token($row['fact_role'],32); $group=hrm_pay_rule_005_ascii_token($row['resolution_group_key'],160); $mode=hrm_pay_rule_005_ascii_token($row['applicability_mode'],32);
    $interval=hrm_pay_rule_005_validate_interval($row['effective_from'],$row['effective_to'],$error);
    if(!is_int($row['rule_version_id'])||$row['rule_version_id']<=0||!is_int($row['publication_id'])||$row['publication_id']<=0||!is_int($row['jurisdiction_id'])||$row['jurisdiction_id']<=0){ $error='applicability_scope_invalid'; return false; }
    if(!in_array($role,hrm_pay_rule_005_fact_roles(),true)||$group===false||!in_array($mode,hrm_pay_rule_005_applicability_modes(),true)){ $error='applicability_metadata_invalid'; return false; }
    if(!is_int($row['precedence_rank'])||$row['precedence_rank']<1||$row['precedence_rank']>65535){ $error='precedence_rank_invalid'; return false; }
    if($interval===false) return false;
    return array('rule_version_id'=>$row['rule_version_id'],'publication_id'=>$row['publication_id'],'jurisdiction_id'=>$row['jurisdiction_id'],'fact_role'=>$role,
        'resolution_group_key'=>$group,'applicability_mode'=>$mode,'precedence_rank'=>$row['precedence_rank'],'effective_from'=>$interval[0],'effective_to'=>$interval[1]);
}

function hrm_pay_rule_005_lineage_contains($lineage, $jurisdiction_id, &$specificity=null)
{
    $specificity=null;
    if(!is_array($lineage)||!is_int($jurisdiction_id)||$jurisdiction_id<=0) return false;
    foreach($lineage as $depth=>$id){
        if(!is_int($id)||$id<=0||!is_int($depth)||$depth<0) return false;
        if($id===$jurisdiction_id){$specificity=$depth;return true;}
    }
    return false;
}

/**
 * Detect contradictory facts: two active facts with the same semantic role are
 * valid only when one governed jurisdiction is an ancestor of the other.
 */
function hrm_pay_rule_005_validate_fact_items($items, &$error=null)
{
    $error=null;
    if(!is_array($items)||count($items)===0){$error='jurisdiction_facts_missing';return false;}
    $by_role=array();
    foreach($items as $i=>$item){
        if(!is_array($item)||!isset($item['fact_role'],$item['jurisdiction_id'],$item['lineage'])||!in_array($item['fact_role'],hrm_pay_rule_005_fact_roles(),true)
            ||!is_int($item['jurisdiction_id'])||$item['jurisdiction_id']<=0||!is_array($item['lineage'])){$error='jurisdiction_fact_invalid';return false;}
        $specificity=null;if(!hrm_pay_rule_005_lineage_contains($item['lineage'],$item['jurisdiction_id'],$specificity)){$error='jurisdiction_fact_lineage_invalid';return false;}
        if(!isset($by_role[$item['fact_role']]))$by_role[$item['fact_role']]=array();$by_role[$item['fact_role']][]=$item;
    }
    foreach($by_role as $role=>$facts){
        for($a=0;$a<count($facts);$a++)for($b=$a+1;$b<count($facts);$b++){
            $sa=null;$sb=null;
            $ab=hrm_pay_rule_005_lineage_contains($facts[$a]['lineage'],$facts[$b]['jurisdiction_id'],$sa);
            $ba=hrm_pay_rule_005_lineage_contains($facts[$b]['lineage'],$facts[$a]['jurisdiction_id'],$sb);
            if(!$ab&&!$ba){$error='contradictory_jurisdiction_facts';return false;}
        }
    }
    return true;
}

/**
 * Pure deterministic resolver. `$selections` maps resolution_group_key to one
 * explicit non-mandatory applicability_id. Required groups never default.
 */
function hrm_pay_rule_005_resolve($facts, $candidates, $required_groups, $selections=array())
{
    $error=null;
    if(!hrm_pay_rule_005_validate_fact_items($facts,$error)) return array('ok'=>false,'outcome'=>$error,'selected'=>array(),'trace'=>array());
    if(!is_array($candidates)||!is_array($required_groups)||!is_array($selections)) return array('ok'=>false,'outcome'=>'resolver_input_invalid','selected'=>array(),'trace'=>array());
    $required=array();
    foreach($required_groups as $g){$g=hrm_pay_rule_005_ascii_token($g,160);if($g===false)return array('ok'=>false,'outcome'=>'required_group_invalid','selected'=>array(),'trace'=>array());$required[$g]=true;}
    if(count($required)===0) return array('ok'=>false,'outcome'=>'required_groups_missing','selected'=>array(),'trace'=>array());
    foreach($selections as $group=>$selection){$token=hrm_pay_rule_005_ascii_token((string)$group,160);if($token===false||!isset($required[$token])||!is_int($selection)||$selection<=0)return array('ok'=>false,'outcome'=>'selection_invalid','selected'=>array(),'trace'=>array());}
    $app=array();$trace=array();
    foreach($candidates as $candidate){
        $v=hrm_pay_rule_005_validate_applicability($candidate,$error);
        if($v===false||!isset($candidate['applicability_id'])||!is_int($candidate['applicability_id'])||$candidate['applicability_id']<=0){return array('ok'=>false,'outcome'=>'candidate_invalid','selected'=>array(),'trace'=>array());}
        $best=-1;
        foreach($facts as $fact){if($fact['fact_role']!==$v['fact_role'])continue;$s=null;if(hrm_pay_rule_005_lineage_contains($fact['lineage'],$v['jurisdiction_id'],$s)&&$s>$best)$best=$s;}
        if($best<0)continue;
        $v['applicability_id']=$candidate['applicability_id'];$v['specificity_depth']=$best;
        if(!isset($app[$v['resolution_group_key']]))$app[$v['resolution_group_key']]=array();$app[$v['resolution_group_key']][]=$v;
        $trace[]=array('applicability_id'=>$v['applicability_id'],'rule_version_id'=>$v['rule_version_id'],'group'=>$v['resolution_group_key'],'rank'=>$v['precedence_rank'],'specificity'=>$best,'mode'=>$v['applicability_mode']);
    }
    $selected=array();
    foreach($required as $group=>$_){
        $rows=$app[$group]??array();$mandatory=array();foreach($rows as $r)if($r['applicability_mode']==='mandatory')$mandatory[]=$r;
        $explicit=array_key_exists($group,$selections)?$selections[$group]:null;
        if($explicit!==null && (!is_int($explicit)||$explicit<=0))return array('ok'=>false,'outcome'=>'selection_invalid','group'=>$group,'selected'=>array(),'trace'=>$trace);
        if(count($mandatory)>0){
            if($explicit!==null)return array('ok'=>false,'outcome'=>'mandatory_override_denied','group'=>$group,'selected'=>array(),'trace'=>$trace);
            usort($mandatory,function($a,$b){if($a['precedence_rank']!==$b['precedence_rank'])return $a['precedence_rank']<=>$b['precedence_rank'];if($a['specificity_depth']!==$b['specificity_depth'])return $b['specificity_depth']<=>$a['specificity_depth'];return $a['applicability_id']<=>$b['applicability_id'];});
            $top=$mandatory[0];$ties=array_values(array_filter($mandatory,function($r)use($top){return $r['precedence_rank']===$top['precedence_rank']&&$r['specificity_depth']===$top['specificity_depth'];}));
            $versions=array();foreach($ties as $r)$versions[$r['rule_version_id']]=true;
            if(count($versions)>1)return array('ok'=>false,'outcome'=>'equal_precedence_conflict','group'=>$group,'selected'=>array(),'trace'=>$trace);
            $selected[$group]=$top;continue;
        }
        if($explicit===null)return array('ok'=>false,'outcome'=>'gap_no_applicable_rule','group'=>$group,'selected'=>array(),'trace'=>$trace);
        $chosen=false;foreach($rows as $r)if($r['applicability_id']===$explicit){$chosen=$r;break;}
        if($chosen===false)return array('ok'=>false,'outcome'=>'selected_rule_not_applicable','group'=>$group,'selected'=>array(),'trace'=>$trace);
        if($chosen['applicability_mode']==='mandatory')return array('ok'=>false,'outcome'=>'selection_mode_invalid','group'=>$group,'selected'=>array(),'trace'=>$trace);
        $selected[$group]=$chosen;
    }
    ksort($selected,SORT_STRING);usort($trace,function($a,$b){return $a['applicability_id']<=>$b['applicability_id'];});
    return array('ok'=>true,'outcome'=>'resolved','selected'=>$selected,'trace'=>$trace,'resolution_sha256'=>hrm_pay_rule_005_hash_payload(array('selected'=>$selected,'trace'=>$trace)));
}
