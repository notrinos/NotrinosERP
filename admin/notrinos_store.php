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
$page_security = 'SA_OPEN';
$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/packages.inc');

if ($SysPrefs->use_popup_windows)
	$js = get_js_open_window(900, 500);

page(_($help_context = 'Notrinos Store'), false, false, '', $js);

include_once($path_to_root.'/includes/ui.inc');

$can_manage_packages = $_SESSION['wa_current_user']->can_access('SA_CREATEMODULES');
$can_manage_languages = $_SESSION['wa_current_user']->can_access('SA_CREATELANGUAGE');

if (!$can_manage_packages && !$can_manage_languages) {
	display_error(_('The security settings on your account do not permit you to access this function.'));
	end_page();
	return;
}

$filters = notrinos_store_filters();

if ($id = find_submit('InstallPackage', false)) {
	if (!$can_manage_packages)
		display_error(_('You do not have permission to install extensions, themes, or charts from the store.'));
	elseif (install_extension($id)) {
		display_notification(sprintf(_("Package '%s' has been installed or updated."), $id));
		meta_forward(notrinos_store_action_url($filters));
	}
}

if ($id = find_submit('InstallLanguage', false)) {
	if (!$can_manage_languages)
		display_error(_('You do not have permission to install languages from the store.'));
	elseif (install_language($id)) {
		display_notification(sprintf(_("Language package '%s' has been installed or updated."), $id));
		meta_forward(notrinos_store_action_url($filters));
	}
}

$catalog = notrinos_store_catalog($filters);
$filtered_catalog = notrinos_store_filter_catalog($catalog, $filters);
$summary = notrinos_store_summary($filtered_catalog);

echo "<div class='notrinos-store-hero'>";
echo "<h2>"._('Notrinos Store')."</h2>";
echo "<p>"._('Browse repository packages from one place inside ERP. This first implementation slice brings extensions, themes, charts of accounts, and languages into a unified catalog while keeping the legacy installer pages available for local and manual packages.')."</p>";
echo "<div class='notrinos-store-meta'>";
echo "<span class='notrinos-store-tag'>".notrinos_store_escape(notrinos_store_repository_tag())."</span>";
echo "<span class='notrinos-store-tag'>"._('Catalog data now comes from notrinos.com while ERP install actions still use the repository installer')."</span>";
echo "</div>";
echo "</div>";

notrinos_store_render_filters($filters);

echo "<div class='notrinos-store-summary'>";
notrinos_store_render_summary_card($summary['total'], _('Visible packages'));
notrinos_store_render_summary_card($summary['installed'], _('Installed in ERP'));
notrinos_store_render_summary_card($summary['updates'], _('Updates available'));
notrinos_store_render_summary_card($summary['free'], _('Free packages'));
echo "</div>";

echo "<div class='notrinos-store-links'>";
// foreach (notrinos_store_type_definitions() as $type => $definition) {
// 	if ($type == 'all')
// 		continue;
// 	echo "<a href='".notrinos_store_escape(notrinos_store_legacy_url($type))."'>";
// 	echo notrinos_store_escape(sprintf(_('%s legacy page'), $definition['label']));
// 	echo "</a>";
// }
echo "</div>";

start_form(false, notrinos_store_action_url($filters), 'notrinos_store_actions');

if (!$summary['total']) {
	echo "<div class='notrinos-store-empty-state'>";
	echo "<h3>"._('No packages matched')."</h3>";
	echo "<p>"._('Adjust search terms, change pricing, or switch tabs to browse another package type.')."</p>";
	echo "</div>";
} else {
	notrinos_store_render_catalog_sections($filtered_catalog, $filters, $can_manage_packages, $can_manage_languages);
}

end_form();
end_page();

/**
 * Escape text for safe HTML output.
 *
 * @param mixed $value
 * @return string
 */
function notrinos_store_escape($value) {
	return htmlspecialchars((string) $value, ENT_QUOTES, 'UTF-8');
}

/**
 * Return store type definitions.
 *
 * @return array
 */
function notrinos_store_type_definitions() {
	return array(
		'all' => array('label' => _('All Packages')),
		'extension' => array('label' => _('Extensions')),
		'theme' => array('label' => _('Themes')),
		'chart' => array('label' => _('Charts of Accounts')),
		'language' => array('label' => _('Languages')),
	);
}

/**
 * Read the current store filters from the query string.
 *
 * @return array
 */
function notrinos_store_filters() {
	$definitions = notrinos_store_type_definitions();
	$type = isset($_GET['type']) ? $_GET['type'] : 'all';
	$price = isset($_GET['price']) ? $_GET['price'] : 'all';
	$sort = isset($_GET['sort']) ? $_GET['sort'] : 'popular';
	$search = isset($_GET['search']) ? trim($_GET['search']) : '';

	if (!isset($definitions[$type]))
		$type = 'all';
	if (!in_array($price, array('all', 'free', 'commercial')))
		$price = 'all';
	if (!in_array($sort, array('popular', 'name', 'updates', 'installed')))
		$sort = 'popular';

	return array(
		'type' => $type,
		'price' => $price,
		'sort' => $sort,
		'search' => $search,
	);
}

