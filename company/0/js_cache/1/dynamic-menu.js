

$(function(){var $nav=$('div.collapsible-nav');var $btn=$('div.collapsible-nav button');var $vlinks=$('div.collapsible-nav .collapsible-menu');var $hlinks=$('div.collapsible-nav .hidden-menu-links');var numOfItems=0;var totalSpace=0;var breakWidths=[];$vlinks.children().outerWidth(function(i,w){totalSpace+=w;numOfItems+=1;breakWidths.push(totalSpace);});var availableSpace,numOfVisibleItems,requiredSpace;function check(minWidth){availableSpace=$vlinks.width()-50;numOfVisibleItems=$vlinks.children().length;requiredSpace=breakWidths[numOfVisibleItems-1];if(requiredSpace > availableSpace){$vlinks.children().last().prependTo($hlinks);numOfVisibleItems-=1;check();}
else if(availableSpace > breakWidths[numOfVisibleItems]){$hlinks.children().first().appendTo($vlinks);numOfVisibleItems+=1;}
$btn.attr("count",numOfItems-numOfVisibleItems);if(numOfVisibleItems===numOfItems){$btn.attr('hidden','hidden');$hlinks.addClass('hidden');}
else
$btn.removeAttr('hidden');}
function reset(){$hlinks.children().appendTo($vlinks);$btn.attr('hidden','hidden');numOfVisibleItems=$vlinks.children().length;}
$(window).on('resize load',function(){check(768);});$(window).on('click',function(){if($(event.target).is($btn))
$hlinks.toggleClass('hidden');else
$hlinks.addClass('hidden');});check(768);});