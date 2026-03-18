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
 * Modern semantic renderer with sidebar navigation and responsive shell.
 */
class renderer {
	/**
	 * Build inline SVG icon markup.
	 *
	 * @param string $name
	 * @param string $class_name
	 * @return string
	 */
	function icon_svg($name, $class_name = 'modern-icon') {
		$icons = array(
			'activity' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"></polyline></svg>',
			'database' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><ellipse cx="12" cy="5" rx="8" ry="3"></ellipse><path d="M4 5v6c0 1.7 3.6 3 8 3s8-1.3 8-3V5"></path><path d="M4 11v6c0 1.7 3.6 3 8 3s8-1.3 8-3v-6"></path></svg>',
			'refresh' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><polyline points="23 4 23 10 17 10"></polyline><polyline points="1 20 1 14 7 14"></polyline><path d="M3.5 9a9 9 0 0 1 14.1-3.4L23 10"></path><path d="M20.5 15a9 9 0 0 1-14.1 3.4L1 14"></path></svg>',
			'search' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="7"></circle><line x1="21" y1="21" x2="16.65" y2="16.65"></line></svg>',
			'folder-plus' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M3 6a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2h-8"></path><line x1="8" y1="14" x2="8" y2="20"></line><line x1="5" y1="17" x2="11" y2="17"></line></svg>',
			'file' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path><polyline points="14 2 14 8 20 8"></polyline></svg>',
			'edit' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M12 20h9"></path><path d="M16.5 3.5a2.1 2.1 0 0 1 3 3L7 19l-4 1 1-4z"></path></svg>',
			'settings-list' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><line x1="8" y1="6" x2="21" y2="6"></line><line x1="8" y1="12" x2="21" y2="12"></line><line x1="8" y1="18" x2="21" y2="18"></line><circle cx="4" cy="6" r="1"></circle><circle cx="4" cy="12" r="1"></circle><circle cx="4" cy="18" r="1"></circle></svg>',
			'chevron-right' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><polyline points="9 18 15 12 9 6"></polyline></svg>',
			'tag' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M20.6 13.4l-7.2 7.2a2 2 0 0 1-2.8 0l-7-7a2 2 0 0 1 0-2.8L10.8 3H21v10.2a2 2 0 0 1-.4 1.2z"></path><circle cx="16" cy="8" r="1.2"></circle></svg>',
			'cart' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="9" cy="20" r="1"></circle><circle cx="18" cy="20" r="1"></circle><path d="M3 4h2l2.2 11.2a2 2 0 0 0 2 1.6h7.8a2 2 0 0 0 2-1.6L21 7H7"></path></svg>',
			'box' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M21 16V8a2 2 0 0 0-1-1.7l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.7l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"></path><polyline points="3.3 7 12 12 20.7 7"></polyline><line x1="12" y1="22" x2="12" y2="12"></line></svg>',
			'factory' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M3 21h18"></path><path d="M5 21V10l7 4v-4l7 4v7"></path><path d="M10 6h4v4h-4z"></path></svg>',
			'building' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="18" height="18" rx="2"></rect><line x1="8" y1="7" x2="8" y2="7"></line><line x1="12" y1="7" x2="12" y2="7"></line><line x1="16" y1="7" x2="16" y2="7"></line><line x1="8" y1="12" x2="8" y2="12"></line><line x1="12" y1="12" x2="12" y2="12"></line><line x1="16" y1="12" x2="16" y2="12"></line><path d="M10 21v-4h4v4"></path></svg>',
			'map' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><polygon points="1 6 8 3 16 6 23 3 23 18 16 21 8 18 1 21 1 6"></polygon><line x1="8" y1="3" x2="8" y2="18"></line><line x1="16" y1="6" x2="16" y2="21"></line></svg>',
			'book' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"></path><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"></path></svg>',
			'users' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"></path><circle cx="9" cy="7" r="4"></circle><path d="M23 21v-2a4 4 0 0 0-3-3.9"></path><path d="M16 3.1a4 4 0 0 1 0 7.8"></path></svg>',
			'cog' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="3"></circle><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 0 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 0 1-4 0v-.2a1.7 1.7 0 0 0-1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 0 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 0 1 0-4h.2a1.7 1.7 0 0 0 1.5-1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 0 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3h0a1.7 1.7 0 0 0 1-1.5V3a2 2 0 0 1 4 0v.2a1.7 1.7 0 0 0 1 1.5h0a1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 0 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8v0a1.7 1.7 0 0 0 1.5 1H21a2 2 0 0 1 0 4h-.2a1.7 1.7 0 0 0-1.5 1z"></path></svg>',
			'layout' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="18" height="18" rx="2"></rect><line x1="3" y1="9" x2="21" y2="9"></line><line x1="9" y1="9" x2="9" y2="21"></line></svg>',
			'key' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="7" cy="15" r="4"></circle><line x1="10.5" y1="12.5" x2="21" y2="2"></line><line x1="17" y1="6" x2="20" y2="9"></line><line x1="14" y1="9" x2="17" y2="12"></line></svg>',
			'help-circle' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><path d="M9.1 9a3 3 0 1 1 5.8 1c0 2-3 3-3 3"></path><line x1="12" y1="17" x2="12" y2="17"></line></svg>',
			'logout' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"></path><polyline points="16 17 21 12 16 7"></polyline><line x1="21" y1="12" x2="9" y2="12"></line></svg>',
			'chevron-down' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><polyline points="6 9 12 15 18 9"></polyline></svg>',
			'menu' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><line x1="3" y1="6" x2="21" y2="6"></line><line x1="3" y1="12" x2="21" y2="12"></line><line x1="3" y1="18" x2="21" y2="18"></line></svg>',
			'bell' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M18 8a6 6 0 1 0-12 0c0 7-3 8-3 8h18s-3-1-3-8"></path><path d="M13.73 21a2 2 0 0 1-3.46 0"></path></svg>',
			'user' => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"></path><circle cx="12" cy="7" r="4"></circle></svg>'
		);

		if (!isset($icons[$name]))
			$name = 'chevron-right';

		return "<span class='".$class_name."' aria-hidden='true'>".$icons[$name]."</span>";
	}