/**
 * Build the store form action URL while preserving the current filters.
 *
 * @param array $filters
 * @return string
 */
function notrinos_store_action_url($filters) {
	$params = array();

	if (isset($_GET['sel_app']) && $_GET['sel_app'] != '')
		$params['sel_app'] = $_GET['sel_app'];
	elseif (isset($_SESSION['sel_app']) && $_SESSION['sel_app'] != '')
		$params['sel_app'] = $_SESSION['sel_app'];

	if ($filters['type'] != 'all')
		$params['type'] = $filters['type'];
	if ($filters['price'] != 'all')
		$params['price'] = $filters['price'];
	if ($filters['sort'] != 'popular')
		$params['sort'] = $filters['sort'];
	if ($filters['search'] != '')
		$params['search'] = $filters['search'];

	$query = http_build_query($params);
	return $_SERVER['PHP_SELF'].($query != '' ? '?'.$query : '');
}

/**
 * Return the legacy page URL for a package type.
 *
 * @param string $type
 * @return string
 */
function notrinos_store_legacy_url($type) {
	return get_legacy_package_manager_url($type);
}

/**
 * Return a short repository status tag for the hero banner.
 *
 * @return string
 */
function notrinos_store_repository_tag() {
	global $repo_auth;

	$branch = isset($repo_auth['branch']) ? trim($repo_auth['branch']) : '';
	if ($branch == '')
		return _('Repository branch is not configured');

	return sprintf(_('Repository branch: %s'), $branch);
}

/**
 * Build the unified store catalog.
 *
 * @return array
 */
function notrinos_store_catalog($filters = array()) {
	$api_catalog = notrinos_store_catalog_from_api($filters);
	if ($api_catalog !== null)
		return $api_catalog;

	return notrinos_store_catalog_from_repository();
}

/**
 * Return whether filters are broad enough for repository fallback on empty API results.
 *
 * @param array $filters
 * @return bool
 */
function notrinos_store_allow_empty_api_fallback($filters) {
	return isset($filters['search'], $filters['price'])
		&& $filters['search'] == ''
		&& $filters['price'] == 'all';
}

/**
 * Build the unified store catalog from the legacy repository helpers.
 *
 * @return array
 */
function notrinos_store_catalog_from_repository() {
	$catalog = array();
	$catalog['extension'] = notrinos_store_remote_only_items(notrinos_store_normalize_packages('extension', get_store_extensions_list('extension')));
	$catalog['theme'] = notrinos_store_remote_only_items(notrinos_store_normalize_packages('theme', get_store_themes_list()));
	$catalog['chart'] = notrinos_store_remote_only_items(notrinos_store_normalize_packages('chart', get_store_charts_list()));
	$catalog['language'] = notrinos_store_remote_only_items(notrinos_store_normalize_packages('language', get_store_languages_list()));

	return $catalog;
}

/**
 * Remove local-only package rows from a catalog slice.
 *
 * @param array $items
 * @return array
 */
function notrinos_store_remote_only_items($items) {
	$remote_items = array();

	foreach ($items as $package_id => $item)
		if (!empty($item['has_available']) || (isset($item['available_version']) && trim((string) $item['available_version']) != ''))
			$remote_items[$package_id] = $item;

	return $remote_items;
}

/**
 * Build the catalog query sent to the website API.
 *
 * @param array $filters
 * @return array
 */
function notrinos_store_api_query_params($filters) {
	global $repo_auth;

	$params = array(
		'compatibility' => isset($repo_auth['branch']) ? trim((string) $repo_auth['branch']) : '',
		'limit' => 0,
		'sort' => 'featured',
	);

	if ($filters['type'] != 'all')
		$params['type'] = $filters['type'];
	if ($filters['search'] != '')
		$params['search'] = $filters['search'];
	if ($filters['price'] == 'free')
		$params['pricing'] = 'free';
	elseif ($filters['price'] == 'commercial')
		$params['pricing'] = 'commercial';

	if ($filters['sort'] == 'name')
		$params['sort'] = 'name';
	elseif ($filters['sort'] == 'popular')
		$params['sort'] = 'downloads';

	return $params;
}

/**
 * Return the local ERP package maps keyed by package name.
 *
 * @return array
 */
function notrinos_store_local_package_lists() {
	return array(
		'extension' => get_local_extensions_list('extension'),
		'theme' => get_local_themes_list(),
		'chart' => get_local_charts_list(),
		'language' => get_local_languages_list(),
	);
}

/**
 * Build the unified store catalog from the website storefront API.
 *
 * @param array $filters
 * @return array|null
 */
