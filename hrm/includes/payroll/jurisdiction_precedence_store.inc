<?php
/** PAY-RULE-005 governed jurisdiction/applicability/fact-set persistence. */
if (!function_exists('hrm_pay_rule_005_validate_jurisdiction_version'))
    include_once($path_to_root.'/hrm/includes/payroll/jurisdiction_precedence_contract.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

function hrm_pay_rule_005_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user']) && isset($_SESSION['wa_current_user']->user)
        ? (int)$_SESSION['wa_current_user']->user : 0;
}
function hrm_pay_rule_005_manage_allowed(){return function_exists('user_check_access')&&user_check_access('SA_HRM_MANAGE_PAY_RULE_JURISDICTION');}
function hrm_pay_rule_005_resolve_allowed(){return function_exists('user_check_access')&&user_check_access('SA_HRM_RESOLVE_PAY_RULE_JURISDICTION');}
function hrm_pay_rule_005_view_allowed(){return function_exists('user_check_access')&&user_check_access('SA_HRM_VIEW_PAY_RULE_RESOLUTION');}

function hrm_pay_rule_005_active_jurisdiction_version($jurisdiction_id,$date,$for_update=false)
{
    global $transaction_level;
    $jurisdiction_id=(int)$jurisdiction_id;
    if($jurisdiction_id<=0||hrm_pay_rule_005_date($date)===false||($for_update&&empty($transaction_level)))return false;
    $r=db_query('SELECT v.jurisdiction_version_id,v.jurisdiction_id,v.version_no,v.predecessor_version_id,v.jurisdiction_type,v.parent_jurisdiction_id,v.hierarchy_depth,v.effective_from,v.effective_to,v.authority_source_ref,v.version_sha256,j.jurisdiction_key '
        .'FROM '.TB_PREF.'hrm_pay_rule_jurisdiction_versions v INNER JOIN '.TB_PREF.'hrm_pay_rule_jurisdictions j ON j.jurisdiction_id=v.jurisdiction_id '
        .'WHERE v.jurisdiction_id='.$jurisdiction_id.' AND v.effective_from<='.db_escape($date).' AND v.effective_to>='.db_escape($date)
        .' ORDER BY v.jurisdiction_version_id LIMIT 2'.($for_update?' FOR UPDATE':''),'could not read PAY-RULE-005 jurisdiction version');
    if(!$r||db_num_rows($r)!==1)return false;$row=db_fetch_assoc($r);if(!$row)return false;
    return array('jurisdiction_version_id'=>(int)$row['jurisdiction_version_id'],'jurisdiction_id'=>(int)$row['jurisdiction_id'],'jurisdiction_key'=>(string)$row['jurisdiction_key'],
        'version_no'=>(int)$row['version_no'],'predecessor_version_id'=>(int)$row['predecessor_version_id'],'jurisdiction_type'=>(string)$row['jurisdiction_type'],
        'parent_jurisdiction_id'=>(int)$row['parent_jurisdiction_id'],'hierarchy_depth'=>(int)$row['hierarchy_depth'],'effective_from'=>(string)$row['effective_from'],
        'effective_to'=>(string)$row['effective_to'],'authority_source_ref'=>(string)$row['authority_source_ref'],'version_sha256'=>(string)$row['version_sha256']);
}

function hrm_pay_rule_005_jurisdiction_lineage($jurisdiction_id,$date,$for_update=false)
{
    $lineage=array();$seen=array();$current=(int)$jurisdiction_id;
    for($guard=0;$guard<=32;$guard++){
        if($current<=0)return false;if(isset($seen[$current]))return false;$seen[$current]=true;
        $v=hrm_pay_rule_005_active_jurisdiction_version($current,$date,$for_update);if(!is_array($v))return false;
        $lineage[(int)$v['hierarchy_depth']]=$current;
        if((int)$v['parent_jurisdiction_id']===0){ksort($lineage,SORT_NUMERIC);if(array_keys($lineage)!==range(0,count($lineage)-1))return false;return array('ids'=>$lineage,'versions'=>$seen,'leaf_version'=>$v);}
        $current=(int)$v['parent_jurisdiction_id'];
    }
    return false;
}

