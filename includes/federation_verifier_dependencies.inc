<?php
/**
 * PAY-SEC-004 isolated federation verifier dependency deployment adaptor.
 *
 * The ERP root Composer baseline intentionally remains PHP >=5.6. This helper
 * accepts only a separately installed, lock-hash-bound vendor tree outside the
 * repository/web root. It performs no installation, network request, assertion
 * verification, protocol transaction mutation, login, session issuance or
 * assurance elevation.
 */

if (!defined('FEDERATION_VERIFIER_DEPLOYMENT_MIN_PHP'))
    define('FEDERATION_VERIFIER_DEPLOYMENT_MIN_PHP', '7.3.0');
if (!defined('FEDERATION_VERIFIER_DEPLOYMENT_MAX_PHP'))
    define('FEDERATION_VERIFIER_DEPLOYMENT_MAX_PHP', '8.5.0');
if (!defined('FEDERATION_VERIFIER_DEPLOYMENT_OIDC_VERSION'))
    define('FEDERATION_VERIFIER_DEPLOYMENT_OIDC_VERSION', '1.0.2');
if (!defined('FEDERATION_VERIFIER_DEPLOYMENT_SAML_VERSION'))
    define('FEDERATION_VERIFIER_DEPLOYMENT_SAML_VERSION', '4.3.2');

function federation_verifier_dependency_policy()
{
    return array(
        'manifest' => 'deploy/federation-verifier/composer.json',
        'php_min' => FEDERATION_VERIFIER_DEPLOYMENT_MIN_PHP,
        'php_max_exclusive' => FEDERATION_VERIFIER_DEPLOYMENT_MAX_PHP,
        'extensions' => array('curl', 'dom', 'json', 'openssl', 'zlib'),
        'packages' => array(
            'jumbojett/openid-connect-php' => FEDERATION_VERIFIER_DEPLOYMENT_OIDC_VERSION,
            'onelogin/php-saml' => FEDERATION_VERIFIER_DEPLOYMENT_SAML_VERSION
        ),
        'classes' => array(
            'Jumbojett\\OpenIDConnectClient',
            'OneLogin\\Saml2\\Auth'
        )
    );
}

function federation_verifier_dependency_path_inside($path, $base)
{
    $path = rtrim(str_replace('\\', '/', $path), '/');
    $base = rtrim(str_replace('\\', '/', $base), '/');
    if ($path === '' || $base === '')
        return false;
    if (DIRECTORY_SEPARATOR === '\\') {
        $path = strtolower($path);
        $base = strtolower($base);
    }
    return $path === $base || strpos($path.'/', $base.'/') === 0;
}

function federation_verifier_dependency_manifest_status($root)
{
    $policy = federation_verifier_dependency_policy();
    $root_real = realpath($root);
    if ($root_real === false)
        return array('valid'=>false, 'reason'=>'repository_root_unresolvable');
    $manifest = $root_real.DIRECTORY_SEPARATOR.str_replace('/', DIRECTORY_SEPARATOR, $policy['manifest']);
    if (!is_file($manifest) || is_link($manifest))
        return array('valid'=>false, 'reason'=>'deployment_manifest_missing');
    $decoded = json_decode((string)file_get_contents($manifest), true);
    if (!is_array($decoded) || !isset($decoded['require']) || !is_array($decoded['require']))
        return array('valid'=>false, 'reason'=>'deployment_manifest_invalid');
    $required = $decoded['require'];
    if (!isset($required['php']) || $required['php'] !== '>=7.3 <8.5')
        return array('valid'=>false, 'reason'=>'deployment_php_boundary_changed');
    foreach ($policy['extensions'] as $extension) {
        $key = 'ext-'.$extension;
        if (!isset($required[$key]) || $required[$key] !== '*')
            return array('valid'=>false, 'reason'=>'deployment_extension_boundary_changed');
    }
    foreach ($policy['packages'] as $package=>$version) {
        if (!isset($required[$package]) || ltrim((string)$required[$package], 'v') !== $version)
            return array('valid'=>false, 'reason'=>'deployment_package_boundary_changed');
    }
    return array(
        'valid'=>true,
        'reason'=>'ok',
        'manifest'=>$manifest,
        'manifest_sha256'=>hash_file('sha256', $manifest)
    );
}

