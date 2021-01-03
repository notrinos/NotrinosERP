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
var replinks = {
	'a, button': function(e) { // traverse menu
		e.onkeydown = function(ev) { 
			ev = ev||window.event;
			key = ev.keyCode||ev.which;
			if(key==37 || key==38 || key==39 || key==40) {
				move_focus(key, e, document.links);
				ev.returnValue = false;
				return false;
			}
		}
	},
	'a.repopts_link': 	function(e) {
		e.onclick = function() {
		    save_focus(this);
		    set_options(this);
			JsHttpRequest.request(this, null);
			return false;
		}
	},
	'a.repclass_link': function(e) {
		e.onclick = function() {
		    save_focus(this);
			showClass(this.id.substring(5)); // id=classX
			return false;
		}
	},
}

function set_options(e)
{
    var replinks = document.getElementsBySelector('a.repopts_link');
	for(var i in replinks)
		replinks[i].style.fontWeight = replinks[i]==e ? 'bold' : 'normal';
}

function showClass(pClass) {
	var classes = document.getElementsBySelector('.repclass');
	for(var i in  classes) {
		cl = classes[i];
		cl.style.display = (cl.id==('TAB_'+pClass)) ? "block" : "none";
	}
	var classlinks = document.getElementsBySelector('a.repclass_link');
	for(var i in classlinks)
		classlinks[i].style.fontWeight = classlinks[i].id == ('class'+pClass) ?
			'bold' : 'normal';

    set_options(); // clear optionset links
	document.getElementById('rep_form').innerHTML = '';
	return false;
}

Behaviour.register(replinks);