function hrm_pay_rule_005_subject_scope($subject_type,$subject_id,$for_update=false)
{
    global $transaction_level;
    if(!in_array($subject_type,hrm_pay_rule_005_subject_types(),true)||(int)$subject_id<=0||($for_update&&empty($transaction_level)))return false;
    $id=(int)$subject_id;$sql='';
    if($subject_type==='legal_entity')$sql='SELECT legal_entity_id subject_id,legal_entity_id FROM '.TB_PREF.'hrm_legal_entities WHERE legal_entity_id='.$id;
    elseif($subject_type==='work_location')$sql='SELECT work_location_id subject_id,legal_entity_id FROM '.TB_PREF.'hrm_work_locations WHERE work_location_id='.$id;
    elseif($subject_type==='employment')$sql='SELECT employment_id subject_id,legal_entity_id FROM '.TB_PREF.'hrm_employments WHERE employment_id='.$id;
    else $sql='SELECT a.assignment_id subject_id,e.legal_entity_id FROM '.TB_PREF.'hrm_assignments a INNER JOIN '.TB_PREF.'hrm_employments e ON e.employment_id=a.employment_id WHERE a.assignment_id='.$id;
    $r=db_query($sql.' LIMIT 2'.($for_update?' FOR UPDATE':''),'could not resolve PAY-RULE-005 subject scope');
    if(!$r||db_num_rows($r)!==1)return false;$row=db_fetch_assoc($r);if(!$row||(int)$row['legal_entity_id']<=0)return false;
    return array('subject_id'=>(int)$row['subject_id'],'legal_entity_id'=>(int)$row['legal_entity_id']);
}

function hrm_pay_rule_005_audit($entity_type,$entity_key,$action,$reason,$legal_entity_id,$restricted)
{
    return append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>(int)$legal_entity_id,
        'entity_type'=>$entity_type,'entity_key'=>$entity_key,'action'=>$action,'reason_code'=>$reason,'before'=>array(),'after'=>array('accepted'=>true),
        'restricted_diff'=>$restricted));
}

