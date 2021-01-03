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
function set_mark(img) {
	var box = document.getElementById('ajaxmark');
	if(box) {
		if(img) box.src = user.theme+'images/'+ img;
		box.style.visibility = img ? 'visible' : 'hidden';
	}
}

function disp_msg(msg, cl) {
	var box = document.getElementById('msgbox');
	box.innerHTML= "<div class='"+(cl || 'err_msg')+"'>"+ msg+'</div>';
//	box.style.display = msg=='' ? 'none':'block';
    if (msg!='') window.scrollTo(0,element_pos(box).y-10);
}

//
//	JsHttpRequest class extensions.
//
// Main functions for asynchronus form submitions
// 	Trigger is the source of request and can have following forms:
// 	- input object - all form values are also submited
//  - arbitrary string - POST var trigger with value 1 is added to request;
//		if form parameter exists also form values are submited, otherwise
//		request is directed to current location
//
JsHttpRequest.request= function(trigger, form, tout) {
//	if (trigger.type=='submit' && !validate(trigger)) return false;
	tout = tout || 10000;	// default timeout value
	document.getElementById('msgbox').innerHTML='';
	set_mark(tout>10000 ? 'progressbar.gif' : 'ajax-loader.gif');
	JsHttpRequest._request(trigger, form, tout, 0);
};

