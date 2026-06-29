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
 * FormulaCache — Central caching facade for the Formula Framework.
 *
 * FormulaCache provides a unified interface to the three cache layers:
 *
 *   L1: Source Cache  — SHA-256(formula) → compiled formula ID
 *   L2: Compiler Cache — Compiled formula ID → serialized AST + metadata
 *   L3: Runtime Cache  — formula hash + context hash → evaluation result
 *
 * Each layer has independent invalidation. The facade coordinates
 * cache lookups, stores, and invalidation across all three layers.
 *
 * ## Cache Key Design
 *
 * L1 key: hash('sha256', $formulaString)
 * L2 key: 'compiled:' . $compiledFormulaId
 * L3 key: 'result:' . $formulaHash . ':' . $contextHash
 *
 * ## Invalidation Rules
 *
 * | Trigger                         | Invalidation Scope     |
 * |---------------------------------|------------------------|
 * | Formula text changes            | Single formula (L1)    |
 * | Compiler version bumps          | All formulas (L2)      |
 * | Language version bumps          | All formulas (L2)      |
 * | Session end / context change    | All runtime (L3)       |
 * | Function metadata changes       | Formulas using function|
 *
 * ## Storage Backend
 *
 * Default implementation uses file-based storage at
 * `company/X/cache/formula/`. The storage directory is
 * created on first use. Files are named by cache key with
 * `.cache` extension.
 *
 * @package Formula\Cache
 * @since   2.0.0
 */
class Formula_Cache_FormulaCache
{
    /** @var string Base directory for cache files */
    private $cacheDir;

    /** @var bool Whether caching is enabled */
    private $enabled;

    /** @var string Compiler version tag for cache invalidation */
    private $compilerVersion;

    /** @var array In-memory L3 runtime cache (session-scoped) */
    private $runtimeCache = array();

    /**
     * Construct a FormulaCache instance.
     *
     * @param string      $cacheDir        Absolute path to cache directory
     * @param bool        $enabled         Whether caching is enabled
     * @param string|null $compilerVersion Compiler version for invalidation (default: FORMULA_COMPILER_VERSION)
     */
    public function __construct(
        $cacheDir,
        $enabled = true,
        $compilerVersion = null
    ) {
        $this->cacheDir        = rtrim((string)$cacheDir, '/\\');
        $this->enabled         = (bool)$enabled;
        $this->compilerVersion = $compilerVersion !== null
            ? (string)$compilerVersion
            : (defined('FORMULA_COMPILER_VERSION') ? FORMULA_COMPILER_VERSION : '1.0.0');

        if ($this->enabled && !is_dir($this->cacheDir)) {
            @mkdir($this->cacheDir, 0755, true);
        }
    }

    // -----------------------------------------------------------------------
    //  L1: Source Cache — formula text → compiled ID
    // -----------------------------------------------------------------------

    /**
     * Get the compiled formula ID for a given formula string.
     *
     * The source cache maps SHA-256(formula) → compiled formula ID.
     * This prevents recompiling identical formula strings.
     *
     * @param string $formula The formula source string
     * @return string|null The compiled formula ID, or null if not cached
     */
    public function getSourceId($formula)
    {
        if (!$this->enabled) {
            return null;
        }

        $key  = $this->sourceKey($formula);
        $data = $this->readFromFile($key);

        if ($data !== null && isset($data['id'])) {
            return $data['id'];
        }

        return null;
    }

    /**
     * Store a compiled formula ID for a formula string.
     *
     * @param string $formula The formula source
     * @param string $id      The compiled formula ID
     * @return void
     */
    public function setSourceId($formula, $id)
    {
        if (!$this->enabled) {
            return;
        }

        $key = $this->sourceKey($formula);

        $this->writeToFile($key, array(
            'id'               => (string)$id,
            'compilerVersion'  => $this->compilerVersion,
            'storedAt'         => time(),
        ));
    }

    /**
     * Compute the SHA-256 key for a formula string.
     *
     * @param string $formula
     * @return string
     */
    public function sourceKey($formula)
    {
        return 'src:' . hash('sha256', (string)$formula);
    }

    // -----------------------------------------------------------------------
    //  L2: Compiler Cache — compiled ID → serialized AST + metadata
    // -----------------------------------------------------------------------

    /**
     * Get the compiled AST and metadata for a formula ID.
     *
     * @param string $compiledId The compiled formula ID
     * @return array|null The compiled data (ast, metadata), or null if not cached
     */
    public function getCompiled($compiledId)
    {
        if (!$this->enabled) {
            return null;
        }

        $key  = 'compiled:' . (string)$compiledId;
        $data = $this->readFromFile($key);

        if ($data !== null
            && isset($data['compilerVersion'])
            && $data['compilerVersion'] === $this->compilerVersion) {
            return $data;
        }

        return null;
    }

    /**
     * Store compiled AST and metadata for a formula ID.
     *
     * @param string $compiledId The compiled formula ID
     * @param array  $data       Serialized AST + metadata
     * @return void
     */
    public function setCompiled($compiledId, array $data)
    {
        if (!$this->enabled) {
            return;
        }

        $data['compilerVersion'] = $this->compilerVersion;
        $key = 'compiled:' . (string)$compiledId;

        $this->writeToFile($key, $data);
    }

