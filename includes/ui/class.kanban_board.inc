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
 * NotrinosERP Core Kanban Board Renderer.
 *
 * Provides a generic, extensible kanban/pipeline board that modules can
 * subclass to render their own boards (CRM pipeline, Project tasks, etc.).
 *
 * CSS lives in the active theme (themes/<theme>/css/kanban.css).  This class
 * only emits structural HTML with well-defined CSS class hooks.
 *
 * Extension points for subclasses:
 *   - Override render_column_header()  to customise column headings
 *   - Override render_card()           to customise card markup
 *   - Override render_summary()        to add a summary bar
 *   - Override get_card_url()          to make cards clickable links
 *   - Override get_card_data_attributes() to attach data-* for JS
 *   - Override get_column_data_attributes() for column-level data-*
 *
 * Hook integration:
 *   The board fires hook_invoke_all() at key render points so that
 *   installed extensions can inject extra markup or modify behaviour
 *   without subclassing.
 *
 * @package  NotrinosERP
 * @category UI
 */
class kanban_board {

	/** @var string Unique board identifier (used in HTML id attributes) */
	protected $board_id;

	/** @var string CSS class prefix applied to all board elements */
	protected $css_prefix;

	/**
	 * Ordered array of columns.
	 * Each entry: array('id'=>…, 'title'=>…, 'meta'=>array(…))
	 * @var array
	 */
	protected $columns = array();

	/**
	 * Cards grouped by column id.
	 * Structure: array( column_id => array( card, card, … ) )
	 * @var array
	 */
	protected $cards = array();

	/** @var array Board-level options (passed by the caller) */
	protected $options = array();

	// ------------------------------------------------------------------
	//  Construction
	// ------------------------------------------------------------------

	/**
	 * @param string $board_id  Unique board identifier.
	 * @param array  $options   Board-level options.
	 *   Recognised keys (all optional):
	 *     'css_prefix'    string  CSS class prefix (default: 'kanban')
	 *     'summary'       bool    Render the summary bar (default: false)
	 *     'draggable'     bool    Add drag-drop data attributes (default: false)
	 *     'board_class'   string  Extra CSS classes on the board wrapper
	 *     'column_class'  string  Extra CSS classes on every column
	 *     'card_class'    string  Extra CSS classes on every card
	 */
	function __construct($board_id, $options = array()) {
		$this->board_id   = $board_id;
		$this->css_prefix = isset($options['css_prefix']) ? $options['css_prefix'] : 'kanban';
		$this->options    = $options;
	}

	// ------------------------------------------------------------------
	//  Data population (call before render())
	// ------------------------------------------------------------------

	/**
	 * Register a column (rendered in add order).
	 *
	 * @param mixed  $id    Column identifier (int or string).
	 * @param string $title Display title.
	 * @param array  $meta  Arbitrary metadata available inside render methods.
	 * @return $this
	 */
	function add_column($id, $title, $meta = array()) {
		$this->columns[$id] = array(
			'id'    => $id,
			'title' => $title,
			'meta'  => $meta,
		);
		if (!isset($this->cards[$id])) {
			$this->cards[$id] = array();
		}
		return $this;
	}

	/**
	 * Assign cards to a column.
	 *
	 * @param mixed $column_id  Must match a previously-added column.
	 * @param array $cards      Array of card data (associative arrays).
	 * @return $this
	 */
	function set_cards($column_id, $cards) {
		$this->cards[$column_id] = $cards;
		return $this;
	}

	/**
	 * Append a single card to a column.
	 *
	 * @param mixed $column_id
	 * @param array $card
	 * @return $this
	 */
	function add_card($column_id, $card) {
		$this->cards[$column_id][] = $card;
		return $this;
	}

	// ------------------------------------------------------------------
	//  Main render entry point
	// ------------------------------------------------------------------

