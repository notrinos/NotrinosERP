<?php
/** Browser security helpers for PAY-CTRY-001 sensitive site-level operations. */
function pay_ctry_001_send_security_headers()
{
    if (!headers_sent()) {
        header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
        header('Pragma: no-cache');
        header('X-Content-Type-Options: nosniff');
        header('X-Frame-Options: SAMEORIGIN');
        header("Referrer-Policy: same-origin");
    }
}
function pay_ctry_001_issue_action_nonce($purpose)
{
    $purpose=(string)$purpose;
    $nonce=bin2hex(random_bytes(24));
    if (!isset($_SESSION['pay_ctry_001_nonces']) || !is_array($_SESSION['pay_ctry_001_nonces']))
        $_SESSION['pay_ctry_001_nonces']=array();
    $_SESSION['pay_ctry_001_nonces'][$purpose]=array(
        'sha256'=>hash('sha256',$nonce),
        'user'=>isset($_SESSION['wa_current_user']->user)?(int)$_SESSION['wa_current_user']->user:0,
        'company'=>function_exists('user_company')?(int)user_company():-1,
        'expires'=>time()+900,
    );
    return $nonce;
}
function pay_ctry_001_consume_action_nonce($purpose,$nonce)
{
    $purpose=(string)$purpose;$nonce=(string)$nonce;
    $all=isset($_SESSION['pay_ctry_001_nonces'])&&is_array($_SESSION['pay_ctry_001_nonces'])?$_SESSION['pay_ctry_001_nonces']:array();
    $row=isset($all[$purpose])?$all[$purpose]:null;
    unset($_SESSION['pay_ctry_001_nonces'][$purpose]);
    if (!is_array($row)||$nonce===''||!isset($row['sha256'],$row['user'],$row['company'],$row['expires'])) return false;
    if ((int)$row['expires'] < time()) return false;
    if ((int)$row['user'] !== (isset($_SESSION['wa_current_user']->user)?(int)$_SESSION['wa_current_user']->user:0)) return false;
    if ((int)$row['company'] !== (function_exists('user_company')?(int)user_company():-1)) return false;
    return hash_equals((string)$row['sha256'],hash('sha256',$nonce));
}
?>
