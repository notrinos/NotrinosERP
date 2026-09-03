<?php
/** HRM-FND-004 final-checker evidence-bound identifier verification promotion continuation. */
$identifier_verification_promotion_raw_method=isset($_SERVER['REQUEST_METHOD'])?(string)$_SERVER['REQUEST_METHOD']:'';
$identifier_verification_promotion_script_name=isset($_SERVER['SCRIPT_NAME'])?(string)$_SERVER['SCRIPT_NAME']:'';
$page_security='SA_EMPLOYEE';
$path_to_root='../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_security.inc');
include_once($path_to_root.'/hrm/includes/db/person_identifier_verification_evidence_db.inc');
include_once($path_to_root.'/hrm/includes/db/person_identifier_verification_promotion_browser_db.inc');
include_once($path_to_root.'/includes/hrm_identifier_verification_promotion_route.inc');

hrm_identifier_verification_promotion_route_security_headers();
$now=time();
$employee_ref=hrm_identifier_verification_promotion_route_employee_ref(
    isset($_POST['employee_id'])?$_POST['employee_id']:(isset($_GET['employee_id'])?$_GET['employee_id']:'')
);
if(!hrm_identifier_verification_promotion_route_authenticated()||$employee_ref===false){
    page(_($help_context='Identifier Verification Promotion'));display_error(_('Restricted identity verification authority is required.'));end_page();exit;
}
if(!hrm_identifier_verification_promotion_route_ready()){
    page(_($help_context='Identifier Verification Promotion'));display_error(_('Software Upgrade must be completed before identifier verification promotion is available.'));end_page();exit;
}

$generic_error='';
if($identifier_verification_promotion_raw_method==='POST'&&isset($_POST['route_action'])&&(string)$_POST['route_action']==='promote_identifier'){
    $intent=check_csrf_token()?hrm_identifier_verification_promotion_route_consume_intent(
        $employee_ref,isset($_POST['promotion_intent'])?(string)$_POST['promotion_intent']:'',$now):false;
    if(!is_array($intent)){
        $generic_error=_('The promotion confirmation was rejected. No identifier state changed.');
    }else{
        $candidate=hrm_identifier_verification_promotion_browser_candidate(
            $employee_ref,(int)$intent['verification_evidence_id'],(int)$intent['evidence_row_version'],
            (int)$intent['request_row_version'],(int)$intent['identifier_row_version']);
        if(!is_array($candidate)){
            $generic_error=_('The evidence or approval custody changed before promotion. No identifier state changed.');
        }else{
            $result=promote_hrm_person_identifier_verification(
                (int)$candidate['verification_evidence_id'],(int)$candidate['evidence_row_version'],
                (int)$candidate['request_row_version'],(int)$candidate['identifier_row_version']);
            if(is_array($result)&&hrm_identifier_verification_promotion_route_set_flash($employee_ref,$result)){
                hrm_identifier_verification_promotion_route_redirect_local($identifier_verification_promotion_script_name,$employee_ref,'success');exit;
            }
            $generic_error=_('The identifier was not promoted. The command retained no partial verification or approval state.');
        }
    }
}

page(_($help_context='Identifier Verification Promotion'));
echo '<div class="center" style="max-width:860px;margin:0 auto;">';
echo '<p>'._('This final-checker continuation promotes only an exact current identifier whose approved request and opaque evidence are still bound to the same final checker and to the already-consumed evidence-registration assurance. It never reveals identifier plaintext and does not create a second assurance event.').'</p>';
if($generic_error!=='')display_error($generic_error);
$flash=hrm_identifier_verification_promotion_route_take_flash($employee_ref);
if(is_array($flash)){
    display_notification($flash['status']==='already_promoted'?_('Identifier verification was already promoted by the exact prior attempt.'):_('Identifier verification and approval promoted.'));
    echo '<p>'.sprintf(_('Evidence #%d is bound to identifier #%d at row version %d.'),(int)$flash['verification_evidence_id'],(int)$flash['identifier_id'],(int)$flash['identifier_row_version']).'</p>';
    echo '<p>'._('The promotion changed only verification/approval metadata through the accepted command. Request, approval, evidence and assurance rows were not rewritten.').'</p>';
}
if(isset($_GET['promotion'])&&$_GET['promotion']==='failed')display_error(_('Identifier promotion failed. No partial verification or approval state was retained.'));

$candidates=get_hrm_identifier_verification_promotion_browser_candidates_masked($employee_ref);
echo '<h3>'._('Evidence-bound identifier promotion').'</h3>';
if(!is_array($candidates)||count($candidates)===0){
    display_warning(_('No exact registered evidence is eligible for promotion by this final checker.'));
}else{
    foreach($candidates as $candidate){
        $intent=hrm_identifier_verification_promotion_route_issue_intent($employee_ref,$candidate,$now);
        if($intent===false){display_error(_('A promotion confirmation could not be prepared.'));continue;}
        start_form(false,$identifier_verification_promotion_script_name);
        hidden('_token',ensure_csrf_token());hidden('route_action','promote_identifier');hidden('employee_id',$employee_ref);hidden('promotion_intent',$intent);
        start_table(TABLESTYLE2);
        label_row(_('Identifier:'),htmlspecialchars((string)$candidate['identifier_type'],ENT_QUOTES,'UTF-8').' — '.htmlspecialchars((string)$candidate['masked_value'],ENT_QUOTES,'UTF-8').' — '.sprintf(_('#%d, evidence-bound row version %d; current row version %d'),(int)$candidate['identifier_id'],(int)$candidate['identifier_row_version'],(int)$candidate['identifier_current_row_version']));
        label_row(_('Evidence:'),sprintf(_('#%d, evidence version %d'),(int)$candidate['verification_evidence_id'],(int)$candidate['evidence_row_version']));
        label_row(_('Approved request:'),sprintf(_('#%d, request version %d, draft #%d'),(int)$candidate['verification_request_id'],(int)$candidate['request_row_version'],(int)$candidate['approval_draft_id']));
        label_row(_('Verification method:'),htmlspecialchars((string)$candidate['verification_method'],ENT_QUOTES,'UTF-8'));
        label_row(_('Evidence source:'),htmlspecialchars((string)$candidate['evidence_source'],ENT_QUOTES,'UTF-8'));
        label_row(_('Registration assurance:'),_('Consumed and exact-action bound'));
        label_row(_('Promotion state:'),$candidate['promotion_state']==='already_promoted'?_('Already promoted; exact retry may reconcile safely'):_('Pending promotion'));
        end_table(1);submit_center('promote',$candidate['promotion_state']==='already_promoted'?_('Reconcile Exact Promotion Retry'):_('Promote Verified Identifier'));end_form();
    }
}
echo '<p>'._('Promotion consumes no new assurance. The command revalidates the exact evidence, request, completed approval draft, final checker and consumed registration-assurance hash under row locks before changing verification metadata.').'</p>';
echo '<p><a href="identifier_verification_evidence.php?employee_id='.rawurlencode($employee_ref).'">'._('Back to Identifier Verification Evidence').'</a></p>';
echo '<p><a href="employees.php?employee_id='.rawurlencode($employee_ref).'">'._('Back to Employee Maintenance').'</a></p></div>';
end_page();
