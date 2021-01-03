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
function focus_budget(i) {
    save_focus(i);
	i.setAttribute('_last', get_amount(i.name));
}

function blur_budget(i) {
	var amount = get_amount(i.name);
	var total = get_amount('Total', 1);
	
	price_format(i.name, amount, 0);
	price_format('Total', total+amount-i.getAttribute('_last'), 0, 1, 1);
}


var budget_calc = {
	'.amount': function(e) {
		e.onblur = function() {
			blur_budget(this);
		  };
		e.onfocus = function() {
			focus_budget(this);
		};
	}
}

Behaviour.register(budget_calc);
