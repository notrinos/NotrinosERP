<?php
/** HRM-FND-004 evidence-bound Person bank-account promotion browser boundary policy. */
if (!defined('HRM_BANK_ACCOUNT_VERIFICATION_PROMOTION_ROUTE_DB_VERSION')) define('HRM_BANK_ACCOUNT_VERIFICATION_PROMOTION_ROUTE_DB_VERSION','1.0.309');
if (!defined('HRM_BANK_ACCOUNT_VERIFICATION_PROMOTION_ROUTE_INTENT_TTL')) define('HRM_BANK_ACCOUNT_VERIFICATION_PROMOTION_ROUTE_INTENT_TTL',300);
if (!defined('HRM_BANK_ACCOUNT_VERIFICATION_PROMOTION_ROUTE_MAX_INTENTS')) define('HRM_BANK_ACCOUNT_VERIFICATION_PROMOTION_ROUTE_MAX_INTENTS',32);

function hrm_bank_account_verification_promotion_route_ready()
{
    return function_exists('get_company_pref')
        && version_compare((string)get_company_pref('version_id', true), HRM_BANK_ACCOUNT_VERIFICATION_PROMOTION_ROUTE_DB_VERSION, '>=');
}

function hrm_bank_account_verification_promotion_route_authenticated()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'],'logged_in') && $_SESSION['wa_current_user']->logged_in()
        && function_exists('hrm_bank_account_verification_request_permission')
        && hrm_bank_account_verification_request_permission();
}

function hrm_bank_account_verification_promotion_route_employee_ref($value)
{
    return function_exists('hrm_bank_account_verification_promotion_browser_employee_ref')
        ? hrm_bank_account_verification_promotion_browser_employee_ref($value) : false;
}

function hrm_bank_account_verification_promotion_route_issue_intent($employee_ref, $candidate, $now = null)
{
    $now = $now === null ? time() : (int)$now;
    $employee_ref = hrm_bank_account_verification_promotion_route_employee_ref($employee_ref);
    if ($employee_ref === false || !is_array($candidate) || $now <= 0) return false;
    foreach (array('verification_evidence_id','evidence_row_version','request_row_version','bank_account_row_version') as $key)
        if (!isset($candidate[$key]) || (int)$candidate[$key] <= 0) return false;
    if (!isset($_SESSION['hrm_bank_account_verification_promotion_intents'])
        || !is_array($_SESSION['hrm_bank_account_verification_promotion_intents']))
        $_SESSION['hrm_bank_account_verification_promotion_intents'] = array();
    foreach ($_SESSION['hrm_bank_account_verification_promotion_intents'] as $intent_key => $intent_row) {
        if (!is_array($intent_row) || !isset($intent_row['expires_at_epoch']) || (int)$intent_row['expires_at_epoch'] <= $now)
            unset($_SESSION['hrm_bank_account_verification_promotion_intents'][$intent_key]);
    }
    if (count($_SESSION['hrm_bank_account_verification_promotion_intents']) >= HRM_BANK_ACCOUNT_VERIFICATION_PROMOTION_ROUTE_MAX_INTENTS)
        return false;
    try { $bytes = random_bytes(32); } catch (Exception $e) { return false; }
    $token = rtrim(strtr(base64_encode($bytes), '+/', '-_'), '=');
    if (preg_match('/^[A-Za-z0-9_-]{43}$/D', $token) !== 1) return false;
    $_SESSION['hrm_bank_account_verification_promotion_intents'][hash('sha256',$token)] = array(
        'employee_ref'=>$employee_ref,
        'verification_evidence_id'=>(int)$candidate['verification_evidence_id'],
        'evidence_row_version'=>(int)$candidate['evidence_row_version'],
        'request_row_version'=>(int)$candidate['request_row_version'],
        'bank_account_row_version'=>(int)$candidate['bank_account_row_version'],
        'expires_at_epoch'=>$now+HRM_BANK_ACCOUNT_VERIFICATION_PROMOTION_ROUTE_INTENT_TTL
    );
    return $token;
}

function hrm_bank_account_verification_promotion_route_consume_intent($employee_ref, $token, $now = null)
{
    $now = $now === null ? time() : (int)$now;
    $employee_ref = hrm_bank_account_verification_promotion_route_employee_ref($employee_ref);
    if ($employee_ref === false || !is_string($token) || preg_match('/^[A-Za-z0-9_-]{43}$/D',$token)!==1 || $now <= 0)
        return false;
    $key = hash('sha256',$token);
    $row = isset($_SESSION['hrm_bank_account_verification_promotion_intents'][$key])
        ? $_SESSION['hrm_bank_account_verification_promotion_intents'][$key] : false;
    unset($_SESSION['hrm_bank_account_verification_promotion_intents'][$key]);
    if (!is_array($row) || (string)$row['employee_ref'] !== $employee_ref || (int)$row['expires_at_epoch'] <= $now)
        return false;
    return $row;
}

function hrm_bank_account_verification_promotion_route_local_path($script_name, $employee_ref, $result)
{
    $suffix='/hrm/manage/person_bank_account_verification_promotion.php';
    if (!is_string($script_name) || substr($script_name,-strlen($suffix)) !== $suffix) return false;
    $employee_ref=hrm_bank_account_verification_promotion_route_employee_ref($employee_ref);
    if ($employee_ref===false) return false;
    $status=in_array($result,array('success','failed'),true)?$result:'failed';
    return $script_name.'?employee_id='.rawurlencode($employee_ref).'&promotion='.$status;
}

function hrm_bank_account_verification_promotion_route_redirect_local($script_name, $employee_ref, $result)
{
    $location=hrm_bank_account_verification_promotion_route_local_path($script_name,$employee_ref,$result);
    if($location===false||headers_sent())return false;
    header('HTTP/1.1 303 See Other');header('Location: '.$location);return true;
}

function hrm_bank_account_verification_promotion_route_set_flash($employee_ref, $result)
{
    $employee_ref=hrm_bank_account_verification_promotion_route_employee_ref($employee_ref);
    if($employee_ref===false||!is_array($result)||!isset($result['verification_evidence_id'],$result['person_bank_account_id'],$result['bank_account_row_version'],$result['status']))return false;
    $status=(string)$result['status'];
    if(!in_array($status,array('promoted','already_promoted'),true))return false;
    $_SESSION['hrm_bank_account_verification_promotion_flash']=array(
        'employee_ref'=>$employee_ref,'verification_evidence_id'=>(int)$result['verification_evidence_id'],
        'person_bank_account_id'=>(int)$result['person_bank_account_id'],'bank_account_row_version'=>(int)$result['bank_account_row_version'],
        'status'=>$status
    );
    return true;
}

function hrm_bank_account_verification_promotion_route_take_flash($employee_ref)
{
    $employee_ref=hrm_bank_account_verification_promotion_route_employee_ref($employee_ref);
    $row=isset($_SESSION['hrm_bank_account_verification_promotion_flash'])?$_SESSION['hrm_bank_account_verification_promotion_flash']:false;
    unset($_SESSION['hrm_bank_account_verification_promotion_flash']);
    return is_array($row)&&$employee_ref!==false&&(string)$row['employee_ref']===$employee_ref?$row:false;
}

function hrm_bank_account_verification_promotion_route_security_headers()
{
    if(headers_sent())return false;
    header('Cache-Control: no-store, max-age=0');header('Pragma: no-cache');header('Referrer-Policy: no-referrer');
    header('X-Frame-Options: DENY');header("Content-Security-Policy: default-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
    header('Permissions-Policy: geolocation=(), microphone=(), camera=()');header('X-Content-Type-Options: nosniff');return true;
}
