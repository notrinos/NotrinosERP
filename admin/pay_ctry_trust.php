<?php
$page_security='SA_PAY_CTRY_TRUST';$path_to_root='..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/admin/db/users_db.inc');
include_once($path_to_root.'/includes/pay_ctry_001_security.inc');
include_once($path_to_root.'/hrm/includes/db/pay_ctry_001_db.inc');
pay_ctry_001_send_security_headers();
page(_($help_context='Country Pack Publisher & Trust Administration'));
if (!pay_ctry_001_site_company_ok()) { display_error(_('Country-pack trust is site-scoped and can only be administered from company 0.')); end_page(); exit; }

function pay_ctry_001_trust_step_up_ok()
{
    if (!isset($_POST['current_password']) || !is_string($_POST['current_password']) || $_POST['current_password']==='') return false;
    return authenticate_user($_SESSION['wa_current_user']->username,$_POST['current_password'])===true;
}
function pay_ctry_001_trust_request_ok($purpose)
{
    return check_csrf_token() && pay_ctry_001_consume_action_nonce($purpose,isset($_POST['action_nonce'])?$_POST['action_nonce']:'') && pay_ctry_001_trust_step_up_ok();
}
$action=isset($_POST['trust_action'])?(string)$_POST['trust_action']:'';
if ($action!=='') {
    $purpose='trust_'.$action;$error=null;$result=false;
    if (!pay_ctry_001_trust_request_ok($purpose)) display_error(_('Trust mutation denied: invalid CSRF/stale request or current-password reauthentication failed.'));
    elseif ($action==='publisher') $result=pay_ctry_001_create_publisher(trim((string)$_POST['publisher_code']),trim((string)$_POST['display_name']),trim((string)$_POST['valid_from']),trim((string)$_POST['valid_to']),$error);
    elseif ($action==='signer') $result=pay_ctry_001_create_signer((int)$_POST['publisher_id'],trim((string)$_POST['signer_code']),trim((string)$_POST['display_name']),$error);
    elseif ($action==='key') $result=pay_ctry_001_import_trust_key((int)$_POST['signer_id'],trim((string)$_POST['key_id']),(string)$_POST['public_key_pem'],trim((string)$_POST['valid_from']),trim((string)$_POST['valid_to']),(int)$_POST['predecessor_key_id'],$error);
    elseif ($action==='revoke_key') $result=pay_ctry_001_revoke_trust_key((int)$_POST['trust_key_id'],trim((string)$_POST['reason_code']),trim((string)$_POST['reason_text']),!empty($_POST['compromised']),$error);
    elseif ($action==='revoke_publisher') $result=pay_ctry_001_revoke_publisher((int)$_POST['publisher_id'],trim((string)$_POST['reason_code']),trim((string)$_POST['reason_text']),$error);
    else $error='unknown_trust_action';
    if ($result===false && $error!==null) display_error(_('Trust mutation rejected: ').(string)$error); elseif ($result!==false) display_notification(_('Immutable country-pack trust evidence was recorded.'));
}
function pay_ctry_001_trust_form_open($action){start_form();hidden('_token',ensure_csrf_token());hidden('trust_action',$action);hidden('action_nonce',pay_ctry_001_issue_action_nonce('trust_'.$action));start_table(TABLESTYLE2);}
function pay_ctry_001_trust_form_close($label){password_row(_('Current password (step-up):'),'current_password',null);end_table(1);submit_center('trust_submit',$label);end_form();}

echo '<p>'._('Trust administration is independent from country-pack installation. Store public keys only; private signing keys must never be imported into ERP.').'</p>';
pay_ctry_001_trust_form_open('publisher');text_row_ex(_('Publisher code:'),'publisher_code',40,64);text_row_ex(_('Display name:'),'display_name',60,160);text_row_ex(_('Valid from (UTC, ISO-8601):'),'valid_from',24,32);text_row_ex(_('Valid to (optional UTC):'),'valid_to',24,32);pay_ctry_001_trust_form_close(_('Create publisher'));
pay_ctry_001_trust_form_open('signer');text_row_ex(_('Publisher ID:'),'publisher_id',12,20);text_row_ex(_('Signer code:'),'signer_code',40,64);text_row_ex(_('Display name:'),'display_name',60,160);pay_ctry_001_trust_form_close(_('Create signer'));
pay_ctry_001_trust_form_open('key');text_row_ex(_('Signer ID:'),'signer_id',12,20);text_row_ex(_('Key ID:'),'key_id',60,96);textarea_row(_('RSA public key PEM:'),'public_key_pem',null,80,12);text_row_ex(_('Valid from (UTC, ISO-8601):'),'valid_from',24,32);text_row_ex(_('Valid to (optional UTC):'),'valid_to',24,32);text_row_ex(_('Predecessor trust-key ID (optional rotation):'),'predecessor_key_id',12,20);pay_ctry_001_trust_form_close(_('Import trusted public key'));
pay_ctry_001_trust_form_open('revoke_key');text_row_ex(_('Trust-key ID:'),'trust_key_id',12,20);text_row_ex(_('Reason code:'),'reason_code',32,48);text_row_ex(_('Reason evidence:'),'reason_text',70,1024);check_row(_('Key compromised:'),'compromised',null);pay_ctry_001_trust_form_close(_('Revoke trust key'));
pay_ctry_001_trust_form_open('revoke_publisher');text_row_ex(_('Publisher ID:'),'publisher_id',12,20);text_row_ex(_('Reason code:'),'reason_code',32,48);text_row_ex(_('Reason evidence:'),'reason_text',70,1024);pay_ctry_001_trust_form_close(_('Revoke publisher'));
end_page();
?>
