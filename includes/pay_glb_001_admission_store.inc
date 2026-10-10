<?php
/**
 * S23: deployment-owned admission reader. No approval writer or default grants.
 * Pins are supplied ONLY by trusted company configuration. Files reside under
 * the existing PAY-AUD private root, outside every served document root.
 * Deployers must protect configuration, directory ACLs and retained revisions.
 */
require_once __DIR__.'/pay_glb_001_legal_entity_bridge.inc';
// The bootstrapped caller loads session_registry, security_db and domain_audit_db.
// Missing existing infrastructure denies; do not create signing keys on a read.

/** Require the real durable session and freshly read role, never cached role alone. */
function pay_glb_001_current_principal($purpose='read')
{
    global $security_areas;
    if (!in_array($purpose,array('read','native_reader','native_execute','native_select','native_replace','native_retro_review','native_business','native_project','native_intent','native_submit','native_approve','native_gl_prepare','native_gl_review','native_gl_post','native_gl_reverse'),true)) return false;
    if (!isset($_SESSION['wa_current_user']) ||
        !($_SESSION['wa_current_user'] instanceof current_user) ||
        !function_exists('session_registry_validate_current_session') ||
        !function_exists('get_security_role') || !function_exists('user_company') ||
        !function_exists('user_check_access')) return false;
    $user = $_SESSION['wa_current_user'];
    if ($user->logged_in() !== true || !is_int($user->company) || $user->company < 0 ||
        $user->cur_con !== $user->company || user_company() !== $user->company ||
        !ctype_digit((string)$user->user) || (int)$user->user <= 0 ||
        session_registry_validate_current_session() !== true) return false;
    $role = get_security_role($user->access);
    if (!is_array($role) || !isset($role['areas'],$role['sections']) ||
        !is_array($role['areas']) || !is_array($role['sections']) ||
        !empty($role['inactive'])) return false;
    $profiles=array('native_execute'=>array('SA_HRM_EXECUTE_PAY_CORE_CALC','SA_HRM_WRITE_PAY_CORE_RESULT'),
        'native_replace'=>array('SA_HRM_EXECUTE_PAY_CORE_CALC','SA_HRM_WRITE_PAY_CORE_RESULT','SA_HRM_SEAL_PAY_CORE_RESULT','SA_HRM_EXECUTE_PAY_CORE_RETRO'),
        'native_retro_review'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_REVIEW_PAY_CORE_RETRO'),
        'native_business'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION'),
        'native_reader'=>array('SA_HRMREPORTS','SA_HRM_VIEW_PAY_REC','SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION'),
        'native_project'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_HRM_EXECUTE_PAY_CORE_PROJECTION'),
        'native_intent'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_HRM_MANAGE_PAY_CORE_PROJECTION'),
        'native_submit'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_HRM_MANAGE_PAY_CORE_PROJECTION','SA_PAYROLLAPPROVE'),
        'native_approve'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_PAYROLLAPPROVE'),
        'native_gl_prepare'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_HRM_MANAGE_PAY_GL_INTENT'),
        'native_gl_review'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_PAY_GL_REVIEW'),
        'native_gl_post'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_PAY_GL_POST'),
        'native_gl_reverse'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_PAY_GL_REVERSE'),
        'native_select'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT'),
        'read'=>array('SA_HRMREPORTS','SA_HRM_VIEW_PAY_REC'));
    $required=$profiles[$purpose];
    foreach ($required as $area) {
        if (!isset($security_areas[$area][0]) || !user_check_access($area) ||
            !in_array((string)$security_areas[$area][0], array_map('strval',$role['areas']),true) ||
            !in_array((string)($security_areas[$area][0] & ~0xff),
                array_map('strval',$role['sections']),true)) return false;
    }
    return array('company_id'=>$user->company,'actor_user_id'=>(int)$user->user);
}

/** Test canonical filesystem containment, including case-insensitive Windows paths. */
function pay_glb_001_path_within($path, $root)
{
    $path = str_replace('\\','/',$path);
    $root = rtrim(str_replace('\\','/',$root),'/');
    if (DIRECTORY_SEPARATOR === '\\') { $path = strtolower($path); $root = strtolower($root); }
    return $path === $root || strpos($path.'/', $root.'/') === 0;
}

