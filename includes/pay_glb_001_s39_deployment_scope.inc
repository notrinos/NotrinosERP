<?php
/** S39 scope-v1 engineering ceiling; this static decision never supplies source admission or deployment authority. */

/** Describe the owner-selected scope without enabling any runtime service or country/provider authority. */
function pay_glb_001_s39_deployment_scope_v1()
{
    return array('schema'=>'PAY_GLB_001_S39_DEPLOYMENT_SCOPE_V1',
        'native_engineering_allowed'=>true,'live_external_providers'=>array(),'live_countries'=>array(),
        'deel_mode'=>'DOCUMENTATION_MOCK_ONLY','country_compliance_authority'=>false,
        'statutory_deadline_authority'=>false,'production_activation_allowed'=>false,
        'synthetic_admission_allowed'=>false,'parent_closure_allowed'=>false);
}

/** Classify source kinds against v1's empty live-provider scope; a result is a boundary status, never an admission. */
function pay_glb_001_s39_source_kind_status($kind)
{
    if(!is_string($kind))return 'SOURCE_KIND_INVALID';
    if($kind==='GOVERNED_NATIVE_V1')return 'NATIVE_REQUIRES_EXISTING_ADMISSION';
    if($kind==='UNAVAILABLE_SOURCE')return 'UNAVAILABLE_SOURCE';
    if(in_array($kind,array('EXTERNAL_PROVIDER','PARTNER_OPERATED','MANAGED_SERVICE','PARTNER','MANAGED','DEEL','DEEL_MOCK'),true))
        return 'LIVE_PROVIDER_UNADMITTED';
    return 'SOURCE_KIND_UNSUPPORTED';
}

/** Reject external/unknown rows and contradictory unavailable rows before minimization; never silently discard mixed rows. */
function pay_glb_001_s39_source_row_in_scope(array $row)
{
    if(!isset($row['source_kind']))return false;
    $state=pay_glb_001_s39_source_kind_status($row['source_kind']);
    if($state==='NATIVE_REQUIRES_EXISTING_ADMISSION')return true;
    return $state==='UNAVAILABLE_SOURCE' && isset($row['payroll_state'],$row['financial_state'])
        && $row['payroll_state']==='UNAVAILABLE_SOURCE' && $row['financial_state']==='UNAVAILABLE_SOURCE'
        && array_key_exists('population',$row) && $row['population']===null
        && array_key_exists('revision_sha256',$row) && $row['revision_sha256']===null;
}

/** Recheck the public row's source ceiling so a forged display cannot turn an unavailable source into financial health. */
function pay_glb_001_s39_public_source_row_in_scope(array $row)
{
    if(!isset($row['source_kind']))return false;
    $state=pay_glb_001_s39_source_kind_status($row['source_kind']);
    if($state==='NATIVE_REQUIRES_EXISTING_ADMISSION')return true;
    return $state==='UNAVAILABLE_SOURCE' && isset($row['population_disclosure'],$row['payroll_state'],$row['financial_state'],$row['revision_reference'])
        && $row['population_disclosure']==='WITHHELD' && $row['payroll_state']==='WITHHELD'
        && $row['financial_state']==='WITHHELD' && $row['revision_reference']==='';
}
