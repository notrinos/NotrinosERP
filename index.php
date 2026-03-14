<?php
/**********************************************************************
	Copyright (C) FrontAccounting, LLC.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
	$path_to_root = '.';
	if (!file_exists($path_to_root.'/config_db.php'))
		header('Location: '.$path_to_root.'/install/index.php');

	$page_security = 'SA_OPEN';
	ini_set('xdebug.auto_trace', 1);
	include_once('includes/session.inc');

	add_access_extensions();
	$app = &$_SESSION['App'];
	if (isset($_GET['application']))
		$app->selected_application = $_GET['application'];

	$selected_application = isset($app->selected_application) && $app->selected_application != ''
		? $app->selected_application
		: user_startup_tab();

	/**
	 * Allow installed extensions to override index startup routing.
	 * The last non-null hook return value wins.
	 */
	$index_startup_context = array(
		'path_to_root' => $path_to_root,
		'selected_application' => $selected_application,
		'application' => $app
	);
	$extension_redirect_url = hook_invoke_last('index_startup_redirect', $index_startup_context);
	if (is_string($extension_redirect_url) && $extension_redirect_url != '') {
		header('Location: '.$extension_redirect_url);
		exit;
	}

	/**
	 * Allow current theme renderer to define startup routing behavior.
	 */
	$theme_renderer_file = $path_to_root.'/themes/'.user_theme().'/renderer.php';
	if (file_exists($theme_renderer_file)) {
		include_once($theme_renderer_file);
		if (class_exists('renderer')) {
			$theme_renderer = new renderer();
			if (method_exists($theme_renderer, 'startup_redirect_url')) {
				$theme_redirect_url = $theme_renderer->startup_redirect_url($selected_application, $path_to_root);
				if (is_string($theme_redirect_url) && $theme_redirect_url != '') {
					header('Location: '.$theme_redirect_url);
					exit;
				}
			}
		}
	}

	$app->display();
