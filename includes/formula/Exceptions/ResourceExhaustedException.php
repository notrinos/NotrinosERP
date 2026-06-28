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
 * ResourceExhaustedException — A resource limit was exceeded during execution.
 *
 * Thrown when formula compilation or execution exceeds configured limits
 * for source length, AST depth, node evaluation count, execution time,
 * or memory consumption. This is a security measure to prevent denial-of-service.
 *
 * @package Formula\Exceptions
 * @since   2.0.0
 */
class Formula_Exceptions_ResourceExhaustedException extends Formula_Exceptions_FormulaException
{
    /** @var string The type of resource that was exhausted */
    protected $resourceType;

    /** @var int The limit that was exceeded */
    protected $limit;

    /**
     * @param string $message      Error message
     * @param string $resourceType Resource type (source_length, ast_depth,
     *                              node_count, exec_time, memory)
     * @param int    $limit        The limit value that was exceeded
     * @param int    $line         Source line (0 if not applicable)
     * @param int    $column       Source column (0 if not applicable)
     */
    public function __construct($message, $resourceType, $limit, $line = 0, $column = 0)
    {
        parent::__construct($message, $line, $column);
        $this->resourceType = (string)$resourceType;
        $this->limit        = (int)$limit;
    }

    /**
     * @return string
     */
    public function getResourceType()
    {
        return $this->resourceType;
    }

    /**
     * @return int
     */
    public function getLimit()
    {
        return $this->limit;
    }
}
