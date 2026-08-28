<?php
/**
 * PAY-SEC-004 atomic OIDC replay, transaction-consume and verifier-decision command.
 *
 * Internal command only. It commits an already-verified bounded decision under
 * the exact live callback-claim lease. It creates no public callback route,
 * identity link, login/session, revocation or elevated-assurance authority.
 */

/** Validate the exact non-secret output of the pinned OIDC verifier adaptor. */
function federation_oidc_verified_decision_candidate($candidate)
{
    if (!is_array($candidate))
        return false;
    $expected=array('verified','provider_id','external_subject_hash','issuer_sha256','client_id_sha256',
        'nonce_sha256','kid_sha256','algorithm','issued_at_epoch','not_before_epoch','expires_at_epoch',
        'verified_at_epoch','assurance_method');
    $keys=array_keys($candidate);sort($keys);$sorted=$expected;sort($sorted);
    if ($keys!==$sorted || $candidate['verified']!==true || (int)$candidate['provider_id']<=0
        || (string)$candidate['algorithm']!=='RS256' || (string)$candidate['assurance_method']!=='oidc_rs256')
        return false;
    foreach(array('external_subject_hash','issuer_sha256','client_id_sha256','nonce_sha256','kid_sha256') as $name)
        if(!is_string($candidate[$name])||preg_match('/^[a-f0-9]{64}$/D',$candidate[$name])!==1)return false;
    foreach(array('issued_at_epoch','expires_at_epoch','verified_at_epoch') as $name)
        if(!is_int($candidate[$name])||$candidate[$name]<=0)return false;
    if($candidate['not_before_epoch']!==null&&(!is_int($candidate['not_before_epoch'])||$candidate['not_before_epoch']<=0))return false;
    if($candidate['expires_at_epoch']<=$candidate['issued_at_epoch']
        ||$candidate['expires_at_epoch']<=$candidate['verified_at_epoch'])return false;
    return $candidate;
}

/** Load and lock the exact callback claim, transaction and verifier custody. */
function federation_oidc_decision_claim_for_update($claim_token_hash)
{
    if(!defined('TB_PREF')||!is_string($claim_token_hash)||preg_match('/^[a-f0-9]{64}$/D',$claim_token_hash)!==1)return false;
    $sql="SELECT cc.callback_claim_id,cc.claim_token_hash,cc.claim_status,cc.claim_expires_at,cc.terminal_at,cc.terminal_reason,"
        ."t.verifier_transaction_id,t.verifier_config_id,t.transaction_key_hash,t.nonce_hash,t.proof_ref_hash,t.expires_at transaction_expires_at,t.consumed_at,"
        ."c.provider_id,c.config_version,c.verifier_profile,c.audience_hash,c.max_clock_skew_seconds,c.assurance_level_cap,c.config_status,"
        ."p.protocol,p.issuer_hash,p.provider_status,p.provider_row_version "
        ."FROM ".TB_PREF."user_federation_oidc_callback_claims cc "
        ."INNER JOIN ".TB_PREF."user_federation_verifier_transactions t ON t.verifier_transaction_id=cc.verifier_transaction_id "
        ."INNER JOIN ".TB_PREF."user_federation_provider_verifier_configs c ON c.verifier_config_id=t.verifier_config_id "
        ."INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id "
        ."WHERE cc.claim_token_hash=".db_escape($claim_token_hash)." LIMIT 2 FOR UPDATE";
    $result=db_query($sql,'could not lock PAY-SEC-004 OIDC decision custody');
    if(!$result)return false;$row=db_fetch_assoc($result);
    if(!$row||db_fetch_assoc($result)!==false)return false;
    return $row;
}

/** Load an exact committed decision/replay tuple for idempotent retry. */
function federation_oidc_decision_committed_row($transaction_id)
{
    $transaction_id=(int)$transaction_id;if($transaction_id<=0)return false;
    $sql="SELECT d.verifier_decision_id,d.verifier_transaction_id,d.replay_marker_id,d.external_subject_hash,d.verified_assurance_level,d.assurance_method,d.verified_at,d.valid_until,"
        ."r.verifier_config_id,r.replay_kind,r.replay_hash,r.observed_at,r.expires_at replay_expires_at "
        ."FROM ".TB_PREF."user_federation_verifier_decisions d INNER JOIN ".TB_PREF."user_federation_verifier_replay_markers r ON r.replay_marker_id=d.replay_marker_id "
        ."WHERE d.verifier_transaction_id=$transaction_id LIMIT 2 FOR UPDATE";
    $result=db_query($sql,'could not verify committed PAY-SEC-004 OIDC decision');
    if(!$result)return false;$row=db_fetch_assoc($result);
    if(!$row||db_fetch_assoc($result)!==false)return false;
    return $row;
}

