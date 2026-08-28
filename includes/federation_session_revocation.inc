<?php
/**********************************************************************
    PAY-SEC-004 targeted federation governance/session revocation command.
***********************************************************************/

/** Normalize a restricted, non-PII governance reason token. */
function federation_revocation_reason($reason)
{
    if (!is_string($reason))
        return false;
    $reason = strtolower(trim($reason));
    if (!preg_match('/^[a-z0-9][a-z0-9_.-]{0,39}$/D', $reason))
        return false;
    return $reason;
}

/** Resolve the authenticated administrator identity without trusting caller input. */
function federation_revocation_actor_id()
{
    if (!isset($_SESSION['wa_current_user']) || !isset($_SESSION['wa_current_user']->user))
        return 0;
    return (int)$_SESSION['wa_current_user']->user;
}

/** Existing user-administration authority is the bounded federation-governance authority. */
function federation_revocation_authorized($company)
{
    if (!function_exists('user_check_access') || !user_check_access('SA_USERS'))
        return false;
    if (function_exists('user_company') && (int)user_company() !== (int)$company)
        return false;
    return federation_revocation_actor_id() > 0;
}

/** Return one exact locked row or false; null means target not found. */
function federation_revocation_one_locked_row($sql, $message)
{
    $result = db_query($sql, $message);
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row)
        return null;
    if (db_fetch_assoc($result) !== false)
        return false;
    return $row;
}

/** Lock only active durable federated sessions bound to the requested governance target. */
function federation_revocation_bound_sessions_for_update($operation, $target_id)
{
    $target_id = (int)$target_id;
    if ($target_id <= 0)
        return false;
    if ($operation === 'provider_disable')
        $scope = 'c.provider_id='.$target_id;
    elseif ($operation === 'configuration_disable')
        $scope = 'c.verifier_config_id='.$target_id;
    elseif ($operation === 'link_deprovision')
        $scope = 'b.external_subject_link_id='.$target_id;
    else
        return false;

    $sql = "SELECT s.id FROM ".TB_PREF."user_sessions s "
        ."INNER JOIN ".TB_PREF."user_federation_session_bindings b ON b.session_id=s.id "
        ."INNER JOIN ".TB_PREF."user_federation_verifier_decisions d ON d.verifier_decision_id=b.verifier_decision_id "
        ."INNER JOIN ".TB_PREF."user_federation_verifier_transactions t ON t.verifier_transaction_id=d.verifier_transaction_id "
        ."INNER JOIN ".TB_PREF."user_federation_provider_verifier_configs c ON c.verifier_config_id=t.verifier_config_id "
        ."INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id "
        ."INNER JOIN ".TB_PREF."user_federation_external_subject_links l ON l.external_subject_link_id=b.external_subject_link_id AND l.provider_id=p.provider_id "
        ."WHERE ".$scope." AND s.auth_method='federated_oidc' AND s.revoked_at IS NULL ORDER BY s.id FOR UPDATE";
    $result = db_query($sql, 'could not lock PAY-SEC-004 targeted federated sessions');
    if (!$result)
        return false;
    $ids = array();
    while ($row = db_fetch_assoc($result)) {
        $id = (int)$row['id'];
        if ($id <= 0 || in_array($id, $ids, true))
            return false;
        $ids[] = $id;
    }
    return $ids;
}

/** Revoke exactly the already locked bound sessions; never revoke by user id. */
function federation_revocation_apply_sessions($session_ids, $actor_id, $reason)
{
    if (!is_array($session_ids))
        return false;
    if (count($session_ids) === 0)
        return true;
    $ids = array();
    foreach ($session_ids as $id) {
        $id = (int)$id;
        if ($id <= 0)
            return false;
        $ids[] = $id;
    }
    $sql = "UPDATE ".TB_PREF."user_sessions SET revoked_at=UTC_TIMESTAMP(),revoked_by=".(int)$actor_id.",revocation_reason=".db_escape($reason)
        ." WHERE revoked_at IS NULL AND auth_method='federated_oidc' AND id IN (".implode(',', $ids).")";
    if (!db_query($sql, 'could not revoke PAY-SEC-004 targeted federated sessions'))
        return false;
    return !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() === count($ids);
}

