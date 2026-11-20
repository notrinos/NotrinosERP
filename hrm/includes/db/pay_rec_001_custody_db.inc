<?php
/** PAY-REC-001 immutable reconciliation/profile/checklist custody schema/readiness helpers. */
include_once(dirname(__FILE__).'/pay_rec_001_readiness_db.inc');
include_once(dirname(__FILE__).'/pay_fil_001_schema_db.inc');
include_once($path_to_root.'/includes/pay_rec_001_custody_contract.inc');

/** Return whether the custody contract is required for the supplied database version. */
function pay_rec_001_custody_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1049','>=');}
/** Return whether the custody schema is required for the supplied database version. */
function pay_rec_001_custody_schema_required_for_database_version($version){return version_compare((string)$version,'1.0.1050','>=');}
/** Return whether the custody guard is required for the supplied database version. */
function pay_rec_001_custody_guard_required_for_database_version($version){return version_compare((string)$version,'1.0.1051','>=');}

?>