/** Append restricted-value-free evidence inside the atomic decision transaction. */
function federation_oidc_decision_audit($company,$row,$decision_id,$replay_marker_id,$candidate,$replay_hash)
{
    return append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_federation_oidc_verifier_decision',
        'entity_key'=>array('verifier_decision_id'=>(int)$decision_id,
            'verifier_transaction_id'=>(int)$row['verifier_transaction_id'],
            'replay_marker_id'=>(int)$replay_marker_id),
        'action'=>'security.federation.oidc_verifier_decision.committed',
        'reason_code'=>'pay_sec_004_oidc_atomic_replay_consume_decision',
        'before'=>null,'after'=>array('verified_decision_committed'=>true),
        'restricted_diff'=>array(
            'provider_id'=>(int)$row['provider_id'],'verifier_config_id'=>(int)$row['verifier_config_id'],
            'provider_row_version'=>(int)$row['provider_row_version'],'config_version'=>(int)$row['config_version'],
            'external_subject_hash'=>(string)$candidate['external_subject_hash'],
            'authorization_code_replay_hash'=>(string)$replay_hash,
            'nonce_hash'=>(string)$candidate['nonce_sha256'],'kid_hash'=>(string)$candidate['kid_sha256'],
            'assurance_method'=>'oidc_rs256','verified_assurance_level'=>1,
            'raw_authorization_code_stored'=>false,'raw_claim_token_stored'=>false,
            'raw_state_stored'=>false,'raw_nonce_stored'=>false,'raw_token_stored'=>false,
            'private_proof_stored_in_database'=>false,'replay_marker_written'=>true,
            'transaction_consumed'=>true,'verifier_decision_written'=>true,
            'callback_claim_terminalized'=>true,'external_subject_link_written'=>false,
            'federated_login_issued'=>false,'session_revocation_changed'=>false,
            'assurance_elevated'=>false,'payment_authority_changed'=>false,
            'payroll_authority_changed'=>false,'accounting_authority_changed'=>false)));
}

/**
 * Atomically commit replay, transaction consumption and the verified decision.
 *
 * Private PKCE proof deletion occurs only after commit. An exact retry of an
 * already committed tuple is verification-only and retries that cleanup.
 */