	/**
	 * Build icon and label text content.
	 *
	 * @param string $icon_name
	 * @param string $label
	 * @return string
	 */
	function icon_label($icon_name, $label) {
		return $this->icon_svg($icon_name, 'modern-icon modern-link-icon')."<span class='modern-link-label'>".$label."</span>";
	}

	/**
	 * Return icon markup for menu function categories.
	 *
	 * @param int $menu
	 * @return string
	 */
	function menu_icon($menu) {
		global $SysPrefs;

		if ($SysPrefs->show_menu_category_icons) {
			if ($menu == MENU_TRANSACTION)
				$icon_name = 'activity';
			elseif ($menu == MENU_SYSTEM)
				$icon_name = 'database';
			elseif ($menu == MENU_UPDATE)
				$icon_name = 'refresh';
			elseif ($menu == MENU_INQUIRY)
				$icon_name = 'search';
			elseif ($menu == MENU_ENTRY)
				$icon_name = 'folder-plus';
			elseif ($menu == MENU_REPORT)
				$icon_name = 'file';
			elseif ($menu == MENU_MAINTENANCE)
				$icon_name = 'edit';
			elseif ($menu == MENU_SETTINGS)
				$icon_name = 'settings-list';
			else
				$icon_name = 'chevron-right';
		} else {
			$icon_name = 'chevron-right';
		}

		return $this->icon_svg($icon_name, 'modern-icon modern-link-icon');
	}

	/**
	 * Build app icon name by application id.
	 *
	 * @param string $application_id
	 * @return string
	 */
	function application_icon($application_id) {
		$application_icons = array(
			'orders' => 'tag',
			'AP' => 'cart',
			'stock' => 'box',
			'manuf' => 'factory',
			'assets' => 'building',
			'proj' => 'map',
			'GL' => 'book',
			'hrm' => 'users',
			'system' => 'cog'
		);

		if (isset($application_icons[$application_id]))
			return $application_icons[$application_id];
		return 'box';
	}

