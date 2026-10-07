<?php
/** PAY-REC-001 independently recomputed evidence + maker/reviewer/approver contract. Pure helpers only. */
include_once(dirname(__FILE__).'/pay_rec_001_custody_contract.inc');
if (!defined('PAY_REC_001_EVIDENCE_REVIEW_CONTRACT_VERSION')) define('PAY_REC_001_EVIDENCE_REVIEW_CONTRACT_VERSION','1.0');

function pay_rec_001_evidence_review_contract()
{
    return array(
        'contract_version'=>PAY_REC_001_EVIDENCE_REVIEW_CONTRACT_VERSION,
        'runtime_status'=>'APPEND_ONLY_EVIDENCE_REVIEW_APPROVAL_ENABLED_CLOSE_DISABLED',
        'currency_scope'=>array('VND'),
        'evidence_kinds'=>array('payroll','gl','payment','filing'),
        'evidence_statuses'=>array('matched','mismatch'),
        'review_decisions'=>array('approved','rejected'),
        'approval_decisions'=>array('approved','rejected'),
        'prepare_permission'=>PAY_REC_001_PREPARE_PERMISSION,
        'review_permission'=>PAY_REC_001_REVIEW_PERMISSION,
        'approve_permission'=>PAY_REC_001_APPROVE_PERMISSION,
        'independent_source_recomputation_required'=>true,
        'exact_membership_hash_required'=>true,
        'payroll_payment_exact_match_required'=>true,
        'maker_reviewer_separation_required'=>true,
        'reviewer_approver_separation_required'=>true,
        'maker_approver_separation_required'=>true,
        'immutable_review_approval_required'=>true,
        'runtime_writer_enabled'=>true,
        'review_runtime_enabled'=>true,
        'approval_runtime_enabled'=>true,
        'close_state_mutation_enabled'=>false,
        'production_close_allowed'=>false,
        'gl_posting_authority_allowed'=>false,
        'payment_execution_authority_allowed'=>false,
        'filing_submission_authority_allowed'=>false,
        'network_authority_allowed'=>false,
        'payroll_calculation_authority_allowed'=>false,
        'historical_payroll_accounting_filing_mutation_allowed'=>false,
        'automatic_role_grant_allowed'=>false
    );
}
function pay_rec_001_evidence_review_contract_sha256(){return pay_rec_001_hash(pay_rec_001_evidence_review_contract());}

/** Canonicalize one independently recomputed evidence membership. */
function pay_rec_001_evidence_membership($kind,array $members,&$error=null)
{
    $error=null;$contract=pay_rec_001_evidence_review_contract();
    if(!in_array($kind,$contract['evidence_kinds'],true)||!count($members))return pay_rec_001_fail($error,'reconciliation_evidence_membership_invalid');
    $canonical=array();$seen=array();$total=0;
    foreach($members as $member){
        if(!is_array($member)||array_keys($member)!==array('source_key','source_sha256','amount_minor'))return pay_rec_001_fail($error,'reconciliation_evidence_member_invalid');
        $key=pay_rec_001_token($member['source_key'],160);$amount=$member['amount_minor'];
        if($key===false||!pay_rec_001_sha($member['source_sha256'])||!is_int($amount)||$amount<0)return pay_rec_001_fail($error,'reconciliation_evidence_member_invalid');
        if(isset($seen[$key]))return pay_rec_001_fail($error,'reconciliation_evidence_member_duplicate');$seen[$key]=true;
        if($amount>PHP_INT_MAX-$total)return pay_rec_001_fail($error,'reconciliation_evidence_amount_overflow');$total+=$amount;
        $canonical[]=array('source_key'=>$key,'source_sha256'=>(string)$member['source_sha256'],'amount_minor'=>$amount);
    }
    usort($canonical,function($a,$b){return strcmp($a['source_key'],$b['source_key'])?:strcmp($a['source_sha256'],$b['source_sha256']);});
    return array('kind'=>$kind,'members'=>$canonical,'item_count'=>count($canonical),'amount_minor'=>$total,'membership_sha256'=>pay_rec_001_hash($canonical));
}

/** Build the exact immutable evidence-set body persisted by the runtime writer. */
function pay_rec_001_evidence_descriptor($kind,array $members,$status,$source_contract_sha256,&$error=null)
{
    $error=null;$membership=pay_rec_001_evidence_membership($kind,$members,$error);if($membership===false)return false;
    $contract=pay_rec_001_evidence_review_contract();
    if(!in_array($status,$contract['evidence_statuses'],true)||!pay_rec_001_sha($source_contract_sha256))return pay_rec_001_fail($error,'reconciliation_evidence_descriptor_invalid');
    $body=array('evidence_kind'=>$kind,'membership_sha256'=>$membership['membership_sha256'],'item_count'=>$membership['item_count'],'amount_minor'=>$membership['amount_minor'],'status_token'=>$status,'current_evidence'=>1,'source_contract_sha256'=>(string)$source_contract_sha256);
    $body['evidence_sha256']=pay_rec_001_hash($body);$body['members']=$membership['members'];return $body;
}

