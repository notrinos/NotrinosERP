<?php
/** PAY-CTRY-001 site-level trust, verification and immutable pack registration. */
include_once($path_to_root.'/includes/pay_ctry_001_verifier.inc');
include_once(dirname(__FILE__).'/pay_ctry_001_schema_db.inc');

function pay_ctry_001_actor_id()
{
    return isset($_SESSION['wa_current_user']) && isset($_SESSION['wa_current_user']->user)
        ? (int)$_SESSION['wa_current_user']->user : 0;
}
function pay_ctry_001_site_company_ok()
{
    return function_exists('user_company') && (int)user_company() === PAY_CTRY_001_SITE_COMPANY_ID;
}
function pay_ctry_001_access($permission)
{
    if (!pay_ctry_001_site_company_ok()) return false;
    if (function_exists('user_check_access')) return user_check_access($permission);
    return isset($_SESSION['wa_current_user']) && method_exists($_SESSION['wa_current_user'], 'can_access')
        && $_SESSION['wa_current_user']->can_access($permission);
}
function pay_ctry_001_sql_hex($bytes)
{
    return "X'".bin2hex((string)$bytes)."'";
}
function pay_ctry_001_sql_nullable_hex($bytes)
{
    return $bytes === null || $bytes === '' ? 'NULL' : pay_ctry_001_sql_hex($bytes);
}
function pay_ctry_001_now_sql($epoch)
{
    return gmdate('Y-m-d H:i:s', (int)$epoch);
}
function pay_ctry_001_sql_to_utc($value)
{
    $value=trim((string)$value);
    if ($value==='') return null;
    $t=strtotime($value.' UTC');
    return $t===false ? null : gmdate('Y-m-d\TH:i:s\Z',$t);
}
function pay_ctry_001_fetch_one($sql)
{
    $q=db_query($sql);
    if(!$q||db_num_rows($q)!==1)return false;
    return db_fetch_assoc($q);
}
function pay_ctry_001_fetch_all($sql)
{
    $q=db_query($sql); if(!$q)return false; $out=array(); while($r=db_fetch_assoc($q))$out[]=$r; return $out;
}
function pay_ctry_001_insert_id()
{
    return function_exists('db_insert_id') ? (int)db_insert_id() : 0;
}
function pay_ctry_001_require_install_authority(&$error)
{
    $error=null;$actor=pay_ctry_001_actor_id();
    if($actor<=0||!pay_ctry_001_access(PAY_CTRY_001_INSTALL_PERMISSION))return pay_ctry_001_fail($error,'install_authority_denied');
    return $actor;
}
function pay_ctry_001_require_trust_authority(&$error)
{
    $error=null;$actor=pay_ctry_001_actor_id();
    if($actor<=0||!pay_ctry_001_access(PAY_CTRY_001_TRUST_PERMISSION))return pay_ctry_001_fail($error,'trust_authority_denied');
    return $actor;
}

/** Store exact archive bytes without passing them through the legacy HTML-encoding db_escape(). */
function pay_ctry_001_store_raw_blob($raw,$actor,&$error)
{
    $error=null;if(!is_string($raw)||strlen($raw)<1||strlen($raw)>PAY_CTRY_001_MAX_ARCHIVE_BYTES)return pay_ctry_001_fail($error,'raw_blob_size_invalid');
    $sha=hash('sha256',$raw);$row=pay_ctry_001_fetch_one('SELECT raw_blob_id,raw_sha256,byte_length,content FROM '.TB_PREF.'pay_ctry_raw_blobs WHERE raw_sha256='.pay_ctry_001_sql_hex($sha).' LIMIT 2 FOR UPDATE');
    if($row!==false){if((int)$row['byte_length']!==strlen($raw)||!hash_equals($sha,hash('sha256',(string)$row['content'])))return pay_ctry_001_fail($error,'raw_blob_custody_mismatch');return array('raw_blob_id'=>(int)$row['raw_blob_id'],'raw_sha256'=>$sha,'existing'=>true);}
    $sql='INSERT INTO '.TB_PREF.'pay_ctry_raw_blobs (raw_sha256,byte_length,media_type,content,received_by,created_at) VALUES ('.pay_ctry_001_sql_hex($sha).','.strlen($raw).','.pay_ctry_001_sql_hex('application/vnd.notrinos.country-pack+zip').','.pay_ctry_001_sql_hex($raw).','.(int)$actor.','.pay_ctry_001_sql_hex(pay_ctry_001_now_sql(time())).')';
    if(!db_query($sql))return pay_ctry_001_fail($error,'raw_blob_insert_failed');$id=pay_ctry_001_insert_id();if($id<=0)return pay_ctry_001_fail($error,'raw_blob_identity_missing');return array('raw_blob_id'=>$id,'raw_sha256'=>$sha,'existing'=>false);
}

