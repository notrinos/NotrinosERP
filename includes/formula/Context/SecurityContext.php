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
 * SecurityContext — Immutable security context for formula execution.
 *
 * Carries the current user's security permissions and allowed companies.
 * Every function and variable provider checks this context before executing
 * restricted operations. The context is assembled by the calling module
 * and is read-only during formula evaluation.
 *
 * Integrates with the existing NotrinosERP security section system
 * (defined in includes/access_levels.inc) through SA_* constants.
 *
 * @package Formula\Context
 * @since   2.0.0
 */
class Formula_Context_SecurityContext
{
    /** @var int Current user ID */
    private $userId;

    /** @var string[] Array of SA_* security areas the user has access to */
    private $permissions;

    /** @var int[] Array of company IDs the user may access */
    private $allowedCompanies;

    /**
     * Construct an immutable security context.
     *
     * @param int      $userId           Current user database ID
     * @param string[] $permissions      Array of SA_* security area identifiers
     * @param int[]    $allowedCompanies Array of company database IDs
     */
    public function __construct($userId, array $permissions = array(), array $allowedCompanies = array())
    {
        $this->userId           = (int)$userId;
        $this->permissions      = $permissions;
        $this->allowedCompanies = $allowedCompanies;
    }

    /**
     * Check whether the current user has a specific security permission.
     *
     * @param string $securityArea The SA_* security area to check (e.g., 'SA_GL_INQUIRY')
     * @return bool True if the user has the permission
     */
    public function hasPermission($securityArea)
    {
        return in_array((string)$securityArea, $this->permissions, true);
    }

    /**
     * Get the current user ID.
     *
     * @return int
     */
    public function getUserId()
    {
        return $this->userId;
    }

    /**
     * Get all permissions the user holds.
     *
     * @return string[]
     */
    public function getPermissions()
    {
        return $this->permissions;
    }

    /**
     * Check whether a company is accessible by the current user.
     *
     * @param int $companyId The company database ID
     * @return bool True if the user may access this company
     */
    public function isCompanyAllowed($companyId)
    {
        return in_array((int)$companyId, $this->allowedCompanies, true);
    }

    /**
     * Get all company IDs the user may access.
     *
     * @return int[]
     */
    public function getAllowedCompanies()
    {
        return $this->allowedCompanies;
    }
}
