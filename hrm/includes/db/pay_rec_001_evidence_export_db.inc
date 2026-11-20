<?php
/** PAY-REC-001 privacy-aware export source/readiness guards. No schema writer authority. */
include_once(dirname(__FILE__).'/pay_rec_001_close_state_db.inc');
include_once($path_to_root.'/includes/pay_rec_001_evidence_export_contract.inc');
function pay_rec_001_evidence_export_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1058','>=');}
function pay_rec_001_evidence_export_runtime_required_for_database_version($version){return version_compare((string)$version,'1.0.1059','>=');}
function pay_rec_001_completion_readiness_required_for_database_version($version){return version_compare((string)$version,'1.0.1060','>=');}
?>