function pay_ctry_001_publisher_context($publisher_code)
{
    $row=pay_ctry_001_fetch_one('SELECT * FROM '.TB_PREF.'pay_ctry_publishers WHERE publisher_code='.pay_ctry_001_sql_hex($publisher_code).' LIMIT 2');
    if($row===false)return false;
    return array('publisher_id'=>(int)$row['publisher_id'],'publisher_code'=>(string)$row['publisher_code'],'display_name'=>(string)$row['display_name'],'valid_from'=>pay_ctry_001_sql_to_utc($row['valid_from']),'valid_to'=>pay_ctry_001_sql_to_utc($row['valid_to']));
}
function pay_ctry_001_signer_context($signer_code)
{
    $row=pay_ctry_001_fetch_one('SELECT * FROM '.TB_PREF.'pay_ctry_signers WHERE signer_code='.pay_ctry_001_sql_hex($signer_code).' LIMIT 2');
    if($row===false)return false;
    return array('signer_id'=>(int)$row['signer_id'],'publisher_id'=>(int)$row['publisher_id'],'signer_code'=>(string)$row['signer_code'],'display_name'=>(string)$row['display_name']);
}
function pay_ctry_001_key_context($key_id)
{
    $row=pay_ctry_001_fetch_one('SELECT * FROM '.TB_PREF.'pay_ctry_trust_keys WHERE key_id='.pay_ctry_001_sql_hex($key_id).' LIMIT 2');
    if($row===false)return false;
    $sq=db_query('SELECT predecessor_key_id,successor_key_id,effective_at,supersession_sha256 FROM '.TB_PREF.'pay_ctry_trust_key_supersessions WHERE predecessor_key_id='.(int)$row['trust_key_id'].' LIMIT 2');if(!$sq||db_num_rows($sq)>1)return false;$sup=db_num_rows($sq)===1?db_fetch_assoc($sq):false;
    $context=array('trust_key_id'=>(int)$row['trust_key_id'],'signer_id'=>(int)$row['signer_id'],'key_id'=>(string)$row['key_id'],'algorithm'=>(string)$row['algorithm'],'public_key_pem'=>(string)$row['public_key_pem'],'public_key_sha256'=>(string)$row['public_key_sha256'],'key_sha256'=>(string)$row['key_sha256'],'valid_from'=>pay_ctry_001_sql_to_utc($row['valid_from']),'valid_to'=>pay_ctry_001_sql_to_utc($row['valid_to']),'trusted_at'=>pay_ctry_001_sql_to_utc($row['trusted_at']));
    if($sup!==false)$context['supersession']=array('predecessor_key_id'=>(int)$sup['predecessor_key_id'],'successor_key_id'=>(int)$sup['successor_key_id'],'effective_at'=>pay_ctry_001_sql_to_utc($sup['effective_at']),'supersession_sha256'=>(string)$sup['supersession_sha256']);
    return $context;
}
function pay_ctry_001_publisher_revoked($publisher_id)
{
    $q=db_query('SELECT publisher_revocation_id FROM '.TB_PREF.'pay_ctry_publisher_revocations WHERE publisher_id='.(int)$publisher_id.' LIMIT 1');return $q&&db_num_rows($q)>0;
}
function pay_ctry_001_key_revoked($key_id)
{
    $q=db_query('SELECT key_revocation_id FROM '.TB_PREF.'pay_ctry_trust_key_revocations WHERE trust_key_id='.(int)$key_id.' LIMIT 1');return $q&&db_num_rows($q)>0;
}

function pay_ctry_001_capability_registry_context($version)
{
    $v=pay_ctry_001_fetch_one('SELECT registry_version_id,registry_version,registry_sha256 FROM '.TB_PREF.'pay_ctry_capability_registry_versions WHERE registry_version='.pay_ctry_001_sql_hex($version).' LIMIT 2');
    if($v===false)return false;$rows=pay_ctry_001_fetch_all('SELECT capability_code,required_constraints_json,allowed_constraints_json,exclusive_group,allowed_artifact_types_json,definition_sha256 FROM '.TB_PREF.'pay_ctry_capability_definitions WHERE registry_version_id='.(int)$v['registry_version_id'].' ORDER BY capability_code');if($rows===false)return false;$defs=array();foreach($rows as $r){$req=json_decode((string)$r['required_constraints_json'],true);$allow=json_decode((string)$r['allowed_constraints_json'],true);$types=json_decode((string)$r['allowed_artifact_types_json'],true);if(!is_array($req)||!is_array($allow)||!is_array($types))return false;$defs[(string)$r['capability_code']]=array('required_constraints'=>$req,'allowed_constraints'=>$allow,'exclusive_group'=>$r['exclusive_group']===null?null:(string)$r['exclusive_group'],'allowed_artifact_types'=>$types,'definition_sha256'=>(string)$r['definition_sha256']);}
    return array('version'=>(string)$v['registry_version'],'registry_sha256'=>(string)$v['registry_sha256'],'definitions'=>$defs);
}

/** Exact installed versions; multiple immutable versions can coexist without becoming active payroll authority. */
function pay_ctry_001_installed_dependency_context()
{
    $rows=pay_ctry_001_fetch_all('SELECT p.pack_code,v.version,v.pack_version_id FROM '.TB_PREF.'pay_ctry_pack_versions v INNER JOIN '.TB_PREF.'pay_ctry_packs p ON p.pack_id=v.pack_id ORDER BY p.pack_code,v.pack_version_id');if($rows===false)return false;$out=array();foreach($rows as $r){$code=(string)$r['pack_code'];if(!isset($out[$code]))$out[$code]=array();$out[$code][(string)$r['version']]=(int)$r['pack_version_id'];}return $out;
}
function pay_ctry_001_dependency_graph_context()
{
    $sql='SELECT p.pack_code,v.version,dp.pack_code AS dep_pack,m.required_version FROM '.TB_PREF.'pay_ctry_dependency_locks l INNER JOIN '.TB_PREF.'pay_ctry_pack_versions v ON v.pack_version_id=l.pack_version_id INNER JOIN '.TB_PREF.'pay_ctry_packs p ON p.pack_id=v.pack_id INNER JOIN '.TB_PREF.'pay_ctry_dependency_lock_members m ON m.dependency_lock_id=l.dependency_lock_id INNER JOIN '.TB_PREF.'pay_ctry_pack_versions rv ON rv.pack_version_id=m.resolved_pack_version_id INNER JOIN '.TB_PREF.'pay_ctry_packs dp ON dp.pack_id=rv.pack_id ORDER BY l.dependency_lock_id,m.ordinal_no';
    $rows=pay_ctry_001_fetch_all($sql);if($rows===false)return false;$g=array();foreach($rows as $r){$n=(string)$r['pack_code'].'@'.(string)$r['version'];if(!isset($g[$n]))$g[$n]=array();$g[$n][]=(string)$r['dep_pack'].'@'.(string)$r['required_version'];}return $g;
}
function pay_ctry_001_build_verifier_context($manifest,$trusted_now,&$error)
{
    $error=null;$publisher=pay_ctry_001_publisher_context($manifest['pack']['publisher_id']);$signer=pay_ctry_001_signer_context($manifest['signing']['signer_id']);$key=pay_ctry_001_key_context($manifest['signing']['key_id']);$registry=pay_ctry_001_capability_registry_context($manifest['capability_registry_version']);$deps=pay_ctry_001_installed_dependency_context();$graph=pay_ctry_001_dependency_graph_context();
    if($deps===false||$graph===false)return pay_ctry_001_fail($error,'dependency_state_unreadable');
    return array('host_db_version'=>PAY_CTRY_001_FINAL_DB_VERSION,'trusted_now'=>(int)$trusted_now,'publisher'=>$publisher,'signer'=>$signer,'key'=>$key,'publisher_revoked'=>is_array($publisher)?pay_ctry_001_publisher_revoked($publisher['publisher_id']):false,'key_revoked'=>is_array($key)?pay_ctry_001_key_revoked($key['trust_key_id']):false,'capability_registry'=>$registry,'installed_dependencies'=>$deps,'dependency_graph'=>$graph);
}

