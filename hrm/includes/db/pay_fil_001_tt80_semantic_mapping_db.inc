<?php
/** PAY-FIL-001 TT80 deterministic semantic mapping publication custody and writer. */
require_once(__DIR__.'/../../../includes/value_helpers.inc');
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_source_admission_db.inc');
include_once(dirname(__FILE__).'/pay_fil_001_runtime_db.inc');
include_once($path_to_root.'/includes/pay_fil_001_tt80_semantic_mapping_contract.inc');

function pay_fil_001_tt80_semantic_mapping_authority_required_for_database_version($version){return version_compare((string)$version,'1.0.1037','>=');}
function pay_fil_001_tt80_semantic_mapping_custody_required_for_database_version($version){return version_compare((string)$version,'1.0.1038','>=');}
function pay_fil_001_tt80_semantic_mapping_writer_required_for_database_version($version){return version_compare((string)$version,'1.0.1039','>=');}

function pay_fil_001_tt80_semantic_mapping_safe_add($a,$b,&$error){$a=(int)$a;$b=(int)$b;if($a<0||$b<0||$a>PHP_INT_MAX-$b)return pay_fil_001_runtime_fail($error,'tt80_semantic_mapping_amount_overflow');return $a+$b;}
function pay_fil_001_tt80_semantic_mapping_derive_indicators(array $facts,&$error=null)
{
    $error=null;$v=array();foreach(array('ct16','ct17','ct18','ct19','ct20','ct21','ct22','ct23','ct24','ct25','ct25_1','ct26','ct27','ct28','ct29','ct30','ct31','ct32') as $k)$v[$k]=0;
    foreach($facts as $r){
      $res=(string)$r['tax_residency'];
      if($res!=='resident'&&$res!=='nonresident')return pay_fil_001_runtime_fail($error,'tt80_semantic_mapping_unknown_residency');
      $term=(string)$r['employment_contract_term_class'];
      if($term==='unknown')return pay_fil_001_runtime_fail($error,'tt80_semantic_mapping_unknown_contract_term');
      if((int)$r['paid_worker']===1){
        $v['ct16']++;
        if($res==='resident'&&$term==='at_least_3_months')$v['ct17']++;
      }
      if((int)$r['withheld_person']===1){
        $v['ct18']++;
        $v[$res==='resident'?'ct19':'ct20']++;
      }
        foreach(array('ct21'=>'filing_taxable_income_minor','ct24'=>'foreign_insurer_nonmandatory_insurance_taxable_minor',
          'ct25'=>'petroleum_contract_exempt_taxable_minor','ct25_1'=>'resolution_exempt_income_minor','ct26'=>'withholding_subject_taxable_income_minor',
          'ct29'=>'personal_income_tax_withheld_minor','ct32'=>'foreign_insurer_nonmandatory_insurance_withheld_minor') as $ct=>$field){
          $n=pay_fil_001_tt80_semantic_mapping_safe_add($v[$ct],$r[$field],$error);
          if($n===false)return false;
          $v[$ct]=$n;
        }
        foreach(array('ct22'=>'filing_taxable_income_minor','ct27'=>'withholding_subject_taxable_income_minor','ct30'=>'personal_income_tax_withheld_minor') as $ct=>$field){$target=$res==='resident'?$ct:array('ct22'=>'ct23','ct27'=>'ct28','ct30'=>'ct31')[$ct];$n=pay_fil_001_tt80_semantic_mapping_safe_add($v[$target],$r[$field],$error);if($n===false)return false;$v[$target]=$n;}
    }
    foreach($v as $k=>$n)$v[$k]=(string)$n;return $v;
}
function pay_fil_001_tt80_publish_semantic_mapping($filing_source_set_id,$tt80_semantic_review_id,$taxpayer_profile_sha256,array $person_fact_sha256s,&$error=null)
{
    $error=null;
    if(!pay_fil_001_require_access(PAY_FIL_001_AUTHORIZE_PERMISSION,$error))return false;
    $publisher=current_user_id_or_zero();
    $company=pay_fil_001_company();
    $set_id=pay_fil_001_posint($filing_source_set_id);
    $review_id=pay_fil_001_posint($tt80_semantic_review_id);
    if($publisher<=0||$company<0||$set_id===false||$review_id===false||!is_sha256_hex($taxpayer_profile_sha256))return pay_fil_001_runtime_fail($error,
      'tt80_semantic_mapping_input_invalid');
    $fact_shas=array();foreach($person_fact_sha256s as $sha){if(!is_sha256_hex($sha))return pay_fil_001_runtime_fail($error,'tt80_semantic_mapping_fact_hash_invalid');$fact_shas[]=$sha;}sort($fact_shas,SORT_STRING);if(count($fact_shas)!==count(array_unique($fact_shas)))return pay_fil_001_runtime_fail($error,'tt80_semantic_mapping_fact_hash_duplicate');
    $review=pay_fil_001_one('SELECT * FROM '.TB_PREF.'hrm_pay_fil_001_tt80_semantic_reviews WHERE tt80_semantic_review_id='
      .(int)$review_id.' LIMIT 2');
    if(!$review||(int)$review['filing_source_set_id']!==$set_id||(string)$review['completeness_status']!=='complete')return pay_fil_001_runtime_fail($error,
      'tt80_semantic_mapping_complete_review_required');
    if(!hash_equals((string)$review['taxpayer_profile_sha256'],(string)$taxpayer_profile_sha256)||!hash_equals((string)$review['person_fact_set_sha256'],
      (string)pay_fil_001_hash($fact_shas))||count($fact_shas)!==(int)$review['person_fact_count'])return pay_fil_001_runtime_fail($error,
      'tt80_semantic_mapping_review_binding_mismatch');
    if((int)$review['reviewed_by']===$publisher)return pay_fil_001_runtime_fail($error,'tt80_semantic_mapping_publisher_reviewer_conflict');
    $s=pay_fil_001_one('SELECT s.filing_source_set_id,s.filing_obligation_id,s.source_set_sha256,s.currency_code,o.company_id,o.legal_entity_id,o.jurisdiction_code,o.obligation_type,o.obligation_sha256 FROM '
      .TB_PREF.'hrm_pay_fil_001_source_sets s INNER JOIN '.TB_PREF.'hrm_pay_fil_001_obligations o ON o.filing_obligation_id=s.filing_obligation_id WHERE s.filing_source_set_id='
      .(int)$set_id.' LIMIT 2');
    if(!$s||(int)$s['company_id']!==$company||$s['jurisdiction_code']!=='VN'||$s['obligation_type']!=='personal-income-tax-withholding'||$s['currency_code']!=='VND')return pay_fil_001_runtime_fail($error,
      'tt80_semantic_mapping_source_scope_invalid');
    $profile=pay_fil_001_one('SELECT * FROM '.TB_PREF.'hrm_pay_fil_001_tt80_taxpayer_profiles WHERE profile_sha256='.db_escape($taxpayer_profile_sha256)
      .' LIMIT 2');
    if(!$profile||(int)$profile['filing_obligation_id']!==(int)$s['filing_obligation_id']||!hash_equals((string)$profile['profile_sha256'],
      (string)pay_fil_001_tt80_semantic_source_profile_hash_from_row($profile)))return pay_fil_001_runtime_fail($error,'tt80_semantic_mapping_taxpayer_profile_invalid');
    if((int)$profile['captured_by']===$publisher)return pay_fil_001_runtime_fail($error,'tt80_semantic_mapping_publisher_maker_conflict');
    $facts=array();
    foreach($fact_shas as $sha){
      $row=pay_fil_001_one('SELECT * FROM '.TB_PREF.'hrm_pay_fil_001_tt80_person_facts WHERE fact_sha256='.db_escape($sha)
        .' LIMIT 2');
      if(!$row||(int)$row['filing_source_set_id']!==$set_id||!hash_equals((string)$row['fact_sha256'],(string)pay_fil_001_tt80_semantic_source_fact_hash_from_row($row)))return pay_fil_001_runtime_fail($error,
        'tt80_semantic_mapping_fact_invalid');
      if((int)$row['captured_by']===$publisher)return pay_fil_001_runtime_fail($error,'tt80_semantic_mapping_publisher_maker_conflict');
      $facts[]=$row;
    }
    $indicators=pay_fil_001_tt80_semantic_mapping_derive_indicators($facts,$error);
    if($indicators===false)return false;
    $indicator_json=json_encode($indicators,JSON_UNESCAPED_SLASHES);
    if(!is_string($indicator_json)||strlen($indicator_json)>2048)return pay_fil_001_runtime_fail($error,'tt80_semantic_mapping_indicator_encoding_invalid');
    $indicator_sha=pay_fil_001_hash($indicators);
    $contract_sha=pay_fil_001_tt80_semantic_mapping_contract_sha256();
    $basis=array('review_sha256'=>(string)$review['review_sha256'],'mapping_contract_sha256'=>$contract_sha,'taxpayer_profile_sha256'=>(string)$taxpayer_profile_sha256,
      'person_fact_set_sha256'=>(string)$review['person_fact_set_sha256'],'indicator_values_sha256'=>$indicator_sha,'published_by'=>$publisher);
    $mapping_sha=pay_fil_001_hash($basis);
    $existing=pay_fil_001_one('SELECT * FROM '.TB_PREF.'hrm_pay_fil_001_tt80_semantic_mappings WHERE tt80_semantic_review_id='
      .(int)$review_id.' LIMIT 2');
    if($existing){
      if(hash_equals((string)$existing['mapping_sha256'],$mapping_sha))return array('tt80_semantic_mapping_id'=>(int)$existing['tt80_semantic_mapping_id'],
        'mapping_sha256'=>$mapping_sha,'indicator_values'=>$indicators,'replayed'=>true,'runtime_generation_allowed'=>false);
      return pay_fil_001_runtime_fail($error,'tt80_semantic_mapping_review_already_published');
    }
    if(!pay_fil_001_begin_owned($error))return false;
    $at=pay_fil_001_now();
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_fil_001_tt80_semantic_mappings (tt80_semantic_review_id,filing_obligation_id,filing_source_set_id,review_sha256,mapping_contract_sha256,taxpayer_profile_sha256,person_fact_set_sha256,indicator_values_json,indicator_values_sha256,mapping_sha256,published_by,published_at,created_at) VALUES ('
      .(int)$review_id.','.(int)$s['filing_obligation_id'].','.$set_id.','.db_escape($review['review_sha256']).','.db_escape($contract_sha)
      .','.db_escape($taxpayer_profile_sha256).','.db_escape($review['person_fact_set_sha256']).','.db_escape($indicator_json)
      .','.db_escape($indicator_sha).','.db_escape($mapping_sha).','.$publisher.','.db_escape($at).','.db_escape($at).')';
    if(!db_query($sql)){
      rollback_transaction();
      return pay_fil_001_runtime_fail($error,'tt80_semantic_mapping_insert_failed');
    }
    $id=(int)db_insert_id();
    commit_transaction();
    return array('tt80_semantic_mapping_id'=>$id,'mapping_sha256'=>$mapping_sha,'indicator_values'=>$indicators,'replayed'=>false,
      'runtime_generation_allowed'=>false);
}
function pay_fil_001_tt80_semantic_mapping_status($filing_source_set_id,&$error=null)
{
    $error=null;
    $id=pay_fil_001_posint($filing_source_set_id);
    if($id===false)return pay_fil_001_runtime_fail($error,'filing_source_set_invalid');
    $q=db_query('SELECT tt80_semantic_mapping_id,tt80_semantic_review_id,review_sha256,mapping_contract_sha256,taxpayer_profile_sha256,person_fact_set_sha256,indicator_values_json,indicator_values_sha256,mapping_sha256,published_by,published_at FROM '
      .TB_PREF.'hrm_pay_fil_001_tt80_semantic_mappings WHERE filing_source_set_id='.(int)$id.' ORDER BY tt80_semantic_mapping_id DESC LIMIT 1');
    if(!$q)return pay_fil_001_runtime_fail($error,'semantic_mapping_custody_unavailable');
    if(db_num_rows($q)===0){
      $error='semantic_mapping_unpublished';
      return false;
    }
    return db_fetch_assoc($q);
}
?>
