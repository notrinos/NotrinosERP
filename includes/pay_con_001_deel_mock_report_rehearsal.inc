<?php
/**
 * PAY-CON-001 offline synthetic G2N CSV inspection. No employee data sink,
 * importer, payroll calculation, DB operation, provider network call or writer.
 */
require_once dirname(__FILE__).'/pay_con_001_deel_mock_adapter.inc';

/** Inspect only the hash-sealed built-in fixture through an exact read request. */
function pay_con_001_deel_mock_report_receipt($request, $fixture)
{
    if (!is_array($request) || ($request['route'] ?? '') !== 'gross_to_net_csv'
        || !pay_con_001_deel_mock_fixture_valid($fixture)) return false;
    $result = pay_con_001_deel_mock_dispatch($request, $fixture);
    if (!is_array($result) || $result !== array(
        'status' => 200, 'data' => $fixture['gross_to_net_csv'],
        'mock_only' => true, 'authoritative' => false)) return false;
    $csv = $result['data'];
    if (!is_string($csv) || strlen($csv) > 16384 || substr($csv, -1) !== "\n"
        || strpos($csv, "\r") !== false || strpos($csv, "\0") !== false) return false;
    $lines = explode("\n", substr($csv, 0, -1));
    if (count($lines) < 2 || count($lines) > 11) return false;
    if (str_getcsv(array_shift($lines), ",", "\"", "") !== array('worker_reference','gross','deductions','net','currency')) return false;
    $seen = array();
    foreach ($lines as $line) {
        if ($line === '' || strpos($line, '"') !== false || strpos($line, ',') === false) return false;
        $fields = str_getcsv($line, ",", "\"", "");
        if (count($fields) !== 5 || !is_string($fields[0])
            || preg_match('/^FAKE-[0-9]{3,6}$/D', $fields[0]) !== 1
            || isset($seen[$fields[0]]) || $fields[4] !== 'VND') return false;
        foreach (array(1,2,3) as $index) {
            if (!is_string($fields[$index]) || preg_match('/^(0|[1-9][0-9]{0,8})$/D', $fields[$index]) !== 1) return false;
        }
        if ((int)$fields[1] !== (int)$fields[2] + (int)$fields[3]) return false;
        $seen[$fields[0]] = true;
    }
    // No worker row, money value, ID mapping or authoritative result is returned.
    return array(
        'classification' => 'SEALED_SYNTHETIC_G2N_READ_ONLY',
        'company_id' => 0,
        'row_count' => count($seen),
        'currency' => 'VND',
        'report_digest_sha256' => hash('sha256', 'PAY-CON-001:DEEL:SYNTHETIC_G2N:v1:'.$csv),
        'real_provider_admitted' => false,
        'results_imported' => false,
        'payroll_result_authority' => false,
        'gl_posted' => false
    );
}
?>
