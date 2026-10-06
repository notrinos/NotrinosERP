<?php
/** PAY-FIL-001 TT80 source-bound renderer/readiness guards. No execution authority. */
include_once(dirname(__FILE__).'/pay_fil_001_local_validation_readiness_db.inc');
include_once($path_to_root.'/includes/pay_fil_001_tt80_renderer_contract.inc');

/**
 * Determine whether a database version includes the candidate renderer contract.
 *
 * @param string $version Database version.
 * @return bool Whether the contract is required.
 */
function pay_fil_001_tt80_renderer_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1025','>=');}
/**
 * Determine whether a database version includes source-binding readiness.
 *
 * @param string $version Database version.
 * @return bool Whether source binding is required.
 */
function pay_fil_001_tt80_source_binding_required_for_database_version($version){return version_compare((string)$version,'1.0.1026','>=');}
/**
 * Determine whether a database version includes the runtime readiness entrypoint.
 *
 * @param string $version Database version.
 * @return bool Whether runtime readiness is required.
 */
function pay_fil_001_tt80_readiness_runtime_required_for_database_version($version){return version_compare((string)$version,'1.0.1027','>=');}

/**
 * Validate the candidate renderer source and its authority ceiling during upgrade.
 *
 * @param fa_patch $patch Active software-upgrade patch.
 * @return bool Whether the source contract is valid.
 */
function pay_fil_001_require_tt80_renderer_contract_source($patch)
{
    global $path_to_root;if(!pay_fil_001_require_local_validator_compatibility_source($patch))return false;
    $p=pay_fil_001_tt80_renderer_profile();
    if($p['renderer_id']!=='pay-fil-001.vn.tt80.05-kk-tncn-xml'||$p['schema_version']!=='2.9.3'||$p['declaration_code']!=='864'||$p['development_gate']!=='OFFICIAL_LOCAL_VALIDATOR_COMPATIBILITY'||$p['semantic_mapping_status']!=='UNPUBLISHED'||$p['official_schema_claimed']!==false||$p['production_generation_allowed']!==false||$p['production_transmission_allowed']!==false||$p['network_authority_allowed']!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 renderer authority ceiling invalid.');
    $raw=@file_get_contents($path_to_root.'/includes/pay_fil_001_tt80_renderer_contract.inc');foreach(array('semantic_mapping_required','semantic_mapping_status','UNPUBLISHED','pay_fil_001_render_tt80_xml_candidate','production_generation_allowed') as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 renderer contract source incomplete: '.$token.'.');
    foreach(array('curl_','fsockopen(','stream_socket_client(','openssl_sign(','file_put_contents(','db_query(') as $token)if(strpos($raw,$token)!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 pure renderer acquired forbidden authority: '.$token.'.');
    return true;
}
/**
 * Validate that readiness binds accepted source custody and remains read-only.
 *
 * @param fa_patch $patch Active software-upgrade patch.
 * @return bool Whether the source-binding contract is valid.
 */
function pay_fil_001_require_tt80_source_binding_readiness_source($patch)
{
    global $path_to_root;if(!pay_fil_001_require_tt80_renderer_contract_source($patch))return false;
    $raw=@file_get_contents($path_to_root.'/hrm/includes/db/pay_fil_001_tt80_renderer_runtime_db.inc');foreach(array('pay_fil_001_tt80_renderer_readiness','semantic_mapping_unpublished','source_set_sha256','obligation_sha256','currency_code','internal-reconciliation-only','PAY_FIL_001_VIEW_PERMISSION') as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 source-binding readiness source incomplete: '.$token.'.');
    foreach(array('INSERT INTO','UPDATE ','DELETE FROM','curl_','fsockopen(','stream_socket_client(','openssl_sign(','file_put_contents(') as $token)if(stripos($raw,$token)!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 readiness path is not read-only: '.$token.'.');
    return true;
}
/**
 * Validate that runtime readiness remains blocked while semantic mapping is unpublished.
 *
 * @param fa_patch $patch Active software-upgrade patch.
 * @return bool Whether the runtime source preserves the fail-closed state.
 */
function pay_fil_001_require_tt80_readiness_runtime_source($patch)
{
    if(!pay_fil_001_require_tt80_source_binding_readiness_source($patch))return false;
    $p=pay_fil_001_tt80_renderer_profile();return $p['semantic_mapping_required']===true&&$p['semantic_mapping_status']==='UNPUBLISHED'&&$p['production_generation_allowed']===false;
}
?>
