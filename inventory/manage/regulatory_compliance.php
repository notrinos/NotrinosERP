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
 * Regulatory Compliance Management.
 *
 * Features:
 *   - Compliance settings (enable/disable frameworks, company license info)
 *   - Regulatory profiles: assign items/categories to frameworks (DSCSA, FSMA 204, UDI)
 *   - DSCSA transaction ledger view with verification actions
 *   - FSMA 204 Critical Tracking Events view with rapid recall lookup
 *   - UDI registration management
 *   - Compliance dashboard summary cards
 */
$page_security = 'SA_REGULATORY';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Regulatory Compliance');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page($_SESSION['page_title'], false, false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/admin/db/company_db.inc');
include_once($path_to_root . '/inventory/includes/db/regulatory_compliance_db.inc');
include_once($path_to_root . '/inventory/includes/regulatory_dscsa.inc');
include_once($path_to_root . '/inventory/includes/regulatory_fsma204.inc');
include_once($path_to_root . '/inventory/includes/regulatory_udi.inc');
include_once($path_to_root . '/inventory/includes/gs1_standards.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');

// =====================================================================
// DETERMINE ACTIVE TAB
// =====================================================================
$active_tab = get_post('tab', 'settings');
if (isset($_GET['tab']))
	$active_tab = $_GET['tab'];

// =====================================================================
// HANDLE SETTINGS SAVE
// =====================================================================
if (isset($_POST['save_settings'])) {
	set_company_pref('regulatory_compliance_enabled', check_value('regulatory_compliance_enabled') ? '1' : '0');
	set_company_pref('dscsa_enabled', check_value('dscsa_enabled') ? '1' : '0');
	set_company_pref('fsma204_enabled', check_value('fsma204_enabled') ? '1' : '0');
	set_company_pref('udi_enabled', check_value('udi_enabled') ? '1' : '0');
	set_company_pref('dscsa_company_license', get_post('dscsa_company_license'));
	set_company_pref('dscsa_company_dea', get_post('dscsa_company_dea'));
	set_company_pref('fsma204_firm_name', get_post('fsma204_firm_name'));
	set_company_pref('fsma204_fda_registration', get_post('fsma204_fda_registration'));
	set_company_pref('udi_company_name', get_post('udi_company_name'));
	set_company_pref('udi_issuing_agency', get_post('udi_issuing_agency'));
	display_notification(_('Regulatory compliance settings have been updated.'));
	$active_tab = 'settings';
}

// =====================================================================
// HANDLE PROFILE ADD/EDIT/DELETE
// =====================================================================
if (isset($_POST['add_profile'])) {
	$input_error = 0;

	$stock_id = get_post('profile_stock_id');
	$category_id = get_post('profile_category_id');
	$framework = get_post('profile_framework');

	if ($stock_id === '' && ($category_id === '' || $category_id === '0')) {
		$input_error = 1;
		display_error(_('Please select either an item or a category.'));
	}
	if ($framework === '') {
		$input_error = 1;
		display_error(_('Please select a regulatory framework.'));
	}

	// Build settings from posted framework-specific fields
	$settings = array();
	if ($framework === REG_DSCSA) {
		$settings['ndc'] = get_post('profile_ndc');
	}

	if ($input_error == 0) {
		$sid = ($stock_id !== '') ? $stock_id : null;
		$cid = ($category_id !== '' && $category_id !== '0') ? (int)$category_id : null;
		if ($sid) $cid = null; // item-level takes priority

		add_regulatory_profile($sid, $cid, $framework, $settings, get_post('profile_notes'));
		display_notification(_('Regulatory profile has been added.'));
	}
	$active_tab = 'profiles';
}

if (isset($_POST['delete_profile']) && isset($_POST['sel_profile'])) {
	delete_regulatory_profile((int)$_POST['sel_profile']);
	display_notification(_('Regulatory profile has been deleted.'));
	$active_tab = 'profiles';
}

if (isset($_POST['toggle_profile']) && isset($_POST['sel_profile'])) {
	$prof = get_regulatory_profile((int)$_POST['sel_profile']);
	if ($prof) {
		$settings = json_decode($prof['settings'], true);
		if (!is_array($settings)) $settings = array();
		update_regulatory_profile((int)$_POST['sel_profile'], !$prof['enabled'], $settings, $prof['notes']);
		display_notification($prof['enabled'] ? _('Profile disabled.') : _('Profile enabled.'));
	}
	$active_tab = 'profiles';
}

// =====================================================================
// HANDLE DSCSA VERIFICATION
// =====================================================================
if (isset($_POST['verify_dscsa']) && isset($_POST['dscsa_id'])) {
	$status = get_post('verify_status', 'verified');
	update_dscsa_verification((int)$_POST['dscsa_id'], $status);
	display_notification(_('DSCSA verification status updated.'));
	$active_tab = 'dscsa';
}

