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

var loadSelect2 = {
	'select': function(e) {

		if((e.hasAttribute('multiple') === false) && $(e).hasClass('nosearch') === false) {
			$(e).select2({
				dropdownAutoWidth : true,
				// break a select option item into multi lines
				templateResult: function(item) {
					var selectionText = item.text.split('\n');
					var returnString = $('<span></span>');
					$.each(selectionText, function(index, value){
						line = value === undefined ? '' : value;
						returnString.append(line + '</br>');
					})
						
					return returnString;
				}
			});
			$(e).on('select2:close', function() {
				$(this).focus();
			});
			$(e).on('select2:open', function(e2){

				$('.dynamic_combo_btn').remove();

				var target_id = $(e2.target).attr('id');
				var search_btn = $('#_'+target_id+'_search').clone();
				var add_btn = $('#_'+target_id+'_add').clone();
				$(search_btn).addClass('dynamic_combo_btn');
				$(add_btn).addClass('dynamic_combo_btn');
				$(search_btn).removeAttr('id hidden');
				$(add_btn).removeAttr('id hidden');

				$('.select2-dropdown').append(search_btn);
				if($(add_btn) !== 'undefined')
					$('.select2-dropdown').append(add_btn);
				$(search_btn).add(add_btn).click(function(){
					$('select').select2('close');
				});
				
			});
		}
	}
}
Behaviour.register(loadSelect2);