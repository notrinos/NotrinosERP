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
 * DIM-NAM-003: the name of a tracking category wherever a form, view or report shows an axis.
 *
 * The naming plan's company label rule: show the CFG-001 company label of the axis (`dimension_category_policy_label()`), and the
 * neutral "Tracking category 1/2" of the term source only when the company has set none. This file is the one place forms, views and
 * report parameters ask for it, in two forms: plain text (PDF, Excel, a value another function escapes) and HTML-escaped (a label echoed
 * into a page, because a company label is company data). Two categories never show the same name: when the company gave both the same
 * label, category 2 is shown with "(2)", so a parameter list keyed by its label keeps both entries.
 *
 * The policy file is loaded on first use, so a page pays for it only when it shows an axis. No write, no output. PHP 5.6 compatible.
 */
include_once(dirname(__FILE__).'/tracking_terms.inc');

/**
 * Plain-text name of one tracking category in the active company.
 *
 * @param int $axis 1 or 2; any other value gives the generic term "Tracking category".
 * @return string Company label, or the neutral label when none is published.
 */
function dimension_axis_label($axis)
{
	$axis = (int)$axis;
	if ($axis !== 1 && $axis !== 2)
		return dimension_term_category();
	if (!function_exists('dimension_category_policy_label'))
		include_once(dirname(__FILE__).'/tracking_category_policy.inc');
	$label = dimension_category_policy_label($axis);
	if ($axis === 2) {
		$other = dimension_category_policy_label(1);
		$same = function_exists('mb_strtolower') ? mb_strtolower($label, 'UTF-8') === mb_strtolower($other, 'UTF-8') : strtolower($label) === strtolower($other);
		if ($same)
			$label .= ' (2)';
	}
	return $label;
}

/**
 * HTML-escaped name of one tracking category, for a label echoed into a page.
 *
 * @param int $axis 1 or 2.
 * @param string $suffix Literal markup appended after the escaped name, such as ':'.
 * @return string
 */
function dimension_axis_label_html($axis, $suffix = '')
{
	return htmlspecialchars(dimension_axis_label($axis), ENT_QUOTES, 'UTF-8').$suffix;
}