// Handle suspect product report
if (isset($_POST['report_suspect'])) {
	$input_error = 0;
	if (get_post('suspect_stock_id') === '') {
		$input_error = 1;
		display_error(_('Please select an item.'));
	}
	if (get_post('suspect_reason') === '') {
		$input_error = 1;
		display_error(_('Please provide a reason.'));
	}

	if ($input_error == 0) {
		$serial_id = get_post('suspect_serial_id') ? (int)get_post('suspect_serial_id') : null;
		$batch_id = get_post('suspect_batch_id') ? (int)get_post('suspect_batch_id') : null;
		report_dscsa_suspect_product(
			$serial_id, $batch_id,
			get_post('suspect_stock_id'),
			get_post('suspect_reason'),
			get_post('suspect_action', 'quarantine'),
			check_value('suspect_quarantine')
		);
		display_notification(_('Suspect product has been reported and quarantined.'));
	}
	$active_tab = 'dscsa';
}

// =====================================================================
// HANDLE FSMA EVENT ADD
// =====================================================================
if (isset($_POST['add_fsma_event'])) {
	$input_error = 0;
	if (get_post('fsma_stock_id') === '') {
		$input_error = 1;
		display_error(_('Please select an item.'));
	}
	if (get_post('fsma_event_type') === '') {
		$input_error = 1;
		display_error(_('Please select an event type.'));
	}

	if ($input_error == 0) {
		$kde_data = array(
			'event_date'           => get_post('fsma_event_date') ? sql_date_format(get_post('fsma_event_date')) . ' ' . date('H:i:s') : date('Y-m-d H:i:s'),
			'quantity'             => get_post('fsma_quantity'),
			'unit_of_measure'      => get_post('fsma_unit'),
			'location_description' => get_post('fsma_location'),
			'loc_code'             => get_post('fsma_loc_code'),
			'trading_partner_name' => get_post('fsma_partner_name'),
			'trading_partner_type' => get_post('fsma_partner_type'),
			'country_of_origin'    => get_post('fsma_country'),
			'harvest_date'         => get_post('fsma_harvest_date') ? sql_date_format(get_post('fsma_harvest_date')) : null,
			'pack_date'            => get_post('fsma_pack_date') ? sql_date_format(get_post('fsma_pack_date')) : null,
			'ship_date'            => get_post('fsma_ship_date') ? sql_date_format(get_post('fsma_ship_date')) : null,
			'notes'                => get_post('fsma_notes'),
		);

		add_fsma_tracking_event(
			get_post('fsma_event_type'),
			get_post('fsma_stock_id'),
			get_post('fsma_batch_id') ? (int)get_post('fsma_batch_id') : null,
			get_post('fsma_lot_code'),
			$kde_data
		);
		display_notification(_('FSMA 204 tracking event has been recorded.'));
	}
	$active_tab = 'fsma';
}

// =====================================================================
// HANDLE UDI ADD/UPDATE/DELETE
// =====================================================================
if (isset($_POST['add_udi'])) {
	$input_error = 0;
	if (get_post('udi_stock_id') === '') {
		$input_error = 1;
		display_error(_('Please select an item.'));
	}
	if (get_post('udi_di_input') === '') {
		$input_error = 1;
		display_error(_('Device Identifier (UDI-DI) is required.'));
	}

	if ($input_error == 0) {
		add_udi_registration(array(
			'stock_id'             => get_post('udi_stock_id'),
			'udi_di'               => get_post('udi_di_input'),
			'issuing_agency'       => get_post('udi_agency', 'GS1'),
			'device_description'   => get_post('udi_description'),
			'brand_name'           => get_post('udi_brand'),
			'version_model'        => get_post('udi_model'),
			'company_name'         => get_post('udi_company'),
			'mri_safety'           => get_post('udi_mri'),
			'device_sterile'       => check_value('udi_sterile'),
			'single_use'           => check_value('udi_single_use'),
			'implantable'          => check_value('udi_implantable'),
			'rx_only'              => check_value('udi_rx_only'),
			'otc'                  => check_value('udi_otc'),
			'fda_listing_number'   => get_post('udi_fda_listing'),
			'fda_premarket_number' => get_post('udi_fda_premarket'),
			'gtin'                 => get_post('udi_gtin'),
			'notes'                => get_post('udi_notes'),
		));
		display_notification(_('UDI registration has been added.'));
	}
	$active_tab = 'udi';
}

if (isset($_POST['delete_udi']) && isset($_POST['sel_udi'])) {
	delete_udi_registration((int)$_POST['sel_udi']);
	display_notification(_('UDI registration has been deleted.'));
	$active_tab = 'udi';
}

// =====================================================================
// RENDER TABS
// =====================================================================
$tabs = array(
	'settings' => _('Settings'),
	'profiles' => _('Regulatory Profiles'),
	'dscsa'    => _('DSCSA (Pharma)'),
	'fsma'     => _('FSMA 204 (Food)'),
	'udi'      => _('UDI (Medical)'),
);

