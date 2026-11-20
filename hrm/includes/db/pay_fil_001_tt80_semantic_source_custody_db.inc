<?php
/** PAY-FIL-001 TT80 semantic source-of-truth custody schema/readiness helpers. */
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_readiness_db.inc');
include_once($path_to_root.'/includes/pay_fil_001_tt80_semantic_source_custody_contract.inc');

function pay_fil_001_tt80_semantic_source_custody_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1031','>=');}
function pay_fil_001_tt80_semantic_source_custody_schema_required_for_database_version($version){return version_compare((string)$version,'1.0.1032','>=');}
function pay_fil_001_tt80_semantic_source_custody_guard_required_for_database_version($version){return version_compare((string)$version,'1.0.1033','>=');}

?>