/** Lock and classify one provider transition. */
function federation_revocation_provider_state($provider_id, $expected_row_version)
{
    $provider_id = (int)$provider_id;$expected_row_version=(int)$expected_row_version;
    if ($provider_id <= 0 || $expected_row_version <= 0)
        return false;
    $row = federation_revocation_one_locked_row("SELECT provider_id,provider_status,provider_row_version FROM ".TB_PREF."user_federation_providers WHERE provider_id=$provider_id LIMIT 2 FOR UPDATE", 'could not lock PAY-SEC-004 provider governance state');
    if (!is_array($row))
        return false;
    $version=(int)$row['provider_row_version'];
    if ((string)$row['provider_status']==='enabled' && $version===$expected_row_version)
        return array('row'=>$row,'idempotent'=>false);
    if ((string)$row['provider_status']==='disabled' && $version===$expected_row_version+1)
        return array('row'=>$row,'idempotent'=>true);
    return false;
}

/** Lock and classify one immutable-version verifier configuration transition. */
function federation_revocation_configuration_state($config_id, $expected_config_version)
{
    $config_id=(int)$config_id;$expected_config_version=(int)$expected_config_version;
    if ($config_id<=0||$expected_config_version<=0)
        return false;
    $row=federation_revocation_one_locked_row("SELECT verifier_config_id,provider_id,config_version,config_status FROM ".TB_PREF."user_federation_provider_verifier_configs WHERE verifier_config_id=$config_id LIMIT 2 FOR UPDATE",'could not lock PAY-SEC-004 verifier configuration governance state');
    if (!is_array($row)||(int)$row['config_version']!==$expected_config_version)
        return false;
    if ((string)$row['config_status']==='enabled')
        return array('row'=>$row,'idempotent'=>false);
    if ((string)$row['config_status']==='disabled')
        return array('row'=>$row,'idempotent'=>true);
    return false;
}

/** Lock and classify one immutable external-subject-link deprovision transition. */
function federation_revocation_link_state($link_id, $expected_verifier_transaction_id)
{
    $link_id=(int)$link_id;$expected_verifier_transaction_id=(int)$expected_verifier_transaction_id;
    if ($link_id<=0||$expected_verifier_transaction_id<=0)
        return false;
    $row=federation_revocation_one_locked_row("SELECT external_subject_link_id,provider_id,user_id,verifier_transaction_id,link_status,deprovisioned_at FROM ".TB_PREF."user_federation_external_subject_links WHERE external_subject_link_id=$link_id LIMIT 2 FOR UPDATE",'could not lock PAY-SEC-004 external-subject link governance state');
    if (!is_array($row)||(int)$row['verifier_transaction_id']!==$expected_verifier_transaction_id)
        return false;
    if ((string)$row['link_status']==='active'&&$row['deprovisioned_at']===null)
        return array('row'=>$row,'idempotent'=>false);
    if ((string)$row['link_status']==='deprovisioned'&&$row['deprovisioned_at']!==null)
        return array('row'=>$row,'idempotent'=>true);
    return false;
}

/** Append one restricted governance event before commit. */
function federation_revocation_audit($company,$operation,$target_id,$expected_token,$revoked_count,$reason)
{
    $actions=array('provider_disable'=>'security.federation.provider.disabled','configuration_disable'=>'security.federation.configuration.disabled','link_deprovision'=>'security.federation.subject_link.deprovisioned');
    if (!isset($actions[$operation]))
        return false;
    return append_domain_audit_event(array('company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_federation_governance','entity_key'=>array('operation'=>$operation,'target_id'=>(int)$target_id),
        'action'=>$actions[$operation],'reason_code'=>$reason,'before'=>array('terminal'=>false),'after'=>array('terminal'=>true),
        'restricted_diff'=>array('expected_lock_token'=>(int)$expected_token,'targeted_federated_sessions_revoked'=>(int)$revoked_count,
            'local_password_sessions_revoked'=>0,'other_provider_sessions_revoked'=>0,'other_configuration_sessions_revoked'=>0,'other_link_sessions_revoked'=>0,
            'raw_external_subject_stored'=>false,'token_or_assertion_material_stored'=>false,'assurance_elevated'=>false,'role_granted'=>false,
            'payment_authority_changed'=>false,'payroll_authority_changed'=>false,'accounting_authority_changed'=>false)))!==false;
}

