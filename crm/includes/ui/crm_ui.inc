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
 * CRM Module UI Helpers
 *
 * Selector combos, display formatters, and common UI components
 * used across all CRM pages.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

// ================================================================
// FILTER FORM HELPERS (uniform CRM filter rendering)
// ================================================================

/**
 * Render an array-based select dropdown as filter cells.
 *
 * Wraps array_selector in the correct filter-field div (form mode)
 * or td (table mode) so it aligns with combo_input-based selectors.
 *
 * @param string|null $label       Label text, or null for no label.
 * @param string      $name        Form field name.
 * @param array       $items       Associative array of value => display text.
 * @param mixed       $selected_id Currently selected value.
 * @param bool        $submit_on_change Submit form on change.
 * @param string|false $spec_option Special (default) option text, false to omit.
 * @param mixed       $spec_id     Value for the special option.
 */
function crm_filter_array_list_cells($label, $name, $items, $selected_id = null, $submit_on_change = false, $spec_option = false, $spec_id = '')
{
    $options = array(
        'select_submit' => $submit_on_change,
    );
    if ($spec_option !== false) {
        $options['spec_option'] = $spec_option;
        $options['spec_id'] = $spec_id;
    }
    if (ui_current_table_mode() != 'table') {
        echo "<div class='filter-field'>\n";
        if ($label !== null)
            echo "<label class='filter-label'>" . $label . "</label>\n";
        echo array_selector($name, $selected_id, $items, $options);
        echo "</div>\n";
        return;
    }
    if ($label !== null) {
        echo "<td>$label</td>\n";
    }
    echo "<td>";
    echo array_selector($name, $selected_id, $items, $options);
    echo "</td>\n";
}

/**
 * Render a text search input as filter cells.
 *
 * Wraps a text input in a filter-field div (form mode) or td (table mode)
 * so it aligns with combo selectors. Supports placeholder attribute.
 *
 * @param string      $name        Form field name.
 * @param string      $placeholder Placeholder text for the input.
 * @param int         $size        Input size attribute.
 * @param int         $max         Input maxlength attribute.
 */
function crm_filter_search_cells($name, $placeholder = '', $size = 20, $max = 100)
{
    global $Ajax;

    $value = get_post($name, '');
    default_focus($name);

    if (ui_current_table_mode() != 'table') {
        echo "<div class='filter-field'>\n";
        echo "<input type='text' name='" . htmlspecialchars($name) . "' value='"
            . htmlspecialchars($value) . "' size='$size' maxlength='$max'"
            . ($placeholder ? " placeholder='" . htmlspecialchars($placeholder) . "'" : '')
            . ">\n";
        echo "</div>\n";
    } else {
        echo "<td>";
        echo "<input type='text' name='" . htmlspecialchars($name) . "' value='"
            . htmlspecialchars($value) . "' size='$size' maxlength='$max'"
            . ($placeholder ? " placeholder='" . htmlspecialchars($placeholder) . "'" : '')
            . ">\n";
        echo "</td>\n";
    }
    $Ajax->addUpdate($name, $name, $value);
}

// ================================================================
// COMBO SELECTORS (for form dropdowns)
// ================================================================

/**
 * Return the sel_app query parameter for CRM links.
 *
 * Uses the current session sel_app value (typically 'crm') so that
 * links within the CRM module do not cause the sidebar to switch
 * away to another application.
 *
 * @param string $prefix  '&' or '&amp;' depending on context.
 * @return string  e.g. '&sel_app=crm' or '' if not set.
 */
function crm_sel_app_param($prefix = '&')
{
    $app = isset($_GET['sel_app']) ? $_GET['sel_app']
        : (isset($_SESSION['sel_app']) ? $_SESSION['sel_app'] : 'crm');
    return $prefix . 'sel_app=' . urlencode($app);
}

/**
 * Lead source selector.
 *
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 * @return string|void
 */
function crm_lead_source_list($name, $selected_id = null, $submit_on_change = false, $spec_option = null)
{
    if ($spec_option === null)
        $spec_option = _('-- Select Source --');
    $sql = "SELECT id, name FROM " . TB_PREF . "crm_lead_sources WHERE active = 1";
    return combo_input($name, $selected_id, $sql, 'id', 'name',
        array(
            'select_submit' => $submit_on_change,
            'spec_option' => $spec_option,
            'spec_id' => '',
        ));
}