echo '<div style="margin-bottom:15px;">';
foreach ($tabs as $key => $label) {
	$active = ($key === $active_tab) ? 'font-weight:bold; border-bottom:3px solid #0d6efd; color:#0d6efd;' : 'color:#495057;';
	echo '<a href="' . $_SERVER['PHP_SELF'] . '?tab=' . $key . '" '
		. 'style="display:inline-block; padding:8px 16px; text-decoration:none; ' . $active . '">'
		. $label . '</a>';
}
echo '</div>';

// =====================================================================
// TAB: SETTINGS
// =====================================================================
if ($active_tab === 'settings') {
	$reg_enabled = get_company_pref('regulatory_compliance_enabled');
	$dscsa_on = get_company_pref('dscsa_enabled');
	$fsma_on = get_company_pref('fsma204_enabled');
	$udi_on = get_company_pref('udi_enabled');

	start_form();
	hidden('tab', 'settings');

	// --- Summary Cards ---
	$summary = get_regulatory_compliance_summary();
	echo '<div style="display:flex; gap:15px; margin-bottom:20px; flex-wrap:wrap;">';

	// Profiles card
	echo '<div style="flex:1; min-width:180px; padding:15px; border-radius:8px; background:#f8f9fa; border-left:4px solid #0d6efd;">';
	echo '<div style="font-size:11px; color:#6c757d; text-transform:uppercase;">' . _('Active Profiles') . '</div>';
	echo '<div style="font-size:24px; font-weight:700; color:#0d6efd;">' . $summary['profiles_total'] . '</div>';
	echo '</div>';

	// DSCSA Pending
	echo '<div style="flex:1; min-width:180px; padding:15px; border-radius:8px; background:#f8f9fa; border-left:4px solid #ffc107;">';
	echo '<div style="font-size:11px; color:#6c757d; text-transform:uppercase;">' . _('DSCSA Pending') . '</div>';
	echo '<div style="font-size:24px; font-weight:700; color:#ffc107;">' . $summary['dscsa_pending'] . '</div>';
	echo '</div>';

	// DSCSA Suspect
	echo '<div style="flex:1; min-width:180px; padding:15px; border-radius:8px; background:#f8f9fa; border-left:4px solid #dc3545;">';
	echo '<div style="font-size:11px; color:#6c757d; text-transform:uppercase;">' . _('DSCSA Suspect') . '</div>';
	echo '<div style="font-size:24px; font-weight:700; color:#dc3545;">' . $summary['dscsa_suspect'] . '</div>';
	echo '</div>';

	// UDI Registered
	echo '<div style="flex:1; min-width:180px; padding:15px; border-radius:8px; background:#f8f9fa; border-left:4px solid #198754;">';
	echo '<div style="font-size:11px; color:#6c757d; text-transform:uppercase;">' . _('UDI Registered') . '</div>';
	echo '<div style="font-size:24px; font-weight:700; color:#198754;">' . $summary['udi_registered'] . '</div>';
	echo '</div>';

	echo '</div>';

	// --- Settings Form ---
	start_outer_table(TABLESTYLE2);

	table_section_title(_('Master Compliance Settings'));
	check_row(_('Enable Regulatory Compliance Module:'), 'regulatory_compliance_enabled', $reg_enabled);

	table_section_title(_('DSCSA — Drug Supply Chain Security Act (US Pharma)'));
	check_row(_('Enable DSCSA Compliance:'), 'dscsa_enabled', $dscsa_on);
	text_row(_('State License Number:'), 'dscsa_company_license', get_company_pref('dscsa_company_license'), 50, 50);
	text_row(_('DEA Registration Number:'), 'dscsa_company_dea', get_company_pref('dscsa_company_dea'), 20, 20);

	table_section_title(_('FSMA 204 — Food Safety Modernization Act (US Food)'));
	check_row(_('Enable FSMA 204 Compliance:'), 'fsma204_enabled', $fsma_on);
	text_row(_('FDA Registered Firm Name:'), 'fsma204_firm_name', get_company_pref('fsma204_firm_name'), 100, 100);
	text_row(_('FDA Registration Number:'), 'fsma204_fda_registration', get_company_pref('fsma204_fda_registration'), 30, 30);

	table_section_title(_('UDI — Unique Device Identification (Medical Devices)'));
	check_row(_('Enable UDI Compliance:'), 'udi_enabled', $udi_on);
	text_row(_('Company Name (for GUDID):'), 'udi_company_name', get_company_pref('udi_company_name'), 100, 100);

	$agencies = get_udi_issuing_agencies();
	$current_agency = get_company_pref('udi_issuing_agency');
	if (!$current_agency) $current_agency = 'GS1';
	array_selector_row(_('Default Issuing Agency:'), 'udi_issuing_agency', $current_agency, $agencies);

	end_outer_table(1);

	submit_center('save_settings', _('Save Settings'), true, '', 'default');

	end_form();

	// --- How it works panel ---
	echo '<div style="margin-top:20px; padding:15px; background:#e8f4fd; border-radius:8px; border:1px solid #b8daff;">';
	echo '<h4 style="margin:0 0 8px 0; color:#004085;">' . _('How Regulatory Compliance Works') . '</h4>';
	echo '<ul style="margin:0; padding-left:20px; color:#004085; font-size:13px;">';
	echo '<li><b>' . _('Step 1:') . '</b> ' . _('Enable the master switch and the specific framework(s) your business requires.') . '</li>';
	echo '<li><b>' . _('Step 2:') . '</b> ' . _('Go to "Regulatory Profiles" tab and assign items or categories to their applicable frameworks.') . '</li>';
	echo '<li><b>' . _('Step 3:') . '</b> ' . _('For DSCSA pharma items, set the NDC (National Drug Code) in the profile settings.') . '</li>';
	echo '<li><b>' . _('Step 4:') . '</b> ' . _('Transaction Info is auto-generated when deliveries/GRNs are processed for regulated items.') . '</li>';
	echo '<li><b>' . _('Step 5:') . '</b> ' . _('Use the DSCSA/FSMA/UDI tabs to review compliance data, verify products, and manage registrations.') . '</li>';
	echo '</ul>';
	echo '</div>';
}

