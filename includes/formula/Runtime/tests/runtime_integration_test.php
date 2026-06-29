<?php
/**
 * Runtime component integration test.
 *
 * Verifies that all runtime classes load and interact correctly.
 */
$path_to_root = 'c:/Users/trana/OneDrive - ll64/www/NotrinosERP-1.0';
require_once $path_to_root . '/includes/formula/formula_bootstrap.inc';

echo "=== Runtime Component Integration Test ===\n\n";

// 1. Verify all runtime classes load
echo "--- 1. Class Loading ---\n";
$runtimeClasses = array(
    'Formula_Runtime_FormulaRuntime',
    'Formula_Runtime_NodeEvaluator',
    'Formula_Runtime_FunctionExecutor',
    'Formula_Runtime_RuntimeSession',
    'Formula_Runtime_ExplainVisitor',
);
$allLoaded = true;
foreach ($runtimeClasses as $class) {
    $exists = class_exists($class, false);
    echo ($exists ? "  PASS" : "  FAIL") . ": $class\n";
    if (!$exists) $allLoaded = false;
}

// 2. Create registries and a simple context
echo "\n--- 2. Runtime Initialization ---\n";
$fnReg = new Formula_Registry_FunctionRegistry();
$varReg = new Formula_Registry_VariableRegistry();

$context = new Formula_Context_FormulaContext(
    array('BASIC' => 8000.0, 'GROSS' => 8000.0, 'DAYS_WORKED' => 22.0, 'WORKING_DAYS' => 22.0, 'HOURLY_RATE' => 45.45, 'OVERTIME_HOURS' => 10.0),
    null,
    null,
    array(),
    true
);

// 3. Create runtime engine, compile and execute formulas
echo "\n--- 3. Compile + Execute ---\n";
$runtime = new Formula_Runtime_FormulaRuntime(
    $fnReg,
    $varReg,
    array('BASIC', 'GROSS', 'DAYS_WORKED', 'PAYABLE_DAYS', 'WORKING_DAYS', 'DAYS_IN_MONTH',
          'LEAVE_DAYS', 'PAID_LEAVE_DAYS', 'ABSENT_DAYS', 'OVERTIME_HOURS', 'UNPAID_LEAVE_DAYS', 'HOURLY_RATE')
);

$tests = array(
    array('BASIC + 500', 8500.0),
    array('BASIC * 0.1', 800.0),
    array('BASIC - 500', 7500.0),
    array('BASIC / 22', 363.6363636),
    array('BASIC + HOURLY_RATE * OVERTIME_HOURS * 1.5', 8681.75),
    array('BASIC - (BASIC * 0.05)', 7600.0),
    array('(BASIC / WORKING_DAYS) * DAYS_WORKED', 8000.0),
    array('2 + 3 * 4', 14.0),
    array('(2 + 3) * 4', 20.0),
    array('2 ^ 3 ^ 2', 512.0),
    array('-5', -5.0),
    array('+5', 5.0),
    array('42', 42.0),
    array('3.14', 3.14),
    array('', 0.0),
);

$pass = 0;
$fail = 0;
foreach ($tests as $test) {
    list($formula, $expected) = $test;
    try {
        $result = $runtime->evaluate($formula, $context);
        $delta = abs((float)$result - $expected);
        $ok = $delta < 0.0001;

        $label = $formula === '' ? '(empty)' : $formula;
        if ($ok) {
            echo "  PASS: \"$label\" => " . (float)$result . "\n";
            $pass++;
        } else {
            echo "  FAIL: \"$label\" => " . (float)$result . " (expected $expected)\n";
            $fail++;
        }
    } catch (Exception $e) {
        echo "  FAIL: \"$formula\" threw: " . get_class($e) . ": " . $e->getMessage() . "\n";
        $fail++;
    }
}

echo "\n=== Results: $pass passed, $fail failed ===\n";
