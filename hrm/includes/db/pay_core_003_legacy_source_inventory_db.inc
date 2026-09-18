<?php
/** Read-only bounded, value-free inventory adapter for PAY-CORE-003 legacy sources. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_003_legacy_mapping_review.inc');
function hrm_pay_core_003_legacy_inventory_specs(){return array(
'pay_element'=>array('table'=>'pay_elements','id'=>'element_id','fields'=>'element_id,element_code,element_name,element_category,is_deduction,amount_type,default_amount,formula,account_code,employer_account,is_taxable,affects_gross,is_statutory,max_amount,min_amount,display_order,description,inactive'),
'salary_structure'=>array('table'=>'salary_structure','id'=>'structure_id','fields'=>'structure_id,position_id,grade_id,element_id,pay_amount,formula,effective_from,effective_to,is_active'),
'employee_salary'=>array('table'=>'employee_salary','id'=>'salary_id','fields'=>'salary_id,employee_id,element_id,amount,formula,effective_from,effective_to,is_active,reference'));
}
function hrm_pay_core_003_legacy_source_inventory($limit_per_source=200){$limit_per_source=(int)$limit_per_source;if($limit_per_source<1||$limit_per_source>200)$limit_per_source=200;$out=array();foreach(hrm_pay_core_003_legacy_inventory_specs() as $kind=>$spec){if(!hrm_migration_table_exists($spec['table']))return false;$r=db_query('SELECT '.$spec['fields'].' FROM '.TB_PREF.$spec['table'].' ORDER BY '.$spec['id'].' ASC LIMIT '.$limit_per_source,'could not read bounded PAY-CORE-003 legacy source inventory');if(!$r)return false;while($row=db_fetch_assoc($r)){$review=hrm_pay_core_003_review_legacy_source($kind,$row);$out[]=array('legacy_source_kind'=>$kind,'legacy_source_id'=>(int)$row[$spec['id']],'source_sha256'=>(string)$review['source_sha256'],'mapping_disposition'=>(string)$review['disposition'],'ambiguity_codes'=>isset($review['ambiguity_codes'])?$review['ambiguity_codes']:array());}}return $out;}
