<?php
/** PAY-FIL-001 TT80 semantic-source assessment and architecture rejection guards. */
include_once(dirname(__FILE__).'/pay_fil_001_tt80_renderer_readiness_db.inc');
include_once($path_to_root.'/includes/pay_fil_001_tt80_semantic_contract.inc');

function pay_fil_001_tt80_semantic_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1028','>=');}
function pay_fil_001_tt80_semantic_source_assessment_required_for_database_version($version){return version_compare((string)$version,'1.0.1029','>=');}
function pay_fil_001_tt80_semantic_architecture_guard_required_for_database_version($version){return version_compare((string)$version,'1.0.1030','>=');}

/** Validate exact indicator coverage and denied authority. */
function pay_fil_001_require_tt80_semantic_contract_source($patch)
{
    global $path_to_root;if(!pay_fil_001_require_tt80_readiness_runtime_source($patch))return false;
    $c=pay_fil_001_tt80_semantic_contract();$ind=$c['indicator_contract'];
    $expected=array('ct16','ct17','ct18','ct19','ct20','ct21','ct22','ct23','ct24','ct25','ct25_1','ct26','ct27','ct28','ct29','ct30','ct31','ct32');
    if(array_keys($ind)!==$expected||$c['source_mapping_status']!=='ARCHITECTURE_REJECTED_SOURCE_GAPS'||$c['runtime_generation_allowed']!==false||$c['production_transmission_allowed']!==false||$c['network_authority_allowed']!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic contract authority/status invalid.');
    foreach(array('ct18'=>'ct19+ct20','ct21'=>'ct22+ct23','ct26'=>'ct27+ct28','ct29'=>'ct30+ct31') as $k=>$eq)if(($ind[$k]['equation']??null)!==$eq)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic equation mismatch: '.$k.'.');
    $raw=@file_get_contents($path_to_root.'/includes/pay_fil_001_tt80_semantic_contract.inc');foreach(array('taxpayer_legal_entity_profile_not_bound_to_filing_source_custody','filing_taxable_income_not_typed_in_pay_core_result_contract','resolution_exempt_income_not_typed','ARCHITECTURE_REJECTED_SOURCE_GAPS') as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic source contract incomplete: '.$token.'.');
    foreach(array('db_query(','INSERT INTO','UPDATE ','DELETE FROM','curl_','fsockopen(','stream_socket_client(','openssl_sign(','file_put_contents(') as $token)if(stripos($raw,$token)!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic contract acquired forbidden authority: '.$token.'.');
    return true;
}

/** Prove that the current accepted source architecture cannot publish a filing-safe mapping. */
function pay_fil_001_require_tt80_semantic_source_assessment($patch)
{
    global $path_to_root;if(!pay_fil_001_require_tt80_semantic_contract_source($patch))return false;
    $runtime=@file_get_contents($path_to_root.'/hrm/includes/db/pay_fil_001_runtime_db.inc');$core=@file_get_contents($path_to_root.'/hrm/includes/db/pay_core_010_governance_db.inc');$vars=@file_get_contents($path_to_root.'/hrm/country_packs/vn_reference/1.0.0/artifacts/variables.json');
    foreach(array('source_kind'=>array("'run'","'result'"),'result_contract'=>array('result_sha256','gross_amount','deduction_amount','net_amount','currency','snapshot_payload_json'),'vn_fact'=>array('vn.tax_residency','vn.taxable_income_after_deductions_vnd')) as $kind=>$tokens){$src=$kind==='source_kind'?$runtime:($kind==='result_contract'?$core:$vars);foreach($tokens as $token)if(!is_string($src)||strpos($src,$token)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic source assessment missing reviewed predecessor token: '.$token.'.');}
    $gaps=pay_fil_001_tt80_semantic_source_gaps();if(count($gaps)!==12)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic source-gap set changed without review.');
    return true;
}

/** Validate that runtime remains fail-closed because the architecture is rejected for source gaps. */
function pay_fil_001_require_tt80_semantic_architecture_guard_source($patch)
{
    global $path_to_root;if(!pay_fil_001_require_tt80_semantic_source_assessment($patch))return false;
    $raw=@file_get_contents($path_to_root.'/hrm/includes/db/pay_fil_001_tt80_renderer_runtime_db.inc');foreach(array('semantic_source_incomplete','ARCHITECTURE_REJECTED_SOURCE_GAPS','semantic_contract_sha256','source_gaps') as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic runtime guard incomplete: '.$token.'.');
    foreach(array('INSERT INTO','UPDATE ','DELETE FROM','begin_transaction(','commit_transaction(','file_put_contents(','curl_','fsockopen(','stream_socket_client(','openssl_sign(') as $token)if(stripos($raw,$token)!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic runtime guard is not read-only: '.$token.'.');
    return true;
}
?>
