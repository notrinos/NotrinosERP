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
 * ManufacturingTemplateProvider — built-in formula templates for Manufacturing/BOM.
 *
 * @package FormulaDesigner\Templates
 * @since   2.0.0
 */
class FormulaDesigner_Templates_ManufacturingTemplateProvider
    implements FormulaDesigner_Contracts_DesignerTemplateInterface
{
    /** @var array */
    private $data = array();

    /**
     * @param array $data
     */
    public function __construct(array $data = array())
    {
        $this->data = $data;
    }

    /** @return string */
    public function getId()
    {
        return isset($this->data['id']) ? (string)$this->data['id'] : '';
    }

    /** @return string */
    public function getFormula()
    {
        return isset($this->data['formula']) ? (string)$this->data['formula'] : '';
    }

    /** @return array */
    public function getMetadata()
    {
        return array(
            'id'          => $this->getId(),
            'module'      => isset($this->data['module']) ? (string)$this->data['module'] : 'manufacturing',
            'label'       => isset($this->data['label']) ? (string)$this->data['label'] : '',
            'description' => isset($this->data['description']) ? (string)$this->data['description'] : '',
            'formula'     => $this->getFormula(),
            'category'    => isset($this->data['category']) ? (string)$this->data['category'] : 'Manufacturing',
            'tags'        => isset($this->data['tags']) && is_array($this->data['tags']) ? $this->data['tags'] : array(),
            'difficulty'  => isset($this->data['difficulty']) ? (string)$this->data['difficulty'] : 'beginner',
        );
    }

    /**
     * Return all 5 manufacturing/BOM formula templates.
     *
     * @return FormulaDesigner_Templates_ManufacturingTemplateProvider[]
     */
    public static function all()
    {
        return array(
            // 1. BOM Component Cost
            new self(array(
                'id'          => 'manufacturing.bom_component_cost',
                'module'      => 'manufacturing',
                'label'       => 'BOM Component Total Cost',
                'description' => 'Calculate total cost of a BOM component: quantity × unit cost.',
                'formula'     => 'ROUND(BOM.Qty * BOM.Cost, 2)',
                'category'    => 'Costing',
                'tags'        => array('bom', 'component', 'cost', 'quantity'),
                'difficulty'  => 'beginner',
            )),

            // 2. Total BOM Cost with Overhead
            new self(array(
                'id'          => 'manufacturing.bom_total_with_overhead',
                'module'      => 'manufacturing',
                'label'       => 'Total BOM Cost (with Overhead)',
                'description' => 'Total BOM cost including overhead percentage.',
                'formula'     => 'ROUND(BOM.TotalCost * (1 + BOM.OverheadPct / 100), 2)',
                'category'    => 'Costing',
                'tags'        => array('bom', 'total', 'overhead', 'cost'),
                'difficulty'  => 'intermediate',
            )),

            // 3. Work Order Completion Percentage
            new self(array(
                'id'          => 'manufacturing.work_order_completion',
                'module'      => 'manufacturing',
                'label'       => 'Work Order Completion %',
                'description' => 'Calculate percentage of work order completed.',
                'formula'     => 'ROUND(Production.QtyCompleted / Production.QtyOrdered * 100, 2)',
                'category'    => 'Production',
                'tags'        => array('work-order', 'completion', 'percentage'),
                'difficulty'  => 'beginner',
            )),

            // 4. Routing Total Time
            new self(array(
                'id'          => 'manufacturing.routing_total_time',
                'module'      => 'manufacturing',
                'label'       => 'Routing Total Time',
                'description' => 'Calculate total production time from routing setup + run time.',
                'formula'     => 'Routing.SetupTime + Routing.RunTime * Production.QtyOrdered',
                'category'    => 'Production',
                'tags'        => array('routing', 'time', 'setup', 'run'),
                'difficulty'  => 'beginner',
            )),

            // 5. Production Cost Variance
            new self(array(
                'id'          => 'manufacturing.production_cost_variance',
                'module'      => 'manufacturing',
                'label'       => 'Production Cost Variance',
                'description' => 'Calculate variance between standard and actual production cost.',
                'formula'     => 'ROUND(Production.ActualCost - Production.StdCost, 2)',
                'category'    => 'Costing',
                'tags'        => array('variance', 'cost', 'standard', 'actual', 'production'),
                'difficulty'  => 'intermediate',
            )),
        );
    }
}
