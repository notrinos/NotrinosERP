<?php
/**
 * PAY-CTRY-004 certification/runtime bridge.
 *
 * This is deliberately a metadata guard only. PAY-CTRY-002 remains the sole
 * company activation authority and PAY-CTRY-003 remains the pack artifact /
 * host-adapter contract.  This bridge never posts payroll, writes accounting,
 * executes pack PHP/SQL, or mutates historical payroll/accounting evidence.
 */
$pay_ctry_004_runtime_root=isset($path_to_root)?$path_to_root:dirname(dirname(dirname(__DIR__)));
foreach(array('/includes/pay_ctry_004_contract.inc','/includes/pay_ctry_002_contract.inc','/includes/pay_ctry_003_contract.inc') as $relative){$candidate=$pay_ctry_004_runtime_root.$relative;if(is_file($candidate))include_once($candidate);}
unset($pay_ctry_004_runtime_root,$relative,$candidate);

function pay_ctry_004_runtime_context_from_activation(array $activation,array $context,&$error=null)
{
    $error=null;
    foreach(array('status','company_id','scope_type','pack_version_id') as $key)if(!array_key_exists($key,$activation)){$error='activation_snapshot_incomplete';return false;}
    if((string)$activation['status']!=='governed'){$error='governed_activation_required';return false;}
    if(pay_ctry_004_uint($activation['company_id'],true)===false||pay_ctry_004_uint($activation['pack_version_id'],false)===false){$error='activation_identity_invalid';return false;}
    $scope=(string)$activation['scope_type'];if(!in_array($scope,array('company','legal_entity','payroll_scope'),true)){$error='activation_scope_invalid';return false;}
    $out=$context;$out['company_id']=(int)$activation['company_id'];
    if($scope==='legal_entity'){
        if(!isset($activation['legal_entity_id'])||pay_ctry_004_uint($activation['legal_entity_id'],false)===false){$error='activation_legal_entity_invalid';return false;}
        $out['legal_entity_id']=(int)$activation['legal_entity_id'];
    }
    if($scope==='payroll_scope'){
        if(!isset($activation['payroll_scope_key'])||trim((string)$activation['payroll_scope_key'])===''){$error='activation_payroll_scope_invalid';return false;}
        $out['payroll_scope_key']=(string)$activation['payroll_scope_key'];
        if(isset($activation['legal_entity_id'])&&$activation['legal_entity_id']!==null&&$activation['legal_entity_id']!==''&&pay_ctry_004_uint($activation['legal_entity_id'],false)!==false)$out['legal_entity_id']=(int)$activation['legal_entity_id'];
    }
    return $out;
}

/**
 * Guard one future payroll use. A caller must supply a current immutable
 * certification/publication snapshot resolved from PAY-CTRY-004 custody.
 * Historical reads remain outside this future-execution guard.
 */
function pay_ctry_004_runtime_certification_guard(array $activation,array $certification,array $claim,array $context,$at,&$error=null)
{
    $error=null;$runtime=pay_ctry_004_runtime_context_from_activation($activation,$context,$error);if($runtime===false)return false;
    foreach(array('status','pack_version_id','semantic_root','evidence_root','expires_at','claim_scope_sha256','suspended','revoked','review_chain_complete','signature_verified') as $key)if(!array_key_exists($key,$certification)){$error='certification_snapshot_incomplete';return false;}
    if((string)$certification['status']!=='Published'){$error='certification_not_published';return false;}
    if((int)$certification['pack_version_id']!==(int)$activation['pack_version_id']){$error='certification_pack_mismatch';return false;}
    foreach(array('semantic_root','evidence_root') as $root)if(!isset($context[$root])||pay_ctry_004_sha256((string)$context[$root])===false){$error='installed_pack_root_missing';return false;}
    if(pay_ctry_004_sha256((string)$certification['semantic_root'])===false||pay_ctry_004_sha256((string)$certification['evidence_root'])===false){$error='certification_root_invalid';return false;}
    if((string)$certification['semantic_root']!==(string)$context['semantic_root']){$error='certification_semantic_root_mismatch';return false;}
    if((string)$certification['evidence_root']!==(string)$context['evidence_root']){$error='certification_evidence_root_mismatch';return false;}
    if(empty($certification['review_chain_complete'])){$error='review_chain_incomplete';return false;}if(empty($certification['signature_verified'])){$error='certification_signature_unverified';return false;}
    if(!empty($certification['suspended'])){$error='certification_suspended';return false;}if(!empty($certification['revoked'])){$error='certification_revoked';return false;}
    if(pay_ctry_004_effective_status('Published',(string)$certification['expires_at'],(string)$at)!=='Published'){$error='certification_not_current';return false;}
    $claim_hash=pay_ctry_004_claim_scope_sha256($claim,$error);if($claim_hash===false||(string)$certification['claim_scope_sha256']!==$claim_hash){$error='claim_scope_mismatch';return false;}
    if(!pay_ctry_004_claim_allows($claim,$runtime,$at,$error))return false;
    $policy=pay_ctry_004_future_execution_policy('Published','active',(string)$certification['expires_at'],(string)$at);if(empty($policy['allowed'])){$error=(string)$policy['reason'];return false;}
    return array('allowed'=>true,'pack_version_id'=>(int)$activation['pack_version_id'],'company_id'=>(int)$activation['company_id'],'claim_scope_sha256'=>$claim_hash,'semantic_root'=>(string)$certification['semantic_root'],'evidence_root'=>(string)$certification['evidence_root'],'historical_read_allowed'=>true,'historical_rewrite_allowed'=>false,'certification_implies_activation'=>false);
}
?>