function federation_verifier_dependency_status($root, $vendor_dir, $lock_file, $expected_lock_sha256)
{
    $status = array('available'=>false, 'reason'=>'unavailable');
    $manifest = federation_verifier_dependency_manifest_status($root);
    if (!$manifest['valid']) {
        $status['reason'] = $manifest['reason'];
        return $status;
    }
    if (version_compare(PHP_VERSION, FEDERATION_VERIFIER_DEPLOYMENT_MIN_PHP, '<')) {
        $status['reason'] = 'php_runtime_too_old';
        return $status;
    }
    if (!version_compare(PHP_VERSION, FEDERATION_VERIFIER_DEPLOYMENT_MAX_PHP, '<')) {
        $status['reason'] = 'php_runtime_future_unreviewed';
        return $status;
    }
    $policy = federation_verifier_dependency_policy();
    foreach ($policy['extensions'] as $extension) {
        if (!extension_loaded($extension)) {
            $status['reason'] = 'required_extension_missing';
            return $status;
        }
    }
    if (!is_string($expected_lock_sha256) || preg_match('/^[a-f0-9]{64}$/', $expected_lock_sha256) !== 1) {
        $status['reason'] = 'lock_sha256_unconfigured';
        return $status;
    }
    $root_real = realpath($root);
    $vendor_real = realpath($vendor_dir);
    $lock_real = realpath($lock_file);
    if ($root_real === false || $vendor_real === false || !is_dir($vendor_real) || is_link($vendor_dir)) {
        $status['reason'] = 'vendor_tree_unavailable';
        return $status;
    }
    if (federation_verifier_dependency_path_inside($vendor_real, $root_real)) {
        $status['reason'] = 'vendor_tree_inside_repository';
        return $status;
    }
    if ($lock_real === false || !is_file($lock_real) || is_link($lock_file)) {
        $status['reason'] = 'lock_file_unavailable';
        return $status;
    }
    if (federation_verifier_dependency_path_inside($lock_real, $root_real)) {
        $status['reason'] = 'lock_file_inside_repository';
        return $status;
    }
    $actual_lock_sha256 = hash_file('sha256', $lock_real);
    if (!is_string($actual_lock_sha256) || !hash_equals($expected_lock_sha256, $actual_lock_sha256)) {
        $status['reason'] = 'lock_sha256_mismatch';
        return $status;
    }
    $autoload = $vendor_real.DIRECTORY_SEPARATOR.'autoload.php';
    $autoload_real = realpath($autoload);
    if ($autoload_real === false || !is_file($autoload_real) || is_link($autoload)
        || !federation_verifier_dependency_path_inside($autoload_real, $vendor_real)) {
        $status['reason'] = 'autoload_unavailable';
        return $status;
    }
    require_once($autoload_real);
    if (!class_exists('Composer\\InstalledVersions')) {
        $status['reason'] = 'composer_inventory_unavailable';
        return $status;
    }
    foreach ($policy['packages'] as $package=>$expected_version) {
        if (!call_user_func(array('Composer\\InstalledVersions', 'isInstalled'), $package)) {
            $status['reason'] = 'required_package_missing';
            return $status;
        }
        $pretty = call_user_func(array('Composer\\InstalledVersions', 'getPrettyVersion'), $package);
        if (!is_string($pretty) || ltrim($pretty, 'v') !== $expected_version) {
            $status['reason'] = 'required_package_version_mismatch';
            return $status;
        }
    }
    foreach ($policy['classes'] as $class) {
        if (!class_exists($class)) {
            $status['reason'] = 'required_verifier_class_missing';
            return $status;
        }
    }
    if (!function_exists('federation_verifier_runtime_status')) {
        $runtime_file = dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_verifier_runtime.inc';
        if (!is_file($runtime_file)) {
            $status['reason'] = 'runtime_capability_gate_missing';
            return $status;
        }
        require_once($runtime_file);
    }
    $oidc = federation_verifier_runtime_status('oidc_code_pkce_v1');
    $saml = federation_verifier_runtime_status('saml_sp_acs_v1');
    if (!is_array($oidc) || empty($oidc['available']) || !is_array($saml) || empty($saml['available'])) {
        $status['reason'] = 'runtime_capability_gate_rejected';
        return $status;
    }
    return array(
        'available'=>true,
        'reason'=>'ok',
        'manifest_sha256'=>$manifest['manifest_sha256'],
        'lock_sha256'=>$actual_lock_sha256,
        'vendor_dir'=>$vendor_real
    );
}

function federation_verifier_dependency_configured_status($root)
{
    if (!defined('FEDERATION_VERIFIER_VENDOR_DIR')
        || !defined('FEDERATION_VERIFIER_LOCK_FILE')
        || !defined('FEDERATION_VERIFIER_LOCK_SHA256'))
        return array('available'=>false, 'reason'=>'deployment_not_configured');
    return federation_verifier_dependency_status(
        $root,
        FEDERATION_VERIFIER_VENDOR_DIR,
        FEDERATION_VERIFIER_LOCK_FILE,
        FEDERATION_VERIFIER_LOCK_SHA256
    );
}

function federation_verifier_dependency_available($root)
{
    $status = federation_verifier_dependency_configured_status($root);
    return is_array($status) && !empty($status['available']);
}