	/**
	 * Render user dropdown menu with avatar icon and name.
	 *
	 * @param string $selected_application_id
	 * @return void
	 */
	function render_user_dropdown($selected_application_id) {
		global $path_to_root, $SysPrefs;

		$user_name = $_SESSION['wa_current_user']->name;
		$username = $_SESSION['wa_current_user']->username;

		echo "<div class='modern-user-dropdown'>";
		echo "<button class='modern-user-trigger' type='button' id='modern-user-trigger' aria-haspopup='true' aria-expanded='false'>";
		echo $this->icon_svg('user', 'modern-icon modern-user-icon');
		echo "<span class='modern-user-name'>" . htmlspecialchars($user_name) . "</span>";
		echo $this->icon_svg('chevron-down', 'modern-icon modern-toggle-icon');
		echo "</button>";
		echo "<div class='modern-user-menu' id='modern-user-menu'>";
		echo "<div class='modern-user-menu-header'>" . htmlspecialchars($user_name) . "</div>";
		echo "<a class='modern-user-menu-item' href='" . $path_to_root . "/admin/dashboard.php?sel_app=" . $selected_application_id . "'>" . $this->icon_label('layout', _('Dashboard')) . "</a>";
		echo "<a class='modern-user-menu-item' href='" . $path_to_root . "/admin/display_prefs.php'>" . $this->icon_label('cog', _('Preferences')) . "</a>";
		echo "<a class='modern-user-menu-item' href='" . $path_to_root . "/admin/change_current_user_password.php?selected_id=" . htmlspecialchars($username) . "'>" . $this->icon_label('key', _('Change password')) . "</a>";
		echo "<div class='modern-user-menu-divider'></div>";
		echo "<a class='modern-user-menu-item modern-user-menu-logout' href='" . $path_to_root . "/access/logout.php'>" . $this->icon_label('logout', _('Logout')) . "</a>";
		echo "</div>";
		echo "</div>";
	}

	/**
	 * Render help button next to the page title (right side).
	 *
	 * @param string $selected_application_id
	 * @return void
	 */
	function render_help_button($selected_application_id) {
		global $path_to_root, $SysPrefs;
		if ($SysPrefs->help_base_url != null) {
			$href = help_url();
			echo "<div class='modern-page-title-actions'>";
			echo "<a class='modern-topbar-link' target='_blank' onclick=\"javascript:openWindow(this.href,this.target); return false;\" href='".$href."'>".$this->icon_label('help-circle', _('Help'))."</a>";
			echo "</div>";
		}
	}

	/**
	 * Render notification button in the topbar.
	 *
	 * @return void
	 */
	function render_notification_button() {
		echo "<button class='modern-notification-button' type='button' aria-label='"._('Notifications')."'>";
		echo $this->icon_svg('bell', 'modern-icon modern-notification-icon');
		echo "</button>";
	}

	/**
	 * Resolve branding assets for the active company.
	 *
	 * @return array
	 */
	function get_company_branding_details() {
		global $path_to_root, $db_connections;

		$company_id = user_company();
		$company_name = $db_connections[$company_id]['name'];
		$project_root = dirname(dirname(dirname(__FILE__)));
		$company_images_path = $project_root.'/company/'.$company_id.'/images';
		$logo_extensions = array('png', 'svg', 'jpg', 'jpeg', 'gif', 'webp');
		$logo_url = '';

		foreach ($logo_extensions as $logo_extension) {
			$logo_file_name = 'logo.'.$logo_extension;
			$logo_file_path = $company_images_path.'/'.$logo_file_name;
			if (!file_exists($logo_file_path))
				continue;

			$logo_url = $path_to_root.'/company/'.$company_id.'/images/'.$logo_file_name;
			$logo_url .= '?v='.filemtime($logo_file_path);
			break;
		}

		return array(
			'company_name' => $company_name,
			'logo_url' => $logo_url
		);
	}

	/**
	 * Render company branding beside the sidebar toggle.
	 *
	 * @param array $branding_details
	 * @return void
	 */
	function render_topbar_branding($branding_details) {
		$company_name = htmlspecialchars($branding_details['company_name'], ENT_QUOTES, 'UTF-8');

		echo "<div class='modern-branding".($branding_details['logo_url'] ? " modern-branding-has-logo" : " modern-branding-text-only")."'>";
		if ($branding_details['logo_url'])
			echo "<img class='modern-brand-logo' src='".$branding_details['logo_url']."' alt='".$company_name."'>";
		else
			echo "<span class='modern-brand-title'>".$company_name."</span>";
		echo "</div>";
	}