function federation_oidc_commit_verified_decision($company,$claim_token,$authorization_code,$candidate)
{
    global $path_to_root;
    $company=(int)$company;$candidate=federation_oidc_verified_decision_candidate($candidate);
    if($company<0||$candidate===false)return false;
    if(!function_exists('federation_oidc_callback_claim_token_hash'))require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_oidc_callback_claim.inc');
    if(!function_exists('federation_oidc_authorization_code_replay_hash'))require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_oidc_token_exchange.inc');
    if(!function_exists('federation_oidc_load_proof'))require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_oidc_transaction.inc');
    if(!function_exists('append_domain_audit_event')){
        $root=isset($path_to_root)?realpath($path_to_root):false;
        if($root===false)return false;$audit=$root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'db'.DIRECTORY_SEPARATOR.'domain_audit_db.inc';
        if(!is_file($audit))return false;require_once($audit);
    }
    $claim_hash=federation_oidc_callback_claim_token_hash($claim_token);
    if($claim_hash===false||!function_exists('append_domain_audit_event')||!db_query('BEGIN','could not start PAY-SEC-004 OIDC atomic decision transaction'))return false;
    $committed=false;$proof_hash=false;$result_value=false;
    do {
        $row=federation_oidc_decision_claim_for_update($claim_hash);if(!is_array($row))break;
        $replay_hash=federation_oidc_authorization_code_replay_hash($company,(int)$row['provider_id'],(int)$row['verifier_config_id'],$authorization_code);
        if($replay_hash===false||(int)$candidate['provider_id']!==(int)$row['provider_id']
            ||(string)$candidate['issuer_sha256']!==(string)$row['issuer_hash']
            ||(string)$candidate['client_id_sha256']!==(string)$row['audience_hash']
            ||(string)$candidate['nonce_sha256']!==(string)$row['nonce_hash']
            ||(string)$row['protocol']!=='oidc'||(string)$row['verifier_profile']!=='oidc_code_pkce_v1'
            ||(int)$row['assurance_level_cap']<1)break;
        $proof_hash=(string)$row['proof_ref_hash'];$now=time();
        $verified_at=gmdate('Y-m-d H:i:s',(int)$candidate['verified_at_epoch']);
        $valid_until=gmdate('Y-m-d H:i:s',(int)$candidate['expires_at_epoch']);

        if((string)$row['claim_status']==='terminal'&&$row['terminal_at']!==null&&(string)$row['terminal_reason']==='decision_committed'){
            $existing=federation_oidc_decision_committed_row((int)$row['verifier_transaction_id']);
            if(!is_array($existing)||(string)$existing['replay_kind']!=='oidc_authorization_code'
                ||(string)$existing['replay_hash']!==$replay_hash
                ||(string)$existing['external_subject_hash']!==(string)$candidate['external_subject_hash']
                ||(int)$existing['verified_assurance_level']!==1||(string)$existing['assurance_method']!=='oidc_rs256'
                ||(string)$existing['verified_at']!==$verified_at||(string)$existing['valid_until']!==$valid_until
                ||$row['consumed_at']===null)break;
            if(!db_query('COMMIT','could not commit PAY-SEC-004 OIDC idempotent decision verification'))break;
            $committed=true;$result_value=array('verifier_decision_id'=>(int)$existing['verifier_decision_id'],
                'verifier_transaction_id'=>(int)$row['verifier_transaction_id'],'provider_id'=>(int)$row['provider_id'],
                'external_subject_hash'=>(string)$candidate['external_subject_hash'],'idempotent_retry'=>true);
            break;
        }

        $proof=federation_oidc_load_proof($company,$proof_hash,$now);
        if(!is_array($proof)||(int)$proof['verifier_config_id']!==(int)$row['verifier_config_id']
            ||(string)$proof['transaction_key_hash']!==(string)$row['transaction_key_hash'])break;

        $claim_expires=strtotime((string)$row['claim_expires_at'].' UTC');$transaction_expires=strtotime((string)$row['transaction_expires_at'].' UTC');
        if((string)$row['claim_status']!=='claimed'||$row['terminal_at']!==null||$row['terminal_reason']!==null
            ||$claim_expires===false||$transaction_expires===false||$now>=$claim_expires||$now>=$transaction_expires
            ||$row['consumed_at']!==null||(string)$row['provider_status']!=='enabled'||(string)$row['config_status']!=='enabled'
            ||(int)$candidate['verified_at_epoch']>$now+60||(int)$candidate['expires_at_epoch']<=$now)break;
        $observed=gmdate('Y-m-d H:i:s',$now);
        $insert="INSERT IGNORE INTO ".TB_PREF."user_federation_verifier_replay_markers(verifier_config_id,replay_kind,replay_hash,observed_at,expires_at) VALUES ("
            .(int)$row['verifier_config_id'].",'oidc_authorization_code',".db_escape($replay_hash).",".db_escape($observed).",".db_escape($valid_until).")";
        if(!db_query($insert,'could not insert PAY-SEC-004 OIDC authorization-code replay marker')||db_num_affected_rows()!==1)break;
        $replay_marker_id=(int)db_insert_id();if($replay_marker_id<=0)break;
        if(!db_query("UPDATE ".TB_PREF."user_federation_verifier_transactions SET consumed_at=".db_escape($observed)." WHERE verifier_transaction_id=".(int)$row['verifier_transaction_id']." AND consumed_at IS NULL",'could not consume PAY-SEC-004 OIDC verifier transaction')||db_num_affected_rows()!==1)break;
        $decision="INSERT INTO ".TB_PREF."user_federation_verifier_decisions(verifier_transaction_id,replay_marker_id,external_subject_hash,verified_assurance_level,assurance_method,verified_at,valid_until) VALUES ("
            .(int)$row['verifier_transaction_id'].",$replay_marker_id,".db_escape($candidate['external_subject_hash']).",1,'oidc_rs256',".db_escape($verified_at).",".db_escape($valid_until).")";
        if(!db_query($decision,'could not append PAY-SEC-004 OIDC verifier decision'))break;
        $decision_id=(int)db_insert_id();if($decision_id<=0)break;
        if(!db_query("UPDATE ".TB_PREF."user_federation_oidc_callback_claims SET claim_status='terminal',terminal_at=".db_escape($observed).",terminal_reason='decision_committed' WHERE callback_claim_id=".(int)$row['callback_claim_id']." AND claim_status='claimed' AND terminal_at IS NULL",'could not terminalize committed PAY-SEC-004 OIDC callback claim')||db_num_affected_rows()!==1)break;
        if(federation_oidc_decision_audit($company,$row,$decision_id,$replay_marker_id,$candidate,$replay_hash)===false)break;
        if(!db_query('COMMIT','could not commit PAY-SEC-004 OIDC atomic decision transaction'))break;
        $committed=true;$result_value=array('verifier_decision_id'=>$decision_id,
            'verifier_transaction_id'=>(int)$row['verifier_transaction_id'],'provider_id'=>(int)$row['provider_id'],
            'external_subject_hash'=>(string)$candidate['external_subject_hash'],'idempotent_retry'=>false);
    }while(false);
    if(!$committed){@db_query('ROLLBACK','could not roll back PAY-SEC-004 OIDC atomic decision transaction');return false;}
    $result_value['proof_cleanup_complete']=federation_oidc_delete_proof($company,$proof_hash)===true;
    $result_value['authentication_issued']=false;$result_value['assurance_elevated']=false;
    return $result_value;
}

