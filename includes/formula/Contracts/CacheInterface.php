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
 * CacheInterface — Contract for formula framework caching layers.
 *
 * All cache layers (Source Cache, Compiler Cache, Runtime Cache,
 * Function Cache) implement this interface. Implementations may
 * use file-based storage, APCu, or any other PSR-16-compatible
 * backend.
 *
 * Keys are SHA-256 hash strings. Values are serialized PHP data.
 *
 * @package Formula\Contracts
 * @since   2.0.0
 */
interface Formula_Contracts_CacheInterface
{
    /**
     * Retrieve a cached value by key.
     *
     * @param string $key     The cache key (SHA-256 hash)
     * @param mixed  $default Value to return if key not found
     * @return mixed The cached value or $default
     */
    public function get($key, $default = null);

    /**
     * Store a value in the cache.
     *
     * @param string   $key   The cache key
     * @param mixed    $value The value to store (must be serializable)
     * @param int|null $ttl   Time-to-live in seconds, null for infinite
     * @return bool True on success
     */
    public function set($key, $value, $ttl = null);

    /**
     * Check whether a key exists in the cache.
     *
     * @param string $key The cache key
     * @return bool True if the key exists and has not expired
     */
    public function has($key);

    /**
     * Delete a specific key from the cache.
     *
     * @param string $key The cache key
     * @return bool True if the key was deleted
     */
    public function invalidate($key);

    /**
     * Clear all cached entries.
     *
     * @return bool True on success
     */
    public function clear();
}