function notrinos_store_catalog_from_api($filters) {
	$params = notrinos_store_api_query_params($filters);
	$response = get_package_storefront_catalog($params);
	if (!is_array($response) || !isset($response['packages']) || !is_array($response['packages']))
		return null;

	$packages = $response['packages'];
	if (!count($packages) && isset($params['compatibility']) && trim((string) $params['compatibility']) != '') {
		unset($params['compatibility']);
		$response = get_package_storefront_catalog($params);
		if (!is_array($response) || !isset($response['packages']) || !is_array($response['packages']))
			return null;
		$packages = $response['packages'];
	}

	if (!count($packages) && notrinos_store_allow_empty_api_fallback($filters))
		return null;

	$catalog = array();
	$local_lists = notrinos_store_local_package_lists();
	foreach (notrinos_store_type_definitions() as $type => $definition)
		if ($type != 'all')
			$catalog[$type] = array();

	foreach ($packages as $package) {
		$type = isset($package['type']) ? trim((string) $package['type']) : '';
		$package_id = isset($package['package_name']) ? trim((string) $package['package_name']) : '';
		if ($type == '' || $package_id == '' || !isset($local_lists[$type]))
			continue;

		$local_package = isset($local_lists[$type][$package_id]) ? $local_lists[$type][$package_id] : array();
		$catalog[$type][$package_id] = notrinos_store_normalize_api_package($type, $package, $local_package);
	}

	return $catalog;
}

/**
 * Normalize one API package for the ERP store UI.
 *
 * @param string $type
 * @param array $package
 * @param array $local_package
 * @return array
 */
function notrinos_store_normalize_api_package($type, $package, $local_package) {
	$package_id = trim((string) $package['package_name']);
	$description = isset($package['description']) ? trim((string) $package['description']) : '';
	if ($description == '' && isset($package['content']))
		$description = trim((string) $package['content']);

	$installed_version = isset($local_package['version']) ? trim((string) $local_package['version']) : '';
	$is_installed = !empty($local_package);
	$available_version = isset($package['version']) ? trim((string) $package['version']) : '';
	$downloads = isset($package['downloads']) ? (int) $package['downloads'] : -1;
	$is_free = isset($package['pricing_model']) && $package['pricing_model'] == 'free';
	$has_update = check_pkg_upgrade($installed_version, $available_version);

	$store_metadata = array(
		'pricing_model' => isset($package['pricing_model']) ? $package['pricing_model'] : '',
		'homepage' => isset($package['homepage']) ? $package['homepage'] : '',
		'docs_url' => isset($package['docs_url']) ? $package['docs_url'] : '',
		'demo_url' => isset($package['demo_url']) ? $package['demo_url'] : '',
		'support_url' => isset($package['support_url']) ? $package['support_url'] : '',
		'changelog_url' => isset($package['changelog_url']) ? $package['changelog_url'] : '',
		'compatibility_min' => isset($package['compatibility_min']) ? $package['compatibility_min'] : '',
		'compatibility_max' => isset($package['compatibility_max']) ? $package['compatibility_max'] : '',
		'release_date' => isset($package['release_date']) ? $package['release_date'] : '',
		'categories' => isset($package['categories']) ? (array) $package['categories'] : array(),
		'tags' => isset($package['tags']) ? (array) $package['tags'] : array(),
		'publisher_name' => isset($package['publisher_name']) ? $package['publisher_name'] : '',
		'publisher_flags' => isset($package['publisher_flags']) ? (array) $package['publisher_flags'] : array(),
		'trust_flags' => isset($package['trust_flags']) ? (array) $package['trust_flags'] : array(),
		'pictures' => isset($package['pictures']) ? (array) $package['pictures'] : array(),
	);

	$search_text = implode(' ', array(
		$package_id,
		isset($package['name']) ? $package['name'] : '',
		$description,
		implode(' ', $store_metadata['categories']),
		implode(' ', $store_metadata['tags']),
		$store_metadata['publisher_name'],
		$store_metadata['compatibility_min'],
		$store_metadata['compatibility_max'],
		implode(' ', $store_metadata['publisher_flags']),
		implode(' ', $store_metadata['trust_flags']),
	));

	$item = array(
		'type' => $type,
		'package' => $package_id,
		'name' => isset($package['name']) && trim((string) $package['name']) != '' ? $package['name'] : $package_id,
		'description' => $description,
		'installed_version' => $installed_version,
		'available_version' => $available_version,
		'price_label' => isset($package['price_label']) && trim((string) $package['price_label']) != '' ? $package['price_label'] : '-',
		'downloads' => $downloads,
		'downloads_label' => $downloads >= 0 ? (string) $downloads : '-',
		'is_free' => $is_free,
		'is_installed' => $is_installed,
		'has_available' => $available_version != '',
		'has_update' => $has_update,
		'store_metadata' => $store_metadata,
		'search_text' => strtolower(trim($search_text)),
		'installed_label' => notrinos_store_installed_label($installed_version, $is_installed),
		'available_label' => $available_version != '' ? $available_version : _('Local/manual'),
		'button_name' => notrinos_store_button_name($type, $package_id, $has_update),
		'button_label' => $is_installed ? _('Update') : _('Install'),
		'button_title' => notrinos_store_button_title($type),
	);

	if (!$has_update)
		$item['button_name'] = '';

	return $item;
}

/**
 * Normalize one local-only package for the ERP store UI.
 *
 * @param string $type
 * @param string $package_id
 * @param array $package
 * @return array
 */