	/**
	 * Render the full kanban board (board wrapper + columns + cards + summary).
	 *
	 * @return void  Output is echo'd directly.
	 */
	function render() {
		$this->invoke_hook('kanban_before_render', $this->board_id);

		$this->render_board_start();

		foreach ($this->columns as $column_id => $column) {
			$column_cards = isset($this->cards[$column_id]) ? $this->cards[$column_id] : array();
			$this->render_column($column_id, $column, $column_cards);
		}

		$this->render_board_end();

		if ($this->get_option('summary', false)) {
			$this->render_summary();
		}

		$this->invoke_hook('kanban_after_render', $this->board_id);
	}

	// ------------------------------------------------------------------
	//  Board wrapper
	// ------------------------------------------------------------------

	/**
	 * Render the opening board wrapper div.
	 */
	protected function render_board_start() {
		$classes = $this->css_prefix . '-board';
		if ($extra = $this->get_option('board_class', '')) {
			$classes .= ' ' . $extra;
		}
		echo "<div id='" . htmlspecialchars($this->board_id) . "' class='" . htmlspecialchars($classes) . "'>";
	}

	/**
	 * Render the closing board wrapper div.
	 */
	protected function render_board_end() {
		echo "</div>"; // board
	}

	// ------------------------------------------------------------------
	//  Column rendering
	// ------------------------------------------------------------------

	/**
	 * Render a single column (header + body with cards).
	 *
	 * @param mixed $column_id
	 * @param array $column     Column definition from add_column().
	 * @param array $cards      Cards belonging to this column.
	 */
	protected function render_column($column_id, $column, $cards) {
		$classes = $this->css_prefix . '-column';
		if ($extra = $this->get_option('column_class', '')) {
			$classes .= ' ' . $extra;
		}

		$data_attrs = $this->render_data_attributes(
			$this->get_column_data_attributes($column_id, $column)
		);

		echo "<div class='" . htmlspecialchars($classes) . "'" . $data_attrs . ">";

		$this->render_column_header($column_id, $column, $cards);
		$this->render_column_body($column_id, $column, $cards);

		echo "</div>"; // column
	}

	/**
	 * Render the column header.
	 *
	 * Override in subclass to customise heading, counters, totals, etc.
	 *
	 * @param mixed $column_id
	 * @param array $column
	 * @param array $cards
	 */
	protected function render_column_header($column_id, $column, $cards) {
		$count = count($cards);
		echo "<div class='" . $this->css_prefix . "-column-header'>";
		echo htmlspecialchars($column['title']);
		echo " <span class='" . $this->css_prefix . "-column-count'>(" . $count . ")</span>";
		echo "</div>";
	}

	/**
	 * Render the column body (the card list area).
	 *
	 * @param mixed $column_id
	 * @param array $column
	 * @param array $cards
	 */
	protected function render_column_body($column_id, $column, $cards) {
		echo "<div class='" . $this->css_prefix . "-column-body'>";
		foreach ($cards as $card) {
			$this->render_card($card, $column_id);
		}
		echo "</div>";
	}

	// ------------------------------------------------------------------
	//  Card rendering
	// ------------------------------------------------------------------

	/**
	 * Render a single card.
	 *
	 * Override in subclass to build module-specific card markup.
	 *
	 * @param array $card       Associative array of card data.
	 * @param mixed $column_id  The column this card belongs to.
	 */
	protected function render_card($card, $column_id) {
		$classes = $this->css_prefix . '-card';
		if ($extra = $this->get_option('card_class', '')) {
			$classes .= ' ' . $extra;
		}

		$data_attrs = $this->render_data_attributes(
			$this->get_card_data_attributes($card, $column_id)
		);

		$url = $this->get_card_url($card, $column_id);
		$onclick = '';
		if ($url) {
			$onclick = " onclick=\"window.location='" . htmlspecialchars($url, ENT_QUOTES) . "'\"";
			$classes .= ' ' . $this->css_prefix . '-card-clickable';
		}

		echo "<div class='" . htmlspecialchars($classes) . "'" . $data_attrs . $onclick . ">";
		$this->render_card_content($card, $column_id);
		echo "</div>";
	}