/** Canonicalize an immutable reconciliation profile before storage. */
function pay_rec_001_profile_descriptor(array $scope,array $policy,$payroll_scope_sha256,&$error=null)
{
    $error=null;foreach(array('company_id','legal_entity_id','payroll_period_id','pay_core_run_id','filing_obligation_id','currency_code') as $k)if(!array_key_exists($k,$scope))return pay_rec_001_fail($error,'reconciliation_profile_scope_incomplete');
    foreach(array('policy_version','tolerance_sha256','rounding_sha256') as $k)if(!array_key_exists($k,$policy))return pay_rec_001_fail($error,'reconciliation_profile_policy_incomplete');
    $ids=array();foreach(array('company_id','legal_entity_id','payroll_period_id','pay_core_run_id','filing_obligation_id') as $k){$ids[$k]=pay_rec_001_uint($scope[$k]);if($ids[$k]===false||($k!=='company_id'&&$ids[$k]<=0))return pay_rec_001_fail($error,'reconciliation_profile_scope_invalid');}
    if($scope['currency_code']!=='VND'||pay_rec_001_token($policy['policy_version'],32)===false||!pay_rec_001_sha($policy['tolerance_sha256'])||!pay_rec_001_sha($policy['rounding_sha256'])||!pay_rec_001_sha($payroll_scope_sha256))return pay_rec_001_fail($error,'reconciliation_profile_scope_invalid');
    $body=$ids+array('currency_code'=>'VND','policy_version'=>(string)$policy['policy_version'],'tolerance_sha256'=>(string)$policy['tolerance_sha256'],'rounding_sha256'=>(string)$policy['rounding_sha256'],'filing_completion_policy_sha256'=>pay_rec_001_filing_completion_policy_sha256(),'payroll_scope_sha256'=>(string)$payroll_scope_sha256);
    $body['profile_sha256']=pay_rec_001_hash($body);return $body;
}

/** Build an immutable independent-review decision; reviewer must differ from maker. */
function pay_rec_001_review_descriptor($close_checklist_id,$checklist_sha256,$readiness_sha256,$maker_id,$reviewer_id,$decision,$review_evidence_sha256,&$error=null)
{
    $error=null;$cid=pay_rec_001_uint($close_checklist_id);$maker=pay_rec_001_uint($maker_id);$reviewer=pay_rec_001_uint($reviewer_id);
    if($cid===false||$cid<=0||$maker===false||$maker<=0||$reviewer===false||$reviewer<=0||$maker===$reviewer||!pay_rec_001_sha($checklist_sha256)||!pay_rec_001_sha($readiness_sha256)||!pay_rec_001_sha($review_evidence_sha256)||!in_array($decision,array('approved','rejected'),true))return pay_rec_001_fail($error,'reconciliation_review_invalid');
    $body=array('close_checklist_id'=>$cid,'checklist_sha256'=>(string)$checklist_sha256,'readiness_sha256'=>(string)$readiness_sha256,'decision'=>$decision,'review_evidence_sha256'=>(string)$review_evidence_sha256,'reviewed_by'=>$reviewer);
    $body['review_sha256']=pay_rec_001_hash($body);return $body;
}

/** Build an immutable approval decision; approver must differ from maker and reviewer. */
function pay_rec_001_approval_descriptor($review_id,$close_checklist_id,$review_sha256,$maker_id,$reviewer_id,$approver_id,$decision,$assurance_evidence_sha256,&$error=null)
{
    $error=null;$rid=pay_rec_001_uint($review_id);$cid=pay_rec_001_uint($close_checklist_id);$maker=pay_rec_001_uint($maker_id);$reviewer=pay_rec_001_uint($reviewer_id);$approver=pay_rec_001_uint($approver_id);
    if($rid===false||$rid<=0||$cid===false||$cid<=0||$maker===false||$maker<=0||$reviewer===false||$reviewer<=0||$approver===false||$approver<=0||$maker===$reviewer||$maker===$approver||$reviewer===$approver||!pay_rec_001_sha($review_sha256)||!pay_rec_001_sha($assurance_evidence_sha256)||!in_array($decision,array('approved','rejected'),true))return pay_rec_001_fail($error,'reconciliation_approval_invalid');
    $body=array('reconciliation_review_id'=>$rid,'close_checklist_id'=>$cid,'review_sha256'=>(string)$review_sha256,'decision'=>$decision,'assurance_evidence_sha256'=>(string)$assurance_evidence_sha256,'approved_by'=>$approver);
    $body['approval_sha256']=pay_rec_001_hash($body);return $body;
}
?>