	/**
	 * Render the vertical application switcher in sidebar.
	 *
	 * @param array $applications
	 * @param string $selected_application_id
	 * @return void
	 */
	function render_sidebar_applications($applications, $selected_application_id) {
		global $path_to_root;

		echo "<ul class='modern-app-switcher'>";
		foreach ($applications as $application) {
			if (!$_SESSION['wa_current_user']->check_application_access($application))
				continue;
			$access = access_string($application->name);
			$tooltip_text = str_replace('&', '', strip_tags($access[0]));
			$is_active = $selected_application_id == $application->id;
			echo "<li class='modern-app-item'>";
			echo "<a class='modern-app-link".($is_active ? ' is-active' : '')."' href='".$path_to_root."/admin/dashboard.php?sel_app=".$application->id."' title='".$tooltip_text."' data-tooltip='".$tooltip_text."' ".$access[1].">";
			echo $this->icon_svg($this->application_icon($application->id), 'modern-icon modern-app-icon');
			echo "<span class='modern-app-label'>".$access[0]."</span>";
			echo '</a>';
			echo '</li>';
		}
		echo '</ul>';
	}

	/**
	 * Return selected app accent colors for contextual UI styling.
	 *
	 * @param string $selected_application_id
	 * @return array
	 */
	function application_theme_colors($selected_application_id) {
		$application_colors = array(
			'orders' => array('accent' => '#2563eb', 'soft' => '#dbeafe'),
			'AP' => array('accent' => '#0f766e', 'soft' => '#ccfbf1'),
			'stock' => array('accent' => '#0369a1', 'soft' => '#e0f2fe'),
			'manuf' => array('accent' => '#0f766e', 'soft' => '#ccfbf1'),
			'assets' => array('accent' => '#334155', 'soft' => '#e2e8f0'),
			'proj' => array('accent' => '#15803d', 'soft' => '#dcfce7'),
			'GL' => array('accent' => '#b45309', 'soft' => '#fef3c7'),
			'hrm' => array('accent' => '#c026d3', 'soft' => '#fae8ff'),
			'system' => array('accent' => '#475569', 'soft' => '#f1f5f9')
		);

		if (isset($application_colors[$selected_application_id]))
			return $application_colors[$selected_application_id];

		return array('accent' => '#3f51b5', 'soft' => '#e8edff');
	}

	/**
	 * Render selected application module groups in sidebar.
	 *
	 * @param object $selected_application
	 * @return void
	 */
	function render_sidebar_modules($selected_application) {
		echo "<div class='modern-sidebar-modules'>";
		foreach ($selected_application->modules as $module) {
			if (!$_SESSION['wa_current_user']->check_module_access($module))
				continue;

			echo "<section class='modern-module-group is-expanded'>";
			echo "<button class='modern-module-toggle' type='button'>";
			echo "<span class='modern-module-title'>".$module->name."</span>";
			echo $this->icon_svg('chevron-down', 'modern-icon modern-toggle-icon');
			echo "</button>";
			echo "<div class='modern-module-links'>";

			foreach ($module->lappfunctions as $app_function)
				$this->render_module_function_link($app_function, $selected_application->id);
			foreach ($module->rappfunctions as $app_function)
				$this->render_module_function_link($app_function, $selected_application->id);

			echo "</div>";
			echo "</section>";
		}
		echo '</div>';
	}

	/**
	 * Append selected app parameter to an internal link.
	 *
	 * @param string $link
	 * @param string $selected_application_id
	 * @return string
	 */
	function app_context_link($link, $selected_application_id) {
		if (!$selected_application_id || strpos($link, 'sel_app=') !== false)
			return $link;

		$delimiter = (strpos($link, '?') !== false) ? '&' : '?';
		return $link.$delimiter.'sel_app='.urlencode($selected_application_id);
	}