function notrinos_store_normalize_local_only_package($type, $package_id, $package) {
	$store_metadata = get_package_store_metadata($package);
	$description = '';
	if (isset($package['Descr']))
		$description = package_meta_text($package['Descr']);
	elseif (isset($package['Description']))
		$description = package_meta_text($package['Description']);

	$installed_version = isset($package['version']) ? trim((string) $package['version']) : '';
	$is_installed = !empty($package);
	$search_text = implode(' ', array(
		$package_id,
		isset($package['name']) ? $package['name'] : '',
		$description,
		implode(' ', $store_metadata['categories']),
		implode(' ', $store_metadata['tags']),
		$store_metadata['publisher_name'],
		$store_metadata['compatibility_min'],
		$store_metadata['compatibility_max'],
		implode(' ', $store_metadata['publisher_flags']),
		implode(' ', $store_metadata['trust_flags'])
	));

	return array(
		'type' => $type,
		'package' => $package_id,
		'name' => isset($package['name']) && trim((string) $package['name']) != '' ? $package['name'] : $package_id,
		'description' => $description,
		'installed_version' => $installed_version,
		'available_version' => '',
		'price_label' => get_package_price_label($package, '-'),
		'downloads' => isset($package['downloads']) && $package['downloads'] !== null ? (int) $package['downloads'] : -1,
		'downloads_label' => isset($package['downloads']) && $package['downloads'] !== null ? (string) ((int) $package['downloads']) : '-',
		'is_free' => package_meta_flag_enabled(@$package['IsFree']) || package_meta_flag_enabled(@$package['is_free']),
		'is_installed' => $is_installed,
		'has_available' => false,
		'has_update' => false,
		'store_metadata' => $store_metadata,
		'search_text' => strtolower(trim($search_text)),
		'installed_label' => notrinos_store_installed_label($installed_version, $is_installed),
		'available_label' => _('Local/manual'),
		'button_name' => '',
		'button_label' => $is_installed ? _('Update') : _('Install'),
		'button_title' => notrinos_store_button_title($type),
	);
}

/**
 * Normalize a package list for the store UI.
 *
 * @param string $type
 * @param array $packages
 * @return array
 */
function notrinos_store_normalize_packages($type, $packages) {
	$items = array();

	foreach ($packages as $package_id => $package) {
		if ($type == 'language' && isset($package['code']) && $package['code'] == 'C')
			continue;

		$store_metadata = get_package_store_metadata($package);

		$description = '';
		if (isset($package['Descr']))
			$description = package_meta_text($package['Descr']);
		elseif (isset($package['Description']))
			$description = package_meta_text($package['Description']);

		$installed_version = isset($package['version']) ? trim((string) $package['version']) : '';
		$available_version = isset($package['available']) ? trim((string) $package['available']) : '';
		$is_installed = isset($package['local_id']) || $installed_version != '';
		$is_free = package_meta_flag_enabled(@$package['IsFree']) || package_meta_flag_enabled(@$package['is_free']);
		$price_label = get_package_price_label($package, '-');
		$downloads = isset($package['downloads']) && $package['downloads'] !== null ? (int) $package['downloads'] : -1;
		$has_update = check_pkg_upgrade($installed_version, $available_version);
		$search_text = implode(' ', array(
			$package_id,
			isset($package['name']) ? $package['name'] : '',
			$description,
			implode(' ', $store_metadata['categories']),
			implode(' ', $store_metadata['tags']),
			$store_metadata['publisher_name'],
			$store_metadata['compatibility_min'],
			$store_metadata['compatibility_max'],
			implode(' ', $store_metadata['publisher_flags']),
			implode(' ', $store_metadata['trust_flags'])
		));

		$items[$package_id] = array(
			'type' => $type,
			'package' => $package_id,
			'name' => isset($package['name']) && trim($package['name']) != '' ? $package['name'] : $package_id,
			'description' => $description,
			'installed_version' => $installed_version,
			'available_version' => $available_version,
			'price_label' => $price_label != '' ? $price_label : '-',
			'downloads' => $downloads,
			'downloads_label' => $downloads >= 0 ? (string) $downloads : '-',
			'is_free' => $is_free,
			'is_installed' => $is_installed,
			'has_available' => $available_version != '',
			'has_update' => $has_update,
			'store_metadata' => $store_metadata,
			'search_text' => strtolower(trim($search_text)),
			'installed_label' => notrinos_store_installed_label($installed_version, $is_installed),
			'available_label' => $available_version != '' ? $available_version : _('Local/manual'),
			'button_name' => notrinos_store_button_name($type, $package_id, $has_update),
			'button_label' => $is_installed ? _('Update') : _('Install'),
			'button_title' => notrinos_store_button_title($type),
		);
		if (!$has_update)
			$items[$package_id]['button_name'] = '';
	}

	return $items;
}

/**
 * Build the installed status label for a store row.
 *
 * @param string $installed_version
 * @param bool $is_installed
 * @return string
 */
function notrinos_store_installed_label($installed_version, $is_installed) {
	if (!$is_installed)
		return _('None');
	if ($installed_version == '-')
		return _('Local/manual');
	if ($installed_version != '')
		return $installed_version;
	return _('Installed');
}

/**
 * Build a submit button name for a store row.
 *
 * @param string $type
 * @param string $package_id
 * @param bool $has_update
 * @return string
 */
