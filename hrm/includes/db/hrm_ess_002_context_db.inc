<?php
/** HRM-ESS-002 server-derived self and manager/team context. */
include_once($path_to_root.'/hrm/includes/hrm_ess_002_contract.inc');
include_once($path_to_root.'/hrm/includes/db/ess_self_scope_db.inc');
include_once($path_to_root.'/hrm/includes/db/ess_manager_scope_db.inc');
function get_hrm_ess_002_current_self_context($as_of=null){
 $as_of=$as_of===null?hrm_ess_002_today():hrm_ess_002_date($as_of);if($as_of===false)return false;
 $self=get_hrm_ess_current_self_scope();if(!is_array($self)||empty($self['worker_id']))return false;
 $d=db_escape($as_of);$q=db_query('SELECT a.assignment_id,a.employment_id,h.legal_entity_id,a.legacy_employee_number FROM '.TB_PREF.'hrm_assignments a INNER JOIN '.TB_PREF.'hrm_employments h ON h.employment_id=a.employment_id WHERE h.worker_id='.(int)$self['worker_id'].' AND (h.start_date IS NULL OR h.start_date<='.$d.') AND (h.end_date IS NULL OR h.end_date>='.$d.') AND (a.effective_from IS NULL OR a.effective_from<='.$d.') AND (a.effective_to IS NULL OR a.effective_to>='.$d.') ORDER BY a.assignment_id','could not resolve ESS-002 self Assignment scope');
 if(!$q||db_num_rows($q)!==1)return false;$a=db_fetch_assoc($q);if(!$a||(int)$a['assignment_id']<=0||(int)$a['employment_id']<=0||(int)$a['legal_entity_id']<=0)return false;
 return $self+array('as_of'=>$as_of,'assignment_id'=>(int)$a['assignment_id'],'employment_id'=>(int)$a['employment_id'],'legal_entity_id'=>(int)$a['legal_entity_id'],'legacy_employee_number'=>(int)$a['legacy_employee_number']);
}
function hrm_ess_002_self_matches_employee($employee_id,$as_of=null){$c=get_hrm_ess_002_current_self_context($as_of);return is_array($c)&&hash_equals((string)$c['employee_id'],(string)$employee_id);}
function hrm_ess_002_self_matches_assignment($assignment_id,$as_of=null){$c=get_hrm_ess_002_current_self_context($as_of);return is_array($c)&&(int)$c['assignment_id']===(int)$assignment_id;}
function get_hrm_ess_002_current_manager_scope($as_of,$capability='team_view'){
 if(!hrm_ess_002_capability_valid($capability)||!hrm_ess_002_access('SA_HRM_ESS_VIEW_TEAM'))return false;
 $as_of=hrm_ess_002_date($as_of);if($as_of===false||$as_of!==hrm_ess_002_today())return false;$self=get_hrm_ess_current_self_scope();if(!is_array($self))return false;
 $actor=(int)$self['worker_id'];$out=array();$direct=hrm_ess_direct_team_scope_for_worker($actor,$as_of);if($direct===false)return false;
 foreach($direct as $row){$row['scope_origin']='direct';$row['delegation_id']=null;$row['represented_manager_worker_id']=$actor;$out[$row['worker_id'].':'.$row['assignment_id']]=$row;}
 if(!function_exists('hrm_ess_002_active_delegated_managers'))include_once($path_to_root.'/hrm/includes/db/hrm_ess_002_proxy_db.inc');
 $delegated=hrm_ess_002_active_delegated_managers($actor,$as_of,$capability);if($delegated===false)return false;
 foreach($delegated as $g){$rows=hrm_ess_direct_team_scope_for_worker((int)$g['manager_worker_id'],$as_of);if($rows===false)return false;foreach($rows as $row){if((int)$row['legal_entity_id']!==(int)$g['legal_entity_id'])continue;$k=$row['worker_id'].':'.$row['assignment_id'];if(!isset($out[$k])){$row['scope_origin']='delegated';$row['delegation_id']=(int)$g['delegation_id'];$row['capability_id']=(int)$g['capability_id'];$row['represented_manager_worker_id']=(int)$g['manager_worker_id'];$out[$k]=$row;}}}
 ksort($out,SORT_STRING);return array_values($out);
}
function hrm_ess_002_manager_subject($worker_id,$assignment_id,$as_of,$capability){$scope=get_hrm_ess_002_current_manager_scope($as_of,$capability);if(!is_array($scope))return false;foreach($scope as $row)if((int)$row['worker_id']===(int)$worker_id&&(int)$row['assignment_id']===(int)$assignment_id)return $row;return false;}
?>
