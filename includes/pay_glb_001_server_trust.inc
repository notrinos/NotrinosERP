<?php
/** S24: pinned public issuers and one-use, actor-bound proofs. No provider activation. */
require_once __DIR__.'/pay_glb_001_admission_store.inc';
require_once __DIR__.'/pay_glb_001_signed_evidence.inc';

/** Canonical V2 signing bytes bind actor, nonce, issuer revision and all admission pins. */
function pay_glb_001_server_proof_bytes($proof, $kind, $issuer, $key_revision, $acl_pin, $trust_pin)
{
    if (!is_array($proof) || !pay_glb_001_sha256($acl_pin) || !pay_glb_001_sha256($trust_pin)) return false;
    $value=$proof; unset($value['signatures']);
    return json_encode(array('domain'=>'PAY-GLB-001-SERVER-PROOF-V2','proof'=>$value,
        'kind'=>$kind,'issuer_id'=>$issuer,'key_revision'=>$key_revision,
        'acl_store_sha256'=>$acl_pin,'trust_store_sha256'=>$trust_pin),JSON_UNESCAPED_SLASHES);
}

/** Consume once through exclusive create; interrupted/partial receipts remain denied. */
function pay_glb_001_consume_server_proof($company, $proof)
{
    global $path_to_root;
    $principal=pay_glb_001_current_principal();
    if ($principal===false || $company!==$principal['company_id'] || !is_array($proof) ||
        !isset($proof['actor_user_id'],$proof['nonce']) || $proof['actor_user_id']!==$principal['actor_user_id'] ||
        !pay_glb_001_sha256($proof['nonce'])) return false;
    $directory=domain_audit_private_root($company).'/pay-glb-001/replays';
    $root=realpath($directory); $app=realpath($path_to_root);
    $web=isset($_SERVER['DOCUMENT_ROOT']) && $_SERVER['DOCUMENT_ROOT']!==''
        ? realpath($_SERVER['DOCUMENT_ROOT']) : dirname($app);
    if ($root===false || $app===false || $web===false || is_link($directory) ||
        pay_glb_001_path_within($root,$app) || pay_glb_001_path_within($root,$web)) return false;
    // Nonce uniqueness is company-wide, across rotations, actors and payloads.
    $path=$root.'/'.hash('sha256','PAY-GLB-001-NONCE-V2|'.$company.'|'.$proof['nonce']).'.json';
    $handle=@fopen($path,'x+b');
    if ($handle===false) return false;
    @chmod($path,0600);
    $record=json_encode(array('company_id'=>$company,'actor_user_id'=>$proof['actor_user_id'],
        'proof_sha256'=>hash('sha256',json_encode($proof,JSON_UNESCAPED_SLASHES)),
        'consumed_at_utc'=>gmdate('Y-m-d\TH:i:s\Z')),JSON_UNESCAPED_SLASHES);
    $stat=fstat($handle);
    $ok=is_string($record) && is_array($stat) &&
        (DIRECTORY_SEPARATOR!=='/' || ($stat['mode'] & 0077)===0) &&
        fwrite($handle,$record)===strlen($record) && fflush($handle);
    if ($ok && function_exists('fsync')) $ok=fsync($handle);
    fclose($handle);
    // Never unlink a partial receipt: a crash must not make a consumed nonce reusable.
    return $ok;
}

/**
 * Verify only server-pinned issuers; request keys/inventories are never accepted.
 * The digest is authenticated evidence, not proof of collection or legal approval.
 * Each call reloads ACL and trust, checks current sessions and consumes the nonce.
 */