	/**
	 * Return startup redirect URL for theme-specific landing behavior.
	 *
	 * @param string $selected_application_id
	 * @param string $path_to_root
	 * @return string
	 */
	function startup_redirect_url($selected_application_id, $path_to_root) {
		if (!$selected_application_id)
			$selected_application_id = user_startup_tab();

		return $path_to_root.'/admin/dashboard.php?sel_app='.urlencode($selected_application_id);
	}

	/**
	 * Render a module function entry with access state.
	 *
	 * @param object $app_function
	 * @return void
	 */
	function render_module_function_link($app_function, $selected_application_id = '') {
		if ($app_function->label == '') {
			echo "<div class='modern-module-separator'></div>";
			return;
		}

		$icon = $this->menu_icon($app_function->category);
		$function_link = $this->app_context_link($app_function->link, $selected_application_id);
		if ($_SESSION['wa_current_user']->can_access_page($app_function->access))
			echo "<div class='modern-module-link'>".menu_link($function_link, $icon."<span class='modern-link-label'>".$app_function->label."</span>")."</div>";
		elseif (!$_SESSION['wa_current_user']->hide_inaccessible_menu_items())
			echo "<div class='modern-module-link modern-module-link-inactive'><span class='inactive'>".access_string($icon."<span class='modern-link-label'>".$app_function->label."</span>", true)."</span></div>";
	}

	/**
	 * Open framework page.
	 *
	 * @return void
	 */
	function wa_header() {
		page(_($help_context = 'Main Menu'), false, true);
	}

	/**
	 * Close framework page.
	 *
	 * @return void
	 */
	function wa_footer() {
		end_page(false, true);
	}

	/**
	 * Render the modern page shell header.
	 *
	 * @param string $title
	 * @param bool $no_menu
	 * @param bool $is_index
	 * @return void
	 */
	function menu_header($title, $no_menu, $is_index) {
		global $path_to_root, $db_connections;

		if (isset($_GET['sel_app']) && $_GET['sel_app'] != '')
			$selected_application_id = $_GET['sel_app'];
		elseif (isset($_SESSION['sel_app']) && $_SESSION['sel_app'] != '')
			$selected_application_id = $_SESSION['sel_app'];
		elseif (isset($_GET['application']) && $_GET['application'] != '')
			$selected_application_id = $_GET['application'];
		else
			$selected_application_id = 'orders';

		$_SESSION['sel_app'] = $selected_application_id;
		$applications = $_SESSION['App']->applications;
		$selected_application = null;
		if (isset($_SESSION['App']->applications[$selected_application_id]))
			$selected_application = $_SESSION['App']->applications[$selected_application_id];
		else
			$selected_application = $_SESSION['App']->get_selected_application();
		$page_title = $title;
		$branding_details = $this->get_company_branding_details();
		$search_placeholder = htmlspecialchars(_('Type / to search menu or actions...'), ENT_QUOTES, 'UTF-8');
		if ($this->is_dashboard_page() && is_object($selected_application) && $selected_application->name != '') {
			$selected_application_title = str_replace('&', '', $selected_application->name);
			$page_title = $title.' / '.$selected_application_title;
		}
		$indicator = $path_to_root.'/themes/'.user_theme().'/images/ajax-loader.gif';

		$app_theme_colors = $this->application_theme_colors($selected_application_id);
		$app_shell_style = "--modern-app-accent: ".$app_theme_colors['accent']."; --modern-app-accent-soft: ".$app_theme_colors['soft'].";";

		echo "<div id='modern-app-shell' class='modern-app-shell' style='".$app_shell_style."'>";

		if (!$no_menu) {
			echo "<header class='modern-topbar'>";
			echo "<div class='modern-topbar-left'>";
			echo "<button id='modern-sidebar-toggle' class='modern-sidebar-toggle' type='button' aria-label='"._('Toggle navigation')."'>".$this->icon_svg('menu', 'modern-icon modern-toggle-icon')."</button>";
			$this->render_topbar_branding($branding_details);
			echo "</div>";
			echo "<div class='modern-topbar-center'>";
			echo "<div class='modern-header-search' role='search'>";
			echo $this->icon_svg('search', 'modern-icon modern-header-search-icon');
			echo "<input type='text' class='modern-header-search-input' placeholder='".$search_placeholder."' data-placeholder-base='".$search_placeholder."' aria-label='"._('Search')."' autocomplete='off' spellcheck='false'>";
			echo "<span class='modern-header-search-shortcut' aria-hidden='true'>/</span>";
			echo "</div>";
			echo "</div>";
			echo "<div class='modern-topbar-right'>";
			echo "<img id='ajaxmark' class='modern-ajax-indicator' src='".$indicator."' style='visibility:hidden;' alt='ajaxmark'>";
			echo "<button id='modern-search-toggle' class='modern-search-toggle' type='button' aria-label='"._('Search')."'>".$this->icon_svg('search', 'modern-icon modern-search-toggle-icon')."</button>";
			$this->render_notification_button();
			$this->render_user_dropdown($selected_application_id);
			echo "</div>";
			echo "</header>";
			$this->render_search_index();

			echo "<div class='modern-shell'>";
			echo "<aside id='modern-sidebar' class='modern-sidebar' aria-label='"._('Main navigation')."'>";
			echo "<div class='modern-sidebar-header'>";
			echo "<span class='modern-sidebar-title'>"._('Applications')."</span>";
			echo '</div>';
			$this->render_sidebar_applications($applications, $selected_application_id);
			if (is_object($selected_application)) {
				echo "<div class='modern-sidebar-section-title'>".str_replace('&', '', $selected_application->name)."</div>";
				$this->render_sidebar_modules($selected_application);
			}
			echo "</aside>";
			echo "<div id='modern-sidebar-overlay' class='modern-sidebar-overlay'></div>";
			echo "<main class='modern-main-content' id='modern-main-content'>";
		} else {
			echo "<main class='modern-main-content modern-main-no-menu' id='modern-main-content'>";
			echo "<div class='modern-floating-indicator'><img id='ajaxmark' class='modern-ajax-indicator' src='".$indicator."' style='visibility:hidden;' alt='ajaxmark'></div>";
		}

		if ($page_title && !$is_index) {
			echo "<section class='modern-page-title'>";
			echo "<div class='modern-page-title-main'>";
			echo "<h2>".$page_title."</h2>";
			if (user_hints())
				echo "<span id='hints' class='modern-page-hint'></span>";
			echo "</div>";
			// render help button to the right of the page title
			$this->render_help_button($selected_application_id);
			echo "</section>";
		}
	}

