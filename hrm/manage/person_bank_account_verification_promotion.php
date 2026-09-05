<?php
/** HRM-FND-004 final-checker evidence-bound Person bank-account verification promotion continuation. */
$bank_account_verification_promotion_raw_method=isset($_SERVER['REQUEST_METHOD'])?(string)$_SERVER['REQUEST_METHOD']:'';
$bank_account_verification_promotion_script_name=isset($_SERVER['SCRIPT_NAME'])?(string)$_SERVER['SCRIPT_NAME']:'';
$page_security='SA_EMPLOYEE';
$path_to_root='../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_security.inc');
include_once($path_to_root.'/hrm/includes/db/person_bank_account_verification_evidence_db.inc');
include_once($path_to_root.'/hrm/includes/db/person_bank_account_verification_promotion_browser_db.inc');
include_once($path_to_root.'/includes/hrm_person_bank_account_verification_promotion_route.inc');

hrm_bank_account_verification_promotion_route_security_headers();
$now=time();
$employee_ref=hrm_bank_account_verification_promotion_route_employee_ref(
    isset($_POST['employee_id'])?$_POST['employee_id']:(isset($_GET['employee_id'])?$_GET['employee_id']:'')
);
if(!hrm_bank_account_verification_promotion_route_authenticated()||$employee_ref===false){
    page(_($help_context='Bank Account Verification Promotion'));display_error(_('Restricted bank verification authority is required.'));end_page();exit;
}
if(!hrm_bank_account_verification_promotion_route_ready()){
    page(_($help_context='Bank Account Verification Promotion'));display_error(_('Software Upgrade must be completed before bank-account verification promotion is available.'));end_page();exit;
}

$generic_error='';
if($bank_account_verification_promotion_raw_method==='POST'&&isset($_POST['route_action'])&&(string)$_POST['route_action']==='promote_bank_account'){
    $intent=check_csrf_token()?hrm_bank_account_verification_promotion_route_consume_intent(
        $employee_ref,isset($_POST['promotion_intent'])?(string)$_POST['promotion_intent']:'',$now):false;
    if(!is_array($intent)){
        $generic_error=_('The promotion confirmation was rejected. No bank-account state changed.');
    }else{
        $candidate=hrm_bank_account_verification_promotion_browser_candidate(
            $employee_ref,(int)$intent['verification_evidence_id'],(int)$intent['evidence_row_version'],
            (int)$intent['request_row_version'],(int)$intent['bank_account_row_version']);
        if(!is_array($candidate)){
            $generic_error=_('The evidence or approval custody changed before promotion. No bank-account state changed.');
        }else{
            $result=promote_hrm_person_bank_account_verification(
                (int)$candidate['verification_evidence_id'],(int)$candidate['evidence_row_version'],
                (int)$candidate['request_row_version'],(int)$candidate['bank_account_row_version']);
            if(is_array($result)&&hrm_bank_account_verification_promotion_route_set_flash($employee_ref,$result)){
                hrm_bank_account_verification_promotion_route_redirect_local($bank_account_verification_promotion_script_name,$employee_ref,'success');exit;
            }
            $generic_error=_('The bank account was not promoted. The command retained no partial verification or approval state.');
        }
    }
}

page(_($help_context='Bank Account Verification Promotion'));
echo '<div class="center" style="max-width:860px;margin:0 auto;">';
echo '<p>'._('This final-checker continuation promotes only an exact current governed bank-account successor whose approved request and opaque evidence remain bound to the same final checker and to the already-consumed evidence-registration assurance. It never reveals bank plaintext and consumes no second assurance event.').'</p>';
if($generic_error!=='')display_error($generic_error);
$flash=hrm_bank_account_verification_promotion_route_take_flash($employee_ref);
if(is_array($flash)){
    display_notification($flash['status']==='already_promoted'?_('Bank-account verification was already promoted by the exact prior attempt.'):_('Bank-account verification and approval promoted.'));
    echo '<p>'.sprintf(_('Evidence #%d is bound to bank account #%d at row version %d.'),(int)$flash['verification_evidence_id'],(int)$flash['person_bank_account_id'],(int)$flash['bank_account_row_version']).'</p>';
    echo '<p>'._('The promotion changed only normalized bank verification/approval metadata through the accepted command. Request, approval, evidence and assurance rows were not rewritten, and no payment election or binding was created.').'</p>';
}
if(isset($_GET['promotion'])&&$_GET['promotion']==='failed')display_error(_('Bank-account promotion failed. No partial verification or approval state was retained.'));

