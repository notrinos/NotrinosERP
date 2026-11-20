<?php
/** PAY-FIL-001 TT80 semantic-source assessment and architecture rejection guards. */
include_once(dirname(__FILE__).'/pay_fil_001_tt80_renderer_readiness_db.inc');
include_once($path_to_root.'/includes/pay_fil_001_tt80_semantic_contract.inc');

function pay_fil_001_tt80_semantic_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1028','>=');}
function pay_fil_001_tt80_semantic_source_assessment_required_for_database_version($version){return version_compare((string)$version,'1.0.1029','>=');}
function pay_fil_001_tt80_semantic_architecture_guard_required_for_database_version($version){return version_compare((string)$version,'1.0.1030','>=');}

?>