	/**
	 * Check if the current page is the dashboard page.
	 *
	 * @return bool
	 */
	function is_dashboard_page() {
		if (!isset($_SERVER['SCRIPT_NAME']))
			return false;

		return basename($_SERVER['SCRIPT_NAME']) === 'dashboard.php';
	}

	/**
	 * Render footer and close page shell.
	 *
	 * @param bool $no_menu
	 * @param bool $is_index
	 * @return void
	 */
	function menu_footer($no_menu, $is_index) {
		global $version, $path_to_root, $Pagehelp, $Ajax, $SysPrefs, $db_connections;

		include_once($path_to_root.'/includes/date_functions.inc');

		if (!$no_menu && isset($_SESSION['wa_current_user'])) {
			$page_help_text = implode('; ', $Pagehelp);
			$Ajax->addUpdate(true, 'hotkeyshelp', $page_help_text);
			echo "<footer class='modern-footer'>";
			echo "<div class='modern-footer-left'>";
			echo "<span>".Today().' | '.Now()."</span>";
			echo "<span>".$db_connections[user_company()]['name']."</span>";
			echo "<span>".$_SERVER['SERVER_NAME']."</span>";
			echo "<span>".$_SESSION['wa_current_user']->name."</span>";
			echo "</div>";
			echo "<div class='modern-footer-right'>";
			echo "<span id='hotkeyshelp'>".$page_help_text."</span>";
			echo "<a target='_blank' href='".$SysPrefs->power_url."'>".$SysPrefs->app_title.' '.$version.' - '._('Theme:').' '.user_theme().show_users_online()."</a>";
			echo "</div>";
			echo "</footer>";
		}

		echo "</main>";
		if (!$no_menu)
			echo "</div>";
		echo "</div>";
	}