function notrinos_store_button_name($type, $package_id, $has_update) {
	if (!$has_update)
		return '';

	if ($type == 'language')
		return 'InstallLanguage'.$package_id;

	return 'InstallPackage'.$package_id;
}

/**
 * Return the shared button title for a package type.
 *
 * @param string $type
 * @return string
 */
function notrinos_store_button_title($type) {
	if ($type == 'language')
		return _('Install or update this language package from the repository.');

	return _('Install or update this repository package.');
}

/**
 * Apply the current store filters and sort order.
 *
 * @param array $catalog
 * @param array $filters
 * @return array
 */
function notrinos_store_filter_catalog($catalog, $filters) {
	global $notrinos_store_sort;

	$filtered = array();
	$notrinos_store_sort = $filters['sort'];

	foreach ($catalog as $type => $items) {
		if ($filters['type'] != 'all' && $filters['type'] != $type)
			continue;

		foreach ($items as $package_id => $item)
			if (notrinos_store_matches_filters($item, $filters))
				$filtered[$type][$package_id] = $item;

		if (isset($filtered[$type]))
			uasort($filtered[$type], 'notrinos_store_compare_items');
	}

	return $filtered;
}

/**
 * Determine whether a package matches the current filters.
 *
 * @param array $item
 * @param array $filters
 * @return bool
 */
function notrinos_store_matches_filters($item, $filters) {
	if ($filters['price'] == 'free' && !$item['is_free'])
		return false;
	if ($filters['price'] == 'commercial' && $item['is_free'])
		return false;

	if ($filters['search'] != '') {
		$needle = strtolower($filters['search']);
		$haystack = strtolower(
			$item['search_text'].' '
			.$item['installed_label'].' '
			.$item['available_label'].' '
			.$item['price_label']
		);

		if (strpos($haystack, $needle) === false)
			return false;
	}

	return true;
}

/**
 * Compare two store rows using the active sort mode.
 *
 * @param array $left
 * @param array $right
 * @return int
 */
function notrinos_store_compare_items($left, $right) {
	global $notrinos_store_sort;

	$comparison = 0;
	switch ($notrinos_store_sort) {
		case 'name':
			$comparison = strcasecmp($left['name'], $right['name']);
			break;

		case 'updates':
			$comparison = (int) $right['has_update'] - (int) $left['has_update'];
			if ($comparison == 0)
				$comparison = notrinos_store_sort_number($right['downloads']) - notrinos_store_sort_number($left['downloads']);
			break;

		case 'installed':
			$comparison = (int) $right['is_installed'] - (int) $left['is_installed'];
			if ($comparison == 0)
				$comparison = (int) $right['has_update'] - (int) $left['has_update'];
			break;

		case 'popular':
		default:
			$comparison = notrinos_store_sort_number($right['downloads']) - notrinos_store_sort_number($left['downloads']);
			if ($comparison == 0)
				$comparison = (int) $right['has_update'] - (int) $left['has_update'];
			break;
	}

	if ($comparison == 0)
		$comparison = strcasecmp($left['name'], $right['name']);

	return $comparison;
}

/**
 * Normalize numeric sort values.
 *
 * @param int $value
 * @return int
 */
function notrinos_store_sort_number($value) {
	return $value >= 0 ? (int) $value : -1;
}

/**
 * Calculate summary counts for the current catalog view.
 *
 * @param array $catalog
 * @return array
 */
function notrinos_store_summary($catalog) {
	$summary = array(
		'total' => 0,
		'installed' => 0,
		'updates' => 0,
		'free' => 0,
	);

	foreach ($catalog as $items) {
		foreach ($items as $item) {
			$summary['total']++;
			if ($item['is_installed'])
				$summary['installed']++;
			if ($item['has_update'])
				$summary['updates']++;
			if ($item['is_free'])
				$summary['free']++;
		}
	}

	return $summary;
}

/**
 * Render the GET filter toolbar.
 *
 * @param array $filters
 * @return void
 */
