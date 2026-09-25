<?php
/** PAY-CTRY-006 predecessor and synthetic structural-proof source-custody checks. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_ctry_001_verifier.inc');
include_once($path_to_root.'/includes/pay_ctry_003_contract.inc');

function pay_ctry_006_fail($patch,$message)
{
    return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$message);
}
function pay_ctry_006_require_predecessor_file($patch,$relative,$sha256)
{
    global $path_to_root;$path=$path_to_root.'/'.$relative;
    $actual=is_file($path)?@hash_file('sha256',$path):false;
    $ok=is_string($actual)&&(function_exists('hash_equals')?hash_equals($sha256,$actual):$actual===$sha256);
    return $ok?true:pay_ctry_006_fail($patch,'PAY-CTRY-006 immutable PAY-CTRY-005 predecessor changed: '.$relative);
}
/** Freeze accepted PAY-CTRY-005 source/local-acceptance evidence without mutating predecessor bytes. */
function pay_ctry_006_require_predecessor($patch)
{
    $frozen=array(
        'hrm/country_packs/vn_reference/1.0.0/package-candidate.json'=>'71d7ae81625a17ad39e865ec4f1f09aeca27cf4a5418e41a03924240c0684412',
        'hrm/country_packs/vn_reference/1.0.0/custody.json'=>'c71b9ab7f2a5526b8c60b96b3f3e9b0d0cd1caa113d5d7776be91b47a4c8fc12',
        'hrm/includes/db/pay_ctry_005_readiness_db.inc'=>'6f5759bbe5d2296a52c7e7b0d207d462bc9afb001757355fb60b1e2d1515ebbb',
        'sql/upgrade_pay_ctry_005_vn_3_completion.php'=>'9f8ba99cca9d73a78f576e20b3c834ac5cd4b1af543ddab98f793be3e8f0b2e7',
        'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/EXECUTION_REPORT_PAY_CTRY_005_COMPLETION_2026-09-25.md'=>'01c5fc4ec43f33807be892124a52972ec58e5a9b4112902271b4e2b7cea1db2d',
        'includes/pay_ctry_003_contract.inc'=>'97021a703a87473e5d6281fcdd1f94ab7adea2c2c3892ce0215d83c55f793b23'
    );
    foreach($frozen as $relative=>$sha)if(!pay_ctry_006_require_predecessor_file($patch,$relative,$sha))return false;
    return true;
}
function pay_ctry_006_load_json($patch,$relative,$label)
{
    global $path_to_root;$raw=@file_get_contents($path_to_root.'/'.$relative);
    if(!is_string($raw))return pay_ctry_006_fail($patch,$label.' source is missing.');
    $data=json_decode($raw,true);
    if(!is_array($data)||json_last_error()!==JSON_ERROR_NONE)return pay_ctry_006_fail($patch,$label.' JSON is invalid.');
    return $data;
}
function pay_ctry_006_labels_ok($data)
{
    $labels=$data['labels']??array();
    return is_array($labels)&&in_array('synthetic',$labels,true)&&in_array('conformance-only',$labels,true)&&in_array('not production',$labels,true);
}
function pay_ctry_006_require_identity($patch)
{
    $base='hrm/country_packs/astervale_conformance/1.0.0/';
    $meta=pay_ctry_006_load_json($patch,$base.'artifacts/metadata.json','PAY-CTRY-006 metadata');if($meta===false)return false;
    $coverage=pay_ctry_006_load_json($patch,$base.'artifacts/coverage.json','PAY-CTRY-006 coverage');if($coverage===false)return false;
    $custody=pay_ctry_006_load_json($patch,$base.'custody.json','PAY-CTRY-006 custody');if($custody===false)return false;
    if(($meta['pack_id']??'')!=='org.notrinos.conformance.astervale'||($meta['jurisdiction_id']??'')!=='AVX-FICTIONAL'||!pay_ctry_006_labels_ok($meta))return pay_ctry_006_fail($patch,'PAY-CTRY-006 synthetic pack identity/labels are incomplete.');
    if(($meta['certification_ceiling']??'')!=='TechnicallyValidated'||($meta['production']??true)!==false||($meta['marketplace_eligible']??true)!==false)return pay_ctry_006_fail($patch,'PAY-CTRY-006 synthetic certification/production ceiling is incomplete.');
    if(($coverage['production_activation_allowed']??true)!==false||($coverage['marketplace_publication_allowed']??true)!==false||($coverage['population_rules']['nearest_rule_fallback']??true)!==false)return pay_ctry_006_fail($patch,'PAY-CTRY-006 coverage must fail closed and deny production/publication.');
    if(($custody['certification_ceiling']??'')!=='TechnicallyValidated'||($custody['source_database_version']??'')!=='1.0.820')return pay_ctry_006_fail($patch,'PAY-CTRY-006 custody ceiling/head mismatch.');
    return true;
}
function pay_ctry_006_require_calculation_structure($patch)
{
    $base='hrm/country_packs/astervale_conformance/1.0.0/';
    $calendar=pay_ctry_006_load_json($patch,$base.'artifacts/calendar.json','PAY-CTRY-006 calendar');if($calendar===false)return false;
    $rules=pay_ctry_006_load_json($patch,$base.'artifacts/rules.json','PAY-CTRY-006 rules');if($rules===false)return false;
    $rates=pay_ctry_006_load_json($patch,$base.'artifacts/rates.json','PAY-CTRY-006 rates');if($rates===false)return false;
    $balances=pay_ctry_006_load_json($patch,$base.'artifacts/balances.json','PAY-CTRY-006 balances');if($balances===false)return false;
    if(($calendar['legal_year']['calendar_year_based']??true)!==false||($calendar['legal_year']['period_frequency']??'')!=='weekly'||($calendar['legal_year']['extra_period_rule']??'')===''||($calendar['mid_period_change']['split_fact_required']??false)!==true)return pay_ctry_006_fail($patch,'PAY-CTRY-006 non-calendar/weekly/anomaly/mid-period model is incomplete.');
    if(($rules['rounding_profile']??'')!=='ROUND-005-HALF-UP'||empty($rules['cumulative_semantics']['prior_state_required'])||empty($rules['period_local_semantics']['prior_state_ignored']))return pay_ctry_006_fail($patch,'PAY-CTRY-006 cumulative/non-cumulative or rounding model is incomplete.');
    if(($rates['dimensions_independent']??false)!==true||($rates['tax_dimension']['selector']??'')==='worker.region_class'||($rates['contribution_dimension']['selector']??'')==='worker.tax_class')return pay_ctry_006_fail($patch,'PAY-CTRY-006 independent classification dimensions are incomplete.');
    if(($balances['calendar_year_reset']??true)!==false||($balances['prior_state_contract']['missing_action']??'')!=='DENY')return pay_ctry_006_fail($patch,'PAY-CTRY-006 state/balance semantics must use legal-year and fail closed.');
    return true;
}
function pay_ctry_006_require_filing_lifecycle($patch)
{
    $base='hrm/country_packs/astervale_conformance/1.0.0/';
    $filing=pay_ctry_006_load_json($patch,$base.'artifacts/filings.json','PAY-CTRY-006 filings');if($filing===false)return false;
    $fixtures=pay_ctry_006_load_json($patch,$base.'evidence/conformance-fixtures.json','PAY-CTRY-006 fixtures');if($fixtures===false)return false;
    if(($filing['obligation']['periodic_monthly']??true)!==false||($filing['obligation']['trigger']??'')!=='approved payroll pay-event'||($filing['obligation']['transport_authority']??true)!==false)return pay_ctry_006_fail($patch,'PAY-CTRY-006 event-triggered filing boundary is incomplete.');
    foreach(array('ORIGINAL','CORRECTION','REPLACEMENT') as $r)if(!isset($filing['relationships'][$r]))return pay_ctry_006_fail($patch,'PAY-CTRY-006 filing lineage is incomplete.');
    $ids=array();foreach(($fixtures['fixtures']??array()) as $f)$ids[$f['id']??'']=true;
    foreach(array('lifecycle-hire','lifecycle-leaver','lifecycle-retro','lifecycle-correction','missing-worker-class','ambiguous-worker-class','unsupported-population','filing-original','filing-correction','filing-replacement','historical-reproduction-after-later-rule') as $id)if(!isset($ids[$id]))return pay_ctry_006_fail($patch,'PAY-CTRY-006 required conformance fixture missing: '.$id);
    return true;
}
/** Verify complete declarative pack, exact roots, closed capabilities, certification ceiling and non-production denial. */
function pay_ctry_006_require_pack_candidate($patch)
{
    global $path_to_root;$base='hrm/country_packs/astervale_conformance/1.0.0/';
    $candidate=pay_ctry_006_load_json($patch,$base.'package-candidate.json','PAY-CTRY-006 package candidate');if($candidate===false)return false;
    if(($candidate['schema']??'')!=='notrinos.synthetic.package-candidate.v1'||($candidate['capability_registry_version']??'')!==PAY_CTRY_003_REGISTRY_VERSION||($candidate['pack']['id']??'')!=='org.notrinos.conformance.astervale'||($candidate['pack']['version']??'')!=='1.0.0')return pay_ctry_006_fail($patch,'PAY-CTRY-006 package identity/registry contract is incomplete.');
    if(($candidate['semantic_root']??'')!=='4996a93803599459df37958674873402e001d4cd0afeba561d6a7760066c7a62'||($candidate['evidence_root']??'')!=='415473aca56b34a879910431c0d40b0c28e6cc85e81765a2bdb547a2aa0a8adf')return pay_ctry_006_fail($patch,'PAY-CTRY-006 semantic/evidence root mismatch.');
    if(($candidate['signing']['state']??'')!=='not-signed-conformance-only'||!array_key_exists('signature_base64',$candidate['signing'])||$candidate['signing']['signature_base64']!==null)return pay_ctry_006_fail($patch,'PAY-CTRY-006 must not fabricate a publisher signature.');
    $members=array();foreach(($candidate['artifacts']??array()) as $a){$rel=$a['path']??'';if(preg_match('/\.(php|inc|sql|js|sh|bat|ps1)$/i',$rel))return pay_ctry_006_fail($patch,'PAY-CTRY-006 executable pack member denied.');$path=$path_to_root.'/'.$base.$rel;$raw=is_file($path)?@file_get_contents($path):false;if(!is_string($raw)||hash('sha256',$raw)!==($a['sha256']??'')||strlen($raw)!==(int)($a['bytes']??-1))return pay_ctry_006_fail($patch,'PAY-CTRY-006 artifact custody mismatch: '.$rel);$e=null;if(!pay_ctry_001_forbidden_content($rel,$raw,$e))return pay_ctry_006_fail($patch,'PAY-CTRY-006 declarative-only boundary failed: '.$rel.' '.$e);$members[$rel]=array('content'=>$raw,'sha256'=>hash('sha256',$raw),'bytes'=>strlen($raw));}
    $roots=pay_ctry_001_compute_roots($candidate,$members);if(($roots['semantic_root']??'')!==($candidate['semantic_root']??'')||($roots['evidence_root']??'')!==($candidate['evidence_root']??''))return pay_ctry_006_fail($patch,'PAY-CTRY-006 exact root reproduction failed.');
    $cap=pay_ctry_006_load_json($patch,$base.'artifacts/capability-map.json','PAY-CTRY-006 capability map');if($cap===false)return false;$claims=$cap['claims']??array();$err=null;if(!pay_ctry_003_validate_capability_set($claims,$err))return pay_ctry_006_fail($patch,'PAY-CTRY-006 closed capability set failed: '.$err);
    $cert=pay_ctry_006_load_json($patch,$base.'evidence/certification-state.json','PAY-CTRY-006 certification state');if($cert===false)return false;
    if(($cert['current_state']??'')!=='TechnicallyValidated'||($cert['maximum_state']??'')!=='TechnicallyValidated'||($cert['certified']??true)!==false||($cert['published']??true)!==false||($cert['production_activation_allowed']??true)!==false)return pay_ctry_006_fail($patch,'PAY-CTRY-006 certification ceiling/production denial failed.');
    return true;
}
?>
