

function set_mark(img){var box=document.getElementById('ajaxmark');if(box){if(img)box.src=user.theme+'images/'+img;box.style.visibility=img ? 'visible':'hidden';}
}
function disp_msg(msg,cl){var box=document.getElementById('msgbox');box.innerHTML="<div class='"+(cl||'err_msg')+"'>"+msg+'</div>';if(msg!='')window.scrollTo(0,element_pos(box).y-10);}
JsHttpRequest.request=function(trigger,form,tout){tout=tout||10000;document.getElementById('msgbox').innerHTML='';set_mark(tout>10000 ? 'progressbar.gif':'ajax-loader.gif');JsHttpRequest._request(trigger,form,tout,0);};JsHttpRequest._request=function(trigger,form,tout,retry){if(trigger.tagName=='A'){var content={};var upload=0;var url=trigger.href;if(trigger.id)
content[trigger.id]=1;}
else{var submitObj=typeof(trigger)=="string" ?
document.getElementsByName(trigger)[0]:trigger;form=form||(submitObj&&submitObj.form);var upload=form&&form.enctype=='multipart/form-data';var url=form ? form.getAttribute('action'):
window.location.toString();var content=this.formInputs(trigger,form,upload);if(!form)
url=url.substring(0,url.indexOf('?'));if(!submitObj){content[trigger]=1;}
}
content['_random']=Math.random()*1234567;var tcheck=setTimeout(
function(){for(var id in JsHttpRequest.PENDING){var call=JsHttpRequest.PENDING[id];if(call!=false){if(call._ldObj.xr)
call._ldObj.xr.onreadystatechange=function(){};call.abort();delete JsHttpRequest.PENDING[id];}
}
set_mark(retry ? 'ajax-loader2.gif':'warning.png');if(retry)
JsHttpRequest._request(trigger,form,tout,retry-1);},
tout);JsHttpRequest.query(
(upload ? "form.":"")+"POST "+url,
content,
function(result,errors){var newwin=0;if(result){for(var i in result){atom=result[i];cmd=atom['n'];property=atom['p'];type=atom['c'];id=atom['t'];data=atom['data'];objElement=document.getElementsByName(id)[0]||document.getElementById(id);if(cmd=='as'){eval("objElement.setAttribute('"+property+"','"+data+"');");}
else if(cmd=='up'){if(objElement){if(objElement.tagName=='INPUT'||objElement.tagName=='TEXTAREA')
objElement.value=data;else
objElement.innerHTML=data;}
}
else if(cmd=='di'){objElement.disabled=data;}
else if(cmd=='fc'){_focus=data;}
else if(cmd=='js'){__isGecko ? eval(data):setTimeout(function(){eval(data);},200);}
else if(cmd=='rd'){window.location=data;}
else if(cmd=='pu'){newwin=1;window.open(data,'REP_WINDOW','toolbar=no,scrollbars=yes,resizable=yes,menubar=no');}
else{errors=errors+'<br>Unknown ajax function: '+cmd;}
}
if(tcheck)
JsHttpRequest.clearTimeout(tcheck);document.getElementById('msgbox').innerHTML=errors;set_mark();Behaviour.apply();if(errors.length>0)
window.scrollTo(0,0);if(!newwin){setFocus();}
}
},
false
);};JsHttpRequest.formInputs=function(inp,objForm,upload){var submitObj=inp;var q={};if(typeof(inp)=="string")
submitObj=document.getElementsByName(inp)[0]||inp;objForm=objForm||(submitObj&&submitObj.form);if(objForm){var formElements=objForm.elements;for(var i=0;i < formElements.length;i++){var el=formElements[i];var name=el.name;if(!el.name)continue;if(upload){if(submitObj.type=='submit'&&el==submitObj){q[name]=el.value;continue;}
}
if(el.type)
if((el.type=='radio'&&el.checked==false)||(el.type=='submit'&&(!submitObj||el.name!=submitObj.name)))
continue;if(el.disabled&&el.disabled==true)
continue;if(name){if(el.type=='select-multiple'){name=name.substr(0,name.length-2);q[name]=new Array;for(var j=0;j < el.length;j++){s=name.substring(0,name.length-2);if(el.options[j].selected==true)
q[name].push(el.options[j].value);}
}
else
if(el.type=='file')
q[name]=el;else{if(el.type=='checkbox')
q[name]=(el.checked==true);else
q[name]=el.value;}
}
}
}
return q;};function price_format(post,num,dec,label,color){var el=label ? document.getElementById(post):document.getElementsByName(post)[0];if(isNaN(num))
num="0";sign=(num==(num=Math.abs(num)));var max=dec=='max';if(max)dec=num==0 ? 2:15-Math.floor(Math.log(Math.abs(num)));if(dec<0)dec=2;decsize=Math.pow(10,dec);num=Math.floor(num*decsize+0.50000000001);cents=num%decsize;num=Math.floor(num/decsize).toString();for(i=cents.toString().length;i<dec;i++){cents="0"+cents;}
if(max)
cents=cents.toString().replace(/0+$/,'');for(var i=0;i < Math.floor((num.length-(1+i))/3);i++)
num=num.substring(0,num.length-(4*i+3))+user.ts+num.substring(num.length-(4*i+3));num=((sign)?'':'-')+num;if(dec!=0&&(!max||cents!=0))
num=num+user.ds+cents;if(label)
el.innerHTML=num;else
el.value=num;if(color)
el.style.color=(sign)? '':'#FF0000';}
function get_amount(doc,label){if(label)
var val=document.getElementById(doc).innerHTML;else
var val=typeof(doc)=="string" ? document.getElementsByName(doc)[0].value:doc.value;val=val.replace(new RegExp('\\'+user.ts,'g'),'');val=+val.replace(new RegExp('\\'+user.ds,'g'),'.');return isNaN(val)? 0:val;}
function goBack(deep){if(window.opener)
window.close();else
window.history.go(deep||-1);}
function setFocus(name,byId){var el=null;if(typeof(name)=='object')
el=name;else{if(!name){if(_focus)
name=_focus;else
if(document.forms.length){var cur=document.getElementsByName('_focus')[document.forms.length-1];if(cur)name=cur.value;}
}
if(name)
if(byId||!(el=document.getElementsByName(name)[0]))
el=document.getElementById(name);}
if(el!=null&&el.focus){var tmp=function(){el.focus();if(el.select)el.select();};setTimeout(tmp,0);}
}

