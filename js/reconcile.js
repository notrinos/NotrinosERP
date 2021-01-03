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
function focus_amount(i) {
    save_focus(i);
	i.setAttribute('_last', get_amount(i.name));
}

function blur_amount(i) {
	var change = get_amount(i.name);

	price_format(i.name, change, user.pdec);
	change = change-i.getAttribute('_last');
	if (i.name=='beg_balance')
		change = -change;

	price_format('difference', get_amount('difference',1,1)+change, user.pdec, 1);
}

var balances = {
	'.amount': function(e) {
		e.onblur = function() {
			blur_amount(this);
		  };
		e.onfocus = function() {
			focus_amount(this);
		};
	}
}

Behaviour.register(balances);