function notrinos_store_render_filters($filters) {
	$type_definitions = notrinos_store_type_definitions();
	$price_options = array(
		'all' => _('All pricing'),
		'free' => _('Free only'),
		'commercial' => _('Commercial only'),
	);
	$sort_options = array(
		'popular' => _('Most downloaded'),
		'updates' => _('Updates first'),
		'installed' => _('Installed first'),
		'name' => _('Name'),
	);

	echo "<div class='notrinos-store-tabs'>";
	foreach ($type_definitions as $type => $definition)
		echo "<a class='notrinos-store-tab".($filters['type'] == $type ? " active" : "")."' href='".notrinos_store_escape(notrinos_store_tab_url($filters, $type))."'>".notrinos_store_escape($definition['label'])."</a>";
	echo "</div>";

	echo "<form method='get' action='".notrinos_store_escape($_SERVER['PHP_SELF'])."' class='notrinos-store-filters'>";
	if (isset($_GET['sel_app']) && $_GET['sel_app'] != '')
		echo hidden('sel_app', notrinos_store_escape($_GET['sel_app']), false);
	echo hidden('type', notrinos_store_escape($filters['type']), false);

	echo "<div class='notrinos-store-filter'>";
	echo "<label for='store_search'>"._('Search')."</label>";
	echo text_input('search', notrinos_store_escape($filters['search']), 28, 200, _('Package, name, description'), "id='store_search' placeholder='".notrinos_store_escape(_('Package, name, description'))."'");
	echo "</div>";

	echo "<div class='notrinos-store-filter'>";
	echo "<label for='store_price'>"._('Pricing')."</label>";
	echo "<span id='store_price'>";
	echo array_selector('price', $filters['price'], $price_options, array('class' => array('store-filter-select')));
	echo "</span>";
	echo "</div>";

	echo "<div class='notrinos-store-filter'>";
	echo "<label for='store_sort'>"._('Sort')."</label>";
	echo "<span id='store_sort'>";
	echo array_selector('sort', $filters['sort'], $sort_options, array('class' => array('store-filter-select')));
	echo "</span>";
	echo "</div>";

	echo "<div class='notrinos-store-filter-actions'>";
	submit('ApplyStoreFilters', _('Apply filters'));
	echo "<a href='".notrinos_store_escape(notrinos_store_filter_reset_url($filters))."'>"._('Clear')."</a>";
	echo "</div>";
	echo "</form>";
}

/**
 * Build the URL for one store type tab.
 *
 * @param array $filters
 * @param string $type
 * @return string
 */
function notrinos_store_tab_url($filters, $type) {
	$tab_filters = $filters;
	$tab_filters['type'] = $type;

	return notrinos_store_action_url($tab_filters);
}

/**
 * Build the filter reset URL while keeping the active tab.
 *
 * @param array $filters
 * @return string
 */
function notrinos_store_filter_reset_url($filters) {
	return notrinos_store_action_url(array(
		'type' => isset($filters['type']) ? $filters['type'] : 'all',
		'price' => 'all',
		'sort' => 'popular',
		'search' => '',
	));
}

/**
 * Render one summary card.
 *
 * @param int $value
 * @param string $label
 * @return void
 */
function notrinos_store_render_summary_card($value, $label) {
	echo "<div class='notrinos-store-card'>";
	echo "<strong>".notrinos_store_escape($value)."</strong>";
	echo "<span>".notrinos_store_escape($label)."</span>";
	echo "</div>";
}

/**
 * Render catalog sections using website-style cards.
 *
 * @param array $catalog
 * @param array $filters
 * @param bool $can_manage_packages
 * @param bool $can_manage_languages
 * @return void
 */
function notrinos_store_render_catalog_sections($catalog, $filters, $can_manage_packages, $can_manage_languages) {
	foreach (notrinos_store_type_definitions() as $type => $definition) {
		if ($type == 'all' || !isset($catalog[$type]) || !count($catalog[$type]))
			continue;
		if ($filters['type'] != 'all' && $filters['type'] != $type)
			continue;

		$visible_items = notrinos_store_section_items($catalog[$type], $filters, $type);

		echo "<div class='notrinos-store-section'>";
		echo "<div class='notrinos-store-section-head'>";
		echo "<div class='notrinos-store-section-head-main'>";
		echo "<h3>".notrinos_store_escape($definition['label'])."</h3>";
		echo "<div class='notrinos-store-section-note'>".notrinos_store_escape(notrinos_store_section_note($type))."</div>";
		echo "</div>";
		echo "<div class='notrinos-store-section-head-actions'>";
		echo "<div class='notrinos-store-section-count'>".notrinos_store_escape(sprintf(_('%s packages'), count($catalog[$type])))."</div>";
		if (notrinos_store_show_view_all($filters, $type))
			echo "<a class='notrinos-store-view-all' href='".notrinos_store_escape(notrinos_store_tab_url($filters, $type))."'>"._('View All')."</a>";
		echo "</div>";
		echo "</div>";
		echo "<div class='storefront-card-grid'>";
		foreach ($visible_items as $item)
			notrinos_store_render_package_card($item, $can_manage_packages, $can_manage_languages);
		echo "</div>";
		echo "</div>";
	}
}

/**
 * Return the packages visible in one section.
 *
 * @param array $items
 * @param array $filters
 * @param string $type
 * @return array
 */
function notrinos_store_section_items($items, $filters, $type) {
	if ($filters['type'] == 'all' && $type != 'all')
		return array_slice($items, 0, 8, true);

	return $items;
}

/**
 * Return whether a section should display a View All button.
 *
 * @param array $filters
 * @param string $type
 * @return bool
 */
function notrinos_store_show_view_all($filters, $type) {
	return $filters['type'] == 'all' && $type != 'all';
}

/**
 * Return the helper note shown under a package section heading.
 *
 * @param string $type
 * @return string
 */
function notrinos_store_section_note($type) {
	$definitions = notrinos_store_type_definitions();
	$label = isset($definitions[$type]) ? $definitions[$type]['label'] : $type;

	return sprintf(_('%s published in the remote repository. Use the legacy page for local or manual package management.'), $label);
}

/**
 * Render one package card in the ERP store catalog.
 *
 * @param array $item
 * @param bool $can_manage_packages
 * @param bool $can_manage_languages
 * @return void
 */