// =====================================================================
// TAB: REGULATORY PROFILES
// =====================================================================
if ($active_tab === 'profiles') {
	// --- Add profile form ---
	start_form();
	hidden('tab', 'profiles');

	echo '<h3>' . _('Add Regulatory Profile') . '</h3>';
	start_outer_table(TABLESTYLE2);

	stock_items_list_cells(_('Item (or leave blank for category):'), 'profile_stock_id', null, _('Select item...'), false, true);

	$cat_sql = "SELECT category_id, description FROM " . TB_PREF . "stock_category";
	combo_input('profile_category_id', null, $cat_sql, 'category_id', 'description',
		array('select_submit' => false, 'spec_option' => _('-- All / Not category-level --'), 'spec_id' => ''));
	echo '</td></tr>';

	$frameworks = get_regulatory_frameworks();
	array_selector_row(_('Regulatory Framework:'), 'profile_framework', null, $frameworks);

	text_row(_('NDC (for DSCSA pharma items):'), 'profile_ndc', '', 20, 20);
	text_row(_('Notes:'), 'profile_notes', '', 60, 255);

	end_outer_table(1);

	submit_center('add_profile', _('Add Profile'), true, '', 'default');
	end_form();

	// --- Profile listing ---
	echo '<h3 style="margin-top:20px;">' . _('Active Regulatory Profiles') . '</h3>';

	$filter_framework = get_post('filter_framework', '');

	start_form();
	hidden('tab', 'profiles');

	start_table(TABLESTYLE, "width='100%'");
	$th = array(_('ID'), _('Item'), _('Category'), _('Framework'), _('Status'), _('Settings'), _('Actions'));
	table_header($th);

	$profiles = get_regulatory_profiles($filter_framework ? $filter_framework : null);
	$k = 0;
	while ($row = db_fetch($profiles)) {
		alt_table_row_color($k);

		label_cell($row['id']);
		label_cell($row['stock_id'] ? $row['stock_id'] . ' - ' . $row['item_name'] : '—');
		label_cell($row['category_id'] ? $row['category_name'] : '—');

		// Framework badge
		$fcolor = get_regulatory_framework_color($row['framework']);
		$flabel = get_regulatory_framework_label($row['framework']);
		label_cell('<span style="display:inline-block; padding:2px 8px; border-radius:4px; background:' . $fcolor . '; color:#fff; font-size:11px;">' . $flabel . '</span>');

		// Enabled/disabled
		$status_color = $row['enabled'] ? '#28a745' : '#dc3545';
		$status_label = $row['enabled'] ? _('Enabled') : _('Disabled');
		label_cell('<span style="color:' . $status_color . '; font-weight:600;">' . $status_label . '</span>');

		// Settings
		$s = json_decode($row['settings'], true);
		$settings_display = '';
		if (is_array($s)) {
			foreach ($s as $sk => $sv) {
				if ($sv !== '' && $sv !== null)
					$settings_display .= $sk . '=' . htmlspecialchars($sv, ENT_QUOTES, 'UTF-8') . ' ';
			}
		}
		label_cell($settings_display ? $settings_display : '—');

		// Actions
		echo '<td>';
		echo '<form method="post" style="display:inline;">';
		echo '<input type="hidden" name="_token" value="' . ensure_csrf_token() . '">';
		echo '<input type="hidden" name="tab" value="profiles">';
		echo '<input type="hidden" name="sel_profile" value="' . $row['id'] . '">';
		echo '<button type="submit" name="toggle_profile" style="margin-right:5px; cursor:pointer; padding:2px 8px; border:1px solid #6c757d; border-radius:3px; background:#f8f9fa;">'
			. ($row['enabled'] ? _('Disable') : _('Enable')) . '</button>';
		echo '<button type="submit" name="delete_profile" onclick="return confirm(\'' . _('Delete this profile?') . '\');" '
			. 'style="cursor:pointer; padding:2px 8px; border:1px solid #dc3545; border-radius:3px; background:#fff; color:#dc3545;">'
			. _('Delete') . '</button>';
		echo '</form>';
		echo '</td>';

		end_row();
	}
	end_table(1);
	end_form();
}