function pay_ctry_001_find_or_create_pack($pack_code,$actor,&$error)
{
    $error=null;$row=pay_ctry_001_fetch_one('SELECT pack_id,pack_code FROM '.TB_PREF.'pay_ctry_packs WHERE pack_code='.pay_ctry_001_sql_hex($pack_code).' LIMIT 2 FOR UPDATE');if($row!==false)return (int)$row['pack_id'];$sql='INSERT INTO '.TB_PREF.'pay_ctry_packs (pack_code,created_by,created_at) VALUES ('.pay_ctry_001_sql_hex($pack_code).','.(int)$actor.','.pay_ctry_001_sql_hex(pay_ctry_001_now_sql(time())).')';if(!db_query($sql))return pay_ctry_001_fail($error,'pack_identity_insert_failed');$id=pay_ctry_001_insert_id();return $id>0?$id:pay_ctry_001_fail($error,'pack_identity_missing');
}

function pay_ctry_001_existing_pack_version($pack_id,$version)
{
    return pay_ctry_001_fetch_one('SELECT * FROM '.TB_PREF.'pay_ctry_pack_versions WHERE pack_id='.(int)$pack_id.' AND version='.pay_ctry_001_sql_hex($version).' LIMIT 2 FOR UPDATE');
}

function pay_ctry_001_insert_pack_version($pack_id,$raw_blob_id,$verified,$actor,&$error)
{
    $error=null;$m=$verified['manifest'];$existing=pay_ctry_001_existing_pack_version($pack_id,$m['pack']['version']);
    if($existing!==false){if((string)$existing['manifest_sha256']!==$verified['manifest_sha256']||(string)$existing['semantic_root']!==$m['semantic_root']||(string)$existing['evidence_root']!==$m['evidence_root']||(string)$existing['publisher_code']!==$m['pack']['publisher_id']||(string)$existing['signer_code']!==$m['signing']['signer_id']||(string)$existing['key_id']!==$m['signing']['key_id'])return pay_ctry_001_fail($error,'pack_version_immutable_collision');return array('pack_version_id'=>(int)$existing['pack_version_id'],'existing'=>true);}
    $caps=$m['capabilities'];usort($caps,function($a,$b){return strcmp($a['id'],$b['id']);});$signed_sql=gmdate('Y-m-d H:i:s',pay_ctry_001_iso_utc_epoch($m['signing']['signed_at']));
    $sql='INSERT INTO '.TB_PREF.'pay_ctry_pack_versions (pack_id,version,manifest_schema_version,raw_blob_id,manifest_sha256,semantic_root,evidence_root,publisher_code,signer_code,key_id,signed_at,capability_registry_version,capabilities_json,host_min_version,host_max_version,registered_by,created_at) VALUES ('.(int)$pack_id.','.pay_ctry_001_sql_hex($m['pack']['version']).','.pay_ctry_001_sql_hex($m['manifest_version']).','.(int)$raw_blob_id.','.pay_ctry_001_sql_hex($verified['manifest_sha256']).','.pay_ctry_001_sql_hex($m['semantic_root']).','.pay_ctry_001_sql_hex($m['evidence_root']).','.pay_ctry_001_sql_hex($m['pack']['publisher_id']).','.pay_ctry_001_sql_hex($m['signing']['signer_id']).','.pay_ctry_001_sql_hex($m['signing']['key_id']).','.pay_ctry_001_sql_hex($signed_sql).','.pay_ctry_001_sql_hex($m['capability_registry_version']).','.pay_ctry_001_sql_hex(pay_ctry_001_canonical_json($caps)).','.pay_ctry_001_sql_hex($m['host']['min_db_version']).','.pay_ctry_001_sql_nullable_hex(isset($m['host']['max_db_version'])?$m['host']['max_db_version']:null).','.(int)$actor.','.pay_ctry_001_sql_hex(pay_ctry_001_now_sql(time())).')';
    if(!db_query($sql))return pay_ctry_001_fail($error,'pack_version_insert_failed');$id=pay_ctry_001_insert_id();if($id<=0)return pay_ctry_001_fail($error,'pack_version_identity_missing');$arts=$m['artifacts'];usort($arts,function($a,$b){return strcmp($a['path'],$b['path']);});$ord=0;foreach($arts as $a){$ord++;$is_evidence=pay_ctry_001_is_evidence_type($a['type'])||strpos($a['path'],'evidence/')===0?1:0;$asql='INSERT INTO '.TB_PREF.'pay_ctry_artifacts (pack_version_id,ordinal_no,normalized_path,artifact_type,media_type,byte_length,sha256,is_evidence,created_at) VALUES ('.$id.','.$ord.','.pay_ctry_001_sql_hex($a['path']).','.pay_ctry_001_sql_hex($a['type']).','.pay_ctry_001_sql_hex($a['media_type']).','.(int)$a['bytes'].','.pay_ctry_001_sql_hex($a['sha256']).','.$is_evidence.','.pay_ctry_001_sql_hex(pay_ctry_001_now_sql(time())).')';if(!db_query($asql))return pay_ctry_001_fail($error,'artifact_insert_failed');}
    return array('pack_version_id'=>$id,'existing'=>false);
}

