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

		if(e.hasAttribute('multiple') === false) {
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
		}
	}
}
Behaviour.register(loadSelect2);