/** Create stable identity plus one immutable effective hierarchy version. */
function hrm_pay_rule_005_write_jurisdiction_version($input)
{
    $actor=hrm_pay_rule_005_actor_id();if($actor<=0||!hrm_pay_rule_005_manage_allowed()||!is_array($input))return false;
    foreach(array('jurisdiction_key','jurisdiction_type','parent_jurisdiction_id','effective_from','effective_to','authority_source_ref') as $k)if(!array_key_exists($k,$input))return false;
    $manage=empty($GLOBALS['transaction_level']);if($manage)begin_transaction();$ok=true;$return=false;
    $key=hrm_pay_rule_005_ascii_token($input['jurisdiction_key'],96);$type=hrm_pay_rule_005_ascii_token($input['jurisdiction_type'],24);$parent=(int)$input['parent_jurisdiction_id'];
    $interval=hrm_pay_rule_005_validate_interval($input['effective_from'],$input['effective_to'],$err);$source=hrm_pay_rule_005_text($input['authority_source_ref'],256);
    if($key===false||!in_array($type,hrm_pay_rule_005_jurisdiction_types(),true)||$parent<0||$interval===false||$source===false)$ok=false;
    $jurisdiction_id=0;
    if($ok){$r=db_query('SELECT jurisdiction_id FROM '.TB_PREF.'hrm_pay_rule_jurisdictions WHERE jurisdiction_key='.db_escape($key).' LIMIT 2 FOR UPDATE','could not lock PAY-RULE-005 jurisdiction identity');
        if(!$r)$ok=false;elseif(db_num_rows($r)===1){$x=db_fetch_assoc($r);$jurisdiction_id=(int)$x['jurisdiction_id'];}
        elseif(db_num_rows($r)!==0)$ok=false;else{if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_jurisdictions (jurisdiction_key,created_by) VALUES ('.db_escape($key).','.$actor.')','could not create PAY-RULE-005 jurisdiction identity'))$ok=false;else$jurisdiction_id=(int)db_insert_id();}}
    $depth=0;$parent_version=false;
    if($ok&&$parent>0){if($parent===$jurisdiction_id)$ok=false;else{$r=db_query('SELECT jurisdiction_version_id,hierarchy_depth,effective_from,effective_to FROM '.TB_PREF.'hrm_pay_rule_jurisdiction_versions WHERE jurisdiction_id='.$parent
            .' AND effective_from<='.db_escape($interval[0]).' AND effective_to>='.db_escape($interval[1]).' LIMIT 2 FOR UPDATE','could not resolve parent jurisdiction coverage');if(!$r||db_num_rows($r)!==1)$ok=false;else{$parent_version=db_fetch_assoc($r);$depth=(int)$parent_version['hierarchy_depth']+1;$lin_start=hrm_pay_rule_005_jurisdiction_lineage($parent,$interval[0],true);$lin_end=hrm_pay_rule_005_jurisdiction_lineage($parent,$interval[1],true);if(!is_array($lin_start)||!is_array($lin_end)||in_array($jurisdiction_id,$lin_start['ids'],true)||in_array($jurisdiction_id,$lin_end['ids'],true)||$depth>32)$ok=false;}}}
    $version_no=1;$predecessor=0;
    if($ok){$r=db_query('SELECT jurisdiction_version_id,version_no,jurisdiction_type,parent_jurisdiction_id,hierarchy_depth,effective_from,effective_to,authority_source_ref,version_sha256 FROM '.TB_PREF.'hrm_pay_rule_jurisdiction_versions WHERE jurisdiction_id='.$jurisdiction_id.' ORDER BY version_no DESC FOR UPDATE','could not lock jurisdiction history');if(!$r)$ok=false;else{
        while($row=db_fetch_assoc($r)){
            if(hrm_pay_rule_005_intervals_overlap($row['effective_from'],$row['effective_to'],$interval[0],$interval[1])){
                $same=(string)$row['effective_from']===$interval[0]&&(string)$row['effective_to']===$interval[1]&&(string)$row['jurisdiction_type']===$type&&(int)$row['parent_jurisdiction_id']===$parent&&(int)$row['hierarchy_depth']===$depth&&(string)$row['authority_source_ref']===$source;
                if($same){$return=(int)$row['jurisdiction_version_id'];$version_no=(int)$row['version_no'];break;}$ok=false;break;
            }
            if((int)$row['version_no']>=$version_no){$version_no=(int)$row['version_no']+1;$predecessor=(int)$row['jurisdiction_version_id'];}
        }
    }}
    $payload=array('jurisdiction_key'=>$key,'jurisdiction_type'=>$type,'version_no'=>$version_no,'parent_jurisdiction_id'=>$parent,'hierarchy_depth'=>$depth,'effective_from'=>$interval?$interval[0]:'','effective_to'=>$interval?$interval[1]:'','authority_source_ref'=>$source);
    $hash=$ok?hrm_pay_rule_005_hash_payload($payload):false;
    if(!$ok&&$manage){cancel_transaction();return false;}
    if($ok&&$return===false){$existing=db_query('SELECT v.jurisdiction_version_id,v.jurisdiction_id,j.jurisdiction_key FROM '.TB_PREF.'hrm_pay_rule_jurisdiction_versions v INNER JOIN '.TB_PREF.'hrm_pay_rule_jurisdictions j ON j.jurisdiction_id=v.jurisdiction_id WHERE v.version_sha256='.db_escape($hash).' LIMIT 2 FOR UPDATE');if($existing&&db_num_rows($existing)===1){$e=db_fetch_assoc($existing);if((string)$e['jurisdiction_key']===$key)$return=(int)$e['jurisdiction_version_id'];else$ok=false;}}
    if($ok&&$return===false){$sql='INSERT INTO '.TB_PREF.'hrm_pay_rule_jurisdiction_versions (jurisdiction_id,version_no,predecessor_version_id,jurisdiction_type,parent_jurisdiction_id,hierarchy_depth,effective_from,effective_to,authority_source_ref,version_sha256,created_by) VALUES ('
        .$jurisdiction_id.','.$version_no.','.($predecessor>0?$predecessor:'NULL').','.db_escape($type).','.($parent>0?$parent:'NULL').','.$depth.','.db_escape($interval[0]).','.db_escape($interval[1]).','.db_escape($source).','.db_escape($hash).','.$actor.')';
        if(!db_query($sql,'could not write PAY-RULE-005 jurisdiction version'))$ok=false;else{$return=(int)db_insert_id();$ok=hrm_pay_rule_005_audit('pay_rule_005_jurisdiction',array('jurisdiction_id'=>$jurisdiction_id,'jurisdiction_version_id'=>$return),'pay.rule_005.jurisdiction.version_created','pay_rule_005_jurisdiction_hierarchy_writer',0,array('jurisdiction_key'=>$key,'jurisdiction_type'=>$type,'parent_jurisdiction_id'=>$parent,'hierarchy_depth'=>$depth,'version_sha256'=>$hash,'production_activation_allowed'=>false))!==false;}}
    if($manage){if($ok)commit_transaction();else cancel_transaction();}return $ok?$return:false;
}

function hrm_pay_rule_005_bind_subject_jurisdiction($input)
{
    $actor=hrm_pay_rule_005_actor_id();if($actor<=0||!hrm_pay_rule_005_manage_allowed())return false;$v=hrm_pay_rule_005_validate_binding($input,$err);if($v===false)return false;
    $manage=empty($GLOBALS['transaction_level']);if($manage)begin_transaction();$ok=true;$return=false;
    $scope=hrm_pay_rule_005_subject_scope($v['subject_type'],$v['subject_id'],true);if(!is_array($scope)||$scope['legal_entity_id']!==$v['subject_legal_entity_id'])$ok=false;
    if($ok&&!is_array(hrm_pay_rule_005_active_jurisdiction_version($v['jurisdiction_id'],$v['effective_from'],true)))$ok=false;
    if($ok&&!is_array(hrm_pay_rule_005_active_jurisdiction_version($v['jurisdiction_id'],$v['effective_to'],true)))$ok=false;
    $hash=$ok?hrm_pay_rule_005_hash_payload($v):false;
    if($ok){$r=db_query('SELECT binding_id FROM '.TB_PREF.'hrm_pay_rule_subject_jurisdiction_bindings WHERE binding_sha256='.db_escape($hash).' LIMIT 2 FOR UPDATE');if($r&&db_num_rows($r)===1){$x=db_fetch_assoc($r);$return=(int)$x['binding_id'];}elseif(!$r||db_num_rows($r)>1)$ok=false;}
    if($ok&&$return===false){$r=db_query('SELECT binding_id FROM '.TB_PREF.'hrm_pay_rule_subject_jurisdiction_bindings WHERE subject_type='.db_escape($v['subject_type']).' AND subject_id='.$v['subject_id'].' AND fact_role='.db_escape($v['fact_role']).' AND effective_from<='.db_escape($v['effective_to']).' AND effective_to>='.db_escape($v['effective_from']).' FOR UPDATE','could not verify binding overlap');if(!$r||db_num_rows($r)!==0)$ok=false;}
    if($ok&&$return===false){$sql='INSERT INTO '.TB_PREF.'hrm_pay_rule_subject_jurisdiction_bindings (subject_type,subject_id,subject_legal_entity_id,fact_role,jurisdiction_id,effective_from,effective_to,source_token,reason_token,binding_sha256,created_by) VALUES ('
        .db_escape($v['subject_type']).','.$v['subject_id'].','.$v['subject_legal_entity_id'].','.db_escape($v['fact_role']).','.$v['jurisdiction_id'].','.db_escape($v['effective_from']).','.db_escape($v['effective_to']).','.db_escape($v['source_token']).','.db_escape($v['reason_token']).','.db_escape($hash).','.$actor.')';
        if(!db_query($sql,'could not append PAY-RULE-005 subject jurisdiction binding'))$ok=false;else{$return=(int)db_insert_id();$ok=hrm_pay_rule_005_audit('pay_rule_005_subject_jurisdiction_binding',array('binding_id'=>$return,'subject_type'=>$v['subject_type'],'subject_id'=>$v['subject_id']),'pay.rule_005.subject_jurisdiction.bound',$v['reason_token'],$v['subject_legal_entity_id'],array('fact_role'=>$v['fact_role'],'jurisdiction_id'=>$v['jurisdiction_id'],'binding_sha256'=>$hash,'jurisdiction_inferred'=>false,'sensitive_plaintext_logged'=>false,'production_activation_allowed'=>false))!==false;}}
    if($manage){if($ok)commit_transaction();else cancel_transaction();}return $ok?$return:false;
}

function hrm_pay_rule_005_resolution_scope($employment_id,$assignment_id,$date,$for_update=false)
{
    global $transaction_level;$employment_id=(int)$employment_id;$assignment_id=(int)$assignment_id;$date=hrm_pay_rule_005_date($date);
    if($employment_id<=0||$assignment_id<=0||$date===false||($for_update&&empty($transaction_level)))return false;
    $qdate=db_escape($date);
    $r=db_query('SELECT e.employment_id,e.worker_id,e.legal_entity_id,e.start_date,e.end_date,e.interval_quality AS employment_interval_quality,a.assignment_id,a.work_location_id,a.effective_from,a.effective_to,a.interval_quality AS assignment_interval_quality FROM '.TB_PREF.'hrm_employments e INNER JOIN '.TB_PREF.'hrm_assignments a ON a.employment_id=e.employment_id WHERE e.employment_id='.$employment_id.' AND a.assignment_id='.$assignment_id.' AND e.start_date IS NOT NULL AND e.start_date<='.$qdate.' AND (e.end_date IS NULL OR e.end_date>='.$qdate.') AND e.interval_quality='.db_escape('exact').' AND a.effective_from IS NOT NULL AND a.effective_from<='.$qdate.' AND (a.effective_to IS NULL OR a.effective_to>='.$qdate.') AND a.interval_quality='.db_escape('exact').' LIMIT 2'.($for_update?' FOR UPDATE':''),'could not lock exact effective-dated PAY-RULE-005 resolution scope');
    if(!$r||db_num_rows($r)!==1)return false;$x=db_fetch_assoc($r);if(!$x||(int)$x['worker_id']<=0||(int)$x['legal_entity_id']<=0)return false;
    return array('employment_id'=>$employment_id,'assignment_id'=>$assignment_id,'worker_id'=>(int)$x['worker_id'],'legal_entity_id'=>(int)$x['legal_entity_id'],'work_location_id'=>(int)$x['work_location_id'],'employment_start_date'=>(string)$x['start_date'],'employment_end_date'=>$x['end_date']===null?null:(string)$x['end_date'],'employment_interval_quality'=>(string)$x['employment_interval_quality'],'assignment_effective_from'=>(string)$x['effective_from'],'assignment_effective_to'=>$x['effective_to']===null?null:(string)$x['effective_to'],'assignment_interval_quality'=>(string)$x['assignment_interval_quality']);
}

function hrm_pay_rule_005_active_bindings_for_scope($scope,$date,$for_update=false)
{
    if(!is_array($scope)||hrm_pay_rule_005_date($date)===false)return false;
    $pairs=array(array('legal_entity',(int)$scope['legal_entity_id']),array('employment',(int)$scope['employment_id']),array('assignment',(int)$scope['assignment_id']));
    if((int)$scope['work_location_id']>0)$pairs[]=array('work_location',(int)$scope['work_location_id']);
    $where=array();foreach($pairs as $p)$where[]='(subject_type='.db_escape($p[0]).' AND subject_id='.(int)$p[1].')';
    $r=db_query('SELECT binding_id,subject_type,subject_id,subject_legal_entity_id,fact_role,jurisdiction_id,effective_from,effective_to,source_token,reason_token,binding_sha256 FROM '.TB_PREF.'hrm_pay_rule_subject_jurisdiction_bindings WHERE ('.implode(' OR ',$where).') AND effective_from<='.db_escape($date).' AND effective_to>='.db_escape($date).' ORDER BY fact_role,subject_type,subject_id,binding_id'.($for_update?' FOR UPDATE':''),'could not read PAY-RULE-005 active jurisdiction bindings');
    if(!$r)return false;$rows=array();while($x=db_fetch_assoc($r))$rows[]=$x;return $rows;
}

function hrm_pay_rule_005_build_fact_material($scope,$date,$for_update=false)
{
    $bindings=hrm_pay_rule_005_active_bindings_for_scope($scope,$date,$for_update);if(!is_array($bindings)||count($bindings)===0)return false;$items=array();$source_rows=array();
    foreach($bindings as $b){if((int)$b['subject_legal_entity_id']!==(int)$scope['legal_entity_id'])return false;$v=hrm_pay_rule_005_active_jurisdiction_version((int)$b['jurisdiction_id'],$date,$for_update);$lin=hrm_pay_rule_005_jurisdiction_lineage((int)$b['jurisdiction_id'],$date,$for_update);if(!is_array($v)||!is_array($lin))return false;
        $item=array('binding_id'=>(int)$b['binding_id'],'fact_role'=>(string)$b['fact_role'],'subject_type'=>(string)$b['subject_type'],'subject_id'=>(int)$b['subject_id'],'jurisdiction_id'=>(int)$b['jurisdiction_id'],'jurisdiction_version_id'=>(int)$v['jurisdiction_version_id'],'lineage'=>$lin['ids']);
        $item['item_sha256']=hrm_pay_rule_005_hash_payload($item);$items[]=$item;$source_rows[]=array('binding_id'=>(int)$b['binding_id'],'binding_sha256'=>(string)$b['binding_sha256'],'jurisdiction_version_id'=>(int)$v['jurisdiction_version_id'],'version_sha256'=>(string)$v['version_sha256']);}
    $err=null;if(!hrm_pay_rule_005_validate_fact_items($items,$err))return false;usort($source_rows,function($a,$b){return $a['binding_id']<=>$b['binding_id'];});
    $scope_state=array('employment_id'=>(int)$scope['employment_id'],'assignment_id'=>(int)$scope['assignment_id'],'legal_entity_id'=>(int)$scope['legal_entity_id'],'work_location_id'=>(int)$scope['work_location_id'],'employment_start_date'=>(string)$scope['employment_start_date'],'employment_end_date'=>$scope['employment_end_date'],'employment_interval_quality'=>(string)$scope['employment_interval_quality'],'assignment_effective_from'=>(string)$scope['assignment_effective_from'],'assignment_effective_to'=>$scope['assignment_effective_to'],'assignment_interval_quality'=>(string)$scope['assignment_interval_quality']);
    return array('items'=>$items,'source_state_sha256'=>hrm_pay_rule_005_hash_payload(array('scope'=>$scope_state,'bindings'=>$source_rows)));
}

function hrm_pay_rule_005_seal_fact_set($employment_id,$assignment_id,$date)
{
    $actor=hrm_pay_rule_005_actor_id();if($actor<=0||!hrm_pay_rule_005_resolve_allowed()||hrm_pay_rule_005_date($date)===false)return false;
    $manage=empty($GLOBALS['transaction_level']);if($manage)begin_transaction();$ok=true;$return=false;$created=false;$scope=hrm_pay_rule_005_resolution_scope($employment_id,$assignment_id,$date,true);if(!is_array($scope))$ok=false;
    $material=$ok?hrm_pay_rule_005_build_fact_material($scope,$date,true):false;if(!is_array($material))$ok=false;
    $payload=$ok?array('employment_id'=>(int)$scope['employment_id'],'assignment_id'=>(int)$scope['assignment_id'],'legal_entity_id'=>(int)$scope['legal_entity_id'],'work_location_id'=>(int)$scope['work_location_id'],'as_of_date'=>$date,'source_state_sha256'=>$material['source_state_sha256'],'items'=>array_map(function($x){return $x['item_sha256'];},$material['items'])):array();
    $hash=$ok?hrm_pay_rule_005_hash_payload($payload):false;
    if($ok){$r=db_query('SELECT fact_set_id FROM '.TB_PREF.'hrm_pay_rule_jurisdiction_fact_sets WHERE fact_set_sha256='.db_escape($hash).' LIMIT 2 FOR UPDATE');if($r&&db_num_rows($r)===1){$x=db_fetch_assoc($r);$return=(int)$x['fact_set_id'];}elseif(!$r||db_num_rows($r)>1)$ok=false;}
    if($ok&&$return===false){$sql='INSERT INTO '.TB_PREF.'hrm_pay_rule_jurisdiction_fact_sets (employment_id,assignment_id,legal_entity_id,work_location_id,as_of_date,source_state_sha256,fact_set_sha256,created_by) VALUES ('
        .$scope['employment_id'].','.$scope['assignment_id'].','.$scope['legal_entity_id'].','.($scope['work_location_id']>0?$scope['work_location_id']:'NULL').','.db_escape($date).','.db_escape($material['source_state_sha256']).','.db_escape($hash).','.$actor.')';if(!db_query($sql,'could not seal PAY-RULE-005 fact set'))$ok=false;else{$return=(int)db_insert_id();$created=true;foreach($material['items'] as $it){$sql='INSERT INTO '.TB_PREF.'hrm_pay_rule_jurisdiction_fact_items (fact_set_id,binding_id,jurisdiction_version_id,fact_role,subject_type,subject_id,jurisdiction_id,item_sha256) VALUES ('.$return.','.$it['binding_id'].','.$it['jurisdiction_version_id'].','.db_escape($it['fact_role']).','.db_escape($it['subject_type']).','.$it['subject_id'].','.$it['jurisdiction_id'].','.db_escape($it['item_sha256']).')';if(!db_query($sql,'could not append PAY-RULE-005 fact-set item')){$ok=false;break;}}}}
    if($ok&&$return!==false&&$created)$ok=hrm_pay_rule_005_audit('pay_rule_005_jurisdiction_fact_set',array('fact_set_id'=>$return,'employment_id'=>(int)$employment_id,'assignment_id'=>(int)$assignment_id),'pay.rule_005.fact_set.sealed','pay_rule_005_fact_set_sealed',(int)$scope['legal_entity_id'],array('as_of_date'=>$date,'fact_set_sha256'=>$hash,'source_state_sha256'=>$material['source_state_sha256'],'explicit_fact_count'=>count($material['items']),'inferred_fact_count'=>0,'production_activation_allowed'=>false))!==false;
    if($manage){if($ok)commit_transaction();else cancel_transaction();}return $ok?$return:false;
}

function hrm_pay_rule_005_write_applicability($input)
{
    $actor=hrm_pay_rule_005_actor_id();if($actor<=0||!hrm_pay_rule_005_manage_allowed())return false;$v=hrm_pay_rule_005_validate_applicability($input,$err);if($v===false)return false;
    $manage=empty($GLOBALS['transaction_level']);if($manage)begin_transaction();$ok=true;$return=false;
    $r=db_query('SELECT rv.rule_version_id,rv.effective_from,rv.effective_to,p.publication_id FROM '.TB_PREF.'hrm_pay_rule_rule_versions rv INNER JOIN '.TB_PREF.'hrm_pay_rule_lifecycle_publications p ON p.rule_version_id=rv.rule_version_id LEFT JOIN '.TB_PREF.'hrm_pay_rule_lifecycle_events e ON e.publication_id=p.publication_id WHERE rv.rule_version_id='.$v['rule_version_id'].' AND p.publication_id='.$v['publication_id'].' AND e.event_id IS NULL LIMIT 2 FOR UPDATE','could not lock published PAY-RULE-004 Rule Version');
    if(!$r||db_num_rows($r)!==1)$ok=false;else{$rv=db_fetch_assoc($r);if(strcmp($v['effective_from'],$rv['effective_from'])<0||strcmp($v['effective_to'],$rv['effective_to'])>0)$ok=false;}
    if($ok&&!is_array(hrm_pay_rule_005_active_jurisdiction_version($v['jurisdiction_id'],$v['effective_from'],true)))$ok=false;
    if($ok&&!is_array(hrm_pay_rule_005_active_jurisdiction_version($v['jurisdiction_id'],$v['effective_to'],true)))$ok=false;
    $hash=$ok?hrm_pay_rule_005_hash_payload($v):false;
    if($ok){$r=db_query('SELECT applicability_id FROM '.TB_PREF.'hrm_pay_rule_version_applicability WHERE applicability_sha256='.db_escape($hash).' LIMIT 2 FOR UPDATE');if($r&&db_num_rows($r)===1){$x=db_fetch_assoc($r);$return=(int)$x['applicability_id'];}elseif(!$r||db_num_rows($r)>1)$ok=false;}
    if($ok&&$return===false){$r=db_query('SELECT applicability_id FROM '.TB_PREF.'hrm_pay_rule_version_applicability WHERE rule_version_id='.$v['rule_version_id'].' AND jurisdiction_id='.$v['jurisdiction_id'].' AND fact_role='.db_escape($v['fact_role']).' AND resolution_group_key='.db_escape($v['resolution_group_key']).' AND effective_from<='.db_escape($v['effective_to']).' AND effective_to>='.db_escape($v['effective_from']).' FOR UPDATE','could not verify applicability overlap');if(!$r||db_num_rows($r)!==0)$ok=false;}
    if($ok&&$return===false){$sql='INSERT INTO '.TB_PREF.'hrm_pay_rule_version_applicability (rule_version_id,publication_id,jurisdiction_id,fact_role,resolution_group_key,applicability_mode,precedence_rank,effective_from,effective_to,applicability_sha256,created_by) VALUES ('
        .$v['rule_version_id'].','.$v['publication_id'].','.$v['jurisdiction_id'].','.db_escape($v['fact_role']).','.db_escape($v['resolution_group_key']).','.db_escape($v['applicability_mode']).','.$v['precedence_rank'].','.db_escape($v['effective_from']).','.db_escape($v['effective_to']).','.db_escape($hash).','.$actor.')';if(!db_query($sql,'could not append PAY-RULE-005 Rule Version applicability'))$ok=false;else{$return=(int)db_insert_id();$ok=hrm_pay_rule_005_audit('pay_rule_005_rule_applicability',array('applicability_id'=>$return,'rule_version_id'=>$v['rule_version_id']),'pay.rule_005.applicability.created','pay_rule_005_rule_applicability_writer',0,array('resolution_group_key'=>$v['resolution_group_key'],'fact_role'=>$v['fact_role'],'applicability_mode'=>$v['applicability_mode'],'precedence_rank'=>$v['precedence_rank'],'applicability_sha256'=>$hash,'precedence_inferred_from_row_order'=>false,'production_activation_allowed'=>false))!==false;}}
    if($manage){if($ok)commit_transaction();else cancel_transaction();}return $ok?$return:false;
}

function hrm_pay_rule_005_load_fact_set($fact_set_id)
{
    $fact_set_id=(int)$fact_set_id;if($fact_set_id<=0||!hrm_pay_rule_005_resolve_allowed())return false;
    $r=db_query('SELECT fact_set_id,employment_id,assignment_id,legal_entity_id,work_location_id,as_of_date,source_state_sha256,fact_set_sha256 FROM '.TB_PREF.'hrm_pay_rule_jurisdiction_fact_sets WHERE fact_set_id='.$fact_set_id.' LIMIT 2');if(!$r||db_num_rows($r)!==1)return false;$set=db_fetch_assoc($r);
    $r=db_query('SELECT i.binding_id,i.jurisdiction_version_id,i.fact_role,i.subject_type,i.subject_id,i.jurisdiction_id,i.item_sha256,v.hierarchy_depth FROM '.TB_PREF.'hrm_pay_rule_jurisdiction_fact_items i INNER JOIN '.TB_PREF.'hrm_pay_rule_jurisdiction_versions v ON v.jurisdiction_version_id=i.jurisdiction_version_id WHERE i.fact_set_id='.$fact_set_id.' ORDER BY i.fact_item_id');if(!$r)return false;$items=array();while($x=db_fetch_assoc($r)){$lin=hrm_pay_rule_005_jurisdiction_lineage((int)$x['jurisdiction_id'],(string)$set['as_of_date'],false);if(!is_array($lin))return false;$items[]=array('binding_id'=>(int)$x['binding_id'],'fact_role'=>(string)$x['fact_role'],'subject_type'=>(string)$x['subject_type'],'subject_id'=>(int)$x['subject_id'],'jurisdiction_id'=>(int)$x['jurisdiction_id'],'jurisdiction_version_id'=>(int)$x['jurisdiction_version_id'],'lineage'=>$lin['ids'],'item_sha256'=>(string)$x['item_sha256']);}
    $set['fact_set_id']=(int)$set['fact_set_id'];$set['employment_id']=(int)$set['employment_id'];$set['assignment_id']=(int)$set['assignment_id'];$set['legal_entity_id']=(int)$set['legal_entity_id'];$set['work_location_id']=(int)$set['work_location_id'];$set['items']=$items;return $set;
}

function hrm_pay_rule_005_applicability_candidates($date,$required_groups,$for_update=false)
{
    if(hrm_pay_rule_005_date($date)===false||!is_array($required_groups)||count($required_groups)===0)return false;$quoted=array();foreach($required_groups as $g){$g=hrm_pay_rule_005_ascii_token($g,160);if($g===false)return false;$quoted[]=db_escape($g);}
    $sql='SELECT a.applicability_id,a.rule_version_id,a.publication_id,a.jurisdiction_id,a.fact_role,a.resolution_group_key,a.applicability_mode,a.precedence_rank,a.effective_from,a.effective_to,a.applicability_sha256 '
        .'FROM '.TB_PREF.'hrm_pay_rule_version_applicability a INNER JOIN '.TB_PREF.'hrm_pay_rule_lifecycle_publications p ON p.publication_id=a.publication_id AND p.rule_version_id=a.rule_version_id LEFT JOIN '.TB_PREF.'hrm_pay_rule_lifecycle_events e ON e.publication_id=p.publication_id '
        .'WHERE e.event_id IS NULL AND a.effective_from<='.db_escape($date).' AND a.effective_to>='.db_escape($date).' AND a.resolution_group_key IN ('.implode(',',$quoted).') ORDER BY a.resolution_group_key,a.precedence_rank,a.applicability_id'.($for_update?' FOR UPDATE':'');
    $r=db_query($sql,'could not read PAY-RULE-005 applicability candidates');if(!$r)return false;$rows=array();while($x=db_fetch_assoc($r)){$x['applicability_id']=(int)$x['applicability_id'];$x['rule_version_id']=(int)$x['rule_version_id'];$x['publication_id']=(int)$x['publication_id'];$x['jurisdiction_id']=(int)$x['jurisdiction_id'];$x['precedence_rank']=(int)$x['precedence_rank'];$rows[]=$x;}return $rows;
}