    /**
     * Invalidate all compiled formulas (on compiler version change).
     *
     * @return void
     */
    public function invalidateAllCompiled()
    {
        if (!$this->enabled) {
            return;
        }

        $files = glob($this->cacheDir . '/compiled_*.cache');
        if (is_array($files)) {
            foreach ($files as $file) {
                @unlink($file);
            }
        }
    }

    // -----------------------------------------------------------------------
    //  L3: Runtime Cache — formula + context → result (session-scoped)
    // -----------------------------------------------------------------------

    /**
     * Get a cached evaluation result.
     *
     * The runtime cache is session-scoped (in-memory) and maps
     * formula-hash + context-hash → result.
     *
     * @param string $formulaHash SHA-256 of formula source
     * @param string $contextHash  SHA-256 of serialized context
     * @return mixed|null The cached result, or null if not cached
     */
    public function getRuntimeResult($formulaHash, $contextHash)
    {
        if (!$this->enabled) {
            return null;
        }

        $key = 'result:' . $formulaHash . ':' . $contextHash;

        return isset($this->runtimeCache[$key]) ? $this->runtimeCache[$key] : null;
    }

    /**
     * Store an evaluation result in the runtime cache.
     *
     * @param string $formulaHash SHA-256 of formula source
     * @param string $contextHash  SHA-256 of serialized context
     * @param mixed  $result       The evaluation result (must be serializable)
     * @return void
     */
    public function setRuntimeResult($formulaHash, $contextHash, $result)
    {
        if (!$this->enabled) {
            return;
        }

        $key = 'result:' . $formulaHash . ':' . $contextHash;
        $this->runtimeCache[$key] = $result;
    }

    /**
     * Clear all runtime cache entries.
     *
     * Called at the end of each request.
     *
     * @return void
     */
    public function clearRuntimeCache()
    {
        $this->runtimeCache = array();
    }

    // -----------------------------------------------------------------------
    //  Cache Management
    // -----------------------------------------------------------------------

    /**
     * Check whether caching is enabled.
     *
     * @return bool
     */
    public function isEnabled()
    {
        return $this->enabled;
    }

    /**
     * Enable or disable caching.
     *
     * @param bool $enabled
     * @return void
     */
    public function setEnabled($enabled)
    {
        $this->enabled = (bool)$enabled;
    }

    /**
     * Clear ALL cache layers.
     *
     * Removes all L1, L2 (persistent) and L3 (in-memory) cache entries.
     *
     * @return void
     */
    public function clearAll()
    {
        if (!$this->enabled) {
            return;
        }

        // Clear persistent caches (L1 + L2)
        $files = glob($this->cacheDir . '/*.cache');
        if (is_array($files)) {
            foreach ($files as $file) {
                @unlink($file);
            }
        }

        // Clear runtime cache (L3)
        $this->clearRuntimeCache();
    }

    /**
     * Get cache statistics for monitoring.
     *
     * @return array
     */
    public function getStats()
    {
        $runtimeEntries = count($this->runtimeCache);
        $fileCount = 0;

        if (is_dir($this->cacheDir)) {
            $files = glob($this->cacheDir . '/*.cache');
            $fileCount = is_array($files) ? count($files) : 0;
        }

        return array(
            'enabled'         => $this->enabled,
            'runtimeEntries'  => $runtimeEntries,
            'persistentFiles' => $fileCount,
            'cacheDir'        => $this->cacheDir,
        );
    }

    // -----------------------------------------------------------------------
    //  File Storage Helpers
    // -----------------------------------------------------------------------

    /**
     * Read a value from a cache file.
     *
     * @param string $key The cache key
     * @return array|null The unserialized data, or null if not found/expired
     */
    private function readFromFile($key)
    {
        $safeKey = $this->safeFileName($key);
        $path    = $this->cacheDir . '/' . $safeKey . '.cache';

        if (!file_exists($path)) {
            return null;
        }

        $content = @file_get_contents($path);
        if ($content === false) {
            return null;
        }

        $data = @unserialize($content);
        if (!is_array($data)) {
            @unlink($path);
            return null;
        }

        // Check TTL
        if (isset($data['_ttl']) && $data['_ttl'] > 0) {
            $storedAt = isset($data['_storedAt']) ? (int)$data['_storedAt'] : 0;
            if (time() - $storedAt > $data['_ttl']) {
                @unlink($path);
                return null;
            }
        }

        unset($data['_ttl'], $data['_storedAt']);
        return $data;
    }

    /**
     * Write a value to a cache file.
     *
     * @param string   $key  The cache key
     * @param array    $data The data to store
     * @param int|null $ttl  TTL in seconds, null for infinite
     * @return void
     */
    private function writeToFile($key, array $data, $ttl = null)
    {
        $safeKey = $this->safeFileName($key);
        $path    = $this->cacheDir . '/' . $safeKey . '.cache';

        $data['_storedAt'] = time();
        if ($ttl !== null) {
            $data['_ttl'] = (int)$ttl;
        }

        $serialized = serialize($data);
        @file_put_contents($path, $serialized, LOCK_EX);
    }

    /**
     * Convert a cache key to a safe file name.
     *
     * @param string $key
     * @return string
     */
    private function safeFileName($key)
    {
        // Replace characters unsafe for filenames
        $safe = preg_replace('/[^a-zA-Z0-9_\-:.]/', '_', $key);
        return $safe;
    }
}