function move_focus(dir,e0,neighbours){var p0=element_pos(e0);var t;var l=0;for(var i=0;i<neighbours.length;i++){var e=neighbours[i];var p=element_pos(e);if(p!=null&&(e.className=='menu_option'||e.className=='printlink'||e.className=='repclass_link'||e.className=='repopts_link')){if(((dir==40)&&(p.y>p0.y))||(dir==38&&(p.y<p0.y))||((dir==37)&&(p.x<p0.x))||((dir==39&&(p.x>p0.x)))){var l1=(p.y-p0.y)*(p.y-p0.y)+(p.x-p0.x)*(p.x-p0.x);if((l1<l)||(l==0))
l=l1;t=e;}
}
}
if(t)
setFocus(t);return t;}
var __isGecko=navigator.userAgent.match(/gecko/i);function element_pos(e){var res=new Object();res.x=0;res.y=0;if(e!==null){res.x=e.offsetLeft;res.y=e.offsetTop;var offsetParent=e.offsetParent;var parentNode=e.parentNode;while(offsetParent!==null&&offsetParent.style.display!='none'){res.x+=offsetParent.offsetLeft;res.y+=offsetParent.offsetTop;if(offsetParent!=document.body&&offsetParent!=document.documentElement){res.x-=offsetParent.scrollLeft;res.y-=offsetParent.scrollTop;}
if(__isGecko){while(offsetParent!=parentNode&&parentNode!==null){res.x-=parentNode.scrollLeft;res.y-=parentNode.scrollTop;parentNode=parentNode.parentNode;}
}
parentNode=offsetParent.parentNode;offsetParent=offsetParent.offsetParent;}
}
if(parentNode!=document.documentElement)return null;return res;}
function string_contains(haystack,needle){var words=haystack.split(' ');return words.indexOf(needle)>-1;}
