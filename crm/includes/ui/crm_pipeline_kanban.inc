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
 * CRM Pipeline Kanban — extends the core kanban_board for opportunity pipeline.
 *
 * Renders opportunities grouped by sales stage with revenue totals,
 * probability percentages, priority badges, and close dates.
 *
 * @package  NotrinosERP
 * @subpackage CRM
 */
class crm_pipeline_kanban extends kanban_board {

	/** @var string Base path to application root */
	protected $path_to_root;

	/** @var float Running pipeline total */
	protected $total_pipeline = 0;

	/** @var float Running weighted pipeline total */
	protected $weighted_pipeline = 0;

	/**
	 * @param string $path_to_root  Application root path for building URLs.
	 * @param array  $options       Extra board options (passed to parent).
	 */
	function __construct($path_to_root, $options = array()) {
		$defaults = array(
			'css_prefix' => 'kanban',
			'summary'    => true,
			'draggable'  => true,
		);
		$options = array_merge($defaults, $options);
		parent::__construct('crm-pipeline-board', $options);
		$this->path_to_root = $path_to_root;
	}

	/**
	 * Populate the board from stage and opportunity data.
	 *
	 * @param array $stages        Array of stage rows keyed by stage id.
	 * @param array $opps_by_stage Opportunities grouped by stage id.
	 */
	function load_pipeline_data($stages, $opps_by_stage) {
		$this->total_pipeline    = 0;
		$this->weighted_pipeline = 0;

		foreach ($stages as $sid => $stage) {
			$cards = isset($opps_by_stage[$sid]) ? $opps_by_stage[$sid] : array();

			$stage_total = 0;
			foreach ($cards as $opp) {
				$stage_total += (float)$opp['expected_revenue'];
			}
			$this->total_pipeline    += $stage_total;
			$this->weighted_pipeline += $stage_total * ((int)$stage['probability'] / 100);

			$this->add_column($sid, $stage['name'], array(
				'probability'  => (int)$stage['probability'],
				'stage_total'  => $stage_total,
			));
			$this->set_cards($sid, $cards);
		}
	}

	/**
	 * Get the pipeline total.
	 *
	 * @return float
	 */
	function get_total_pipeline() {
		return $this->total_pipeline;
	}

	/**
	 * Get the weighted pipeline total.
	 *
	 * @return float
	 */
	function get_weighted_pipeline() {
		return $this->weighted_pipeline;
	}

	// ------------------------------------------------------------------
	//  Column overrides
	// ------------------------------------------------------------------

	/** {@inheritdoc} */
	protected function render_column_header($column_id, $column, $cards) {
		$count = count($cards);
		$meta  = $column['meta'];

		echo "<div class='" . $this->css_prefix . "-column-header'>";
		echo htmlspecialchars($column['title']);
		echo " <span class='" . $this->css_prefix . "-column-count'>(" . $count . ")</span>";
		echo "</div>";

		echo "<div class='" . $this->css_prefix . "-column-meta'>";
		echo _('Total') . ': ' . price_format($meta['stage_total']);
		echo " &middot; " . $meta['probability'] . '%';
		echo "</div>";
	}

	/** {@inheritdoc} */
	protected function get_column_data_attributes($column_id, $column) {
		return array('stage-id' => (int)$column_id);
	}

	// ------------------------------------------------------------------
	//  Card overrides
	// ------------------------------------------------------------------

	/** {@inheritdoc} */
	protected function render_card_content($card, $column_id) {
		echo "<div class='" . $this->css_prefix . "-card-title'>" . htmlspecialchars($card['title']) . "</div>";

		if (!empty($card['company_name'])) {
			echo "<div class='" . $this->css_prefix . "-card-subtitle'>" . htmlspecialchars($card['company_name']) . "</div>";
		}

		echo "<div class='" . $this->css_prefix . "-card-meta'>";
		echo price_format($card['expected_revenue']);
		echo " &nbsp; ";
		echo crm_priority_badge($card['priority']);
		echo "</div>";

		if (!empty($card['expected_close_date'])) {
			echo "<div class='" . $this->css_prefix . "-card-footer'>" . _('Close') . ': ' . sql2date($card['expected_close_date']) . "</div>";
		}
	}

	/** {@inheritdoc} */
	protected function get_card_url($card, $column_id) {
		return $this->path_to_root . '/crm/transactions/opportunity_entry.php?LeadID=' . (int)$card['id'] . crm_sel_app_param();
	}

	/** {@inheritdoc} */
	protected function get_card_data_attributes($card, $column_id) {
		$attrs = parent::get_card_data_attributes($card, $column_id);
		$attrs['lead-id'] = (int)$card['id'];
		return $attrs;
	}

	// ------------------------------------------------------------------
	//  Summary override
	// ------------------------------------------------------------------

	/** {@inheritdoc} */
	protected function render_summary() {
		$total_opps = 0;
		foreach ($this->cards as $column_cards) {
			$total_opps += count($column_cards);
		}

		echo "<div class='" . $this->css_prefix . "-summary'>";
		echo "<strong>" . _('Pipeline Summary') . ":</strong> ";
		echo _('Total') . ": " . price_format($this->total_pipeline);
		echo " &nbsp;|&nbsp; ";
		echo _('Weighted') . ": " . price_format($this->weighted_pipeline);
		echo " &nbsp;|&nbsp; ";
		echo _('Opportunities') . ": " . $total_opps;
		echo "</div>";
	}
}