JsHttpRequest._request = function(trigger, form, tout, retry) {
		if (trigger.tagName=='A') {
			var content = {};
			var upload = 0;
			var url = trigger.href;
			if (trigger.id) content[trigger.id] = 1;
		} else {
		var submitObj = typeof(trigger) == "string" ?
			document.getElementsByName(trigger)[0] : trigger;

		form = form || (submitObj && submitObj.form);

		var upload = form && form.enctype=='multipart/form-data';

		var url = form ? form.getAttribute('action') :
		  window.location.toString();

		var content = this.formInputs(trigger, form, upload);

		if (!form) url = url.substring(0, url.indexOf('?'));

		if (!submitObj) {
			content[trigger] = 1;
			}
		}
			// this is to avoid caching problems
		content['_random'] = Math.random()*1234567;

		var tcheck = setTimeout(
			function() {
				for(var id in JsHttpRequest.PENDING)  {
					var call = JsHttpRequest.PENDING[id];
				 	if (call != false) {
					if (call._ldObj.xr) // needed for gecko
						call._ldObj.xr.onreadystatechange = function(){};
					call.abort(); // why this doesn't kill request in firebug?
//						call._ldObj.xr.abort();
						delete JsHttpRequest.PENDING[id];
					}
				}
				set_mark(retry ? 'ajax-loader2.gif':'warning.png' );
				if(retry)
					JsHttpRequest._request(trigger, form, tout, retry-1);
			}, tout );

        JsHttpRequest.query(
            (upload ? "form." : "")+"POST "+url, // force form loader
	    	content,
            // Function is called when an answer arrives.
	    function(result, errors) {
                // Write the answer.
			var newwin = 0;
	        if (result) {
		  	  for(var i in result ) {
			  atom = result[i];
			  cmd = atom['n'];
			  property = atom['p'];
			  type = atom['c'];
			  id = atom['t'];
			  data = atom['data'];
//				debug(cmd+':'+property+':'+type+':'+id);
			// seek element by id if there is no elemnt with given name
			  objElement = document.getElementsByName(id)[0] || document.getElementById(id);
    		  if(cmd=='as') {
				  eval("objElement.setAttribute('"+property+"','"+data+"');");
			  } else if(cmd=='up') {
//				if(!objElement) alert('No element "'+id+'"');
				if(objElement) {
			    if (objElement.tagName == 'INPUT' || objElement.tagName == 'TEXTAREA')
				  objElement.value = data;
			    else
				  objElement.innerHTML = data; // selector, div, span etc
				}
		  	  } else if(cmd=='di') { // disable/enable element
				  objElement.disabled = data;
			  } else if(cmd=='fc') { // set focus
				  _focus = data;
			  } else if(cmd=='js') {	// evaluate js code
				__isGecko ? eval(data) : setTimeout(function(){eval(data);}, 200); // timeout required by IE7/8
			  } else if(cmd=='rd') {	// client-side redirection
				  window.location = data;
			  } else if(cmd=='pu') {	// pop-up
			  	  newwin = 1;
			  	  window.open(data,'REP_WINDOW','toolbar=no,scrollbars=yes,resizable=yes,menubar=no');
			  } else {
				  errors = errors+'<br>Unknown ajax function: '+cmd;
			}
		  }
		 if(tcheck)
		   JsHttpRequest.clearTimeout(tcheck);
        // Write errors to the debug div.
		  document.getElementById('msgbox').innerHTML = errors;
		  set_mark();

		  Behaviour.apply();

		  if (errors.length>0)
			window.scrollTo(0,0);
			//document.getElementById('msgbox').scrollIntoView(true);
	  // Restore focus if we've just lost focus because of DOM element refresh
		  	if(!newwin) {
		  		setFocus();
			}
		}
            },
	        false  // do not disable caching
        );
	};
	// collect all form input values plus inp trigger value
	JsHttpRequest.formInputs = function(inp, objForm, upload)
	{
		var submitObj = inp;
		var q = {};

		if (typeof(inp) == "string")
			submitObj = document.getElementsByName(inp)[0]||inp;

		objForm = objForm || (submitObj && submitObj.form);

		if (objForm)
		{
			var formElements = objForm.elements;
			for( var i=0; i < formElements.length; i++)
			{
			  var el = formElements[i];
			  var name = el.name;
				if (!el.name) continue;
				if(upload) { // for form containing file inputs collect all
					// form elements and add value of trigger submit button
					// (internally form is submitted via form.submit() not button click())
					if (submitObj.type=='submit' && el==submitObj)
					{
						q[name] =  el.value;
						continue;
					}
				}
				if (el.type )
				  if(
				  (el.type == 'radio' && el.checked == false)
				  || (el.type == 'submit' && (!submitObj || el.name!=submitObj.name)))
					continue;
				if (el.disabled && el.disabled == true)
					continue;
				if (name)
				{
					if(el.type=='select-multiple')
					{
						name = name.substr(0,name.length-2);
						q[name] = new Array;
						for (var j = 0; j < el.length; j++)
						{
							s = name.substring(0, name.length-2);
							if (el.options[j].selected == true)
								q[name].push(el.options[j].value);
						}
					}
					else
					if (el.type=='file')
						q[name] = el;
					else
					{
						if (el.type == 'checkbox') {
							q[name] = (el.checked == true);
						} else {
							q[name] = el.value;
						}
					}
				}
			}
		}
		return q;
	};
//
//	User price formatting
//
function price_format(post, num, dec, label, color) {
	var el = label ? document.getElementById(post) : document.getElementsByName(post)[0];
	//num = num.toString().replace(/\$|\,/g,'');
	if(isNaN(num))
		num = "0";
	sign = (num == (num = Math.abs(num)));
	var max = dec=='max';
	if(max) dec = num==0 ? 2 : 15 - Math.floor(Math.log(Math.abs(num)));
	if(dec<0) dec = 2;
	decsize = Math.pow(10, dec);
	num = Math.floor(num*decsize+0.50000000001);
	cents = num%decsize;
	num = Math.floor(num/decsize).toString();
	for( i=cents.toString().length; i<dec; i++){
		cents = "0"+cents;
	}
	if (max) // strip trailing 0
		cents = cents.toString().replace(/0+$/,'');
	for (var i = 0; i < Math.floor((num.length-(1+i))/3); i++)
		num = num.substring(0,num.length-(4*i+3))+user.ts+
			num.substring(num.length-(4*i+3));
	 num = ((sign)?'':'-') + num;
	if(dec!=0 && (!max || cents!=0))
		num = num + user.ds + cents;
	if(label)
	    el.innerHTML = num;
	else
	    el.value = num;
	if(color) {
			el.style.color = (sign) ? '' : '#FF0000';
	}
}

