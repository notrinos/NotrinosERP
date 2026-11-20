<?php
/** PAY-REC-001 source-only architecture readiness checks. No schema or runtime writer authority. */
include_once($path_to_root.'/includes/pay_rec_001_contract.inc');
include_once($path_to_root.'/hrm/includes/db/pay_fil_001_tt80_generated_validation_db.inc');
function pay_rec_001_filing_lineage_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1046','>=');}
function pay_rec_001_filing_completion_policy_required_for_database_version($version){return version_compare((string)$version,'1.0.1047','>=');}
function pay_rec_001_architecture_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1048','>=');}
?>
