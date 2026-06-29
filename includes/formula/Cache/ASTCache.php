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
 * ASTCache — L2 cache layer for compiled formula ASTs.
 *
 * Maps compiled formula identifiers to serialized AST + metadata
 * for persistent cross-request caching. This is the second cache
 * tier checked — after the source cache confirms a formula has
 * been compiled before, the ASTCache retrieves the compiled result.
 *
 * ## Cache Key
 *
 *   'compiled:' . $compiledFormulaId → serialized AST + metadata
 *
 * ## Invalidation
 *
 *   - Compiler version bumps → ALL entries invalidated
 *   - Language version bumps → ALL entries invalidated
 *   - Manual invalidation via invalidate(key)
 *
 * ## Storage
 *
 * File-based storage via FormulaCache backend.
 *
 * @package Formula\Cache
 * @since   2.0.0
 * @see     Formula_Cache_FormulaCache
 */
class Formula_Cache_ASTCache implements Formula_Contracts_CacheInterface
{
    /** @var Formula_Cache_FormulaCache */
    private $backend;

    /**
     * Construct an AST cache.
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
     * Get compiled AST data for a formula ID.
     *
     * @param string $key     The compiled formula ID
     * @param mixed  $default Value to return if key not found
     * @return array|null The compiled data array, or $default
     */
    public function get($key, $default = null)
    {
        $data = $this->backend->getCompiled($key);

        return $data !== null ? $data : $default;
    }

    /**
     * Store compiled AST data for a formula ID.
     *
     * @param string   $key   The compiled formula ID
     * @param mixed    $value The compiled data array (ast + metadata)
     * @param int|null $ttl   Time-to-live in seconds, null for infinite
     * @return bool True on success
     */
    public function set($key, $value, $ttl = null)
    {
        if (!is_array($value)) {
            return false;
        }

        $this->backend->setCompiled($key, $value);
        return true;
    }

    /**
     * Check if a compiled formula exists in the cache.
     *
     * @param string $key The compiled formula ID
     * @return bool
     */
    public function has($key)
    {
        return $this->backend->getCompiled($key) !== null;
    }

    /**
     * Invalidate a specific compiled formula.
     *
     * Note: file-based invalidation by specific ID is not directly
     * supported in the current backend. This method exists for
     * interface compliance and future backends that support
     * targeted invalidation.
     *
     * @param string $key The compiled formula ID
     * @return bool
     */
    public function invalidate($key)
    {
        // Targeted invalidation is supported by removing the
        // specific cache file for this compiled ID.
        $safeKey = preg_replace('/[^a-zA-Z0-9_\-:.]/', '_', 'compiled:' . $key);
        $path = $this->backend->cacheDir . '/' . $safeKey . '.cache';

        if (file_exists($path)) {
            @unlink($path);
        }

        return true;
    }

    /**
     * Clear ALL compiled formula cache entries.
     *
     * @return bool True on success
     */
    public function clear()
    {
        $this->backend->invalidateAllCompiled();
        return true;
    }

    // -----------------------------------------------------------------------
    //  ASTCache-Specific Methods
    // -----------------------------------------------------------------------

    /**
     * Generate a unique compiled formula ID.
     *
     * Used when storing a newly compiled formula for the first time.
     *
     * @param string $formulaHash SHA-256 of the formula source
     * @return string A unique compiled formula ID
     */
    public function generateId($formulaHash)
    {
        return $formulaHash . '.' . uniqid('', true);
    }

    /**
     * Store compiled data keyed by the formula source hash.
     *
     * @param string $formulaHash SHA-256 of formula source
     * @param array  $data        Serialized AST + metadata
     * @return string The generated compiled formula ID
     */
    public function store($formulaHash, array $data)
    {
        $id = $this->generateId($formulaHash);
        $this->backend->setCompiled($id, $data);
        return $id;
    }

    /**
     * Get the current compiler version used for invalidation.
     *
     * @return string
     */
    public function getCompilerVersion()
    {
        return $this->backend->compilerVersion;
    }
}
