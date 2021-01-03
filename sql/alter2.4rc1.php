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

class fa2_4rc1 extends fa_patch {
	var $previous = '2.4.0';		// applicable database version
	var $version = '2.4.1';	// version installed
	var $description;
	var $sql = ''; // 'alter2.4rc1.sql';
	var $preconf = true;
	var	$max_upgrade_time = 900;	// table recoding is really long process

	function __construct() {
		parent::__construct();
		$this->description = _('Upgrade from version 2.4beta to 2.4RC1');
	}

    /*
	    Shows parameters to be selected before upgrade (if any)
	*/
    function show_params($comp)
	{
	  display_note(_('Set optimal parameters and start upgrade:'));
	  start_table(TABLESTYLE);
	  start_row();
		table_section_title(_("Fixed Assets Defaults"));
		gl_all_accounts_list_row(_("Loss On Asset Disposal Account:"), 'default_loss_on_asset_disposal_act', '5660',
			true, false, _("None (will be set later)"));
	  end_row();
	  end_table();
	  br();
    }

	/*
	    Fetches selected upgrade parameters.
    */
	function prepare()
    {
		$this->fixed_disposal_act = get_post('default_loss_on_asset_disposal_act');
		return true;
	}
	//
	//	Install procedure. All additional changes 
	//	not included in sql file should go here.
	//
	function install($company, $force=false)
	{
		// key 
		$sec_updates = array(
			'SA_SETUPCOMPANY' => array(
				'SA_ASSET', 'SA_ASSETCATEGORY', 'SA_ASSETCLASS',
				'SA_ASSETSTRANSVIEW','SA_ASSETTRANSFER', 'SA_ASSETDISPOSAL',
				'SA_DEPRECIATION', 'SA_ASSETSANALYTIC'),
		);
		$result = $this->update_security_roles($sec_updates);

		$pref = $this->companies[$company]['tbpref'];

		if ($result)
			if (!db_query("UPDATE ".$pref."sys_prefs SET value=".db_escape($this->fixed_disposal_act)
					." WHERE name='default_loss_on_asset_disposal_act'")
			)
				return $this->log_error(sprintf(_("Cannot update sys prefs setting:\n%s"), db_error_msg($db)));

		return $result;
	}

	//
	// optional procedure done after upgrade fail, before backup is restored
	//
	function post_fail($company)
	{
		$pref = $this->companies[$company]['tbpref'];
		db_query("DROP TABLE IF EXISTS " . $pref . 'stock_fa_class');

		db_query("DELETE FROM ".$pref."sys_prefs "
			."WHERE `name` in (
				'default_loss_on_asset_disposal_act',
				'depreciation_period',
				'use_manufacturing',
				'use_fixed_assets')");
	}

}

$install = new fa2_4rc1;