// =====================================================================
// TAB: DSCSA
// =====================================================================
if ($active_tab === 'dscsa') {
	start_form();
	hidden('tab', 'dscsa');

	// --- Filter form ---
	echo '<h3>' . _('DSCSA Transaction Ledger') . '</h3>';
	start_table(TABLESTYLE_NOBORDER);
	start_row();

	$types = get_dscsa_transaction_types();
	echo '<td>' . _('Type:') . '</td><td>';
	echo array_selector('dscsa_filter_type', get_post('dscsa_filter_type', ''),
		array_merge(array('' => _('All')), $types));
	echo '</td>';

	$statuses = get_dscsa_verification_statuses();
	echo '<td>' . _('Status:') . '</td><td>';
	echo array_selector('dscsa_filter_status', get_post('dscsa_filter_status', ''),
		array_merge(array('' => _('All')), $statuses));
	echo '</td>';

	text_cells(_('Serial:'), 'dscsa_filter_serial', get_post('dscsa_filter_serial', ''), 20, 50);

	submit_cells('dscsa_filter', _('Filter'), '', '', 'default');
	end_row();
	end_table();

	// --- Transaction listing ---
	$filters = array();
	if (get_post('dscsa_filter_type'))
		$filters['transaction_type'] = get_post('dscsa_filter_type');
	if (get_post('dscsa_filter_status'))
		$filters['verification_status'] = get_post('dscsa_filter_status');
	if (get_post('dscsa_filter_serial'))
		$filters['serial_no'] = get_post('dscsa_filter_serial');

	$result = get_dscsa_transactions($filters);

	start_table(TABLESTYLE, "width='100%'");
	$th = array(_('ID'), _('Type'), _('Date'), _('Item'), _('Serial'), _('Lot'),
		_('NDC'), _('Sender'), _('Receiver'), _('Status'), _('Actions'));
	table_header($th);

	$k = 0;
	while ($row = db_fetch($result)) {
		alt_table_row_color($k);

		label_cell($row['id']);

		$type_label = get_dscsa_transaction_types();
		label_cell(isset($type_label[$row['transaction_type']]) ? $type_label[$row['transaction_type']] : $row['transaction_type']);

		label_cell(sql2date($row['transaction_date']));
		label_cell($row['stock_id'] . ' - ' . $row['item_name']);
		label_cell($row['serial_no'] ? $row['serial_no'] : '—');
		label_cell($row['lot_number'] ? $row['lot_number'] : '—');
		label_cell($row['ndc'] ? $row['ndc'] : '—');
		label_cell($row['sender_name'] ? $row['sender_name'] : '—');
		label_cell($row['receiver_name'] ? $row['receiver_name'] : '—');

		// Status badge
		$scolor = get_dscsa_status_color($row['verification_status']);
		$slabel = get_dscsa_verification_statuses();
		$sl = isset($slabel[$row['verification_status']]) ? $slabel[$row['verification_status']] : $row['verification_status'];
		label_cell('<span style="display:inline-block; padding:2px 8px; border-radius:4px; background:' . $scolor . '; color:#fff; font-size:11px;">' . $sl . '</span>');

		// Actions
		echo '<td>';
		if ($row['verification_status'] === 'pending') {
			echo '<form method="post" style="display:inline;">';
			echo '<input type="hidden" name="_token" value="' . ensure_csrf_token() . '">';
			echo '<input type="hidden" name="tab" value="dscsa">';
			echo '<input type="hidden" name="dscsa_id" value="' . $row['id'] . '">';
			echo '<input type="hidden" name="verify_status" value="verified">';
			echo '<button type="submit" name="verify_dscsa" style="cursor:pointer; padding:2px 6px; border:1px solid #28a745; border-radius:3px; background:#fff; color:#28a745; font-size:11px;">'
				. _('Verify') . '</button>';
			echo '</form> ';
			echo '<form method="post" style="display:inline;">';
			echo '<input type="hidden" name="_token" value="' . ensure_csrf_token() . '">';
			echo '<input type="hidden" name="tab" value="dscsa">';
			echo '<input type="hidden" name="dscsa_id" value="' . $row['id'] . '">';
			echo '<input type="hidden" name="verify_status" value="failed">';
			echo '<button type="submit" name="verify_dscsa" style="cursor:pointer; padding:2px 6px; border:1px solid #dc3545; border-radius:3px; background:#fff; color:#dc3545; font-size:11px;">'
				. _('Fail') . '</button>';
			echo '</form>';
		}
		if ($row['gs1_barcode']) {
			echo ' <span title="' . htmlspecialchars($row['gs1_barcode'], ENT_QUOTES, 'UTF-8') . '" '
				. 'style="cursor:help; color:#0d6efd; font-size:11px;">&#128204; GS1</span>';
		}
		echo '</td>';

		end_row();
	}
	end_table(1);

	// --- Report Suspect Product ---
	echo '<h3 style="margin-top:20px;">' . _('Report Suspect Product') . '</h3>';
	echo '<div style="padding:15px; background:#fff3cd; border:1px solid #ffc107; border-radius:8px; margin-bottom:15px;">';
	echo '<p style="margin:0 0 10px 0; font-size:13px; color:#856404;">'
		. _('Report a product as suspect under DSCSA. The product will be quarantined and a transaction record created.')
		. '</p>';

	start_outer_table(TABLESTYLE2);
	stock_items_list_cells(_('Item:'), 'suspect_stock_id', null, _('Select item...'), false, true);
	text_row(_('Serial ID (if known):'), 'suspect_serial_id', '', 10, 11);
	text_row(_('Batch ID (if known):'), 'suspect_batch_id', '', 10, 11);
	textarea_row(_('Reason:'), 'suspect_reason', '', 60, 3);
	$actions = get_dscsa_quarantine_actions();
	array_selector_row(_('Action:'), 'suspect_action', 'quarantine', $actions);
	check_row(_('Auto-quarantine the item:'), 'suspect_quarantine', 1);
	end_outer_table(0);

	echo '</div>';
	submit_center('report_suspect', _('Report Suspect Product'), true, '', 'default');

	end_form();
}

