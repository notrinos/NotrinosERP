<?php
/**
 * PAY-SEC-004 federation verifier runtime capability gate foundation.
 *
 * This file does not verify assertions, fetch trust material, create protocol
 * transactions, authenticate users, issue sessions, or elevate assurance. It
 * only reports whether a separately installed, reviewed verifier dependency is
 * present at an accepted version/runtime boundary. Absence or ambiguity fails
 * closed.
 */

if (!defined('FEDERATION_VERIFIER_OIDC_PACKAGE'))
    define('FEDERATION_VERIFIER_OIDC_PACKAGE', 'jumbojett/openid-connect-php');
if (!defined('FEDERATION_VERIFIER_OIDC_MIN_VERSION'))
    define('FEDERATION_VERIFIER_OIDC_MIN_VERSION', '1.0.2');
if (!defined('FEDERATION_VERIFIER_OIDC_MAX_VERSION'))
    define('FEDERATION_VERIFIER_OIDC_MAX_VERSION', '2.0.0');
if (!defined('FEDERATION_VERIFIER_OIDC_MIN_PHP'))
    define('FEDERATION_VERIFIER_OIDC_MIN_PHP', '7.2.0');
if (!defined('FEDERATION_VERIFIER_SAML_PACKAGE'))
    define('FEDERATION_VERIFIER_SAML_PACKAGE', 'onelogin/php-saml');
if (!defined('FEDERATION_VERIFIER_SAML_MIN_VERSION'))
    define('FEDERATION_VERIFIER_SAML_MIN_VERSION', '4.3.2');
if (!defined('FEDERATION_VERIFIER_SAML_MAX_VERSION'))
    define('FEDERATION_VERIFIER_SAML_MAX_VERSION', '5.0.0');
if (!defined('FEDERATION_VERIFIER_SAML_MIN_PHP'))
    define('FEDERATION_VERIFIER_SAML_MIN_PHP', '7.3.0');

/** Return the accepted dependency/profile policy, or false for an unknown profile. */
function federation_verifier_runtime_profile_policy($profile)
{
    if ($profile === 'oidc_code_pkce_v1') {
        return array(
            'protocol' => 'oidc',
            'package' => FEDERATION_VERIFIER_OIDC_PACKAGE,
            'min_version' => FEDERATION_VERIFIER_OIDC_MIN_VERSION,
            'max_version_exclusive' => FEDERATION_VERIFIER_OIDC_MAX_VERSION,
            'min_php' => FEDERATION_VERIFIER_OIDC_MIN_PHP,
            'required_class' => 'Jumbojett\\OpenIDConnectClient',
            'private_material_kind' => 'oidc_jwks'
        );
    }
    if ($profile === 'saml_sp_acs_v1') {
        return array(
            'protocol' => 'saml',
            'package' => FEDERATION_VERIFIER_SAML_PACKAGE,
            'min_version' => FEDERATION_VERIFIER_SAML_MIN_VERSION,
            'max_version_exclusive' => FEDERATION_VERIFIER_SAML_MAX_VERSION,
            'min_php' => FEDERATION_VERIFIER_SAML_MIN_PHP,
            'required_class' => 'OneLogin\\Saml2\\Auth',
            'private_material_kind' => 'saml_idp_certificate'
        );
    }
    return false;
}

/**
 * Resolve an installed Composer package version only when Composer can prove it.
 * No package is loaded or installed here.
 */
function federation_verifier_runtime_installed_version($package)
{
    if (!class_exists('Composer\\InstalledVersions'))
        return false;
    if (!method_exists('Composer\\InstalledVersions', 'isInstalled')
        || !call_user_func(array('Composer\\InstalledVersions', 'isInstalled'), $package))
        return false;
    if (method_exists('Composer\\InstalledVersions', 'getPrettyVersion')) {
        $version = call_user_func(array('Composer\\InstalledVersions', 'getPrettyVersion'), $package);
        if (is_string($version) && $version !== '')
            return ltrim($version, 'v');
    }
    if (method_exists('Composer\\InstalledVersions', 'getVersion')) {
        $version = call_user_func(array('Composer\\InstalledVersions', 'getVersion'), $package);
        if (is_string($version) && $version !== '')
            return ltrim($version, 'v');
    }
    return false;
}

/** Return true only for the exact accepted package-version interval. */
function federation_verifier_runtime_version_is_accepted($version, $minimum, $maximum_exclusive)
{
    if (!is_string($version) || $version === '')
        return false;
    return version_compare($version, $minimum, '>=')
        && version_compare($version, $maximum_exclusive, '<');
}

/**
 * Return a restricted, non-secret readiness record. Availability is false for
 * missing/ambiguous packages, unsupported PHP runtimes, missing required
 * classes, or unknown profiles.
 */
function federation_verifier_runtime_status($profile)
{
    $policy = federation_verifier_runtime_profile_policy($profile);
    if ($policy === false) {
        return array(
            'profile' => (string)$profile,
            'available' => false,
            'reason' => 'unsupported_profile'
        );
    }

    $status = array(
        'profile' => (string)$profile,
        'protocol' => $policy['protocol'],
        'package' => $policy['package'],
        'available' => false,
        'reason' => 'dependency_unavailable',
        'installed_version' => null,
        'min_version' => $policy['min_version'],
        'max_version_exclusive' => $policy['max_version_exclusive'],
        'min_php' => $policy['min_php'],
        'private_material_kind' => $policy['private_material_kind']
    );

    if (version_compare(PHP_VERSION, $policy['min_php'], '<')) {
        $status['reason'] = 'php_runtime_below_profile_floor';
        return $status;
    }

    $installed = federation_verifier_runtime_installed_version($policy['package']);
    if ($installed === false) {
        $status['reason'] = 'dependency_version_unprovable';
        return $status;
    }
    $status['installed_version'] = $installed;

    if (!federation_verifier_runtime_version_is_accepted(
        $installed,
        $policy['min_version'],
        $policy['max_version_exclusive'])) {
        $status['reason'] = 'dependency_version_outside_accepted_range';
        return $status;
    }

    if (!class_exists($policy['required_class'])) {
        $status['reason'] = 'dependency_class_unavailable';
        return $status;
    }

    $status['available'] = true;
    $status['reason'] = 'accepted_dependency_present';
    return $status;
}

/** Fail-closed convenience predicate for later verifier-command code. */
function federation_verifier_runtime_available($profile)
{
    $status = federation_verifier_runtime_status($profile);
    return isset($status['available']) && $status['available'] === true;
}
