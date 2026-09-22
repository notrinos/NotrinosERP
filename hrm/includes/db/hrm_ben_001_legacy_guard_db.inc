<?php
/** HRM-BEN-001 never reclassifies statutory deductions as optional benefits. */
function hrm_ben_001_statutory_import_allowed(){return false;}
function hrm_ben_001_classify_legacy_source($table){$t=strtolower(trim((string)$table));if(in_array($t,array('payroll_deductions','employee_deductions','salary_deductions','taxes','statutory_deductions'),true)||strpos($t,'statutory')!==false||strpos($t,'tax')!==false||strpos($t,'deduction')!==false)return 'statutory_or_payroll_deduction';if($t==='employee_dependents')return 'external_dependant_authority';return 'unknown';}
function hrm_ben_001_require_non_statutory_source($table,&$error=null){$error=null;$c=hrm_ben_001_classify_legacy_source($table);if($c!=='unknown'){$error='legacy_source_not_optional_benefit_authority';return false;}return true;}
?>