/** Load one fresh bounded revision from a fixed private path, using independent pins. */
function pay_glb_001_read_admission_store($kind)
{
    global $db_connections, $path_to_root;
    if (!in_array($kind,array('acl','trust','mapping','native','selection','correction','business'),true)) return false;
    $principal = pay_glb_001_current_principal($kind==='native'?'native_execute':($kind==='selection'?'native_select':($kind==='correction'?'native_retro_review':($kind==='business'?'native_business':'read'))));
    if ($principal === false) return false;
    $company = $principal['company_id'];
    $config = isset($db_connections[$company]['pay_glb_001_admission'])
        ? $db_connections[$company]['pay_glb_001_admission'] : false;
    if (!is_array($config) || !isset($config[$kind],$config['execution_region']) ||
        !pay_glb_001_code($config['execution_region']) ||
        !pay_glb_001_exact_keys($config[$kind],array('sha256','revision')) ||
        !pay_glb_001_sha256($config[$kind]['sha256']) ||
        !is_int($config[$kind]['revision']) || $config[$kind]['revision'] <= 0) return false;
    if (!function_exists('domain_audit_private_root')) return false;
    $private = domain_audit_private_root($company);
    $app = realpath($path_to_root);
    $web = isset($_SERVER['DOCUMENT_ROOT']) && $_SERVER['DOCUMENT_ROOT'] !== ''
        ? realpath($_SERVER['DOCUMENT_ROOT']) : ($app === false ? false : dirname($app));
    if ($private === false || $app === false || $web === false) return false;
    $directory = $private.DIRECTORY_SEPARATOR.'pay-glb-001';
    $path = $directory.DIRECTORY_SEPARATOR.$kind.'.json';
    clearstatcache(true,$path);
    $resolved = realpath($path);
    $root = realpath($directory);
    if ($resolved === false || $root === false || !is_file($path) || is_link($path) ||
        is_link($directory) || !pay_glb_001_path_within($resolved,$root) ||
        pay_glb_001_path_within($root,$app) || pay_glb_001_path_within($root,$web)) return false;
    $handle = @fopen($path,'rb');
    if ($handle === false) return false;
    if (!flock($handle,LOCK_SH)) { fclose($handle); return false; }
    $stat = fstat($handle);
    $bytes = stream_get_contents($handle,262145);
    flock($handle,LOCK_UN); fclose($handle);
    if (!is_array($stat) || ($stat['mode'] & 0170000) !== 0100000 ||
        (DIRECTORY_SEPARATOR === '/' && ($stat['mode'] & 0022) !== 0) ||
        !is_string($bytes) || strlen($bytes) > 262144 ||
        !hash_equals($config[$kind]['sha256'],hash('sha256',$bytes))) return false;
    $record = json_decode($bytes,true,32);
    $now = gmdate('Y-m-d\TH:i:s\Z');
    if (!pay_glb_001_exact_keys($record,array('schema','kind','company_id','revision',
        'approved_at_utc','expires_at_utc','reviewer_user_id','approver_user_id',
        'approval_evidence_sha256','records','revocations')) ||
        $record['schema'] !== 'PAY_GLB_001_SERVER_ADMISSION_V1' ||
        $record['kind'] !== $kind || $record['company_id'] !== $company ||
        $record['revision'] !== $config[$kind]['revision'] ||
        !pay_glb_001_iso_utc($record['approved_at_utc']) ||
        !pay_glb_001_iso_utc($record['expires_at_utc']) ||
        $record['approved_at_utc'] > $now || $record['expires_at_utc'] <= $now ||
        strtotime($record['expires_at_utc'])-strtotime($record['approved_at_utc']) > 86400 ||
        !is_int($record['reviewer_user_id']) || !is_int($record['approver_user_id']) ||
        $record['reviewer_user_id'] <= 0 || $record['approver_user_id'] <= 0 ||
        $record['reviewer_user_id'] === $record['approver_user_id'] ||
        in_array($principal['actor_user_id'],array($record['reviewer_user_id'],
            $record['approver_user_id']),true) ||
        !pay_glb_001_sha256($record['approval_evidence_sha256']) ||
        !is_array($record['records']) || count($record['records']) > 1024 ||
        array_keys($record['records']) !== (count($record['records']) ? range(0,count($record['records'])-1) : array()) ||
        !is_array($record['revocations']) || count($record['revocations']) > 4096 ||
        array_keys($record['revocations']) !== (count($record['revocations']) ? range(0,count($record['revocations'])-1) : array())) return false;
    $revoked = array();
    foreach ($record['revocations'] as $revocation) {
        if (!pay_glb_001_exact_keys($revocation,array('record_id','revoked_at_utc','evidence_sha256')) ||
            !pay_glb_001_code($revocation['record_id']) || isset($revoked[$revocation['record_id']]) ||
            !pay_glb_001_iso_utc($revocation['revoked_at_utc']) ||
            $revocation['revoked_at_utc'] > $now || !pay_glb_001_sha256($revocation['evidence_sha256'])) return false;
        $revoked[$revocation['record_id']] = true;
    }
    $record['revoked_ids'] = $revoked;
    $record['execution_region'] = $config['execution_region'];
    $record['store_sha256'] = $config[$kind]['sha256'];
    return $record;
}

