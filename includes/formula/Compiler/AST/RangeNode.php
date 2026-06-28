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
 * RangeNode — AST node for cell range references (e.g., A1:B10).
 *
 * Used by the Report Builder module for spreadsheet formula evaluation.
 * The range is resolved against the spreadsheet data grid at runtime.
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
class Formula_Compiler_AST_RangeNode extends Formula_Compiler_AST_Node
{
    /** @var string Start cell reference (e.g., 'A1') */
    public $startCell;

    /** @var string End cell reference (e.g., 'B10') */
    public $endCell;

    /** @var string|null Sheet name for cross-sheet references (e.g., 'Sheet1') */
    public $sheetName;

    public function __construct($startCell, $endCell, $sheetName = null, $line = 0, $column = 0)
    {
        parent::__construct($line, $column);
        $this->startCell = (string)$startCell;
        $this->endCell   = (string)$endCell;
        $this->sheetName = $sheetName !== null ? (string)$sheetName : null;
    }

    public function accept(Formula_Compiler_AST_NodeVisitor $visitor) { return $visitor->visitRange($this); }
    public function getChildren() { return array(); }

    public function serialize()
    {
        $data = parent::serialize();
        $data['startCell'] = $this->startCell;
        $data['endCell']   = $this->endCell;
        $data['sheetName'] = $this->sheetName;
        return $data;
    }
}
