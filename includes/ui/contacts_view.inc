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
include_once($path_to_root. '/includes/ui/simple_crud_class.inc');
include_once($path_to_root . "/includes/db/crm_contacts_db.inc");

/*
	View/Edit class for contacts
*/
class contacts extends simple_crud {
	var $id;
	var $entity;
	var $sub_class;
	var $class;
	
	function __construct($name, $id, $class, $subclass=null) {
		$fields = array('ref','name','name2','address', 'phone', 'phone2','fax',
			'email','lang','notes', 'assgn' =>array('fld'=>'contacts'));

		parent::__construct($name, $fields);
		$this->class = $class;
		$this->subclass = $subclass;
		$this->entity = $id;
	}

	function list_view() {
		br();

			$contacts = get_crm_persons($this->class, $this->subclass, $this->entity);
			start_table(TABLESTYLE, "width='80%'");

			$th = array(_('Assignment'), _("Reference"), _("Full Name"), _("Phone"), _("Sec Phone"), _("Fax"),
				_("email"),  '',
				"&nbsp;");

			table_header($th);
			$k = 0; 

			$last = $myrow = db_fetch($contacts);
			while ($last)
			{
					
				if ($myrow['id'] != $last['id']) {
					alt_table_row_color($k);
					label_cell(implode('<br>',$description));
					label_cell($last["ref"]);
					label_cell($last["name"].' '.$last["name2"]);
					label_cell($last["phone"]);
					label_cell($last["phone2"]);
					label_cell($last["fax"]);
					email_cell($last["email"]);

				 	edit_button_cell("{$this->name}Edit[{$last['id']}]", _("Edit"));

					// delete button removes only category unless this is the last contact for this person
					delete_button_cell("{$this->name}Delete[{$last['id']}]", _("Delete"));
					end_row();
					$description = array();
					$last = $myrow;
				}

				if ($myrow) {
					$description[] = $myrow['description'];
					$myrow = db_fetch($contacts);
				}
			}

			//inactive_control_row($th);
			end_table(1);
	}
	
	function editor_view()
	{
		br();

		start_outer_table(TABLESTYLE);

		table_section(1);
		table_section_title(_("Contact data"));

		text_row(_("First Name:"), 'name', @$this->data['name'], 35, 40);
		text_row(_("Last Name:"), 'name2', @$this->data['name2'], 35, 40);
		text_row(_("Reference:"), 'ref', @$this->data['ref'], 35, 40);

		crm_category_types_list_row(_("Contact active for:"), 'assgn', @$this->data['contacts'],
			array('subclass' => @$this->subclass,
				'class' => @$this->class, 
				'multi' =>true)
				);

		text_row(_("Phone:"), 'phone', @$this->data['phone'], 32, 30);
		text_row(_("Secondary Phone Number:"), 'phone2', @$this->data['phone2'], 32, 30);
		text_row(_("Fax Number:"), 'fax', @$this->data['fax'], 32, 30);
		email_row(_("E-mail:"), 'email', @$this->data['email'], 35, 55);

		table_section(2);
		table_section_title("&nbsp;");
		textarea_row(_("Address:"), 'address', @$this->data['address'], 30, 4);
		languages_list_row( _("Document Language:"), 'lang', @$this->data['lang'], _("Customer default"));

		textarea_row(_("Notes:"), 'notes', @$this->data['notes'], 30, 4);
		end_outer_table(1);
	}

	function db_insert() {
			$dat = $this->data;
			return add_crm_person($dat['ref'], $dat['name'], $dat['name2'], $dat['address'],
				$dat['phone'], $dat['phone2'], $dat['fax'], $dat['email'], $dat['lang'],
				$dat['notes'], $dat['contacts'], $this->entity);
	}

	function db_update() {
			return update_crm_person($this->selected_id, $_POST['ref'], $_POST['name'], $_POST['name2'], 
				$_POST['address'], $_POST['phone'], $_POST['phone2'], $_POST['fax'], $_POST['email'], 
				$_POST['lang'], $_POST['notes'], $_POST['assgn'], $this->entity, $this->class);
	}

	function insert_check() {
		if (strlen($_POST['name']) == 0) {
			display_error(_("The contact name cannot be empty."));
			set_focus('name');
			return false;
		}
		if (strlen($_POST['ref']) == 0) {
			display_error(_("Contact reference cannot be empty."));
			set_focus('ref');
			return false;
		}
		if (count_array(@$_POST['assgn']) == 0) {
			display_error(_("You have to select at least one category."));
			set_focus('assgn');
			return false;
		}
		return true; 
	}

	function db_read() {
		return get_crm_person($this->selected_id);
	}

	function delete_check() {
		return true;
	}
	//
	//	Delete all contacts for person in current class/entity
	//
	function db_delete() {
		if (delete_crm_contacts($this->selected_id, $this->class, $this->entity)) {
			// if this is the last relation - delete contact data
			$rels = get_person_contacts($this->selected_id);
			if (count($rels)==0) {
				return delete_crm_person($this->selected_id);
			}
		}
		return false;
	}
}