/** Return read-only existing-link/local-user eligibility for a committed decision. */
function federation_oidc_existing_link_eligibility($decision_id,$now_epoch=null)
{
    $decision_id=(int)$decision_id;$now=$now_epoch===null?time():$now_epoch;
    if($decision_id<=0||!is_int($now)||$now<=0||!defined('TB_PREF'))return false;
    $sql="SELECT d.verifier_decision_id,d.external_subject_hash,d.valid_until,t.verifier_transaction_id,c.provider_id,c.config_status,p.provider_status,"
        ."l.external_subject_link_id,l.user_id,l.link_status,l.deprovisioned_at,u.inactive "
        ."FROM ".TB_PREF."user_federation_verifier_decisions d "
        ."INNER JOIN ".TB_PREF."user_federation_verifier_transactions t ON t.verifier_transaction_id=d.verifier_transaction_id "
        ."INNER JOIN ".TB_PREF."user_federation_provider_verifier_configs c ON c.verifier_config_id=t.verifier_config_id "
        ."INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id "
        ."LEFT JOIN ".TB_PREF."user_federation_external_subject_links l ON l.provider_id=c.provider_id AND l.external_subject_hash=d.external_subject_hash "
        ."LEFT JOIN ".TB_PREF."users u ON u.id=l.user_id WHERE d.verifier_decision_id=$decision_id LIMIT 2";
    $result=db_query($sql,'could not read PAY-SEC-004 existing-link eligibility');if(!$result)return false;
    $row=db_fetch_assoc($result);if(!$row||db_fetch_assoc($result)!==false)return false;
    $valid_until=strtotime((string)$row['valid_until'].' UTC');$reason='eligible';$eligible=true;
    if($valid_until===false||$now>=$valid_until){$reason='decision_expired';$eligible=false;}
    elseif((string)$row['provider_status']!=='enabled'){$reason='provider_disabled';$eligible=false;}
    elseif((string)$row['config_status']!=='enabled'){$reason='configuration_disabled';$eligible=false;}
    elseif($row['external_subject_link_id']===null){$reason='existing_link_required';$eligible=false;}
    elseif((string)$row['link_status']!=='active'||$row['deprovisioned_at']!==null){$reason='link_inactive';$eligible=false;}
    elseif((int)$row['inactive']!==0){$reason='local_user_inactive';$eligible=false;}
    return array('eligible'=>$eligible,'reason'=>$reason,'verifier_decision_id'=>$decision_id,
        'external_subject_link_id'=>$eligible?(int)$row['external_subject_link_id']:0,
        'user_id'=>$eligible?(int)$row['user_id']:0,'authentication_issued'=>false,
        'session_created'=>false,'assurance_elevated'=>false);
}