function notrinos_store_render_package_card($item, $can_manage_packages, $can_manage_languages) {
	$type_definitions = notrinos_store_type_definitions();
	$type_label = isset($type_definitions[$item['type']]) ? $type_definitions[$item['type']]['label'] : ucfirst($item['type']);
	$detail_url = notrinos_store_package_detail_url($item);
	$image_url = notrinos_store_package_image_url($item);
	$compatibility = notrinos_store_package_compatibility_label($item);
	$card_links = notrinos_store_package_links($item);
	$can_install = $item['button_name'] != '' && (($item['type'] == 'language' && $can_manage_languages) || ($item['type'] != 'language' && $can_manage_packages));

	echo "<article class='storefront-card'>";
	if ($detail_url != '')
		echo "<a class='storefront-card-media' href='".notrinos_store_escape($detail_url)."' target='_blank' rel='noopener'>";
	else
		echo "<div class='storefront-card-media'>";

	if ($image_url != '')
		echo "<img src='".notrinos_store_escape($image_url)."' alt='".notrinos_store_escape($item['name'])."'>";
	else
		echo "<span class='storefront-card-placeholder'>".notrinos_store_escape($type_label)."</span>";

	if ($detail_url != '')
		echo "</a>";
	else
		echo "</div>";

	echo "<div class='storefront-card-body'>";
	echo "<p class='storefront-card-kicker'>".notrinos_store_escape(notrinos_store_package_category_label($item, $type_label))."</p>";
	echo "<h3>";
	if ($detail_url != '')
		echo "<a href='".notrinos_store_escape($detail_url)."' target='_blank' rel='noopener'>".notrinos_store_escape($item['name'])."</a>";
	else
		echo notrinos_store_escape($item['name']);
	echo "</h3>";
	echo "<p class='storefront-card-copy'>".notrinos_store_escape(notrinos_store_excerpt($item['description']))."</p>";
	notrinos_store_render_package_badges($item);
	echo "<div class='storefront-card-meta'>";
	if ($item['downloads_label'] != '-')
		echo "<span>".notrinos_store_escape(sprintf(_('%s downloads'), $item['downloads_label']))."</span>";
	echo "<span>".notrinos_store_escape(sprintf(_('Installed: %s'), $item['installed_label']))."</span>";
	echo "<span>".notrinos_store_escape(sprintf(_('Available: %s'), $item['available_label']))."</span>";
	echo "</div>";

	if ($compatibility != '')
		echo "<p class='storefront-card-compatibility'>".notrinos_store_escape(sprintf(_('Compatible with NotrinosERP %s'), $compatibility))."</p>";

	echo "<div class='storefront-card-actions'>";
	if ($can_install)
		echo "<button type='submit' class='notrinos-store-install-button' name='".notrinos_store_escape($item['button_name'])."' value='1' title='".notrinos_store_escape($item['button_title'])."'><span>".notrinos_store_escape($item['button_label'])."</span></button>";
	elseif ($item['button_name'] != '')
		echo "<span class='notrinos-store-install-button-disabled'>".notrinos_store_escape(_('Install permission required'))."</span>";

	foreach ($card_links as $label => $url)
		echo "<a class='storefront-inline-link' href='".notrinos_store_escape($url)."' target='_blank' rel='noopener'>".notrinos_store_escape($label)."</a>";

	echo "</div>";
	echo "</div>";
	echo "</article>";
}

/**
 * Render storefront-style badges for one ERP package card.
 *
 * @param array $item
 * @return void
 */
function notrinos_store_render_package_badges($item) {
	$badges = array();

	if ($item['has_update'])
		$badges[] = array('label' => _('Update available'), 'class' => 'storefront-badge storefront-badge--featured');
	if ($item['is_installed'])
		$badges[] = array('label' => _('Installed'), 'class' => 'storefront-badge storefront-badge--trust');
	if (notrinos_store_metadata_has_flag(isset($item['store_metadata']['tags']) ? $item['store_metadata']['tags'] : array(), array('featured')))
		$badges[] = array('label' => _('Featured'), 'class' => 'storefront-badge storefront-badge--featured');
	if (notrinos_store_metadata_has_flag(isset($item['store_metadata']['publisher_flags']) ? $item['store_metadata']['publisher_flags'] : array(), array('verified_publisher', 'verified')))
		$badges[] = array('label' => _('Verified publisher'), 'class' => 'storefront-badge');
	if (notrinos_store_metadata_has_flag(isset($item['store_metadata']['publisher_flags']) ? $item['store_metadata']['publisher_flags'] : array(), array('partner_ready', 'partner')))
		$badges[] = array('label' => _('Partner ready'), 'class' => 'storefront-badge');
	if (notrinos_store_metadata_has_flag(isset($item['store_metadata']['trust_flags']) ? $item['store_metadata']['trust_flags'] : array(), array('fulfilled_by_notrinos', 'fulfilled_notrinos', 'notrinos_fulfilled')))
		$badges[] = array('label' => _('Fulfilled by Notrinos'), 'class' => 'storefront-badge storefront-badge--trust');
	if (notrinos_store_metadata_has_flag(isset($item['store_metadata']['trust_flags']) ? $item['store_metadata']['trust_flags'] : array(), array('signed_package', 'signed')))
		$badges[] = array('label' => _('Signed package'), 'class' => 'storefront-badge');
	if ($item['price_label'] != '' && $item['price_label'] != '-')
		$badges[] = array('label' => $item['price_label'], 'class' => 'storefront-badge storefront-badge--price');

	if (!count($badges))
		return;

	echo "<div class='storefront-card-badges'>";
	foreach ($badges as $badge)
		echo "<span class='".notrinos_store_escape($badge['class'])."'>".notrinos_store_escape($badge['label'])."</span>";
	echo "</div>";
}