/**
 * Lead source selector rendered as table cells.
 *
 * @param string $label
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 */
function crm_lead_source_list_cells($label, $name, $selected_id = null, $submit_on_change = false, $spec_option = null)
{
    if (ui_current_table_mode() != 'table') {
        echo "<div class='filter-field'>\n";
        if ($label !== null) echo "<label class='filter-label'>" . $label . "</label>\n";
        echo crm_lead_source_list($name, $selected_id, $submit_on_change, $spec_option);
        echo "</div>\n";
        return;
    }
    if ($label !== null) {
        echo "<td>$label</td>\n";
    }
    echo "<td>";
    echo crm_lead_source_list($name, $selected_id, $submit_on_change, $spec_option);
    echo "</td>\n";
}

/**
 * Lead source selector rendered as table row.
 *
 * @param string $label
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 */
function crm_lead_source_list_row($label, $name, $selected_id = null, $submit_on_change = false)
{
    echo "<tr><td class='label'>$label</td>";
    crm_lead_source_list_cells(null, $name, $selected_id, $submit_on_change);
    echo "</tr>\n";
}

/**
 * Sales stage selector.
 *
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 * @return string|void
 */
function crm_sales_stage_list($name, $selected_id = null, $submit_on_change = false)
{
    $sql = "SELECT id, CONCAT(name, ' (', probability, '%)') as display_name
            FROM " . TB_PREF . "crm_sales_stages WHERE active = 1";
    return combo_input($name, $selected_id, $sql, 'id', 'display_name',
        array(
            'order' => 'sequence',
            'select_submit' => $submit_on_change,
            'spec_option' => _('-- Select Stage --'),
            'spec_id' => '',
        ));
}

/**
 * Sales stage selector as table row.
 *
 * @param string $label
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 */
function crm_sales_stage_list_row($label, $name, $selected_id = null, $submit_on_change = false)
{
    echo "<tr><td class='label'>$label</td><td>";
    echo crm_sales_stage_list($name, $selected_id, $submit_on_change);
    echo "</td></tr>\n";
}

/**
 * Lost reason selector.
 *
 * @param string $name
 * @param mixed  $selected_id
 * @return string|void
 */
function crm_lost_reason_list($name, $selected_id = null)
{
    $sql = "SELECT id, description FROM " . TB_PREF . "crm_lost_reasons
            WHERE active = 1";
    return combo_input($name, $selected_id, $sql, 'id', 'description',
        array(
            'spec_option' => _('-- Select Reason --'),
            'spec_id' => '',
        ));
}

/**
 * Lost reason selector as table row.
 *
 * @param string $label
 * @param string $name
 * @param mixed  $selected_id
 */
function crm_lost_reason_list_row($label, $name, $selected_id = null)
{
    echo "<tr><td class='label'>$label</td><td>";
    echo crm_lost_reason_list($name, $selected_id);
    echo "</td></tr>\n";
}

/**
 * Activity type selector.
 *
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 * @return string|void
 */
function crm_activity_type_list($name, $selected_id = null, $submit_on_change = false, $spec_option = null)
{
    if ($spec_option === null)
        $spec_option = _('-- Select Type --');
    $sql = "SELECT id, CONCAT(name, ' [', category, ']') as display_name
            FROM " . TB_PREF . "crm_activity_types WHERE active = 1";
    return combo_input($name, $selected_id, $sql, 'id', 'display_name',
        array(
            'select_submit' => $submit_on_change,
            'spec_option' => $spec_option,
            'spec_id' => '',
        ));
}

/**
 * Activity type selector as table row.
 *
 * @param string $label
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 */
function crm_activity_type_list_cells($label, $name, $selected_id = null, $submit_on_change = false)
{
    if (ui_current_table_mode() != 'table') {
        echo "<div class='filter-field'>\n";
        if ($label !== null) echo "<label class='filter-label'>" . $label . "</label>\n";
        echo crm_activity_type_list($name, $selected_id, $submit_on_change);
        echo "</div>\n";
        return;
    }
    if ($label !== null) {
        echo "<td>$label</td>\n";
    }
    echo "<td>";
    echo crm_activity_type_list($name, $selected_id, $submit_on_change);
    echo "</td>\n";
}