function pay_glb_001_verify_server_proof($entity_codes, $payload_sha256, $proof)
{
    if (!pay_glb_001_sha256($payload_sha256) ||
        !pay_glb_001_exact_keys($proof,array('company_id','actor_user_id','entity_codes','purpose',
            'provider_identity','issued_at_utc','expires_at_utc','nonce','payload_sha256','signatures')) ||
        !pay_glb_001_iso_utc($proof['issued_at_utc']) || !pay_glb_001_iso_utc($proof['expires_at_utc']) ||
        !is_string($proof['nonce']) || preg_match('/^[a-f0-9]{64}$/D',$proof['nonce'])!==1 ||
        !pay_glb_001_code($proof['provider_identity']) ||
        $proof['payload_sha256']!==$payload_sha256 || $proof['purpose']!=='PAY_GLB_001_INTERNAL_READ' ||
        !pay_glb_001_exact_keys($proof['signatures'],array('object_acl','residency','source_mapping'))) return false;
    $access=pay_glb_001_resolve_server_access($entity_codes);
    $store=$access===false ? false : pay_glb_001_read_admission_store('trust');
    $principal=pay_glb_001_current_principal(); $now=gmdate('Y-m-d\TH:i:s\Z');
    if ($access===false || $store===false || $principal===false ||
        $proof['company_id']!==$principal['company_id'] || $proof['actor_user_id']!==$principal['actor_user_id'] ||
        $proof['entity_codes']!==$access['scope']['entity_codes'] ||
        $proof['issued_at_utc']>$now || $proof['expires_at_utc']<=$now ||
        strtotime($now)-strtotime($proof['issued_at_utc'])>60 ||
        strtotime($proof['expires_at_utc'])-strtotime($proof['issued_at_utc'])>120 ||
        $proof['expires_at_utc']>$store['expires_at_utc']) return false;
    $issuers=array(); $fingerprints=array(); $record_ids=array();
    foreach ($store['records'] as $key) {
        if (!pay_glb_001_exact_keys($key,array('record_id','issuer_id','kind','key_revision',
            'provider_identity','public_key_pem','mathematical_key_sha256','valid_from_utc','expires_at_utc')) ||
            !pay_glb_001_code($key['record_id']) || isset($record_ids[$key['record_id']]) || !pay_glb_001_code($key['issuer_id']) ||
            isset($issuers[$key['issuer_id']]) || !is_int($key['key_revision']) || $key['key_revision']<=0 ||
            !pay_glb_001_code($key['provider_identity']) ||
            !in_array($key['kind'],array('OBJECT_ACL','RESIDENCY','SOURCE_MAPPING'),true) ||
            !pay_glb_001_iso_utc($key['valid_from_utc']) || !pay_glb_001_iso_utc($key['expires_at_utc']) ||
            $key['expires_at_utc']<=$key['valid_from_utc'] ||
            !pay_glb_001_sha256($key['mathematical_key_sha256'])) return false;
        $details=pay_glb_001_trusted_rsa_details(array('kind'=>$key['kind'],
            'public_key_pem'=>$key['public_key_pem']),$key['kind']);
        if ($details===false) return false;
        $fingerprint=hash('sha256',base64_encode($details['rsa']['n']).':'.base64_encode($details['rsa']['e']));
        if (!hash_equals($fingerprint,$key['mathematical_key_sha256']) || isset($fingerprints[$fingerprint])) return false;
        $record_ids[$key['record_id']]=true; $fingerprints[$fingerprint]=true; $issuers[$key['issuer_id']]=$key;
    }
    foreach (array('object_acl'=>'OBJECT_ACL','residency'=>'RESIDENCY','source_mapping'=>'SOURCE_MAPPING') as $name=>$kind) {
        $sig=$proof['signatures'][$name];
        if (!pay_glb_001_exact_keys($sig,array('issuer_id','key_revision','signature_base64')) ||
            !is_string($sig['issuer_id']) || !isset($issuers[$sig['issuer_id']])) return false;
        $key=$issuers[$sig['issuer_id']];
        if ($key['kind']!==$kind || $key['key_revision']!==$sig['key_revision'] ||
            $key['provider_identity']!==$proof['provider_identity'] ||
            isset($store['revoked_ids'][$key['record_id']]) || $key['valid_from_utc']>$proof['issued_at_utc'] ||
            $key['expires_at_utc']<$proof['expires_at_utc'] || !is_string($sig['signature_base64']) ||
            strlen($sig['signature_base64'])>2048) return false;
        $bytes=pay_glb_001_server_proof_bytes($proof,$kind,$key['issuer_id'],$key['key_revision'],
            $access['store_sha256'],$store['store_sha256']);
        $signature=base64_decode($sig['signature_base64'],true);
        if ($bytes===false || $signature===false ||
            @openssl_verify($bytes,$signature,$key['public_key_pem'],OPENSSL_ALGO_SHA256)!==1) return false;
    }
    // Fence an operator change during crypto work; old payloads cannot survive
    // a changed ACL/trust file, session revocation or an expired proof.
    $current_acl=pay_glb_001_read_admission_store('acl');
    $current_trust=pay_glb_001_read_admission_store('trust');
    if ($current_acl===false || $current_trust===false ||
        $current_acl['store_sha256']!==$access['store_sha256'] ||
        $current_trust['store_sha256']!==$store['store_sha256'] ||
        $proof['expires_at_utc']<=gmdate('Y-m-d\TH:i:s\Z') ||
        !pay_glb_001_consume_server_proof($principal['company_id'],$proof) ||
        !pay_glb_001_audit_admission('trust',true,$store['store_sha256'])) return false;
    return array('signature_verified'=>true,'payload_sha256'=>$payload_sha256,
        'provider_identity'=>$proof['provider_identity'],'production_route_allowed'=>false,
        'provider_execution_allowed'=>false,'close_ready'=>false);
}
