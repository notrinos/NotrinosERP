<?php
/**
 * Metadata-only provider descriptor for PAY-AI-001.
 *
 * This is intentionally not an invocation interface. A future reviewed slice
 * may introduce execution only after residency, security, minimization,
 * permission-filtering and audit controls are accepted.
 */
interface PayAI001ProviderDescriptor
{
    /** @return string Stable non-secret provider code. */
    public function getProviderCode();

    /** @return string Stable model/version identifier. */
    public function getModelVersion();

    /** @return string SHA-256 of the approved residency/security policy. */
    public function getProviderPolicySha256();
}
?>