function pay_ctry_001_insert_dependency_lock($pack_version_id,$manifest,$installed,&$error)
{
    $error=null;$deps=$manifest['dependencies'];usort($deps,function($a,$b){$c=strcmp($a['pack_id'],$b['pack_id']);return $c!==0?$c:strcmp($a['version'],$b['version']);});$lock_payload=array();foreach($deps as $dep){if(!isset($installed[$dep['pack_id']])||!isset($installed[$dep['pack_id']][$dep['version']]))return pay_ctry_001_fail($error,'dependency_resolution_disappeared');$lock_payload[]=array('pack_id'=>$dep['pack_id'],'version'=>$dep['version'],'resolved_pack_version_id'=>(int)$installed[$dep['pack_id']][$dep['version']]);}$sha=hash('sha256',pay_ctry_001_canonical_json(array('lock_version'=>'dependency-lock-v1','pack_version_id'=>(int)$pack_version_id,'members'=>$lock_payload)));$row=pay_ctry_001_fetch_one('SELECT dependency_lock_id,lock_sha256 FROM '.TB_PREF.'pay_ctry_dependency_locks WHERE pack_version_id='.(int)$pack_version_id.' LIMIT 2 FOR UPDATE');if($row!==false){if((string)$row['lock_sha256']!==$sha)return pay_ctry_001_fail($error,'dependency_lock_collision');return (int)$row['dependency_lock_id'];}$sql='INSERT INTO '.TB_PREF.'pay_ctry_dependency_locks (pack_version_id,lock_sha256,created_at) VALUES ('.(int)$pack_version_id.','.pay_ctry_001_sql_hex($sha).','.pay_ctry_001_sql_hex(pay_ctry_001_now_sql(time())).')';if(!db_query($sql))return pay_ctry_001_fail($error,'dependency_lock_insert_failed');$id=pay_ctry_001_insert_id();$ord=0;foreach($lock_payload as $member){$ord++;$msha=hash('sha256',pay_ctry_001_canonical_json($member));$msql='INSERT INTO '.TB_PREF.'pay_ctry_dependency_lock_members (dependency_lock_id,ordinal_no,dependency_pack_code,required_version,resolved_pack_version_id,member_sha256,created_at) VALUES ('.$id.','.$ord.','.pay_ctry_001_sql_hex($member['pack_id']).','.pay_ctry_001_sql_hex($member['version']).','.(int)$member['resolved_pack_version_id'].','.pay_ctry_001_sql_hex($msha).','.pay_ctry_001_sql_hex(pay_ctry_001_now_sql(time())).')';if(!db_query($msql))return pay_ctry_001_fail($error,'dependency_lock_member_insert_failed');}return $id;
}

