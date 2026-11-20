<?php
/** Source-only local development compatibility readiness; production authority remains denied. */
include_once(dirname(__FILE__).'/pay_fil_001_readiness_db.inc');

/** Return only the exact source-only compatibility edge activation boundary. */
function pay_fil_001_local_validator_compatibility_required_for_database_version($version)
{
    return version_compare((string)$version, '1.0.1024', '>=');
}
?>
