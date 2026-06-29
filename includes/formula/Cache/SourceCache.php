<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

/**
 * SourceCache — L1 cache layer for formula source text.
 *
 * Maps formula source strings (via SHA-256 fingerprint) to compiled
 * formula identifiers. This is the first cache tier checked during
 * compilation — if a formula has been compiled before and its source
 * hasn't changed, recompilation is skipped entirely.
 *
 * ## Cache Key
 *
 *   SHA-256(trimmed, normalized formula string) → compiled formula ID
 *
 * ## Invalidation
 *
 *   - Formula text changes → different SHA-256 → cache miss (automatic)
 *   - Manual invalidation via invalidate(key)
 *   - Full flush via clear()
 *
 * ## Storage
 *
 * Delegates to FormulaCache for persistent file-based storage.
 * This class provides the source-cache-specific interface while
 * the actual I/O is handled by the underlying cache backend.
 *
 * @package Formula\Cache
 * @since   2.0.0
 * @see     Formula_Cache_FormulaCache
 */
class Formula_Cache_SourceCache implements Formula_Contracts_CacheInterface
{
    /** @var Formula_Cache_FormulaCache */
    private $backend;

    /**
     * Construct a source cache.
     *
     * @param Formula_Cache_FormulaCache $backend The underlying cache storage
     */
    public function __construct(Formula_Cache_FormulaCache $backend)
    {
        $this->backend = $backend;
    }

    // -----------------------------------------------------------------------
    //  CacheInterface
    // -----------------------------------------------------------------------

    /**
     * Get the compiled formula ID for a formula string.
     *
     * @param string $key     SHA-256 hash of formula source
     * @param mixed  $default Value to return if key not found
     * @return string|null The compiled formula ID, or $default
     */
    public function get($key, $default = null)
    {
        $id = $this->backend->getSourceId($key);

        return $id !== null ? $id : $default;
    }

    /**
     * Store a compiled formula ID for a formula string.
     *
     * @param string   $key   SHA-256 hash of formula source
     * @param mixed    $value The compiled formula ID
     * @param int|null $ttl   Time-to-live (ignored — source cache is permanent)
     * @return bool True on success
     */
    public function set($key, $value, $ttl = null)
    {
        $this->backend->setSourceId($key, $value);
        return true;
    }

    /**
     * Check if a formula has been compiled and cached.
     *
     * @param string $key SHA-256 hash of formula source
     * @return bool
     */
    public function has($key)
    {
        return $this->backend->getSourceId($key) !== null;
    }

    /**
     * Invalidate a specific formula's source cache entry.
     *
     * Since the source cache key is the SHA-256 of the formula text,
     * invalidation by hash is not directly supported — the key IS the
     * formula fingerprint. A formula that changed text would have a
     * different hash and would not match. This method exists for
     * interface compliance and performs no operation.
     *
     * @param string $key SHA-256 hash
     * @return bool Always true
     */
    public function invalidate($key)
    {
        // Source cache keys are content-derived. Invalidation
        // is implicit — a changed formula means a changed key.
        // Manual invalidation is not needed for this layer.
        return true;
    }

    /**
     * Clear all source cache entries.
     *
     * This clears ALL persistent cache files, including both
     * source cache (L1) and compiler cache (L2) since they
     * share the same file storage.
     *
     * @return bool True on success
     */
    public function clear()
    {
        $this->backend->clearAll();
        return true;
    }

    // -----------------------------------------------------------------------
    //  Source-Cache-Specific Methods
    // -----------------------------------------------------------------------

    /**
     * Compute the SHA-256 hash for a formula string.
     *
     * This is the standard key derivation for the source cache.
     *
     * @param string $formula The raw formula string
     * @return string SHA-256 hex digest
     */
    public function computeHash($formula)
    {
        return hash('sha256', (string)$formula);
    }

    /**
     * Store a compiled formula ID, keyed by the formula source
     * string (not pre-hashed).
     *
     * Convenience method that computes the hash internally.
     *
     * @param string $formula    The formula source string
     * @param string $compiledId The compiled formula ID
     * @return string The computed SHA-256 hash used as key
     */
    public function setByFormula($formula, $compiledId)
    {
        $hash = $this->computeHash($formula);
        $this->backend->setSourceId($formula, $compiledId);
        return $hash;
    }

    /**
     * Get the compiled formula ID by formula source string.
     *
     * Convenience method that computes the hash internally.
     *
     * @param string $formula The formula source string
     * @return string|null The compiled formula ID, or null
     */
    public function getByFormula($formula)
    {
        return $this->backend->getSourceId($formula);
    }
}