// =====================================================================
// TAB: FSMA 204
// =====================================================================
if ($active_tab === 'fsma') {
	start_form();
	hidden('tab', 'fsma');

	// --- FSMA Statistics ---
	$stats = get_fsma_compliance_stats();
	echo '<div style="display:flex; gap:15px; margin-bottom:20px; flex-wrap:wrap;">';
	echo '<div style="flex:1; min-width:150px; padding:12px; border-radius:8px; background:#f8f9fa; border-left:4px solid #198754;">';
	echo '<div style="font-size:11px; color:#6c757d; text-transform:uppercase;">' . _('Total Events') . '</div>';
	echo '<div style="font-size:24px; font-weight:700; color:#198754;">' . $stats['total_events'] . '</div>';
	echo '</div>';
	echo '<div style="flex:1; min-width:150px; padding:12px; border-radius:8px; background:#f8f9fa; border-left:4px solid #0d6efd;">';
	echo '<div style="font-size:11px; color:#6c757d; text-transform:uppercase;">' . _('Lots Tracked') . '</div>';
	echo '<div style="font-size:24px; font-weight:700; color:#0d6efd;">' . $stats['lots_tracked'] . '</div>';
	echo '</div>';
	foreach (get_fsma_event_types() as $etype => $elabel) {
		$cnt = isset($stats['events_by_type'][$etype]) ? $stats['events_by_type'][$etype] : 0;
		$ec = get_fsma_event_type_color($etype);
		echo '<div style="flex:1; min-width:120px; padding:12px; border-radius:8px; background:#f8f9fa; border-left:4px solid ' . $ec . ';">';
		echo '<div style="font-size:11px; color:#6c757d; text-transform:uppercase;">' . $elabel . '</div>';
		echo '<div style="font-size:24px; font-weight:700; color:' . $ec . ';">' . $cnt . '</div>';
		echo '</div>';
	}
	echo '</div>';

	// --- Add CTE Event ---
	echo '<h3>' . _('Record Critical Tracking Event') . '</h3>';
	start_outer_table(TABLESTYLE2);

	$event_types = get_fsma_event_types();
	array_selector_row(_('Event Type:'), 'fsma_event_type', null, $event_types);
	stock_items_list_cells(_('Item:'), 'fsma_stock_id', null, _('Select item...'), false, true);
	text_row(_('Traceability Lot Code (TLC):'), 'fsma_lot_code', '', 50, 50);
	text_row(_('Batch ID:'), 'fsma_batch_id', '', 10, 11);
	date_row(_('Event Date:'), 'fsma_event_date', '', true);
	text_row(_('Quantity:'), 'fsma_quantity', '', 15, 20);
	text_row(_('Unit of Measure:'), 'fsma_unit', '', 20, 20);
	text_row(_('Location:'), 'fsma_location', '', 60, 200);
	text_row(_('Trading Partner:'), 'fsma_partner_name', '', 60, 100);

	$partner_types = array('' => '—', 'supplier' => _('Supplier'), 'customer' => _('Customer'), 'transporter' => _('Transporter'));
	array_selector_row(_('Partner Type:'), 'fsma_partner_type', null, $partner_types);

	text_row(_('Country of Origin:'), 'fsma_country', '', 50, 50);
	date_row(_('Harvest Date:'), 'fsma_harvest_date', '', true, 0, 0, 0, null, true);
	date_row(_('Pack Date:'), 'fsma_pack_date', '', true, 0, 0, 0, null, true);
	textarea_row(_('Notes:'), 'fsma_notes', '', 60, 3);

	end_outer_table(1);
	submit_center('add_fsma_event', _('Record Event'), true, '', 'default');

	// --- FSMA Event Listing ---
	echo '<h3 style="margin-top:20px;">' . _('Recent Critical Tracking Events') . '</h3>';
	$events = get_fsma_tracking_events();

	start_table(TABLESTYLE, "width='100%'");
	$th = array(_('ID'), _('Type'), _('Date'), _('Item'), _('Lot Code'), _('Qty'),
		_('Location'), _('Partner'), _('Notes'));
	table_header($th);

	$k = 0;
	$count = 0;
	while ($row = db_fetch($events)) {
		if (++$count > 50) break; // Limit display
		alt_table_row_color($k);

		label_cell($row['id']);
		$ec = get_fsma_event_type_color($row['event_type']);
		$el = get_fsma_event_type_label($row['event_type']);
		label_cell('<span style="display:inline-block; padding:2px 8px; border-radius:4px; background:' . $ec . '; color:#fff; font-size:11px;">' . $el . '</span>');
		label_cell(sql2date($row['event_date']));
		label_cell($row['stock_id'] . ' - ' . $row['item_name']);
		label_cell($row['lot_code'] ? $row['lot_code'] : '—');
		label_cell($row['quantity'] ? number_format2($row['quantity'], 2) : '—');
		label_cell($row['location_description'] ? $row['location_description'] : '—');
		label_cell($row['trading_partner_name'] ? $row['trading_partner_name'] : '—');
		label_cell($row['notes'] ? $row['notes'] : '—');
		end_row();
	}
	end_table(1);

	// --- Rapid Recall Lookup ---
	echo '<h3 style="margin-top:20px;">' . _('24-Hour Rapid Recall Lookup') . '</h3>';
	echo '<div style="padding:15px; background:#f8d7da; border:1px solid #f5c6cb; border-radius:8px; margin-bottom:15px;">';
	echo '<p style="margin:0 0 10px 0; font-size:13px; color:#721c24;">'
		. _('Enter an item and lot code to instantly identify all affected locations and customers.')
		. '</p>';

	start_outer_table(TABLESTYLE2);
	stock_items_list_cells(_('Item:'), 'recall_stock_id', null, _('Select item...'), false, true);
	text_row(_('Lot / TLC:'), 'recall_lot_code', get_post('recall_lot_code', ''), 50, 50);
	end_outer_table(0);
	echo '</div>';
	submit_center('fsma_recall_lookup', _('Lookup'), true, '', 'default');

	if (isset($_POST['fsma_recall_lookup']) && get_post('recall_stock_id') && get_post('recall_lot_code')) {
		$recall_data = fsma_rapid_recall_lookup(get_post('recall_stock_id'), get_post('recall_lot_code'));

		echo '<div style="margin-top:15px; padding:15px; background:#fff; border:1px solid #dee2e6; border-radius:8px;">';
		echo '<h4>' . _('Recall Lookup Results') . '</h4>';
		echo '<p>' . sprintf(_('Total events: %d | Shipping events: %d | Customers affected: %d | Total quantity: %s'),
			count($recall_data['affected_events']),
			count($recall_data['shipping_events']),
			count($recall_data['customer_list']),
			number_format2($recall_data['total_quantity'], 2)
		) . '</p>';

		if (!empty($recall_data['customer_list'])) {
			echo '<h5>' . _('Affected Customers') . '</h5>';
			echo '<ul>';
			foreach ($recall_data['customer_list'] as $c) {
				echo '<li><b>' . htmlspecialchars($c['name'], ENT_QUOTES, 'UTF-8') . '</b> (' . $c['type'] . ')</li>';
			}
			echo '</ul>';
		}
		if (!empty($recall_data['location_list'])) {
			echo '<h5>' . _('Affected Locations') . '</h5>';
			echo '<ul>';
			foreach ($recall_data['location_list'] as $lc => $ln) {
				echo '<li>' . htmlspecialchars($ln, ENT_QUOTES, 'UTF-8') . ' (' . $lc . ')</li>';
			}
			echo '</ul>';
		}
		echo '</div>';
	}

	end_form();
}