function pay_ctry_001_report_details($parsed,$status,$failure,$context)
{
    $m=is_array($parsed)&&isset($parsed['manifest'])?$parsed['manifest']:array();return array('raw_sha256'=>is_array($parsed)&&isset($parsed['raw_sha256'])?$parsed['raw_sha256']:null,'manifest_sha256'=>is_array($parsed)&&isset($parsed['manifest_sha256'])?$parsed['manifest_sha256']:null,'pack_id'=>isset($m['pack']['id'])?$m['pack']['id']:null,'version'=>isset($m['pack']['version'])?$m['pack']['version']:null,'publisher_id'=>isset($m['pack']['publisher_id'])?$m['pack']['publisher_id']:null,'signer_id'=>isset($m['signing']['signer_id'])?$m['signing']['signer_id']:null,'key_id'=>isset($m['signing']['key_id'])?$m['signing']['key_id']:null,'status'=>$status,'failure_code'=>$failure,'verifier_version'=>PAY_CTRY_001_VERIFIER_VERSION,'site_scope_company'=>PAY_CTRY_001_SITE_COMPANY_ID,'company_activation_authority'=>false,'payroll_execution_authority'=>false);
}
function pay_ctry_001_insert_verification_report($raw_blob_id,$pack_version_id,$parsed,$status,$failure,$actor,&$error)
{
    $error=null;$m=is_array($parsed)&&isset($parsed['manifest'])?$parsed['manifest']:array();$details=pay_ctry_001_report_details($parsed,$status,$failure,array());$details_json=pay_ctry_001_canonical_json($details);$report_payload=array('raw_blob_id'=>(int)$raw_blob_id,'pack_version_id'=>$pack_version_id===null?null:(int)$pack_version_id,'details'=>$details,'verified_by'=>(int)$actor);$sha=hash('sha256',pay_ctry_001_canonical_json($report_payload));$old=pay_ctry_001_fetch_one('SELECT verification_report_id FROM '.TB_PREF.'pay_ctry_verification_reports WHERE report_sha256='.pay_ctry_001_sql_hex($sha).' LIMIT 2');if($old!==false)return (int)$old['verification_report_id'];
    $accepted=$status==='accepted';$trust=$accepted?'trusted':'rejected';$time=$accepted?'trusted_time_valid':(($failure!==null&&(strpos($failure,'time')!==false||strpos($failure,'expired')!==false||strpos($failure,'backdated')!==false))?'rejected':'not_reached');$revoke=$accepted?'not_revoked':(($failure!==null&&strpos($failure,'revoked')!==false)?'rejected':'not_reached');$forbidden=$accepted?'pass':(($failure!==null&&strpos($failure,'forbidden')!==false)?'rejected':'not_reached');$cap=$accepted?'pass':(($failure!==null&&strpos($failure,'capability')!==false)?'rejected':'not_reached');$dep=$accepted?'pass':(($failure!==null&&strpos($failure,'dependency')!==false)?'rejected':'not_reached');
    $cols=array('raw_blob_id'=>(int)$raw_blob_id,'pack_version_id'=>$pack_version_id,'candidate_pack_code'=>isset($m['pack']['id'])?$m['pack']['id']:null,'candidate_version'=>isset($m['pack']['version'])?$m['pack']['version']:null,'manifest_schema_version'=>isset($m['manifest_version'])?$m['manifest_version']:null,'manifest_sha256'=>is_array($parsed)&&isset($parsed['manifest_sha256'])?$parsed['manifest_sha256']:null,'publisher_code'=>isset($m['pack']['publisher_id'])?$m['pack']['publisher_id']:null,'signer_code'=>isset($m['signing']['signer_id'])?$m['signing']['signer_id']:null,'key_id'=>isset($m['signing']['key_id'])?$m['signing']['key_id']:null,'semantic_root'=>isset($m['semantic_root'])?$m['semantic_root']:null,'evidence_root'=>isset($m['evidence_root'])?$m['evidence_root']:null);
    $sql='INSERT INTO '.TB_PREF.'pay_ctry_verification_reports (raw_blob_id,pack_version_id,candidate_pack_code,candidate_version,manifest_schema_version,manifest_sha256,publisher_code,signer_code,key_id,trust_decision,trusted_time_decision,revocation_decision,semantic_root,evidence_root,forbidden_content_result,capability_result,dependency_result,final_status,failure_code,verifier_version,details_json,report_sha256,verified_by,created_at) VALUES ('.(int)$cols['raw_blob_id'].','.($cols['pack_version_id']===null?'NULL':(int)$cols['pack_version_id']).','.pay_ctry_001_sql_nullable_hex($cols['candidate_pack_code']).','.pay_ctry_001_sql_nullable_hex($cols['candidate_version']).','.pay_ctry_001_sql_nullable_hex($cols['manifest_schema_version']).','.pay_ctry_001_sql_nullable_hex($cols['manifest_sha256']).','.pay_ctry_001_sql_nullable_hex($cols['publisher_code']).','.pay_ctry_001_sql_nullable_hex($cols['signer_code']).','.pay_ctry_001_sql_nullable_hex($cols['key_id']).','.pay_ctry_001_sql_hex($trust).','.pay_ctry_001_sql_hex($time).','.pay_ctry_001_sql_hex($revoke).','.pay_ctry_001_sql_nullable_hex($cols['semantic_root']).','.pay_ctry_001_sql_nullable_hex($cols['evidence_root']).','.pay_ctry_001_sql_hex($forbidden).','.pay_ctry_001_sql_hex($cap).','.pay_ctry_001_sql_hex($dep).','.pay_ctry_001_sql_hex($status).','.pay_ctry_001_sql_nullable_hex($failure).','.pay_ctry_001_sql_hex(PAY_CTRY_001_VERIFIER_VERSION).','.pay_ctry_001_sql_hex($details_json).','.pay_ctry_001_sql_hex($sha).','.(int)$actor.','.pay_ctry_001_sql_hex(pay_ctry_001_now_sql(time())).')';if(!db_query($sql))return pay_ctry_001_fail($error,'verification_report_insert_failed');$id=pay_ctry_001_insert_id();return $id>0?$id:pay_ctry_001_fail($error,'verification_report_identity_missing');
}

function pay_ctry_001_insert_install_receipt($idempotency_key,$raw_blob_id,$report_id,$pack_version_id,$actor,&$error)
{
    $error=null;if(!is_string($idempotency_key)||preg_match('/^[A-Za-z0-9._:-]{16,128}$/D',$idempotency_key)!==1)return pay_ctry_001_fail($error,'idempotency_key_invalid');$idem=hash('sha256',$idempotency_key);$row=pay_ctry_001_fetch_one('SELECT * FROM '.TB_PREF.'pay_ctry_install_receipts WHERE idempotency_sha256='.pay_ctry_001_sql_hex($idem).' LIMIT 2 FOR UPDATE');if($row!==false){if((int)$row['raw_blob_id']!==(int)$raw_blob_id||(int)$row['pack_version_id']!==(int)$pack_version_id)return pay_ctry_001_fail($error,'idempotency_replay_mismatch');return array('install_receipt_id'=>(int)$row['install_receipt_id'],'receipt_sha256'=>(string)$row['receipt_sha256'],'existing'=>true);}$payload=array('idempotency_sha256'=>$idem,'raw_blob_id'=>(int)$raw_blob_id,'verification_report_id'=>(int)$report_id,'pack_version_id'=>(int)$pack_version_id,'installed_by'=>(int)$actor);$sha=hash('sha256',pay_ctry_001_canonical_json($payload));$sql='INSERT INTO '.TB_PREF.'pay_ctry_install_receipts (idempotency_sha256,raw_blob_id,verification_report_id,pack_version_id,receipt_sha256,installed_by,created_at) VALUES ('.pay_ctry_001_sql_hex($idem).','.(int)$raw_blob_id.','.(int)$report_id.','.(int)$pack_version_id.','.pay_ctry_001_sql_hex($sha).','.(int)$actor.','.pay_ctry_001_sql_hex(pay_ctry_001_now_sql(time())).')';if(!db_query($sql))return pay_ctry_001_fail($error,'install_receipt_insert_failed');return array('install_receipt_id'=>pay_ctry_001_insert_id(),'receipt_sha256'=>$sha,'existing'=>false);
}