/**
 * Activity type selector as table row.
 *
 * @param string $label
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 */
function crm_activity_type_list_row($label, $name, $selected_id = null, $submit_on_change = false)
{
    echo "<tr><td class='label'>$label</td><td>";
    echo crm_activity_type_list($name, $selected_id, $submit_on_change);
    echo "</td></tr>\n";
}

/**
 * Sales team selector.
 *
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 * @return string|void
 */
function crm_sales_team_list($name, $selected_id = null, $submit_on_change = false, $spec_option = null)
{
    if ($spec_option === null)
        $spec_option = _('-- Select Team --');
    $sql = "SELECT id, name FROM " . TB_PREF . "crm_sales_teams WHERE active = 1";
    return combo_input($name, $selected_id, $sql, 'id', 'name',
        array(
            'select_submit' => $submit_on_change,
            'spec_option' => $spec_option,
            'spec_id' => '',
        ));
}

/**
 * Sales team selector as table cells.
 *
 * @param string $label
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 */
function crm_sales_team_list_cells($label, $name, $selected_id = null, $submit_on_change = false, $spec_option = null)
{
    if (ui_current_table_mode() != 'table') {
        echo "<div class='filter-field'>\n";
        if ($label !== null) echo "<label class='filter-label'>" . $label . "</label>\n";
        echo crm_sales_team_list($name, $selected_id, $submit_on_change, $spec_option);
        echo "</div>\n";
        return;
    }
    if ($label !== null) {
        echo "<td>$label</td>\n";
    }
    echo "<td>";
    echo crm_sales_team_list($name, $selected_id, $submit_on_change, $spec_option);
    echo "</td>\n";
}

/**
 * Sales team selector as table row.
 *
 * @param string $label
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 */
function crm_sales_team_list_row($label, $name, $selected_id = null, $submit_on_change = false)
{
    echo "<tr><td class='label'>$label</td><td>";
    echo crm_sales_team_list($name, $selected_id, $submit_on_change);
    echo "</td></tr>\n";
}

/**
 * Assignee (user) selector for filter forms.
 *
 * @param string $name        Form field name.
 * @param mixed  $selected_id Currently selected user id.
 * @param bool   $submit_on_change
 * @param string $spec_option Default "all" option text.
 * @return void
 */
function crm_assignee_list($name, $selected_id = null, $submit_on_change = false, $spec_option = null)
{
    if ($spec_option === null)
        $spec_option = _('All Assignees');
    if ($selected_id === null && isset($_POST[$name]))
        $selected_id = $_POST[$name];
    $users_sql = "SELECT id, real_name FROM " . TB_PREF . "users ORDER BY real_name";
    $users_result = db_query($users_sql);
    $onchange = $submit_on_change ? " onchange='this.form.submit();'" : '';
    echo "<select name='" . htmlspecialchars($name) . "'" . $onchange . ">";
    echo "<option value='0'>" . htmlspecialchars($spec_option) . "</option>";
    while ($u = db_fetch($users_result)) {
        $sel = ($selected_id == $u['id']) ? ' selected' : '';
        echo "<option value='" . (int)$u['id'] . "'" . $sel . ">" . htmlspecialchars($u['real_name']) . "</option>";
    }
    echo "</select>";
}

/**
 * Assignee (user) selector as table cells.
 *
 * @param string $label
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 * @param string $spec_option
 */
function crm_assignee_list_cells($label, $name, $selected_id = null, $submit_on_change = false, $spec_option = null)
{
    if (ui_current_table_mode() != 'table') {
        echo "<div class='filter-field'>\n";
        if ($label !== null) echo "<label class='filter-label'>" . $label . "</label>\n";
        crm_assignee_list($name, $selected_id, $submit_on_change, $spec_option);
        echo "</div>\n";
        return;
    }
    if ($label !== null) {
        echo "<td>$label</td>\n";
    }
    echo "<td>";
    crm_assignee_list($name, $selected_id, $submit_on_change, $spec_option);
    echo "</td>\n";
}

/**
 * Appointment type selector.
 *
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 * @return string|void
 */
function crm_appointment_type_list($name, $selected_id = null, $submit_on_change = false)
{
    $sql = "SELECT id, name FROM " . TB_PREF . "crm_appointment_types WHERE active = 1";
    return combo_input($name, $selected_id, $sql, 'id', 'name',
        array(
            'select_submit' => $submit_on_change,
            'spec_option' => _('-- Select Type --'),
            'spec_id' => '',
        ));
}

