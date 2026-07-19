<?php
/**
 * Retired anonymous password-by-email compatibility page.
 *
 * No account lookup, password mutation, or email is performed. A replacement
 * recovery flow must use verified identity and single-use hashed material.
 */
if (!isset($path_to_root) || isset($_GET['path_to_root']) || isset($_POST['path_to_root']))
	die(_('Restricted access'));
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/page/header.inc');

$def_theme = 'default';
$title = $SysPrefs->app_title.' '.$version.' - '._('Password recovery unavailable');
$encoding = isset($_SESSION['language']->encoding) ? $_SESSION['language']->encoding : 'iso-8859-1';
$rtl = isset($_SESSION['language']->dir) ? $_SESSION['language']->dir : 'ltr';
if (!headers_sent()) {
	header('X-Frame-Options: SAMEORIGIN');
	header('Cache-Control: no-store');
}

echo "<!DOCTYPE html>\n<html dir='".$rtl."'>\n<head><title>".$title."</title>\n";
echo "<meta charset='".$encoding."'>\n<meta name='viewport' content='width=device-width,initial-scale=1'>\n";
echo "<link href='".$path_to_root."/themes/".$def_theme."/default.css' rel='stylesheet'>\n";
echo "<link href='".$path_to_root."/themes/".$def_theme."/local_style/access.css' rel='stylesheet'>\n";
echo "</head><body id='loginscreen'>\n";
echo "<div class='login-title-bar'>".htmlspecialchars($title)."</div>\n";
echo "<div class='login-card'>\n<div class='login-logo'>";
echo "<img src='".$path_to_root."/themes/default/images/notrinos_erp.png' alt='NotrinosERP' height='50'>";
echo "</div>\n<div class='login-message'>";
echo _('Password recovery by email is unavailable. Contact an authorized system administrator.');
echo "</div>\n<div class='login-submit'><a href='".$path_to_root."/index.php'>"._('Return to login')."</a></div>\n";
echo "</div>\n</body></html>\n";
