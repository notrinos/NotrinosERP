<?php
/**********************************************************************
	Copyright (C) NotrinosERP.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
***********************************************************************/

class StoreApp extends application {
	function __construct() {
		parent::__construct('store', _($this->help_context = 'App St&ore'));

		$this->add_module(_('Catalog'));
		$this->add_lapp_function(0, _('Browse &Packages'), 'admin/notrinos_store.php?', 'SA_CREATEMODULES', MENU_INQUIRY);
		$this->add_rapp_function(0, _('Manage &Languages'), 'admin/notrinos_store.php?type=language', 'SA_CREATELANGUAGE', MENU_INQUIRY);

		$this->add_module(_('Local Management'));
		$this->add_lapp_function(1, _('Local &Extensions'), 'admin/inst_module.php?legacy=manage', 'SA_CREATEMODULES', MENU_UPDATE);
		$this->add_lapp_function(1, _('Local &Themes'), 'admin/inst_theme.php?legacy=manage', 'SA_CREATEMODULES', MENU_UPDATE);
		$this->add_rapp_function(1, _('Local &Charts of Accounts'), 'admin/inst_chart.php?legacy=manage', 'SA_CREATEMODULES', MENU_UPDATE);
		$this->add_rapp_function(1, _('Manual &Languages'), 'admin/inst_lang.php?legacy=manage', 'SA_CREATELANGUAGE', MENU_UPDATE);
	}
}