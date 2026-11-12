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
 * DIM-LFC-002 historical Dimension access.
 *
 * Historical filters, searches and reports must be able to name every master that ever classified a
 * stored transaction, including a closed master and the retained identity of a master deleted by
 * DIM-LFC-001C. This is a read-only projection: it grants no edit, reopen or new-posting authority.
 * The current company is the connection's own database/prefix, so the projection is company-scoped.
 */

/**
 * Build the explicit historical label for one master or for a retained/deleted identity.
 *
 * @param array|false $row Master row with reference/name/closed, or false when the identity no longer exists
 * @param int|string $id Exact stored native Dimension identity
 * @return string Label naming the state explicitly; never blank for a positive identity
 */
function dimension_history_label($row, $id) {
	$id = (int)$id;
	if (!is_array($row))
		return sprintf(_('Unknown Dimension #%d (deleted)'), $id);
	$label = trim((string)$row['reference'].'  '.(string)$row['name']);
	if ($label === '')
		$label = sprintf(_('Dimension #%d'), $id);
	if ((int)$row['closed'] === 1)
		$label .= ' ['._('closed').']';
	return $label;
}

/**
 * Return every historical master of the current company plus the exact selected retained identity.
 *
 * Closed and open masters of both axes are included regardless of the posting preference, because the
 * preference governs new events only. A selected identity whose master row no longer exists (a
 * legitimately deleted historical Dimension) is added explicitly instead of being replaced by the
 * blank/all option, so a drilldown keeps its original numeric ID.
 *
 * @param int|string $selected_id Selected stored identity, including a deleted one
 * @param bool $escape Escape option text for HTML rendering
 * @return array Native ID => explicit historical label
 */
function dimension_history_options($selected_id = 0, $escape = false) {
	$options = array();
	$result = db_query("SELECT id, reference, name, closed FROM ".TB_PREF."dimensions ORDER BY reference, id",
		'The historical dimensions could not be retrieved');
	while ($row = db_fetch($result)) {
		$id = (int)$row['id'];
		$label = dimension_history_label($row, $id);
		$options[$id] = $escape ? htmlspecialchars($label, ENT_QUOTES, 'UTF-8') : $label;
	}
	$selected_id = is_numeric($selected_id) && (int)$selected_id > 0 ? (int)$selected_id : 0;
	if ($selected_id && !isset($options[$selected_id])) {
		$label = dimension_history_label(false, $selected_id);
		$options[$selected_id] = $escape ? htmlspecialchars($label, ENT_QUOTES, 'UTF-8') : $label;
	}
	return $options;
}
