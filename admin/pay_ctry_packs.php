<?php
$page_security='SA_PAY_CTRY_INSTALL';$path_to_root='..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/pay_ctry_001_security.inc');
include_once($path_to_root.'/hrm/includes/db/pay_ctry_001_db.inc');
pay_ctry_001_send_security_headers();
page(_($help_context='Country Pack Verification & Registration'));
if (!pay_ctry_001_site_company_ok()) { display_error(_('Country-pack trust is site-scoped and can only be administered from company 0.')); end_page(); exit; }

if (isset($_POST['verify_country_pack'])) {
    $error=null;
    $ok=check_csrf_token() && pay_ctry_001_consume_action_nonce('verify_country_pack',isset($_POST['action_nonce'])?$_POST['action_nonce']:'');
    if (!$ok) display_error(_('The country-pack request was stale or already used. Reload and try again.'));
    elseif (empty($_POST['site_only_ack'])) display_error(_('Confirm that verification registers only a site artifact and does not activate payroll.'));
    elseif (!isset($_FILES['country_pack']) || !is_array($_FILES['country_pack']) || (int)$_FILES['country_pack']['error']!==UPLOAD_ERR_OK || !is_uploaded_file($_FILES['country_pack']['tmp_name'])) display_error(_('A valid uploaded country-pack archive is required.'));
    elseif ((int)$_FILES['country_pack']['size']<=0 || (int)$_FILES['country_pack']['size']>PAY_CTRY_001_MAX_ARCHIVE_BYTES) display_error(_('The uploaded archive exceeds the governed size limit.'));
    else {
        $name=(string)$_FILES['country_pack']['name'];$ext=strtolower(pathinfo($name,PATHINFO_EXTENSION));
        if (!in_array($ext,array('zip','cpack'),true)) display_error(_('Country packs must use the .zip or .cpack archive extension.'));
        else {
            $raw=@file_get_contents($_FILES['country_pack']['tmp_name']);
            if (!is_string($raw)) display_error(_('The uploaded archive could not be read.'));
            else {
                $nonce=(string)$_POST['action_nonce'];$idem='pctry-install:'.hash('sha256',$nonce."\0".hash('sha256',$raw));
                $result=pay_ctry_001_verify_and_register($raw,$idem,$error,time());
                if ($result===false) display_error(_('Country-pack registration failed: ').(string)$error);
                elseif (!isset($result['status']) || $result['status']!=='accepted') display_error(_('Country pack rejected by verifier: ').(isset($result['failure_code'])?(string)$result['failure_code']:(string)$error));
                else display_notification(sprintf(_('Country pack verified and registered as site artifact. Pack-version ID %d; report ID %d. No company or payroll activation was granted.'),(int)$result['pack_version_id'],(int)$result['verification_report_id']));
            }
        }
    }
}
if (isset($_POST['reverify_country_pack'])) {
    $error=null;$ok=check_csrf_token()&&pay_ctry_001_consume_action_nonce('reverify_country_pack',isset($_POST['reverify_nonce'])?$_POST['reverify_nonce']:'');
    if (!$ok) display_error(_('The reverification request was stale or already used.'));
    else {$idem='pctry-reverify:'.hash('sha256',(string)$_POST['reverify_nonce'].'\0'.(string)(int)$_POST['raw_blob_id']);$r=pay_ctry_001_reverify_raw_blob((int)$_POST['raw_blob_id'],$idem,$error,time());($r===false||!isset($r['status'])||$r['status']!=='accepted')?display_error(_('Reverification rejected: ').(isset($r['failure_code'])?(string)$r['failure_code']:(string)$error)):display_notification(_('Stored raw bytes were reverified without rewriting historical evidence.'));}
}

echo '<p>'._('Country packs are constrained signed artifacts. Verification never executes PHP/SQL, invokes extension hooks, or activates a pack for any company, legal entity, payroll period, or payroll run.').'</p>';
start_form(true);hidden('_token',ensure_csrf_token());hidden('action_nonce',pay_ctry_001_issue_action_nonce('verify_country_pack'));
start_table(TABLESTYLE2);file_row(_('Signed country-pack archive:'),'country_pack','country_pack');check_row(_('Register only as a site-level verified artifact:'),'site_only_ack',null);end_table(1);
submit_center('verify_country_pack',_('Verify and register country pack'));end_form();

start_form();hidden('_token',ensure_csrf_token());hidden('reverify_nonce',pay_ctry_001_issue_action_nonce('reverify_country_pack'));
start_table(TABLESTYLE2);text_row_ex(_('Raw blob ID:'),'raw_blob_id',12,20);end_table(1);submit_center('reverify_country_pack',_('Reverify stored raw bytes'));end_form();
end_page();
?>
