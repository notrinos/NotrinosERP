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

$(function() {
	var $nav = $('div.collapsible-nav');
	var $btn = $('div.collapsible-nav button');
	var $vlinks = $('div.collapsible-nav .collapsible-menu');
	var $hlinks = $('div.collapsible-nav .hidden-menu-links');

	var numOfItems = 0;
	var totalSpace = 0;
	var breakWidths = [];

	// Get initial state
	$vlinks.children().outerWidth(function(i, w) {
		totalSpace += w;
		numOfItems += 1;
		breakWidths.push(totalSpace);
	});

	var availableSpace, numOfVisibleItems, requiredSpace;

	function check(minWidth) {
		// Get instant state
		availableSpace = $vlinks.width() - 50;
		numOfVisibleItems = $vlinks.children().length;
		requiredSpace = breakWidths[numOfVisibleItems - 1];

		// There is not enought space
		if (requiredSpace > availableSpace) {
			$vlinks.children().last().prependTo($hlinks);
			numOfVisibleItems -= 1;
			check();
		}// There is more than enough space
		else if (availableSpace > breakWidths[numOfVisibleItems]) {
			$hlinks.children().first().appendTo($vlinks);
			numOfVisibleItems += 1;
		}
		// Update the button accordingly
		$btn.attr("count", numOfItems - numOfVisibleItems);
		if (numOfVisibleItems === numOfItems) {
			$btn.attr('hidden', 'hidden');
			$hlinks.addClass('hidden');
		}
		else
			$btn.removeAttr('hidden');

		// if($(window).width() <= minWidth)
			// reset();
	}

	function reset() {
		$hlinks.children().appendTo($vlinks);
		$btn.attr('hidden', 'hidden');

		numOfVisibleItems = $vlinks.children().length;
	}

	$(window).on('resize load', function() {
		check(768);
	});
	
	$(window).on('click', function() {
		if ($(event.target).is($btn))
			$hlinks.toggleClass('hidden');
		else
			$hlinks.addClass('hidden');
	});

	check(768);
});