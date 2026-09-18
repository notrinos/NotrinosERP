<?php
/** Read-only bounded inventory adapter for legacy payroll_periods mapping review. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_002_contract.inc');
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_002_legacy_mapping_review.inc');

function hrm_pay_core_002_legacy_period_inventory($limit=200)
{
    $limit=(int)$limit;
    if($limit<1||$limit>200)$limit=200;
    if(!hrm_migration_table_exists('payroll_periods'))return false;
    $fields='period_id,period_name,from_date,to_date,pay_date,department_id,status,total_gross,total_deductions,total_net,total_employer_cost,approved_by,approval_date,gl_trans_no,created_by,created_date,notes';
    $r=db_query('SELECT '.$fields.' FROM '.TB_PREF.'payroll_periods ORDER BY period_id ASC LIMIT '.$limit,'could not read bounded PAY-CORE-002 legacy payroll-period inventory');
    if(!$r)return false;
    $out=array();
    while($row=db_fetch_assoc($r)){
        $review=hrm_pay_core_002_review_legacy_period_mapping($row);
        $out[]=array(
            'legacy_period_id'=>(int)$row['period_id'],
            'legacy_status'=>(int)$row['status'],
            'department_scoped'=>($row['department_id']!==null&&(int)$row['department_id']>0),
            'mapping_disposition'=>(string)$review['disposition'],
            'ambiguity_codes'=>isset($review['ambiguity_codes'])?$review['ambiguity_codes']:array()
        );
    }
    return $out;
}
