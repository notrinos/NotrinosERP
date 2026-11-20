<?php
/** PAY-REC-001 evidence/review authority and immutable review/approval schema helpers. */
include_once(dirname(__FILE__).'/pay_rec_001_custody_db.inc');
include_once($path_to_root.'/includes/pay_rec_001_evidence_review_contract.inc');
function pay_rec_001_evidence_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1052','>=');}
function pay_rec_001_review_schema_required_for_database_version($version){return version_compare((string)$version,'1.0.1053','>=');}
function pay_rec_001_evidence_runtime_required_for_database_version($version){return version_compare((string)$version,'1.0.1054','>=');}
?>