$candidates=get_hrm_bank_account_verification_promotion_browser_candidates_masked($employee_ref);
echo '<h3>'._('Evidence-bound bank-account promotion').'</h3>';
if(!is_array($candidates)||count($candidates)===0){
    display_warning(_('No exact registered bank-account verification evidence is eligible for promotion by this final checker.'));
}else{
    foreach($candidates as $candidate){
        $intent=hrm_bank_account_verification_promotion_route_issue_intent($employee_ref,$candidate,$now);
        if($intent===false){display_error(_('A promotion confirmation could not be prepared.'));continue;}
        start_form(false,$bank_account_verification_promotion_script_name);
        hidden('_token',ensure_csrf_token());hidden('route_action','promote_bank_account');hidden('employee_id',$employee_ref);hidden('promotion_intent',$intent);
        start_table(TABLESTYLE2);
        label_row(_('Bank account:'),htmlspecialchars((string)$candidate['masked_account'],ENT_QUOTES,'UTF-8').' / '.htmlspecialchars((string)$candidate['masked_routing'],ENT_QUOTES,'UTF-8').' — '.htmlspecialchars((string)$candidate['payment_method'],ENT_QUOTES,'UTF-8').' — '.sprintf(_('#%d, evidence-bound row version %d; current row version %d'),(int)$candidate['person_bank_account_id'],(int)$candidate['bank_account_row_version'],(int)$candidate['bank_account_current_row_version']));
        label_row(_('Evidence:'),sprintf(_('#%d, evidence version %d'),(int)$candidate['verification_evidence_id'],(int)$candidate['evidence_row_version']));
        label_row(_('Approved request:'),sprintf(_('#%d, request version %d, draft #%d'),(int)$candidate['verification_request_id'],(int)$candidate['request_row_version'],(int)$candidate['approval_draft_id']));
        label_row(_('Verification method:'),htmlspecialchars((string)$candidate['verification_method'],ENT_QUOTES,'UTF-8'));
        label_row(_('Evidence source:'),htmlspecialchars((string)$candidate['evidence_source'],ENT_QUOTES,'UTF-8'));
        label_row(_('Registration assurance:'),_('Consumed and exact-action bound'));
        label_row(_('Promotion state:'),$candidate['promotion_state']==='already_promoted'?_('Already promoted; exact retry may reconcile safely'):_('Pending promotion'));
        end_table(1);submit_center('promote',$candidate['promotion_state']==='already_promoted'?_('Reconcile Exact Promotion Retry'):_('Promote Verified Bank Account'));end_form();
    }
}
echo '<p>'._('Promotion consumes no new assurance. The command revalidates the exact evidence, request, completed approval draft, final checker and consumed registration-assurance hash under row locks before changing verification metadata.').'</p>';
echo '<p>'._('This continuation does not select or reveal bank plaintext and does not create payment election, allocation, binding, execution, payroll-post, bank-transaction, GL or accounting authority.').'</p>';
echo '<p><a href="person_bank_account_verification_evidence.php?employee_id='.rawurlencode($employee_ref).'">'._('Back to Bank Account Verification Evidence').'</a></p>';
echo '<p><a href="employees.php?employee_id='.rawurlencode($employee_ref).'">'._('Back to Employee Maintenance').'</a></p></div>';
end_page();
