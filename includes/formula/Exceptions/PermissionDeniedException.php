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
 * PermissionDeniedException — Insufficient security permissions.
 *
 * Thrown when a formula attempts to execute a function or resolve
 * a variable that requires a security permission the current user
 * does not have. Integrates with NotrinosERP's SA_* security constant
 * system.
 *
 * @package Formula\Exceptions
 * @since   2.0.0
 */
class Formula_Exceptions_PermissionDeniedException extends Formula_Exceptions_FormulaException
{
    /** @var string The required SA_* permission */
    protected $requiredPermission;

    /** @var string The function or variable name that was denied */
    protected $resourceName;

    /**
     * @param string $message            Error message
     * @param string $requiredPermission The SA_* permission required
     * @param string $resourceName       The function/variable name that was denied
     * @param int    $line               Source line
     * @param int    $column             Source column
     */
    public function __construct($message, $requiredPermission, $resourceName, $line = 0, $column = 0)
    {
        parent::__construct($message, $line, $column);
        $this->requiredPermission = (string)$requiredPermission;
        $this->resourceName       = (string)$resourceName;
    }

    /**
     * @return string
     */
    public function getRequiredPermission()
    {
        return $this->requiredPermission;
    }

    /**
     * @return string
     */
    public function getResourceName()
    {
        return $this->resourceName;
    }
}