/** Verify exact archive bytes and register only immutable site-level artifacts; never activates payroll use. */
function pay_ctry_001_verify_and_register($raw,$idempotency_key,&$error,$trusted_now=null)
{
    $error=null;$actor=pay_ctry_001_require_install_authority($error);if($actor===false)return false;if($trusted_now===null)$trusted_now=time();begin_transaction();$blob=pay_ctry_001_store_raw_blob($raw,$actor,$error);if($blob===false){cancel_transaction();return false;}$parsed=pay_ctry_001_parse_candidate($raw,$parse_error);if($parsed===false){$report=pay_ctry_001_insert_verification_report($blob['raw_blob_id'],null,array('raw_sha256'=>$blob['raw_sha256']), 'rejected',$parse_error,$actor,$report_error);if($report===false){cancel_transaction();$error=$report_error;return false;}commit_transaction();$error=$parse_error;return array('status'=>'rejected','verification_report_id'=>$report,'raw_blob_id'=>$blob['raw_blob_id'],'failure_code'=>$parse_error);}$ctx=pay_ctry_001_build_verifier_context($parsed['manifest'],$trusted_now,$ctx_error);if($ctx===false){$report=pay_ctry_001_insert_verification_report($blob['raw_blob_id'],null,$parsed,'rejected',$ctx_error,$actor,$report_error);if($report===false){cancel_transaction();$error=$report_error;return false;}commit_transaction();$error=$ctx_error;return array('status'=>'rejected','verification_report_id'=>$report,'raw_blob_id'=>$blob['raw_blob_id'],'failure_code'=>$ctx_error);}$verified=pay_ctry_001_verify_parsed_candidate($parsed,$ctx,$verify_error);if($verified===false){$report=pay_ctry_001_insert_verification_report($blob['raw_blob_id'],null,$parsed,'rejected',$verify_error,$actor,$report_error);if($report===false){cancel_transaction();$error=$report_error;return false;}commit_transaction();$error=$verify_error;return array('status'=>'rejected','verification_report_id'=>$report,'raw_blob_id'=>$blob['raw_blob_id'],'failure_code'=>$verify_error);}$pack_id=pay_ctry_001_find_or_create_pack($verified['manifest']['pack']['id'],$actor,$error);if($pack_id===false){cancel_transaction();return false;}$pv=pay_ctry_001_insert_pack_version($pack_id,$blob['raw_blob_id'],$verified,$actor,$error);if($pv===false){cancel_transaction();return false;}$installed=pay_ctry_001_installed_dependency_context();if($installed===false||pay_ctry_001_insert_dependency_lock($pv['pack_version_id'],$verified['manifest'],$installed,$error)===false){cancel_transaction();if($error===null)$error='dependency_lock_state_unreadable';return false;}$report=pay_ctry_001_insert_verification_report($blob['raw_blob_id'],$pv['pack_version_id'],$verified,'accepted',null,$actor,$error);if($report===false){cancel_transaction();return false;}$receipt=pay_ctry_001_insert_install_receipt($idempotency_key,$blob['raw_blob_id'],$report,$pv['pack_version_id'],$actor,$error);if($receipt===false){cancel_transaction();return false;}commit_transaction();return array('status'=>'accepted','raw_blob_id'=>$blob['raw_blob_id'],'pack_version_id'=>$pv['pack_version_id'],'verification_report_id'=>$report,'install_receipt_id'=>$receipt['install_receipt_id'],'receipt_sha256'=>$receipt['receipt_sha256'],'company_activation_authority'=>false,'payroll_execution_authority'=>false);
}

/** Re-verification is append-only: old attestations are never rewritten after expiry/revocation. */
function pay_ctry_001_reverify_raw_blob($raw_blob_id,$idempotency_key,&$error,$trusted_now=null)
{
    $error=null;$actor=pay_ctry_001_require_install_authority($error);if($actor===false)return false;$row=pay_ctry_001_fetch_one('SELECT content,raw_sha256,byte_length FROM '.TB_PREF.'pay_ctry_raw_blobs WHERE raw_blob_id='.(int)$raw_blob_id.' LIMIT 2');if($row===false)return pay_ctry_001_fail($error,'raw_blob_not_found');$raw=(string)$row['content'];if(strlen($raw)!==(int)$row['byte_length']||hash('sha256',$raw)!==(string)$row['raw_sha256'])return pay_ctry_001_fail($error,'raw_blob_restore_custody_mismatch');return pay_ctry_001_verify_and_register($raw,$idempotency_key,$error,$trusted_now);
}