// =====================================================================
// TAB: UDI
// =====================================================================
if ($active_tab === 'udi') {
	start_form();
	hidden('tab', 'udi');

	// --- UDI Statistics ---
	$stats = get_udi_compliance_stats();
	echo '<div style="display:flex; gap:15px; margin-bottom:20px; flex-wrap:wrap;">';
	echo '<div style="flex:1; min-width:150px; padding:12px; border-radius:8px; background:#f8f9fa; border-left:4px solid #dc3545;">';
	echo '<div style="font-size:11px; color:#6c757d; text-transform:uppercase;">' . _('Total Registrations') . '</div>';
	echo '<div style="font-size:24px; font-weight:700; color:#dc3545;">' . $stats['total_registrations'] . '</div>';
	echo '</div>';
	echo '<div style="flex:1; min-width:150px; padding:12px; border-radius:8px; background:#f8f9fa; border-left:4px solid #28a745;">';
	echo '<div style="font-size:11px; color:#6c757d; text-transform:uppercase;">' . _('GUDID Submitted') . '</div>';
	echo '<div style="font-size:24px; font-weight:700; color:#28a745;">' . $stats['gudid_submitted'] . '</div>';
	echo '</div>';
	echo '<div style="flex:1; min-width:150px; padding:12px; border-radius:8px; background:#f8f9fa; border-left:4px solid #ffc107;">';
	echo '<div style="font-size:11px; color:#6c757d; text-transform:uppercase;">' . _('GUDID Pending') . '</div>';
	echo '<div style="font-size:24px; font-weight:700; color:#ffc107;">' . $stats['gudid_pending'] . '</div>';
	echo '</div>';
	echo '<div style="flex:1; min-width:150px; padding:12px; border-radius:8px; background:#f8f9fa; border-left:4px solid #6f42c1;">';
	echo '<div style="font-size:11px; color:#6c757d; text-transform:uppercase;">' . _('Implantable') . '</div>';
	echo '<div style="font-size:24px; font-weight:700; color:#6f42c1;">' . $stats['implantable_count'] . '</div>';
	echo '</div>';
	echo '</div>';

	// --- Add UDI Registration ---
	echo '<h3>' . _('Add UDI Registration') . '</h3>';
	start_outer_table(TABLESTYLE2);

	stock_items_list_cells(_('Item:'), 'udi_stock_id', null, _('Select item...'), false, true);
	text_row(_('Device Identifier (UDI-DI):'), 'udi_di_input', '', 50, 50);
	$agencies = get_udi_issuing_agencies();
	array_selector_row(_('Issuing Agency:'), 'udi_agency', 'GS1', $agencies);
	text_row(_('GTIN:'), 'udi_gtin', '', 14, 14);
	textarea_row(_('Device Description:'), 'udi_description', '', 60, 2);
	text_row(_('Brand Name:'), 'udi_brand', '', 60, 100);
	text_row(_('Version/Model:'), 'udi_model', '', 60, 100);
	text_row(_('Company Name:'), 'udi_company', get_company_pref('udi_company_name'), 60, 100);

	$mri_options = get_udi_mri_safety_options();
	array_selector_row(_('MRI Safety:'), 'udi_mri', '', $mri_options);

	check_row(_('Device is Sterile:'), 'udi_sterile', 0);
	check_row(_('Single Use:'), 'udi_single_use', 0);
	check_row(_('Implantable:'), 'udi_implantable', 0);
	check_row(_('Prescription Only (Rx):'), 'udi_rx_only', 0);
	check_row(_('Over-the-Counter (OTC):'), 'udi_otc', 0);

	text_row(_('FDA Listing Number:'), 'udi_fda_listing', '', 20, 20);
	text_row(_('FDA Premarket Number (510(k)/PMA):'), 'udi_fda_premarket', '', 20, 20);
	textarea_row(_('Notes:'), 'udi_notes', '', 60, 2);

	end_outer_table(1);
	submit_center('add_udi', _('Add Registration'), true, '', 'default');

	// --- UDI Listing ---
	echo '<h3 style="margin-top:20px;">' . _('UDI Registrations') . '</h3>';
	$registrations = get_udi_registrations();

	start_table(TABLESTYLE, "width='100%'");
	$th = array(_('ID'), _('Item'), _('UDI-DI'), _('Agency'), _('Brand'), _('GTIN'),
		_('Sterile'), _('Implantable'), _('GUDID'), _('Actions'));
	table_header($th);

	$k = 0;
	while ($row = db_fetch($registrations)) {
		alt_table_row_color($k);

		label_cell($row['id']);
		label_cell($row['stock_id'] . ' - ' . $row['item_name']);
		label_cell($row['udi_di']);
		label_cell($row['issuing_agency']);
		label_cell($row['brand_name'] ? $row['brand_name'] : '—');
		label_cell($row['gtin'] ? $row['gtin'] : '—');
		label_cell($row['device_sterile'] ? '<span style="color:#28a745;">&#10004;</span>' : '—');
		label_cell($row['implantable'] ? '<span style="color:#dc3545; font-weight:bold;">&#10004;</span>' : '—');
		label_cell($row['gudid_submitted'] ? '<span style="color:#28a745;">&#10004; ' . sql2date($row['gudid_submission_date']) . '</span>'
			: '<span style="color:#ffc107;">Pending</span>');

		echo '<td>';
		echo '<form method="post" style="display:inline;">';
		echo '<input type="hidden" name="_token" value="' . ensure_csrf_token() . '">';
		echo '<input type="hidden" name="tab" value="udi">';
		echo '<input type="hidden" name="sel_udi" value="' . $row['id'] . '">';
		echo '<button type="submit" name="delete_udi" onclick="return confirm(\'' . _('Delete this registration?') . '\');" '
			. 'style="cursor:pointer; padding:2px 8px; border:1px solid #dc3545; border-radius:3px; background:#fff; color:#dc3545; font-size:11px;">'
			. _('Delete') . '</button>';
		echo '</form>';
		echo '</td>';

		end_row();
	}
	end_table(1);

	end_form();
}

end_page();
