<?php
/**
 * PAY-GLB-001 S20: role-separated, revocation-sensitive trust inventory
 * contract. Valid RSA signatures and supplied key inventory CANNOT prove keys
 * were admitted, pinned in a server-owned store or checked against revocations.
 * No private keys, file I/O, session trust, or route activation.
 */
require_once __DIR__.'/pay_glb_001_signed_evidence.inc';

/** Check signed key inventory and actor separation without establishing issuer trust. */
function pay_glb_001_trust_inventory_candidate($scope, $admission, $manifest,
    $signed, $keys, $inventory)
{
    // Signature integrity alone does not validate the actor used for separation.
    if (!is_array($admission) || !isset($admission['actor_user_id']) ||
        !is_int($admission['actor_user_id']) || $admission['actor_user_id'] <= 0 ||
        !pay_glb_001_signed_bundle_valid($scope,$admission,$manifest,$signed,$keys) ||
        !is_array($inventory) || count($inventory) !== 3 ||
        array_keys($inventory) !== array('object_acl','residency','source_mapping')) return false;
    $ids = array(); $fingerprints = array();
    foreach (array('object_acl'=>'OBJECT_ACL','residency'=>'RESIDENCY',
            'source_mapping'=>'SOURCE_MAPPING') as $name=>$role) {
        $item = $inventory[$name];
        if (!pay_glb_001_exact_keys($item, array('kind','issuer_id',
                'mathematical_key_sha256','registry_revision','reviewer_user_id',
                'approver_user_id','valid_from_utc','expires_at_utc','revoked_at_utc')) ||
            $item['kind'] !== $role || $item['issuer_id'] !== $signed[$name]['issuer_id'] ||
            isset($ids[$item['issuer_id']]) ||
            !isset($keys[$item['issuer_id']]) ||
            !is_int($item['registry_revision']) || $item['registry_revision'] <= 0 ||
            !is_int($item['reviewer_user_id']) || !is_int($item['approver_user_id']) ||
            $item['reviewer_user_id'] <= 0 || $item['approver_user_id'] <= 0 ||
            $item['reviewer_user_id'] === $item['approver_user_id'] ||
            $item['reviewer_user_id'] === $admission['actor_user_id'] ||
            $item['approver_user_id'] === $admission['actor_user_id'] ||
            !pay_glb_001_iso_utc($item['valid_from_utc']) ||
            !pay_glb_001_iso_utc($item['expires_at_utc']) ||
            $item['valid_from_utc'] > $scope['as_of_utc'] ||
            $item['expires_at_utc'] <= $scope['as_of_utc'] ||
            $signed[$name]['expires_at_utc'] > $item['expires_at_utc'] ||
            $item['revoked_at_utc'] !== null) return false;
        $details = pay_glb_001_trusted_rsa_details($keys[$item['issuer_id']], $role);
        if ($details === false) return false;
        $fingerprint = hash('sha256',base64_encode($details['rsa']['n']).':' .
            base64_encode($details['rsa']['e']));
        if (!pay_glb_001_sha256($item['mathematical_key_sha256']) ||
            !hash_equals($fingerprint,$item['mathematical_key_sha256']) ||
            isset($fingerprints[$fingerprint])) return false;
        $ids[$item['issuer_id']] = true;
        $fingerprints[$fingerprint] = true;
    }
    return array('candidate_consistent'=>true,'server_trust_store_verified'=>false,
        'registry_revocation_verified'=>false,'authorize_request'=>false,
        'company_id'=>$scope['company_id'],'issuer_count'=>3);
}
?>