	/**
	 * Render selected application content cards on index.
	 *
	 * @param object $waapp
	 * @return void
	 */
	function display_applications(&$waapp) {
		$selected_application = $waapp->get_selected_application();
		if (!$_SESSION['wa_current_user']->check_application_access($selected_application))
			return;

		if (method_exists($selected_application, 'render_index')) {
			$selected_application->render_index();
			return;
		}

		echo "<section class='modern-workspace'>";
		echo "<header class='modern-workspace-header'>";
		echo "<h2 class='modern-workspace-title'>".$selected_application->name."</h2>";
		echo "<p class='modern-workspace-subtitle'>"._('Workspace')."</p>";
		echo "</header>";
		echo "<div class='modern-workspace-grid'>";

		foreach ($selected_application->modules as $module) {
			if (!$_SESSION['wa_current_user']->check_module_access($module))
				continue;
			echo "<article class='modern-module-card'>";
			echo "<h3 class='modern-module-card-title'>".$module->name."</h3>";
			echo "<div class='modern-module-card-links'>";
			foreach ($module->lappfunctions as $app_function)
				$this->render_workspace_link($app_function);
			foreach ($module->rappfunctions as $app_function)
				$this->render_workspace_link($app_function);
			echo "</div>";
			echo "</article>";
		}

		echo "</div>";
		echo "</section>";
	}

	/**
	 * Build a flat array of all accessible menu items for the search index.
	 *
	 * Each entry contains the label, link, application name, module name,
	 * and category so the client-side search can filter and display results
	 * without any server round-trip.
	 *
	 * @return array
	 */
	function build_search_index() {
		global $path_to_root;

		$applications = $_SESSION['App']->applications;
		$current_user = $_SESSION['wa_current_user'];
		$items = array();

		foreach ($applications as $application) {
			if (!$current_user->check_application_access($application))
				continue;

			$application_name = str_replace('&', '', $application->name);

			foreach ($application->modules as $module) {
				if (!$current_user->check_module_access($module))
					continue;

				$all_functions = array_merge($module->lappfunctions, $module->rappfunctions);
				foreach ($all_functions as $app_function) {
					if ($app_function->label == '' || !$current_user->can_access_page($app_function->access))
						continue;

					$function_link = $this->app_context_link($app_function->link, $application->id);
					$absolute_link = $path_to_root.'/'.$function_link;
					$clean_label = str_replace('&', '', strip_tags($app_function->label));
					$items[] = array(
						'l' => $clean_label,
						'u' => $absolute_link,
						'a' => $application_name,
						'i' => $application->id,
						'm' => $module->name,
						'c' => $app_function->category
					);
				}
			}
		}

		return $items;
	}

	/**
	 * Output the search index as an inline JSON script tag and the results container.
	 *
	 * @return void
	 */
	function render_search_index() {
		$search_index = $this->build_search_index();
		echo "<script>window.__searchIndex=".json_encode($search_index, JSON_HEX_TAG | JSON_HEX_AMP).";</script>";
		echo "<div id='modern-search-results' class='modern-search-results' style='display:none;'></div>";
	}

	/**
	 * Render links used in workspace cards.
	 *
	 * @param object $app_function
	 * @return void
	 */
	function render_workspace_link($app_function) {
		if ($app_function->label == '')
			return;
		$icon = $this->menu_icon($app_function->category);
		$current_application_id = isset($_SESSION['sel_app']) ? $_SESSION['sel_app'] : '';
		$function_link = $this->app_context_link($app_function->link, $current_application_id);
		if ($_SESSION['wa_current_user']->can_access_page($app_function->access))
			echo "<div class='modern-workspace-link'>".menu_link($function_link, $icon."<span class='modern-link-label'>".$app_function->label."</span>")."</div>";
		elseif (!$_SESSION['wa_current_user']->hide_inaccessible_menu_items())
			echo "<div class='modern-workspace-link modern-workspace-link-inactive'><span class='inactive'>".access_string($icon."<span class='modern-link-label'>".$app_function->label."</span>", true)."</span></div>";
	}
}
