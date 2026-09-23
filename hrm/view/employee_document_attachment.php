<?php
/** HRM employee-document protected stream with HR and ESS self-scope authorization. */
$path_to_root='../..';$page_security='SA_OPEN';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui/ui_msgs.inc');
include_once($path_to_root.'/hrm/includes/db/employee_document_db.inc');
include_once($path_to_root.'/hrm/includes/db/employee_db.inc');
include_once($path_to_root.'/hrm/includes/hrm_security.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_ess_002_context_db.inc');
include_once($path_to_root.'/admin/db/attachments_db.inc');
include_once($path_to_root.'/includes/attachment_service.inc');
if(!headers_sent()){header('Cache-Control: private, no-store, max-age=0');header('Pragma: no-cache');header('Referrer-Policy: no-referrer');header('X-Content-Type-Options: nosniff');header('X-Frame-Options: DENY');}
if(!isset($_SESSION['wa_current_user'])||!$_SESSION['wa_current_user']->logged_in()){http_response_code(403);exit();}
$access_key=isset($_GET['key'])?strtolower(trim((string)$_GET['key'])):'';if(!preg_match('/^[a-f0-9]{64}$/D',$access_key)){http_response_code(400);echo _('Invalid document reference.');exit();}
$doc=get_employee_document_by_access_key($access_key);if(!$doc){http_response_code(404);echo _('Document not found.');exit();}
$hr_authorized=hrm_user_can_access_sensitive_field(HRM_FIELD_RESTRICTED_DOCUMENT,HRM_FIELD_ACTION_VIEW);
$self_authorized=false;if(function_exists('user_check_access')&&user_check_access('SA_HRM_ESS_VIEW_DOCUMENTS')){$self=get_hrm_ess_002_current_self_context();$self_authorized=is_array($self)&&hash_equals((string)$self['employee_id'],(string)$doc['employee_id']);}
if(!$hr_authorized&&!$self_authorized){hrm_log_sensitive_field_access(HRM_FIELD_RESTRICTED_DOCUMENT,HRM_FIELD_ACTION_VIEW,'denied_content_stream');http_response_code(403);exit();}
if(empty($doc['attachment_id'])||!isset($doc['content_state'])||$doc['content_state']!=='available'){http_response_code(404);exit();}
$attachment=get_attachment((int)$doc['attachment_id']);if(!$attachment){http_response_code(404);exit();}
if((int)$attachment['type_no']!==ST_EMPLOYEE){http_response_code(403);exit();}
$employee_number=resolve_employee_number($doc['employee_id']);if($employee_number===false){http_response_code(404);exit();}
if((int)$attachment['trans_no']!==$employee_number){http_response_code(403);exit();}
$custom=attachment_custom_data_array(isset($attachment['custom_data'])?$attachment['custom_data']:'');if(!isset($custom['storage_backend'])||$custom['storage_backend']!=='hrm_private_v1'||!isset($custom['content_state'])||$custom['content_state']!=='available'||!isset($custom['plaintext_sha256'])||!attachment_hash_equals($custom['plaintext_sha256'],$doc['content_sha256'])){http_response_code(404);exit();}
hrm_log_sensitive_field_access(HRM_FIELD_RESTRICTED_DOCUMENT,HRM_FIELD_ACTION_VIEW,isset($_GET['dl'])?'granted_content_download':'granted_content_view');
stream_attachment_file($attachment,isset($_GET['dl'])?'download':'inline');