/** Retain a minimal decision in the established immutable PAY-AUD gateway. */
function pay_glb_001_audit_admission($kind, $allowed, $digest)
{
    $principal = pay_glb_001_current_principal();
    if ($principal === false || !function_exists('append_domain_audit_event') ||
        !in_array($kind,array('acl','trust','mapping'),true) || !is_bool($allowed) ||
        !pay_glb_001_sha256($digest)) return false;
    $receipt = append_domain_audit_event(array('company_id'=>$principal['company_id'],
        'entity_type'=>'pay_glb_admission','entity_key'=>array('kind'=>$kind,'digest'=>$digest),
        'action'=>$allowed ? 'pay_glb.admission.allowed' : 'pay_glb.admission.denied',
        'reason_code'=>'pay_glb_'.$kind,'restricted_diff'=>array()));
    return is_array($receipt) && isset($receipt['event_id']) && (int)$receipt['event_id'] > 0;
}

/** Resolve only explicitly requested actor/entity leases; no company-wide fallback. */
function pay_glb_001_resolve_server_access($entity_codes)
{
    $principal = pay_glb_001_current_principal();
    if ($principal === false || !is_array($entity_codes) || count($entity_codes) < 1 ||
        count($entity_codes) > 32 || array_keys($entity_codes) !== range(0,count($entity_codes)-1)) return false;
    $seen = array();
    foreach ($entity_codes as $code) {
        if (!pay_glb_001_code($code) || isset($seen[$code])) return false;
        $seen[$code] = true;
    }
    sort($entity_codes,SORT_STRING);
    $store = pay_glb_001_read_admission_store('acl');
    if ($store === false) return false;
    $now = gmdate('Y-m-d\TH:i:s\Z'); $grants = array(); $matched = array(); $ids = array();
    foreach ($store['records'] as $lease) {
        if (!pay_glb_001_exact_keys($lease,array('record_id','actor_user_id','entity_code',
            'purpose','object_access','residency_decision','permitted_regions',
            'review_evidence_sha256','approved_at_utc','expires_at_utc')) ||
            !pay_glb_001_code($lease['record_id']) || isset($ids[$lease['record_id']]) ||
            !is_int($lease['actor_user_id']) || $lease['actor_user_id'] <= 0 ||
            !pay_glb_001_code($lease['entity_code'])) return false;
        $ids[$lease['record_id']] = true;
        if ($lease['actor_user_id'] !== $principal['actor_user_id'] ||
            !in_array($lease['entity_code'],$entity_codes,true)) continue;
        if (isset($matched[$lease['entity_code']]) || isset($store['revoked_ids'][$lease['record_id']]) ||
            $lease['purpose'] !== 'PAY_GLB_001_INTERNAL_READ' ||
            $lease['object_access'] !== 'ALLOW_VIEW_AGGREGATE' || $lease['residency_decision'] !== 'APPROVED' ||
            !is_array($lease['permitted_regions']) || count($lease['permitted_regions']) > 32 ||
            !in_array($store['execution_region'],$lease['permitted_regions'],true) ||
            !pay_glb_001_sha256($lease['review_evidence_sha256']) ||
            !pay_glb_001_iso_utc($lease['approved_at_utc']) || !pay_glb_001_iso_utc($lease['expires_at_utc']) ||
            $lease['approved_at_utc'] > $now || $lease['expires_at_utc'] <= $now ||
            $lease['expires_at_utc'] > $store['expires_at_utc']) {
            pay_glb_001_audit_admission('acl',false,$store['store_sha256']); return false;
        }
        $matched[$lease['entity_code']] = true;
        $grants[] = array('company_id'=>$principal['company_id'],'entity_code'=>$lease['entity_code'],
            'object_access'=>$lease['object_access'],'residency_decision'=>$lease['residency_decision'],
            'purpose'=>$lease['purpose'],'review_evidence_sha256'=>$lease['review_evidence_sha256'],
            'approved_until_utc'=>$lease['expires_at_utc']);
    }
    $scope = count($matched) === count($entity_codes) ? pay_glb_001_server_scope($grants,$now) : false;
    $entities = $scope === false ? false : pay_glb_001_read_bound_legal_entities($scope,$grants);
    if (!pay_glb_001_audit_admission('acl',$entities !== false,$store['store_sha256']) || $entities === false) return false;
    return array('scope'=>$scope,'entities'=>$entities,'store_sha256'=>$store['store_sha256'],
        'revision'=>$store['revision'],'production_route_allowed'=>false);
}