/**
 * Appointment type selector as table row.
 *
 * @param string $label
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 */
function crm_appointment_type_list_row($label, $name, $selected_id = null, $submit_on_change = false)
{
    echo "<tr><td class='label'>$label</td><td>";
    echo crm_appointment_type_list($name, $selected_id, $submit_on_change);
    echo "</td></tr>\n";
}

/**
 * Email template selector rendered as table row.
 *
 * @param string $label
 * @param string $name
 * @param mixed  $selected_id
 */
function crm_email_template_list_row($label, $name, $selected_id = null)
{
    echo "<tr><td class='label'>$label</td><td>";
    echo crm_email_template_list($name, $selected_id);
    echo "</td></tr>\n";
}

/**
 * Campaign selector.
 *
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 * @return string|void
 */
function crm_campaign_list($name, $selected_id = null, $submit_on_change = false)
{
    $sql = "SELECT id, name FROM " . TB_PREF . "crm_campaigns";
    return combo_input($name, $selected_id, $sql, 'id', 'name',
        array(
            'select_submit' => $submit_on_change,
            'spec_option' => _('-- Select Campaign --'),
            'spec_id' => '',
        ));
}

/**
 * Email template selector.
 *
 * @param string $name
 * @param mixed  $selected_id
 * @return string|void
 */
function crm_email_template_list($name, $selected_id = null)
{
    $sql = "SELECT id, name FROM " . TB_PREF . "crm_email_templates WHERE active = 1";
    return combo_input($name, $selected_id, $sql, 'id', 'name',
        array(
            'spec_option' => _('-- Select Template --'),
            'spec_id' => '',
        ));
}

/**
 * Activity plan selector.
 *
 * @param string $name
 * @param mixed  $selected_id
 * @return string|void
 */
function crm_activity_plan_list($name, $selected_id = null)
{
    $sql = "SELECT id, plan_name FROM " . TB_PREF . "crm_activity_plans WHERE active = 1";
    return combo_input($name, $selected_id, $sql, 'id', 'plan_name',
        array(
            'spec_option' => _('-- Select Plan --'),
            'spec_id' => '',
        ));
}

/**
 * CRM tag multi-select checkboxes rendered as table row.
 * Delegates to the core tag_color_checkboxes_row() with TAG_CRM type.
 *
 * @param string $label
 * @param string $name   Base name for checkboxes (e.g. 'tags').
 * @param array  $selected_ids  Array of currently selected tag IDs.
 */
function crm_tag_checkboxes_row($label, $name, $selected_ids = array())
{
    tag_color_checkboxes_row($label, $name, TAG_CRM, $selected_ids);
}

// ================================================================
// LEAD STATUS SELECTORS
// ================================================================

/**
 * Lead status dropdown.
 *
 * @param string $name
 * @param string $selected
 * @param bool   $submit_on_change
 */
function crm_lead_status_list($name, $selected = null, $submit_on_change = false)
{
    if ($selected === null && isset($_POST[$name]))
        $selected = $_POST[$name];
    $statuses = crm_lead_statuses();
    echo "<select name='" . htmlspecialchars($name) . "' id='" . htmlspecialchars($name) . "'";
    if ($submit_on_change) {
        echo " onchange='this.form.submit();'";
    }
    echo ">\n";
    echo "<option value=''>" . _('-- All Statuses --') . "</option>\n";
    foreach ($statuses as $key => $label) {
        $sel = ($selected === $key) ? ' selected' : '';
        echo "<option value='" . htmlspecialchars($key) . "'$sel>"
            . htmlspecialchars($label) . "</option>\n";
    }
    echo "</select>\n";
}

/**
 * Lead status selector as table row.
 *
 * @param string $label
 * @param string $name
 * @param string $selected
 * @param bool   $submit_on_change
 */
function crm_lead_status_list_cells($label, $name, $selected = null, $submit_on_change = false)
{
    if (ui_current_table_mode() != 'table') {
        echo "<div class='filter-field'>\n";
        if ($label !== null) echo "<label class='filter-label'>" . $label . "</label>\n";
        crm_lead_status_list($name, $selected, $submit_on_change);
        echo "</div>\n";
        return;
    }
    if ($label !== null) {
        echo "<td>$label</td>\n";
    }
    echo "<td>";
    crm_lead_status_list($name, $selected, $submit_on_change);
    echo "</td>\n";
}

