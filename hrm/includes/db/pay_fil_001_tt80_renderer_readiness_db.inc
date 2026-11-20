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

?>