/**
 * Return whether any metadata value matches a known flag alias.
 *
 * @param array $values
 * @param array $aliases
 * @return bool
 */
function notrinos_store_metadata_has_flag($values, $aliases) {
	$normalized_values = array();

	foreach ((array) $values as $value)
		$normalized_values[notrinos_store_normalize_flag((string) $value)] = true;

	foreach ((array) $aliases as $alias)
		if (isset($normalized_values[notrinos_store_normalize_flag((string) $alias)]))
			return true;

	return false;
}

/**
 * Normalize one metadata flag into a comparison key.
 *
 * @param string $value
 * @return string
 */
function notrinos_store_normalize_flag($value) {
	return preg_replace('/[^a-z0-9]+/', '', strtolower(trim((string) $value)));
}

/**
 * Return the website storefront detail URL for one package.
 *
 * @param array $item
 * @return string
 */
function notrinos_store_package_detail_url($item) {
	$base_url = get_package_storefront_base_url();
	if ($base_url == '' || empty($item['package']) || empty($item['type']))
		return '';

	return $base_url.'/downloads.php?'.http_build_query(array('package' => $item['package'], 'type' => $item['type']));
}

/**
 * Return the first package screenshot URL when available.
 *
 * @param array $item
 * @return string
 */
function notrinos_store_package_image_url($item) {
	$pictures = isset($item['store_metadata']['pictures']) ? (array) $item['store_metadata']['pictures'] : array();

	foreach ($pictures as $picture) {
		$picture = trim((string) $picture);
		if ($picture != '')
			return $picture;
	}

	return '';
}

/**
 * Return the package category label used by the card kicker.
 *
 * @param array $item
 * @param string $fallback
 * @return string
 */
function notrinos_store_package_category_label($item, $fallback) {
	$categories = isset($item['store_metadata']['categories']) ? (array) $item['store_metadata']['categories'] : array();

	foreach ($categories as $category) {
		$category = trim((string) $category);
		if ($category != '')
			return $category;
	}

	return $fallback;
}

/**
 * Return the publisher label shown in a package card.
 *
 * @param array $item
 * @return string
 */
function notrinos_store_package_publisher($item) {
	$publisher = isset($item['store_metadata']['publisher_name']) ? trim((string) $item['store_metadata']['publisher_name']) : '';

	return $publisher != '' ? $publisher : _('Notrinos');
}

/**
 * Return the compatibility label for a package card.
 *
 * @param array $item
 * @return string
 */
function notrinos_store_package_compatibility_label($item) {
	$minimum = isset($item['store_metadata']['compatibility_min']) ? trim((string) $item['store_metadata']['compatibility_min']) : '';
	$maximum = isset($item['store_metadata']['compatibility_max']) ? trim((string) $item['store_metadata']['compatibility_max']) : '';

	return trim($minimum.' - '.$maximum, ' -');
}

/**
 * Return a human-readable release label for a package card.
 *
 * @param array $item
 * @return string
 */
function notrinos_store_package_release_label($item) {
	$release_date = isset($item['store_metadata']['release_date']) ? trim((string) $item['store_metadata']['release_date']) : '';
	if ($release_date == '')
		return '';

	$timestamp = @strtotime($release_date);
	return $timestamp ? date('M d, Y', $timestamp) : $release_date;
}

/**
 * Return the external links shown in a package card.
 *
 * @param array $item
 * @return array
 */
function notrinos_store_package_links($item) {
	$links = array();
	$detail_url = notrinos_store_package_detail_url($item);
	$metadata = isset($item['store_metadata']) ? $item['store_metadata'] : array();

	if ($detail_url != '')
		$links[_('Details')] = $detail_url;
	if (!empty($metadata['demo_url']))
		$links[_('Demo')] = $metadata['demo_url'];
	if (!empty($metadata['support_url']))
		$links[_('Support')] = $metadata['support_url'];
	if (!empty($metadata['changelog_url']))
		$links[_('Changelog')] = $metadata['changelog_url'];

	return array_slice($links, 0, 4, true);
}

/**
 * Create a compact description excerpt.
 *
 * @param string $text
 * @param int $limit
 * @return string
 */
function notrinos_store_excerpt($text, $limit=120) {
	$text = trim(preg_replace('/\s+/', ' ', (string) $text));
	if ($text == '')
		return _('No description available.');
	if (strlen($text) <= $limit)
		return $text;

	return rtrim(substr($text, 0, $limit - 3)).'...';
}
