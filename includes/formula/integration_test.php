<?php
/**
 * Integration test for FormulaFacade and all supporting types.
 *
 * Run: php integration_test.php
 */
$path_to_root = dirname(dirname(__DIR__));
$GLOBALS['path_to_root'] = $path_to_root;
define('FORMULA_BOOTSTRAP', true);
require __DIR__ . '/formula_bootstrap.inc';

echo "=== Formula Framework Integration Test ===\n";
echo "PathToRoot: $path_to_root\n\n";

$errors = array();
$files_to_check = array(
    'Formula_Context_FormulaContext',
    'Formula_Context_FormulaContextBuilder',
    'Formula_Context_CompanyContext',
    'Formula_Context_SecurityContext',
    'Formula_Compiler_CompiledFormula',
    'Formula_Compiler_FormulaMetadata',
    'Formula_Compiler_ValidationResult',
    'Formula_Diagnostics_ExplainResult',
    'Formula_Runtime_FormulaRuntime',
    'Formula_Compiler_AST_NullNode',
    'Formula_Compiler_AST_LiteralNode',
    'Formula_Compiler_AST_VariableNode',
    'Formula_Compiler_AST_FunctionNode',
    'Formula_Compiler_AST_BinaryOperatorNode',
    'Formula_Compiler_AST_UnaryOperatorNode',
    'Formula_Compiler_AST_ConditionalNode',
    'Formula_Compiler_AST_ComparisonNode',
    'Formula_Compiler_AST_LogicalNode',
    'Formula_Compiler_AST_RangeNode',
    'FormulaFacade',
);

foreach ($files_to_check as $class_name) {
    if (class_exists($class_name, true)) {
        echo "  OK  $class_name\n";
    } else {
        echo "  MISS $class_name\n";
        $errors[] = $class_name;
    }
}

if (!empty($errors)) {
    echo "\n=== ERRORS: Missing classes ===\n";
    echo implode(', ', $errors) . "\n";
    exit(1);
}

echo "\n=== ALL CLASSES LOADED ===\n\n";

echo "--- FormulaFacade Methods ---\n";
$methods = get_class_methods('FormulaFacade');
sort($methods);
foreach ($methods as $m) {
    echo "  $m\n";
}

echo "\n--- Testing version() ---\n";
$ver = FormulaFacade::version();
echo json_encode($ver, JSON_PRETTY_PRINT) . "\n";

echo "\n--- Testing ProviderMetadata ---\n";
try {
    $provider = new Formula_Registry_ProviderMetadata(array(
        'namespaces' => array('test'),
        'version' => '1.0',
        'description' => 'Test provider for integration test',
    ));
    echo "  ProviderMetadata created OK\n";
} catch (Exception $e) {
    echo "  ERROR: " . $e->getMessage() . "\n";
}

echo "\n--- Testing compile() without engine ---\n";
try {
    FormulaFacade::compile('BASIC + 500');
    echo "  compile() succeeded\n";
} catch (Exception $e) {
    echo "  Expected: " . $e->getMessage() . "\n";
}

echo "\n--- Testing evaluate() without engine ---\n";
try {
    $ctx = Formula_Context_FormulaContextBuilder::create()->build();
    FormulaFacade::evaluate('BASIC', $ctx);
    echo "  evaluate() succeeded\n";
} catch (Exception $e) {
    echo "  Expected: " . $e->getMessage() . "\n";
}

echo "\n--- Testing FormulaContext ---\n";
try {
    $ctx = Formula_Context_FormulaContextBuilder::create()
        ->withVariable('BASIC', 5000)
        ->build();
    echo "  BASIC = " . $ctx->getVariable('BASIC') . "\n";
    echo "  toArray() keys: " . implode(', ', array_keys($ctx->toArray())) . "\n";
} catch (Exception $e) {
    echo "  ERROR: " . $e->getMessage() . "\n";
}

echo "\n--- Testing CompiledFormula serialize ---\n";
try {
    $checksum = hash('sha256', 'BASIC + 500');
    $node = new Formula_Compiler_AST_LiteralNode(42, 'integer', 1, 1);
    $meta = new Formula_Compiler_FormulaMetadata(array(
        'sourceChecksum' => $checksum,
    ));
    $compiled = new Formula_Compiler_CompiledFormula($node, $meta, $checksum);
    $serialized = serialize($compiled);
    echo "  serialize() worked, length: " . strlen($serialized) . "\n";
    
    $unserialized = unserialize($serialized);
    echo "  unserialize() worked\n";
} catch (Exception $e) {
    echo "  ERROR: " . $e->getMessage() . "\n";
}

echo "\n--- Testing ValidationResult ---\n";
$vr = new Formula_Compiler_ValidationResult();
$vr->addError('Test error at line 1');
$vr->addWarning('Test warning');
echo "  isValid: " . ($vr->isValid() ? 'true' : 'false') . "\n";
echo "  hasWarnings: " . ($vr->warningCount() > 0 ? 'true' : 'false') . "\n";
echo "  errorCount: " . $vr->errorCount() . "\n";

echo "\n=== INTEGRATION TEST COMPLETE ===\n";
