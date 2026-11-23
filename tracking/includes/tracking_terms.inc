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
 * DIM-NAM-001: the one source of the user-facing module vocabulary (Cost & Profit Tracking naming plan, section 2).
 *
 * Every term is a literal `_()` call so gettext extracts it and a translator sees it once; a later rename edits this file only. The
 * internal names do not change: application id `proj`, `ST_DIMENSION`, `TAG_DIMENSION`, every `SA_*` area, report IDs 501/502/707, URLs,
 * tables and columns. Where an axis is shown, the company label of DIM-CFG-001 (`dimension_category_policy_label()`) wins; the neutral
 * labels here are its fallback. HRM keeps "Cost Center" for axis 1.
 *
 * Pure functions: no database access, no output, no side effect. PHP 5.6 compatible syntax.
 */

/**
 * The module name shown in page titles, report headers, the guide and the security section.
 *
 * @return string
 */
function dimension_term_module_name()
{
    return _('Cost & Profit Tracking');
}

/**
 * The application tab label with its hotkey marker (T: P is Purchases and C is CRM). `access_string()` underlines the letter after the
 * last ampersand that is followed by a letter or digit, so the ampersand of "Cost & Profit" stays text.
 *
 * @return string
 */
function dimension_term_module_tab()
{
    return _('Cost & Profit &Tracking');
}

/**
 * The name of what a company tracks (one axis), singular.
 *
 * @return string
 */
function dimension_term_category()
{
    return _('Tracking category');
}

/**
 * The name of what a company tracks (the axes), plural.
 *
 * @return string
 */
function dimension_term_categories()
{
    return _('Tracking categories');
}

/**
 * The neutral name of one entry in a category (one dimension record), singular.
 *
 * @return string
 */
function dimension_term_value()
{
    return _('Tracking value');
}

/**
 * The neutral name of entries in a category, plural.
 *
 * @return string
 */
function dimension_term_values()
{
    return _('Tracking values');
}

/**
 * The neutral label of one axis, used only when the company has set no label of its own.
 *
 * @param int $axis 1 or 2; any other value is treated as axis 1.
 * @return string
 */
function dimension_term_neutral_category_label($axis)
{
    if ((int)$axis === 2)
        return _('Tracking category 2');
    return _('Tracking category 1');
}

/**
 * The one-release notice that lets existing users find the renamed module.
 *
 * @return string
 */
function dimension_term_former_name_notice()
{
    return _('Formerly Dimensions.');
}

/**
 * The help sentence for accountants who know the concept under another name.
 *
 * @return string
 */
function dimension_term_also_known_as()
{
    return _('Also known as dimensions, analytic accounts or cost centres.');
}

/**
 * The page and report names of the label map (naming plan section 2), keyed by a stable id, without hotkey markers.
 *
 * Menu entries add their own hotkey marker when they adopt a name (DIM-NAM-002).
 *
 * @return array<string,string>
 */
function dimension_term_page_names()
{
    return array(
        'overview' => _('Overview'),
        'user_guide' => _('User Guide'),
        'getting_started' => _('Getting Started'),
        'explorer' => _('Cost & Profit Explorer'),
        'new_value' => _('New Tracking Value'),
        'open_values' => _('Open Tracking Values'),
        'find_values' => _('Find Tracking Values'),
        'customer_economics' => _('Customer Economics'),
        'missing_tracking' => _('Missing or Invalid Tracking'),
        'budget_alerts' => _('Budget Alerts'),
        'hierarchy' => _('Tracking Hierarchy'),
        'reports' => _('Cost & Profit Reports'),
        'report_501' => _('Tracking Summary'),
        'report_502' => _('Tracking Comparison and Exceptions'),
        'tags' => _('Tracking Tags'),
        'categories' => _('Tracking Categories'),
        'allocation_journal' => _('Cost Allocation Journal'),
        'allocation_drivers' => _('Cost Allocation Drivers'),
        'security_section' => _('Cost & Profit Tracking'),
        'security_section_setup' => _('Cost & Profit Tracking setup'),
    );
}
