<?php
/** PAY-API-001 bounded payroll business-data read contract. */
include_once(dirname(__FILE__).'/pay_api_001_contract.inc');
if (!defined('PAY_API_001_BUSINESS_READ_CONTRACT_VERSION')) define('PAY_API_001_BUSINESS_READ_CONTRACT_VERSION', '1.0.876');
if (!defined('PAY_API_001_PAYROLL_PERIOD_SUMMARY_RESOURCE')) define('PAY_API_001_PAYROLL_PERIOD_SUMMARY_RESOURCE', 'payroll.period.summary');
if (!defined('PAY_API_001_PAYROLL_PERIOD_SUMMARY_CONTRACT')) define('PAY_API_001_PAYROLL_PERIOD_SUMMARY_CONTRACT', 'v1');
if (!defined('PAY_API_001_PAYROLL_PERIOD_SCOPE_TYPE')) define('PAY_API_001_PAYROLL_PERIOD_SCOPE_TYPE', 'payroll_period');

function pay_api_001_business_data_dispatch_enabled_for_database_version($database_version)
{
    return pay_api_001_business_data_dispatch_allowed_for_database_version($database_version);
}

function pay_api_001_business_data_dispatch_enabled()
{
    if (!function_exists('get_company_pref')) return false;
    $version = get_company_pref('version_id', true);
    return is_string($version) && pay_api_001_business_data_dispatch_enabled_for_database_version($version);
}

function pay_api_001_payroll_period_scope_id($scope_type, $scope_key)
{
    if ((string)$scope_type !== PAY_API_001_PAYROLL_PERIOD_SCOPE_TYPE || !is_string($scope_key)) return false;
    if (preg_match('/^[1-9][0-9]{0,9}$/D', $scope_key) !== 1) return false;
    $id = (int)$scope_key;
    return $id > 0 && $id <= 2147483647 && (string)$id === $scope_key ? $id : false;
}

function pay_api_001_payroll_period_summary_request($resource_key, $contract_version, $scope_type, $scope_key, $cursor_sha256=null)
{
    if ((string)$resource_key !== PAY_API_001_PAYROLL_PERIOD_SUMMARY_RESOURCE || (string)$contract_version !== PAY_API_001_PAYROLL_PERIOD_SUMMARY_CONTRACT) return false;
    if ($cursor_sha256 !== null) return false;
    $period_id = pay_api_001_payroll_period_scope_id($scope_type, $scope_key);
    if ($period_id === false) return false;
    return array('period_id'=>$period_id,'resource_key'=>PAY_API_001_PAYROLL_PERIOD_SUMMARY_RESOURCE,'contract_version'=>PAY_API_001_PAYROLL_PERIOD_SUMMARY_CONTRACT,'scope_type'=>PAY_API_001_PAYROLL_PERIOD_SCOPE_TYPE,'scope_key'=>(string)$period_id);
}

function pay_api_001_business_read_decimal($value)
{
    if (is_int($value)) return (string)$value;
    if (!is_string($value) || $value === '' || strlen($value) > 80) return false;
    return preg_match('/^-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?$/D', $value) === 1 ? $value : false;
}

function pay_api_001_payroll_period_summary_project_row($row)
{
    if (!is_array($row)) return false;
    foreach (array('period_id','from_date','to_date','pay_date','status','total_gross','total_deductions','total_net','total_employer_cost') as $k) if (!array_key_exists($k,$row)) return false;
    $id = (int)$row['period_id']; $status = (int)$row['status'];
    if ($id <= 0 || !in_array($status,array(2,3,4,5,6),true)) return false;
    foreach (array('from_date','to_date') as $k) if (!is_string($row[$k]) || preg_match('/^\d{4}-\d{2}-\d{2}$/D',$row[$k])!==1) return false;
    if ($row['pay_date'] !== null && (!is_string($row['pay_date']) || preg_match('/^\d{4}-\d{2}-\d{2}$/D',$row['pay_date'])!==1)) return false;
    $amounts=array(); foreach (array('total_gross','total_deductions','total_net','total_employer_cost') as $k){$amounts[$k]=pay_api_001_business_read_decimal((string)$row[$k]);if($amounts[$k]===false)return false;}
    return array('period_id'=>$id,'from_date'=>$row['from_date'],'to_date'=>$row['to_date'],'pay_date'=>$row['pay_date'],'status'=>$status,'total_gross'=>$amounts['total_gross'],'total_deductions'=>$amounts['total_deductions'],'total_net'=>$amounts['total_net'],'total_employer_cost'=>$amounts['total_employer_cost']);
}
?>