function pay_ctry_001_validate_utc_input($value,&$error)
{
    $error=null;$epoch=pay_ctry_001_iso_utc_epoch($value);if($epoch===false)return pay_ctry_001_fail($error,'utc_timestamp_invalid');return array('epoch'=>$epoch,'sql'=>gmdate('Y-m-d H:i:s',$epoch));
}
function pay_ctry_001_create_publisher($publisher_code,$display_name,$valid_from,$valid_to,&$error)
{
    $error=null;$actor=pay_ctry_001_require_trust_authority($error);if($actor===false)return false;if(!pay_ctry_001_ascii_token($publisher_code,'/^[a-z0-9][a-z0-9._-]{2,63}$/D',64)||!is_string($display_name)||trim($display_name)===''||strlen($display_name)>160)return pay_ctry_001_fail($error,'publisher_identity_invalid');$vf=pay_ctry_001_validate_utc_input($valid_from,$error);if($vf===false)return false;$vt=$valid_to===''||$valid_to===null?null:pay_ctry_001_validate_utc_input($valid_to,$error);if($valid_to!==''&&$valid_to!==null&&$vt===false)return false;if($vt!==null&&$vt['epoch']<$vf['epoch'])return pay_ctry_001_fail($error,'publisher_validity_invalid');$payload=array('publisher_code'=>$publisher_code,'display_name'=>$display_name,'valid_from'=>$valid_from,'valid_to'=>$valid_to===''?null:$valid_to);$sha=hash('sha256',pay_ctry_001_canonical_json($payload));$sql='INSERT INTO '.TB_PREF.'pay_ctry_publishers (publisher_code,display_name,valid_from,valid_to,identity_sha256,created_by,created_at) VALUES ('.pay_ctry_001_sql_hex($publisher_code).','.pay_ctry_001_sql_hex($display_name).','.pay_ctry_001_sql_hex($vf['sql']).','.($vt===null?'NULL':pay_ctry_001_sql_hex($vt['sql'])).','.pay_ctry_001_sql_hex($sha).','.$actor.','.pay_ctry_001_sql_hex(pay_ctry_001_now_sql(time())).')';if(!db_query($sql))return pay_ctry_001_fail($error,'publisher_insert_failed');return array('publisher_id'=>pay_ctry_001_insert_id(),'identity_sha256'=>$sha);
}
function pay_ctry_001_create_signer($publisher_id,$signer_code,$display_name,&$error)
{
    $error=null;$actor=pay_ctry_001_require_trust_authority($error);if($actor===false)return false;if((int)$publisher_id<=0||!pay_ctry_001_ascii_token($signer_code,'/^[a-z0-9][a-z0-9._-]{2,63}$/D',64)||!is_string($display_name)||trim($display_name)===''||strlen($display_name)>160)return pay_ctry_001_fail($error,'signer_identity_invalid');$pub=pay_ctry_001_fetch_one('SELECT publisher_id,publisher_code FROM '.TB_PREF.'pay_ctry_publishers WHERE publisher_id='.(int)$publisher_id.' LIMIT 2');if($pub===false||pay_ctry_001_publisher_revoked((int)$publisher_id))return pay_ctry_001_fail($error,'publisher_unavailable');$payload=array('publisher_id'=>(int)$publisher_id,'publisher_code'=>(string)$pub['publisher_code'],'signer_code'=>$signer_code,'display_name'=>$display_name);$sha=hash('sha256',pay_ctry_001_canonical_json($payload));$sql='INSERT INTO '.TB_PREF.'pay_ctry_signers (publisher_id,signer_code,display_name,identity_sha256,created_by,created_at) VALUES ('.(int)$publisher_id.','.pay_ctry_001_sql_hex($signer_code).','.pay_ctry_001_sql_hex($display_name).','.pay_ctry_001_sql_hex($sha).','.$actor.','.pay_ctry_001_sql_hex(pay_ctry_001_now_sql(time())).')';if(!db_query($sql))return pay_ctry_001_fail($error,'signer_insert_failed');return array('signer_id'=>pay_ctry_001_insert_id(),'identity_sha256'=>$sha);
}
function pay_ctry_001_import_trust_key($signer_id,$key_id,$public_key_pem,$valid_from,$valid_to,$predecessor_key_id,&$error)
{
    $error=null;$actor=pay_ctry_001_require_trust_authority($error);if($actor===false)return false;
    if((int)$signer_id<=0||!pay_ctry_001_ascii_token($key_id,'/^[A-Za-z0-9._:-]{3,96}$/D',96)||!is_string($public_key_pem)||strpos($public_key_pem,'PRIVATE KEY')!==false)return pay_ctry_001_fail($error,'trust_key_input_invalid');
    $pk=@openssl_pkey_get_public($public_key_pem);if($pk===false)return pay_ctry_001_fail($error,'trust_key_public_material_invalid');$details=@openssl_pkey_get_details($pk);
    if(!is_array($details)||!isset($details['type'],$details['bits'],$details['key'])||(int)$details['type']!==OPENSSL_KEYTYPE_RSA||(int)$details['bits']<2048)return pay_ctry_001_fail($error,'trust_key_strength_or_type_invalid');
    $canonical_pem=str_replace("\r\n","\n",trim((string)$details['key']))."\n";$vf=pay_ctry_001_validate_utc_input($valid_from,$error);if($vf===false)return false;$vt=$valid_to===''||$valid_to===null?null:pay_ctry_001_validate_utc_input($valid_to,$error);if($valid_to!==''&&$valid_to!==null&&$vt===false)return false;if($vt!==null&&$vt['epoch']<$vf['epoch'])return pay_ctry_001_fail($error,'trust_key_validity_invalid');
    $signer=pay_ctry_001_fetch_one('SELECT signer_id,publisher_id,signer_code FROM '.TB_PREF.'pay_ctry_signers WHERE signer_id='.(int)$signer_id.' LIMIT 2');if($signer===false||pay_ctry_001_publisher_revoked((int)$signer['publisher_id']))return pay_ctry_001_fail($error,'signer_unavailable');
    $finger=hash('sha256',$canonical_pem);$epoch=time();$trusted_iso=gmdate('Y-m-d\\TH:i:s\\Z',$epoch);$trusted_at=pay_ctry_001_now_sql($epoch);$payload=array('signer_id'=>(int)$signer_id,'key_id'=>$key_id,'algorithm'=>'rsa-sha256','public_key_sha256'=>$finger,'valid_from'=>gmdate('Y-m-d\\TH:i:s\\Z',$vf['epoch']),'valid_to'=>$vt===null?null:gmdate('Y-m-d\\TH:i:s\\Z',$vt['epoch']),'trusted_at'=>$trusted_iso);$sha=hash('sha256',pay_ctry_001_canonical_json($payload));
    begin_transaction();$sql='INSERT INTO '.TB_PREF.'pay_ctry_trust_keys (signer_id,key_id,algorithm,public_key_pem,public_key_sha256,valid_from,valid_to,trusted_at,key_sha256,imported_by,created_at) VALUES ('.(int)$signer_id.','.pay_ctry_001_sql_hex($key_id).','.pay_ctry_001_sql_hex('rsa-sha256').','.pay_ctry_001_sql_hex($canonical_pem).','.pay_ctry_001_sql_hex($finger).','.pay_ctry_001_sql_hex($vf['sql']).','.($vt===null?'NULL':pay_ctry_001_sql_hex($vt['sql'])).','.pay_ctry_001_sql_hex($trusted_at).','.pay_ctry_001_sql_hex($sha).','.$actor.','.pay_ctry_001_sql_hex($trusted_at).')';if(!db_query($sql)){cancel_transaction();return pay_ctry_001_fail($error,'trust_key_insert_failed');}$new=pay_ctry_001_insert_id();
    if((int)$predecessor_key_id>0){$pred=pay_ctry_001_fetch_one('SELECT trust_key_id,signer_id,key_id FROM '.TB_PREF.'pay_ctry_trust_keys WHERE trust_key_id='.(int)$predecessor_key_id.' LIMIT 2 FOR UPDATE');if($pred===false||(int)$pred['signer_id']!==(int)$signer_id||(int)$pred['trust_key_id']===$new){cancel_transaction();return pay_ctry_001_fail($error,'trust_key_predecessor_invalid');}$sp=array('predecessor_key_id'=>(int)$pred['trust_key_id'],'successor_key_id'=>$new,'effective_at'=>$trusted_iso);$ssha=hash('sha256',pay_ctry_001_canonical_json($sp));$ssql='INSERT INTO '.TB_PREF.'pay_ctry_trust_key_supersessions (predecessor_key_id,successor_key_id,effective_at,supersession_sha256,created_by,created_at) VALUES ('.(int)$pred['trust_key_id'].','.$new.','.pay_ctry_001_sql_hex($trusted_at).','.pay_ctry_001_sql_hex($ssha).','.$actor.','.pay_ctry_001_sql_hex($trusted_at).')';if(!db_query($ssql)){cancel_transaction();return pay_ctry_001_fail($error,'trust_key_supersession_insert_failed');}}
    commit_transaction();return array('trust_key_id'=>$new,'public_key_sha256'=>$finger,'key_sha256'=>$sha);
}
function pay_ctry_001_revoke_trust_key($trust_key_id,$reason_code,$reason_text,$compromised,&$error)
{
    $error=null;$actor=pay_ctry_001_require_trust_authority($error);if($actor===false)return false;if((int)$trust_key_id<=0||!pay_ctry_001_ascii_token($reason_code,'/^[a-z][a-z0-9_-]{2,47}$/D',48)||!is_string($reason_text)||trim($reason_text)===''||strlen($reason_text)>1024)return pay_ctry_001_fail($error,'key_revocation_input_invalid');$key=pay_ctry_001_fetch_one('SELECT trust_key_id,key_id FROM '.TB_PREF.'pay_ctry_trust_keys WHERE trust_key_id='.(int)$trust_key_id.' LIMIT 2 FOR UPDATE');if($key===false)return pay_ctry_001_fail($error,'trust_key_not_found');$reason_sha=hash('sha256',$reason_text);$epoch=time();$at=gmdate('Y-m-d\\TH:i:s\\Z',$epoch);$sql_at=pay_ctry_001_now_sql($epoch);$payload=array('trust_key_id'=>(int)$trust_key_id,'key_id'=>(string)$key['key_id'],'effective_at'=>$at,'reason_code'=>$reason_code,'reason_sha256'=>$reason_sha,'compromised'=>$compromised?true:false);$sha=hash('sha256',pay_ctry_001_canonical_json($payload));$sql='INSERT INTO '.TB_PREF.'pay_ctry_trust_key_revocations (trust_key_id,effective_at,reason_code,reason_sha256,compromised,revocation_sha256,revoked_by,created_at) VALUES ('.(int)$trust_key_id.','.pay_ctry_001_sql_hex($sql_at).','.pay_ctry_001_sql_hex($reason_code).','.pay_ctry_001_sql_hex($reason_sha).','.($compromised?1:0).','.pay_ctry_001_sql_hex($sha).','.$actor.','.pay_ctry_001_sql_hex($sql_at).')';if(!db_query($sql))return pay_ctry_001_fail($error,'trust_key_revocation_insert_failed');return array('key_revocation_id'=>pay_ctry_001_insert_id(),'revocation_sha256'=>$sha);
}
function pay_ctry_001_revoke_publisher($publisher_id,$reason_code,$reason_text,&$error)
{
    $error=null;$actor=pay_ctry_001_require_trust_authority($error);if($actor===false)return false;if((int)$publisher_id<=0||!pay_ctry_001_ascii_token($reason_code,'/^[a-z][a-z0-9_-]{2,47}$/D',48)||!is_string($reason_text)||trim($reason_text)===''||strlen($reason_text)>1024)return pay_ctry_001_fail($error,'publisher_revocation_input_invalid');$pub=pay_ctry_001_fetch_one('SELECT publisher_id,publisher_code FROM '.TB_PREF.'pay_ctry_publishers WHERE publisher_id='.(int)$publisher_id.' LIMIT 2 FOR UPDATE');if($pub===false)return pay_ctry_001_fail($error,'publisher_not_found');$reason_sha=hash('sha256',$reason_text);$epoch=time();$at=gmdate('Y-m-d\\TH:i:s\\Z',$epoch);$sql_at=pay_ctry_001_now_sql($epoch);$payload=array('publisher_id'=>(int)$publisher_id,'publisher_code'=>(string)$pub['publisher_code'],'effective_at'=>$at,'reason_code'=>$reason_code,'reason_sha256'=>$reason_sha);$sha=hash('sha256',pay_ctry_001_canonical_json($payload));$sql='INSERT INTO '.TB_PREF.'pay_ctry_publisher_revocations (publisher_id,effective_at,reason_code,reason_sha256,revocation_sha256,revoked_by,created_at) VALUES ('.(int)$publisher_id.','.pay_ctry_001_sql_hex($sql_at).','.pay_ctry_001_sql_hex($reason_code).','.pay_ctry_001_sql_hex($reason_sha).','.pay_ctry_001_sql_hex($sha).','.$actor.','.pay_ctry_001_sql_hex($sql_at).')';if(!db_query($sql))return pay_ctry_001_fail($error,'publisher_revocation_insert_failed');return array('publisher_revocation_id'=>pay_ctry_001_insert_id(),'revocation_sha256'=>$sha);
}
?>