/**
 * Lead status selector as table row.
 *
 * @param string $label
 * @param string $name
 * @param string $selected
 * @param bool   $submit_on_change
 */
function crm_lead_status_list_row($label, $name, $selected = null, $submit_on_change = false)
{
    echo "<tr><td class='label'>$label</td><td>";
    crm_lead_status_list($name, $selected, $submit_on_change);
    echo "</td></tr>\n";
}

/**
 * Priority selector.
 *
 * @param string $name
 * @param int    $selected
 */
function crm_priority_list($name, $selected = null, $include_all = false)
{
    if ($selected === null && isset($_POST[$name]))
        $selected = $_POST[$name];
    $priorities = crm_priority_levels();
    echo "<select name='" . htmlspecialchars($name) . "'>\n";
    if ($include_all) {
        $all_text = is_string($include_all) ? $include_all : _('-- All --');
        $sel = ($selected === null || $selected === '' || $selected === '-1') ? ' selected' : '';
        echo "<option value='-1'$sel>" . htmlspecialchars($all_text) . "</option>\n";
    }
    foreach ($priorities as $key => $label) {
        $sel = ($selected !== null && $selected !== '' && $selected !== '-1' && (int)$selected === $key) ? ' selected' : '';
        echo "<option value='$key'$sel>" . htmlspecialchars($label) . "</option>\n";
    }
    echo "</select>\n";
}

/**
 * Priority selector as table cells.
 *
 * @param string $label
 * @param string $name
 * @param int    $selected
 * @param bool   $submit_on_change
 */
function crm_priority_list_cells($label, $name, $selected = null, $submit_on_change = false, $include_all = false)
{
    if (ui_current_table_mode() != 'table') {
        echo "<div class='filter-field'>\n";
        if ($label !== null) echo "<label class='filter-label'>" . $label . "</label>\n";
        crm_priority_list($name, $selected, $include_all);
        echo "</div>\n";
        return;
    }
    if ($label !== null) {
        echo "<td>$label</td>\n";
    }
    echo "<td>";
    crm_priority_list($name, $selected, $include_all);
    echo "</td>\n";
}

/**
 * Priority selector as table row.
 *
 * @param string $label
 * @param string $name
 * @param int    $selected
 */
function crm_priority_list_row($label, $name, $selected = null)
{
    echo "<tr><td class='label'>$label</td><td>";
    crm_priority_list($name, $selected);
    echo "</td></tr>\n";
}

// ================================================================
// DISPLAY HELPERS
// ================================================================

/**
 * Return a colored badge for a lead status.
 *
 * @param string $status
 * @return string  HTML span element.
 */
function crm_status_badge($status)
{
    $colors = array(
        'new'       => '#2196F3',
        'contacted' => '#ff9800',
        'qualified' => '#4caf50',
        'converted' => '#9c27b0',
        'lost'      => '#f44336',
    );
    $statuses = crm_lead_statuses();
    $label = isset($statuses[$status]) ? $statuses[$status] : $status;
    $color = isset($colors[$status]) ? $colors[$status] : '#607d8b';
    $css_class = 'crm-badge crm-badge-' . htmlspecialchars($status);

    return "<span class='$css_class' style='display:inline-block; padding:2px 8px; background:$color; "
        . "color:#fff; border-radius:3px; font-size:0.85em;'>"
        . htmlspecialchars($label) . "</span>";
}

/**
 * Return a colored badge for a priority level.
 *
 * @param int $priority
 * @return string  HTML span element.
 */
function crm_priority_badge($priority)
{
    $colors = array(
        CRM_PRIORITY_NORMAL => '#607d8b',
        CRM_PRIORITY_LOW    => '#2196F3',
        CRM_PRIORITY_MEDIUM => '#ff9800',
        CRM_PRIORITY_HIGH   => '#f44336',
    );
    $class_map = array(
        CRM_PRIORITY_NORMAL => 'normal',
        CRM_PRIORITY_LOW    => 'low',
        CRM_PRIORITY_MEDIUM => 'medium',
        CRM_PRIORITY_HIGH   => 'high',
    );
    $levels = crm_priority_levels();
    $label = isset($levels[$priority]) ? $levels[$priority] : _('Normal');
    $color = isset($colors[$priority]) ? $colors[$priority] : '#607d8b';
    $css_suffix = isset($class_map[$priority]) ? $class_map[$priority] : 'normal';
    $css_class = 'crm-badge crm-priority-' . $css_suffix;

    return "<span class='$css_class' style='display:inline-block; padding:2px 8px; background:$color; "
        . "color:#fff; border-radius:3px; font-size:0.85em;'>"
        . htmlspecialchars($label) . "</span>";
}