function get_amount(doc, label) {
	    if(label)
			var val = document.getElementById(doc).innerHTML;
	    else
			var val = typeof(doc) == "string" ?
			document.getElementsByName(doc)[0].value : doc.value;

		val = val.replace(new RegExp('\\'+user.ts, 'g'),'');
		val = +val.replace(new RegExp('\\'+user.ds, 'g'),'.');
		return isNaN(val) ? 0 : val;
}

function goBack(deep) {
	if (window.opener)
	 window.close();
	else
	 window.history.go(deep || -1);
}

function setFocus(name, byId) {
 var el = null;
 if(typeof(name)=='object')
 	el = name;
 else {
	if(!name) { // page load/ajax update
		if (_focus)
			name = _focus;	// last focus set in onfocus handlers
		else
	 		if (document.forms.length) {	// no current focus (first page display) -  set it from from last form
			  var cur = document.getElementsByName('_focus')[document.forms.length-1];
			  if(cur) name = cur.value;
			}
	  }
      if (name)
	    if(byId || !(el = document.getElementsByName(name)[0]))
		  el = document.getElementById(name);
  }
  if (el != null && el.focus) {
    // The timeout is needed to prevent unpredictable behaviour on IE & Gecko.
    // Using tmp var prevents crash on IE5

    var tmp = function() {el.focus(); if (el.select) el.select();};
	setTimeout(tmp, 0);
  }
}
/*
	Find closest element in neighbourhood and set focus.
	dir is arrow keycode.
*/
function move_focus(dir, e0, neighbours)
{
	var p0 = element_pos(e0);
	var t;
	var l=0;
	for(var i=0; i<neighbours.length; i++) {
		var e = neighbours[i];
		var p = element_pos(e);
		if (p!=null && (e.className=='menu_option' || e.className=='printlink'
				 || e.className == 'repclass_link' || e.className == 'repopts_link')) {
			if (((dir==40) && (p.y>p0.y)) || (dir==38 && (p.y<p0.y))
				|| ((dir==37) && (p.x<p0.x)) || ((dir==39 && (p.x>p0.x)))) {
					var l1 = (p.y-p0.y)*(p.y-p0.y)+(p.x-p0.x)*(p.x-p0.x);
					if ((l1<l) || (l==0)) {
						l = l1; t = e;
					}
			}
		}
	}
	if (t)
		setFocus(t);
	return t;
}

var __isGecko = navigator.userAgent.match(/gecko/i); // i.e. Gecko or KHTML, like Gecko ;)
//returns the absolute position of some element within document
function element_pos(e) {
	var res = new Object();
		res.x = 0; res.y = 0;
	if (e !== null) {
		res.x = e.offsetLeft;
		res.y = e.offsetTop;
		var offsetParent = e.offsetParent;
		var parentNode = e.parentNode;

		while (offsetParent !== null && offsetParent.style.display != 'none') {
			res.x += offsetParent.offsetLeft;
			res.y += offsetParent.offsetTop;
			// the second case is for IE6/7 in some doctypes
			if (offsetParent != document.body && offsetParent != document.documentElement) {
				res.x -= offsetParent.scrollLeft;
				res.y -= offsetParent.scrollTop;
			}
			      //next lines are necessary to support FireFox problem with offsetParent
			if (__isGecko) {
				while (offsetParent != parentNode && parentNode !== null) {
					res.x -= parentNode.scrollLeft;
					res.y -= parentNode.scrollTop;

					parentNode = parentNode.parentNode;
				}
			}
			parentNode = offsetParent.parentNode;
			offsetParent = offsetParent.offsetParent;
		}
	}
	// parentNode has style.display set to none
	if (parentNode != document.documentElement) return null;
	return res;
}

function string_contains(haystack, needle) {
  var words = haystack.split(' ');
  return words.indexOf(needle) > -1;
}
