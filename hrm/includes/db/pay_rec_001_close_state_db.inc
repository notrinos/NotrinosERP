<?php
/** PAY-REC-001 development close-state, concurrency and recovery authority/schema helpers. */
include_once(dirname(__FILE__).'/pay_rec_001_evidence_review_db.inc');
include_once($path_to_root.'/includes/pay_rec_001_close_state_contract.inc');
function pay_rec_001_close_state_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1055','>=');}
function pay_rec_001_close_state_schema_required_for_database_version($version){return version_compare((string)$version,'1.0.1056','>=');}
function pay_rec_001_close_state_runtime_required_for_database_version($version){return version_compare((string)$version,'1.0.1057','>=');}
?>