/** Execute one authorized, optimistic, targeted terminal governance transition. */
function federation_revocation_execute($company,$operation,$target_id,$expected_token,$reason)
{
    global $path_to_root;
    $company=(int)$company;$target_id=(int)$target_id;$expected_token=(int)$expected_token;
    $reason=federation_revocation_reason($reason);
    if ($company<0||$target_id<=0||$expected_token<=0||$reason===false||!federation_revocation_authorized($company))
        return false;
    $actor_id=federation_revocation_actor_id();
    if (!function_exists('append_domain_audit_event')) {
        $root=isset($path_to_root)?realpath($path_to_root):false;
        if ($root===false)return false;
        $audit=$root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'db'.DIRECTORY_SEPARATOR.'domain_audit_db.inc';
        if (!is_file($audit))return false;require_once($audit);
    }
    if (!function_exists('append_domain_audit_event')||!db_query('BEGIN','could not start PAY-SEC-004 targeted revocation transaction'))
        return false;
    $committed=false;$result=false;
    do {
        if ($operation==='provider_disable')$state=federation_revocation_provider_state($target_id,$expected_token);
        elseif ($operation==='configuration_disable')$state=federation_revocation_configuration_state($target_id,$expected_token);
        elseif ($operation==='link_deprovision')$state=federation_revocation_link_state($target_id,$expected_token);
        else $state=false;
        if (!is_array($state))break;
        $sessions=federation_revocation_bound_sessions_for_update($operation,$target_id);if(!is_array($sessions))break;
        if (!empty($state['idempotent'])) {
            if (count($sessions)!==0)break;
            if (!db_query('COMMIT','could not commit PAY-SEC-004 idempotent targeted revocation verification'))break;
            $committed=true;$result=array('operation'=>$operation,'target_id'=>$target_id,'revoked_session_count'=>0,'idempotent_retry'=>true);break;
        }
        if (!federation_revocation_apply_sessions($sessions,$actor_id,$reason))break;
        if ($operation==='provider_disable') {
            $sql="UPDATE ".TB_PREF."user_federation_providers SET provider_status='disabled',provider_row_version=provider_row_version+1,updated_at=UTC_TIMESTAMP(),updated_by=$actor_id WHERE provider_id=$target_id AND provider_status='enabled' AND provider_row_version=$expected_token";
        } elseif ($operation==='configuration_disable') {
            $sql="UPDATE ".TB_PREF."user_federation_provider_verifier_configs SET config_status='disabled' WHERE verifier_config_id=$target_id AND config_status='enabled' AND config_version=$expected_token";
        } else {
            $sql="UPDATE ".TB_PREF."user_federation_external_subject_links SET link_status='deprovisioned',deprovisioned_at=UTC_TIMESTAMP(),deprovisioned_by=$actor_id,deprovision_reason=".db_escape($reason)." WHERE external_subject_link_id=$target_id AND link_status='active' AND deprovisioned_at IS NULL AND verifier_transaction_id=$expected_token";
        }
        if (!db_query($sql,'could not apply PAY-SEC-004 federation governance terminal transition'))break;
        if (function_exists('db_num_affected_rows')&&(int)db_num_affected_rows()!==1)break;
        if (!federation_revocation_audit($company,$operation,$target_id,$expected_token,count($sessions),$reason))break;
        if (!db_query('COMMIT','could not commit PAY-SEC-004 targeted revocation transaction'))break;
        $committed=true;$result=array('operation'=>$operation,'target_id'=>$target_id,'revoked_session_count'=>count($sessions),'idempotent_retry'=>false);
    } while(false);
    if (!$committed){@db_query('ROLLBACK','could not roll back PAY-SEC-004 targeted revocation transaction');return false;}
    return $result;
}

function federation_disable_provider($company,$provider_id,$expected_provider_row_version,$reason)
{
    return federation_revocation_execute($company,'provider_disable',$provider_id,$expected_provider_row_version,$reason);
}
function federation_disable_verifier_configuration($company,$verifier_config_id,$expected_config_version,$reason)
{
    return federation_revocation_execute($company,'configuration_disable',$verifier_config_id,$expected_config_version,$reason);
}
function federation_deprovision_external_subject_link($company,$external_subject_link_id,$expected_verifier_transaction_id,$reason)
{
    return federation_revocation_execute($company,'link_deprovision',$external_subject_link_id,$expected_verifier_transaction_id,$reason);
}
