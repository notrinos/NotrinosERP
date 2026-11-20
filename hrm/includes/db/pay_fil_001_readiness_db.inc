<?php
/** PAY-FIL-001 predecessor, dependency and least-privilege readiness. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_fil_001_contract.inc');
include_once($path_to_root.'/includes/pay_fil_001_official_schema_contract.inc');


function pay_fil_001_tt80_form_legal_basis_review_required_for_database_version($version){return version_compare((string)$version,'1.0.1022','>=');}
function pay_fil_001_electronic_filing_legal_boundary_review_required_for_database_version($version){return version_compare((string)$version,'1.0.1023','>=');}

function pay_fil_001_htkk_evidence_receipt_required_for_database_version($version){return version_compare((string)$version,'1.0.1017','>=');}
function pay_fil_001_htkk_structure_profile_required_for_database_version($version){return version_compare((string)$version,'1.0.1018','>=');}
function pay_fil_001_official_schema_evidence_gap_guard_required_for_database_version($version){return version_compare((string)$version,'1.0.1019','>=');}
function pay_fil_001_htkk_schema_candidate_receipt_required_for_database_version($version){return version_compare((string)$version,'1.0.1020','>=');}
function pay_fil_001_htkk_local_xsd_compatibility_required_for_database_version($version){return version_compare((string)$version,'1.0.1021','>=');}

?>
