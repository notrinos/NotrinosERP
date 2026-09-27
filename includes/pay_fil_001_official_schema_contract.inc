<?php
/** PAY-FIL-001 named official-schema and authorization-foundation contract. Pure helpers only. */
if (!defined('PAY_FIL_001_OFFICIAL_SCHEMA_CONTRACT_VERSION')) define('PAY_FIL_001_OFFICIAL_SCHEMA_CONTRACT_VERSION','1.0');

/** Build the deterministic custody hash for one reviewed official-schema authority candidate. */
function pay_fil_001_official_schema_authority_sha256(array $x)
{
    $keys=array('company_id','legal_entity_id','jurisdiction_code','obligation_type','country_pack_id','country_pack_version','schema_id','schema_version','schema_artifact_sha256','legal_evidence_sha256','technical_evidence_sha256','certification_evidence_sha256','generation_allowed','transmission_allowed','connector_id','connector_version','effective_from','effective_to','reviewed_by','approved_by');
    $p=array();foreach($keys as $k){if(!array_key_exists($k,$x))return false;$p[$k]=$x[$k];}return pay_fil_001_hash($p);
}

/** Validate a future named official-schema authority without granting filing/submission authority. */
function pay_fil_001_validate_official_schema_authority(array $x,&$e=null)
{
    $e=null;$required=array('company_id','legal_entity_id','jurisdiction_code','obligation_type','country_pack_id','country_pack_version','schema_id','schema_version','schema_artifact_sha256','legal_evidence_sha256','technical_evidence_sha256','certification_evidence_sha256','generation_allowed','transmission_allowed','connector_id','connector_version','effective_from','effective_to','reviewed_by','approved_by');
    foreach($required as $k)if(!array_key_exists($k,$x))return pay_fil_001_fail($e,'official_schema_authority_field_missing');
    if(pay_fil_001_uint($x['company_id'])===false||pay_fil_001_posint($x['legal_entity_id'])===false||pay_fil_001_token($x['jurisdiction_code'],32)===false||pay_fil_001_token($x['obligation_type'],64)===false||pay_fil_001_token($x['country_pack_id'],96)===false||pay_fil_001_token($x['country_pack_version'],32)===false||pay_fil_001_token($x['schema_id'],96)===false||pay_fil_001_token($x['schema_version'],32)===false)return pay_fil_001_fail($e,'official_schema_authority_scope_invalid');
    foreach(array('schema_artifact_sha256','legal_evidence_sha256','technical_evidence_sha256','certification_evidence_sha256') as $k)if(!pay_fil_001_sha($x[$k]))return pay_fil_001_fail($e,'official_schema_authority_evidence_invalid');
    if($x['generation_allowed']!==true||$x['transmission_allowed']!==false)return pay_fil_001_fail($e,'official_schema_authority_execution_boundary_invalid');
    if((string)$x['connector_id']!=='none'||(string)$x['connector_version']!=='none')return pay_fil_001_fail($e,'official_schema_authority_connector_not_deferred');
    if(pay_fil_001_date($x['effective_from'])===false||pay_fil_001_date($x['effective_to'])===false||$x['effective_from']>$x['effective_to'])return pay_fil_001_fail($e,'official_schema_authority_dates_invalid');
    $reviewed=pay_fil_001_posint($x['reviewed_by']);$approved=pay_fil_001_posint($x['approved_by']);if($reviewed===false||$approved===false||$reviewed===$approved)return pay_fil_001_fail($e,'official_schema_authority_sod_invalid');
    $p=$x;$p['company_id']=pay_fil_001_uint($x['company_id']);$p['legal_entity_id']=pay_fil_001_posint($x['legal_entity_id']);$p['reviewed_by']=$reviewed;$p['approved_by']=$approved;$p['authority_sha256']=pay_fil_001_official_schema_authority_sha256($p);return $p;
}

/** Prove that a country-pack filing boundary explicitly names a generation-capable schema. */
function pay_fil_001_boundary_names_official_schema(array $boundary,$schema_id,$schema_version)
{
    if(!isset($boundary['official_supported_schemas'])||!is_array($boundary['official_supported_schemas']))return false;
    foreach($boundary['official_supported_schemas'] as $entry){if(!is_array($entry))continue;if((string)(isset($entry['schema_id'])?$entry['schema_id']:'')===(string)$schema_id&&(string)(isset($entry['schema_version'])?$entry['schema_version']:'')===(string)$schema_version&&(isset($entry['generation_allowed'])&&$entry['generation_allowed']===true))return true;}
    return false;
}
?>
