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
/*
	Loads the PDF library (TCPDF and FPDI, managed by Composer in vendor/) with
	NotrinosERP settings. Pages, reports and extensions include this file
	instead of the library itself, so the path stays stable when the library
	is upgraded through composer.json and composer.lock.
*/

// Report fonts: the library font definitions in use (copied from
// vendor/tecnickcom/tcpdf/fonts) plus fonts installed by language packages.
// config.php may point K_PATH_FONTS elsewhere.
if (!defined('K_PATH_FONTS'))
	define('K_PATH_FONTS', dirname(__FILE__).'/../fonts/');

// Skip the library's bundled config file; tcpdf_autoconfig.php fills in any
// constant that is not defined here.
if (!defined('K_TCPDF_EXTERNAL_CONFIG'))
	define('K_TCPDF_EXTERNAL_CONFIG', true);
if (!defined('K_CELL_HEIGHT_RATIO'))
	define('K_CELL_HEIGHT_RATIO', 1.25);
if (!defined('K_PATH_CACHE') && defined('VARLIB_PATH') && is_dir(VARLIB_PATH) && is_writable(VARLIB_PATH))
	define('K_PATH_CACHE', rtrim(VARLIB_PATH, '/\\').'/');

require_once(dirname(__FILE__).'/../../vendor/autoload.php');

if (!function_exists('check_image_file')) {
	/**
	 * Checks that an uploaded image can be placed in a PDF by the report library.
	 *
	 * @param string $filename Path of the uploaded image file.
	 * @return string An empty string when the image is usable, otherwise a translated error message.
	 */
	function check_image_file($filename) {
		$probe = new class('P', 'pt', 'A4') extends TCPDF {
			/**
			 * Reports library errors to the caller instead of ending the request.
			 *
			 * @param string $msg Library error message.
			 * @return void
			 */
			public function Error($msg) {
				throw new RuntimeException($msg);
			}
		};
		try {
			$probe->AddPage();
			if ($probe->Image($filename, 0, 0) === false)
				return _('Unsupported image file format.');
		}
		catch (Throwable $error) {
			return _('Unsupported image file format.');
		}
		return '';
	}
}
