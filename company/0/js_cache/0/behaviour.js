

var Behaviour={list:new Array,
register:function(sheet){Behaviour.list.push(sheet);},
start:function(){Behaviour.addLoadEvent(function(){Behaviour.apply();});},
apply:function(){for(h=0;sheet=Behaviour.list[h];h++){for(selector in sheet){var sels=selector.split(',');for(var n=0;n < sels.length;n++){list=document.getElementsBySelector(sels[n]);if(!list)
continue;for(i=0;element=list[i];i++){sheet[selector](element);}
}
}
}
},
addLoadEvent:function(func){var oldonload=window.onload;if(typeof window.onload!='function')
window.onload=func;else{window.onload=function(){oldonload();func();}
}
}
}
Behaviour.start();
function getAllChildren(e){return e.all ? e.all:e.getElementsByTagName('*');}
document.getElementsBySelector=function(selector){if(!document.getElementsByTagName)
return new Array();var tokens=selector.split(' ');var currentContext=new Array(document);for(var i=0;i < tokens.length;i++){token=tokens[i].replace(/^\s+/,'').replace(/\s+$/,'');if(token.indexOf('#')>-1){var bits=token.split('#');var tagName=bits[0];var id=bits[1];var element=document.getElementById(id);if(element===null||(tagName&&element.nodeName.toLowerCase()!=tagName)){return new Array();}
currentContext=new Array(element);continue;}
if(token.indexOf('.')>-1){var bits=token.split('.');var tagName=bits[0];var className=bits[1];if(!tagName)
tagName='*';var found=new Array;var foundCount=0;for(var h=0;h < currentContext.length;h++){var elements;if(tagName=='*')
elements=getAllChildren(currentContext[h]);else
elements=currentContext[h].getElementsByTagName(tagName);for(var j=0;j < elements.length;j++){found[foundCount++]=elements[j];}
}
currentContext=new Array;var currentContextIndex=0;for(var k=0;k < found.length;k++){if(found[k].getAttribute('class')!=null&&found[k].getAttribute('class').match(new RegExp('\\b'+className+'\\b')))
currentContext[currentContextIndex++]=found[k];}
continue;}

if(token.match(new RegExp('^(\\w*)\\[(\\w+)([=~\\|\\^\\$\\*]?)=?"?([^\\]"]*)"?\\]$'))){var tagName=RegExp.$1;var attrName=RegExp.$2;var attrOperator=RegExp.$3;var attrValue=RegExp.$4;if(!tagName)
tagName='*';var found=new Array;var foundCount=0;for(var h=0;h < currentContext.length;h++){var elements;if(tagName=='*')
elements=getAllChildren(currentContext[h]);else
elements=currentContext[h].getElementsByTagName(tagName);for(var j=0;j < elements.length;j++){found[foundCount++]=elements[j];}
}
currentContext=new Array;var currentContextIndex=0;var checkFunction;switch(attrOperator){case '=':
checkFunction=function(e){return(e.getAttribute(attrName)==attrValue);};break;case '~':
checkFunction=function(e){var a=e.getAttribute(attrName);return(a&&a.match(new RegExp('\\b'+attrValue+'\\b')));};break;case '|':
checkFunction=function(e){var a=e.getAttribute(attrName);return(a&&a.match(new RegExp('^'+attrValue+'-?')));};break;case '^':
checkFunction=function(e){var a=e.getAttribute(attrName);return(a&&a.indexOf(attrValue)==0);};break;case '$':
checkFunction=function(e){var a=e.getAttribute(attrName);return(a&&a.lastIndexOf(attrValue)==e.getAttribute(attrName).length-attrValue.length);};break;case '*':
checkFunction=function(e){var a=e.getAttribute(attrName);return(a&&a.indexOf(attrValue)>-1);};break;default:
checkFunction=function(e){return e.getAttribute(attrName);};}
currentContext=new Array;var currentContextIndex=0;for(var k=0;k < found.length;k++){if(checkFunction(found[k]))
currentContext[currentContextIndex++]=found[k];}
continue;}
if(!currentContext[0])
return;tagName=token;var found=new Array;var foundCount=0;for(var h=0;h < currentContext.length;h++){var elements=currentContext[h].getElementsByTagName(tagName);for(var j=0;j < elements.length;j++){found[foundCount++]=elements[j];}
}
currentContext=found;}
return currentContext;}