	/**
	 * Render the inner content of a card.
	 *
	 * Override in subclass to provide module-specific card content.
	 *
	 * @param array $card
	 * @param mixed $column_id
	 */
	protected function render_card_content($card, $column_id) {
		$title = isset($card['title']) ? $card['title'] : '';
		echo "<div class='" . $this->css_prefix . "-card-title'>" . htmlspecialchars($title) . "</div>";
	}

	/**
	 * Return the URL a card should navigate to on click, or empty string.
	 *
	 * Override in subclass to provide navigation.
	 *
	 * @param array $card
	 * @param mixed $column_id
	 * @return string
	 */
	protected function get_card_url($card, $column_id) {
		return '';
	}

	/**
	 * Return data-* attributes for a card element.
	 *
	 * @param array $card
	 * @param mixed $column_id
	 * @return array  key=>value pairs (keys without 'data-' prefix).
	 */
	protected function get_card_data_attributes($card, $column_id) {
		$attrs = array();
		if (isset($card['id'])) {
			$attrs['card-id'] = $card['id'];
		}
		if ($this->get_option('draggable', false)) {
			$attrs['draggable'] = 'true';
		}
		return $attrs;
	}

	/**
	 * Return data-* attributes for a column element.
	 *
	 * @param mixed $column_id
	 * @param array $column
	 * @return array  key=>value pairs (keys without 'data-' prefix).
	 */
	protected function get_column_data_attributes($column_id, $column) {
		return array('column-id' => $column_id);
	}

	// ------------------------------------------------------------------
	//  Summary bar
	// ------------------------------------------------------------------

	/**
	 * Render a summary bar below the board.
	 *
	 * Override in subclass to show totals, weighted values, counts, etc.
	 */
	protected function render_summary() {
		$total_cards = 0;
		foreach ($this->cards as $column_cards) {
			$total_cards += count($column_cards);
		}
		echo "<div class='" . $this->css_prefix . "-summary'>";
		echo "<strong>" . _('Summary') . ":</strong> ";
		echo _('Total items') . ": " . $total_cards;
		echo "</div>";
	}

	// ------------------------------------------------------------------
	//  Helpers
	// ------------------------------------------------------------------

	/**
	 * Get a board option with a default fallback.
	 *
	 * @param string $key
	 * @param mixed  $default
	 * @return mixed
	 */
	protected function get_option($key, $default = null) {
		return isset($this->options[$key]) ? $this->options[$key] : $default;
	}

	/**
	 * Return all registered columns.
	 *
	 * @return array
	 */
	function get_columns() {
		return $this->columns;
	}

	/**
	 * Return all cards (grouped by column id).
	 *
	 * @return array
	 */
	function get_cards() {
		return $this->cards;
	}

	/**
	 * Return the board id.
	 *
	 * @return string
	 */
	function get_board_id() {
		return $this->board_id;
	}

	/**
	 * Build a data-attributes string from an associative array.
	 *
	 * @param array $attrs  e.g. array('stage-id'=>5) => ' data-stage-id="5"'
	 * @return string       Ready to insert into an HTML tag (leading space included).
	 */
	protected function render_data_attributes($attrs) {
		if (empty($attrs)) {
			return '';
		}
		$html = '';
		foreach ($attrs as $key => $value) {
			$html .= ' data-' . htmlspecialchars($key) . '="' . htmlspecialchars($value) . '"';
		}
		return $html;
	}

	/**
	 * Fire a hook via the global hook system so extensions can react.
	 *
	 * @param string $hook_name  Hook method name.
	 * @param mixed  $data       Data passed by reference to hook handlers.
	 */
	protected function invoke_hook($hook_name, &$data) {
		if (function_exists('hook_invoke_all')) {
			hook_invoke_all($hook_name, $data);
		}
	}
}