/**
 * Return a colored badge for an activity status.
 *
 * @param string $status
 * @return string
 */
function crm_activity_status_badge($status)
{
    $colors = array(
        'planned'   => '#2196F3',
        'done'      => '#4caf50',
        'cancelled' => '#9e9e9e',
        'overdue'   => '#f44336',
    );
    $statuses = crm_activity_statuses();
    $label = isset($statuses[$status]) ? $statuses[$status] : $status;
    $color = isset($colors[$status]) ? $colors[$status] : '#607d8b';
    $css_class = 'crm-badge crm-badge-' . htmlspecialchars($status);

    return "<span class='$css_class' style='display:inline-block; padding:2px 8px; background:$color; "
        . "color:#fff; border-radius:3px; font-size:0.85em;'>"
        . htmlspecialchars($label) . "</span>";
}

/**
 * Render pipeline probability as a progress bar.
 *
 * @param int $probability 0-100
 * @return string  HTML div element.
 */
function crm_probability_bar($probability)
{
    $percent = max(0, min(100, (int)$probability));
    if ($percent >= 80) {
        $color = '#4caf50';
    } elseif ($percent >= 50) {
        $color = '#ff9800';
    } else {
        $color = '#2196F3';
    }

    return "<div style='width:80px; height:14px; background:#e0e0e0; border-radius:7px; display:inline-block; vertical-align:middle;'>"
        . "<div style='width:{$percent}%; height:100%; background:$color; border-radius:7px;'></div>"
        . "</div> <small>{$percent}%</small>";
}

/**
 * Display tags inline for an entity.
 *
 * @param string $entity_type
 * @param int    $entity_id
 * @return string
 */
function crm_display_tags($entity_type, $entity_id)
{
    $tags = get_crm_entity_tags($entity_type, $entity_id);
    if (empty($tags)) {
        return '';
    }
    $html = '';
    foreach ($tags as $tag) {
        $html .= "<span style='display:inline-block; padding:1px 6px; margin:1px; "
            . "background:" . htmlspecialchars($tag['color']) . "; color:#fff; "
            . "border-radius:3px; font-size:0.8em;'>"
            . htmlspecialchars($tag['name']) . "</span>";
    }
    return $html;
}

/**
 * Include all CRM DB and constants files.
 * Call this at the top of CRM pages after session.inc.
 */
function include_crm_files()
{
    global $path_to_root;

    include_once($path_to_root . '/crm/includes/crm_constants.inc');
    include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
    include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');
    include_once($path_to_root . '/crm/includes/db/crm_activities_db.inc');
    include_once($path_to_root . '/crm/includes/db/crm_campaigns_db.inc');
    include_once($path_to_root . '/crm/includes/db/crm_contracts_db.inc');
    include_once($path_to_root . '/crm/includes/db/crm_appointments_db.inc');
    include_once($path_to_root . '/crm/includes/db/crm_teams_db.inc');
    include_once($path_to_root . '/crm/includes/db/crm_communication_db.inc');
    include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');
}

/**
 * Output CRM page JavaScript includes.
 *
 * Call before end_page() on CRM pages. Always includes crm_common.js;
 * pass $include_activities = true on activity-related pages.
 *
 * @param bool $include_activities  Include crm_activities.js.
 */
function crm_page_scripts($include_activities = false)
{
    global $path_to_root;

    $common_js = $path_to_root . '/crm/js/crm_common.js';
    echo "<script src='" . $common_js . "?v=" . @filemtime($common_js) . "'></script>\n";

    if ($include_activities) {
        $act_js = $path_to_root . '/crm/js/crm_activities.js';
        echo "<script src='" . $act_js . "?v=" . @filemtime($act_js) . "'></script>\n";
    }
}
