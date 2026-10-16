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
			var employeeOptions = e.hasAttribute('data-employee-search') ? employeeSelectorOptions(e) : {};
			$(e).select2($.extend({
				dropdownAutoWidth : true,
				templateResult: function(item) {
					// replace(/</g, '&lt;') : prevent the code in the option’s value from being executed by the browser.
					// split('\n') : break a select option item into multi lines
					var selectionText = item.text.replace(/</g, '&lt;').split('\n');
					var returnString = $('<span></span>');
					$.each(selectionText, function(index, value){
						line = value === undefined ? '' : value;
						returnString.append(line + '</br>');
					})
						
					return returnString;
				}
			}, employeeOptions));
			$(e).on('select2:close', function() {
				var mainContent = document.getElementById('modern-main-content');
				var selectElementRect = this.getBoundingClientRect();
				var isInsideViewport = true;

				if (mainContent) {
					var mainContentRect = mainContent.getBoundingClientRect();
					isInsideViewport = selectElementRect.top >= mainContentRect.top
						&& selectElementRect.bottom <= mainContentRect.bottom;
				}

				// Prevent off-screen refocus from forcing the main content scroll position.
				if (isInsideViewport) {
					$(this).focus();
				}

				if (window.jQuery) {
					$('.modern-main-content, .modern-sidebar').off('scroll.select2');
					$(window).off('scroll.select2 resize.select2 orientationchange.select2');
				}
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

var employeeSelectorRequestTail = null;

/**
 * Serialize employee reads so normal rotating session cookies reach the browser.
 * Cancellation rejects/ignores a superseded result; an active HTTP response
 * drains instead of discarding its Set-Cookie header. Queued cancelled reads
 * never reach the server. This queue includes popup hydration across controls.
 */
function employeeSelectorRequest(options) {
	var previous = employeeSelectorRequestTail || $.Deferred().resolve().promise();
	var complete = $.Deferred(), outcome = $.Deferred(), cancelled = false;
	var handle = outcome.promise();
	handle.abort = function() {
		cancelled = true;
		outcome.reject({status: 0}, 'abort');
	};
	employeeSelectorRequestTail = complete.promise();
	previous.always(function() {
		if (cancelled) { complete.resolve(); return; }
		$.ajax(options).done(function(data, status, xhr) {
			if (!cancelled) outcome.resolve(data, status, xhr);
		}).fail(function(xhr, reason, error) {
			if ([401,403,409].indexOf(xhr.status) !== -1) {
				document.querySelectorAll('select[data-employee-search]').forEach(function(control) {
					employeeSelectorInvalidate(control, 'Employee access or choices changed. Reload the page.');
				});
			}
			if (!cancelled) outcome.reject(xhr, reason, error);
		}).always(function() { complete.resolve(); });
	});
	return handle;
}

/** Configure only employee controls; Select2 supplies keyboard/loading/empty UI. */
function employeeSelectorRemoteOptions(e) {
	var sequence = 0, request = null, term = null, cursors = {};
	e._employeeSearchGeneration = (e._employeeSearchGeneration || 0) + 1;
	var generation = e._employeeSearchGeneration;
	if (e._employeeSearchRequest) e._employeeSearchRequest.abort();
	var box = document.getElementsByName(e.getAttribute('rel'))[0];
	if (box) {
		box.className = 'employee-search-code'; box.style.display = 'inline-block';
		// Freeform server search must not run the legacy nearest-option matcher.
		box.onblur = null; box.onchange = null;
		box.onkeydown = function(event) {
			if (event.key === 'Enter') {
				event.preventDefault();
				var button = e.form && e.form.elements.namedItem('_'+e.name+'_button');
				if (button) button.click();
			}
		};
	}
	var status = e._employeeSelectorStatus || document.createElement('span');
	e._employeeSelectorStatus = status;
	status.setAttribute('role', 'status'); status.setAttribute('aria-live', 'polite');
	e.parentNode.appendChild(status);
	var resultOptions = {
		ajax: {
			delay: 300,
			data: function(params) {
				++sequence;
				if (request) request.abort();
				var page = params.page || 1, nextTerm = params.term || '';
				if (term !== nextTerm || page === 1) { term = nextTerm; cursors = {}; }
				var token = e.form && e.form.querySelector('input[name="_token"]');
				return { control: e.getAttribute('data-employee-search'), term: nextTerm,
					cursor: cursors[page] || '', _token: token ? token.value : '' };
			},
			transport: function(params, success, failure) {
				var ownSequence = sequence;
				var instance = $(e).data('select2');
				if (generation !== e._employeeSearchGeneration || !document.documentElement.contains(e) || !instance || !instance.isOpen())
					return {abort: function() {}};
				if (request) request.abort();
				status.textContent = 'Loading employees…';
				request = employeeSelectorRequest({url: e.getAttribute('data-employee-url'), type: 'POST',
					data: params.data, dataType: 'json'});
				e._employeeSearchRequest = request;
				request.done(function(data) {
					if (ownSequence !== sequence || generation !== e._employeeSearchGeneration || !document.documentElement.contains(e)) return;
					status.textContent = '';
					success(data);
				}).fail(function(xhr, reason) {
					if (ownSequence !== sequence || generation !== e._employeeSearchGeneration || reason === 'abort' || !document.documentElement.contains(e)) return;
					status.textContent = xhr.status === 409 ? 'Employees changed. Search again to refresh the choices.' : xhr.status === 401 || xhr.status === 403
						? 'Employee access expired or denied. Reload the page.' : 'Employee search failed. Retry or reload the page.';
					if (xhr.status !== 401 && xhr.status !== 403) employeeSelectorShowFallback(e);
					failure(xhr);
				});
				return request;
			},
			processResults: function(data, params) {
				cursors[(params.page || 1)+1] = data.cursor;
				// Adjacent departments remain grouped in the original legacy-name order.
				var groups = [], group = null;
				data.results.forEach(function(item) {
					if (!item.department) { groups.push(item); group = null; return; }
					if (!group || group.text !== item.department) {
						group = {text: item.department, children: []}; groups.push(group);
					}
					group.children.push(item);
				});
				return {results: groups, pagination: data.pagination};
			}
		},
		// Keep all values as strings, including numeric-looking IDs and the blank option.
		templateResult: function(item) { return $('<span>').text(item.text); }
	};
    $(e).off('.employeeSelector').on('select2:closing.employeeSelector', function() {
        ++sequence;
        if (request) request.abort();
        status.textContent = '';
    }).on('change.employeeSelector', function() {
		e._employeeHydrationSequence = (e._employeeHydrationSequence || 0) + 1;
		if (e._employeeHydrationRequest) e._employeeHydrationRequest.abort();
		// Retain only special/current choices in the DOM, not every earlier search page.
		Array.prototype.slice.call(e.options).forEach(function(option) {
			if (!option.selected && option.value !== '') option.remove();
		});
	});
	return resultOptions;
}

/** Hydrate a popup's exact ID through the same cohort and permission boundary. */
function employeeSelectorHydrate(e, value) {
	if (e._employeeLocal) return employeeSelectorRefresh(e, String(value));
	var token = e.form && e.form.querySelector('input[name="_token"]');
	var ownSequence = e._employeeHydrationSequence = (e._employeeHydrationSequence || 0) + 1;
	if (e._employeeHydrationRequest) e._employeeHydrationRequest.abort();
	return e._employeeHydrationRequest = employeeSelectorRequest({url: e.getAttribute('data-employee-url'), type: 'POST', dataType: 'json',
		data: {control: e.getAttribute('data-employee-search'), selected: [String(value)], _token: token ? token.value : ''}
	}).done(function(data) {
		var item = data.results.filter(function(row) { return row.id === String(value); })[0];
		if (ownSequence !== e._employeeHydrationSequence || !document.documentElement.contains(e)) return;
		if (!item) { window.alert('Employee is unavailable for this control. Search again.'); return; }
		e._employeeHydrationRequest = null;
		Array.prototype.slice.call(e.options).forEach(function(option) { if (option.value === item.id) option.remove(); });
		var option = new Option(item.text, item.id, true, true);
		e.add(option); $(e).trigger('change');
	}).fail(function(xhr, reason) { if (reason !== 'abort' && ownSequence === e._employeeHydrationSequence && document.documentElement.contains(e)) window.alert('Employee selection failed. Reload and retry.'); });
}

/** Make this control's existing form-search path available after transport failure. */
function employeeSelectorShowFallback(e) {
	if (!e.form) return;
	var name = e.name.replace(/\[\]$/, '');
	['edit','button','update','more'].forEach(function(suffix) {
		var field = e.form.elements.namedItem('_'+name+'_'+suffix);
		if (field && field.style) field.style.display = 'inline-block';
	});
}

/** Use document-local complete choices; retain remote support for older rendered controls. */
function employeeSelectorOptions(e) {
	if (!e.hasAttribute('data-employee-snapshot') && !e._employeeLocal) return employeeSelectorRemoteOptions(e);
	var snapshot;
	try { snapshot = e.hasAttribute('data-employee-snapshot') ? JSON.parse(e.getAttribute('data-employee-snapshot'))
		: e._employeeLocal.snapshot; } catch (error) { snapshot = null; }
	e.removeAttribute('data-employee-snapshot');
	if (e._employeeSearchRequest) e._employeeSearchRequest.abort();
	if (e._employeeLocal && e._employeeLocal.request) e._employeeLocal.request.abort();
	if (e._employeeLocal && e._employeeLocal.timer) clearTimeout(e._employeeLocal.timer);
	e._employeeSearchGeneration = (e._employeeSearchGeneration || 0) + 1;
	var state = e._employeeLocal = {snapshot: null, generation: e._employeeSearchGeneration, sequence: 0, request: null};
	var status = e._employeeSelectorStatus || document.createElement('span');
	e._employeeSelectorStatus = status;
	status.setAttribute('role', 'status'); status.setAttribute('aria-live', 'polite');
	e.parentNode.appendChild(status);
	var box = document.getElementsByName(e.getAttribute('rel'))[0];
	if (box) {
		box.className = 'employee-search-code'; box.style.display = 'inline-block';
		box.onblur = null; box.onchange = null;
	}
	employeeSelectorInstall(e, snapshot);
	var Utils = $.fn.select2.amd.require('select2/utils');
	var SelectAdapter = $.fn.select2.amd.require('select2/data/select');
	var Results = $.fn.select2.amd.require('select2/results');
	var diacritics = $.fn.select2.amd.require('select2/diacritics');
	/** Match the bundled local Select2's case/diacritic folding, including Unicode names. */
	function fold(value) {
		return String(value || '').replace(/[^\u0000-\u007E]/g, function(letter) { return diacritics[letter] || letter; }).toUpperCase();
	}
	/** Filter the complete domain for both the dropdown and native autocomplete suggestions. */
	function matches(raw) {
		var data = state.snapshot, term = fold(raw), all = !term || term === '*', found = [];
		if (!data) return found;
		if (data.special && (all || fold(data.special.text).indexOf(term) !== -1)) found.push(data.special);
		data.results.forEach(function(item) {
			if (!data.show_inactive && item.inactive && item.id !== e.value && item.id !== raw && (all || fold(item.name).trim() !== term)) return;
			if (!all && fold(item.id).indexOf(term) === -1 && fold(item.name).indexOf(term) === -1
				&& fold(item.text).indexOf(term) === -1 && fold(item.department).indexOf(term) === -1) return;
			found.push(item);
		});
		return found;
	}
	if (box) {
		var datalist = document.getElementById(box.getAttribute('list'));
		if (!datalist) {
			datalist = document.createElement('datalist'); datalist.id = (e.id || e.name)+'_employee_choices';
			box.parentNode.appendChild(datalist); box.setAttribute('list', datalist.id);
		}
		state.datalist = datalist;
		/** Keep suggestions bounded while searching every loaded employee for each new term. */
		state.updateSuggestions = function(announce) {
			var found = matches(box.value.trim()), fragment = document.createDocumentFragment();
			found.slice(0,50).forEach(function(item) {
				var option = document.createElement('option'); option.value = item.id; option.label = item.text;
				fragment.appendChild(option);
			});
			datalist.textContent = ''; datalist.appendChild(fragment);
			if (state.snapshot) status.textContent = announce && found.length > 50
				? 'Showing 50 suggestions. Use the dropdown to browse all '+found.length+' matches.' : '';
		};
		/** Exact IDs win; duplicate names never silently pick a different employee. */
		function chooseTypedEmployee() {
			if (!state.snapshot || Date.now() >= state.snapshot.expires*1000) return false;
			var raw = box.value.trim(), items = state.snapshot.results, item;
			if (state.snapshot.special && raw === state.snapshot.special.id) item = state.snapshot.special;
			if (!item) item = items.filter(function(row) { return row.id === raw; })[0];
			if (!item) {
				var exact = items.filter(function(row) { return fold(row.text).trim() === fold(raw) || fold(row.name).trim() === fold(raw); });
				if (exact.length === 1) item = exact[0];
			}
			if (!item) return false;
			if (e.value !== item.id) {
				if (!Array.prototype.some.call(e.options, function(option) { return option.value === item.id; })) e.add(new Option(item.text, item.id));
				$(e).val(item.id).trigger('change');
			}
			box.value = item.id; state.updateSuggestions(); return true;
		}
		box.oninput = state.updateSuggestions;
		box.onfocus = state.updateSuggestions;
		box.onchange = chooseTypedEmployee; box.onblur = chooseTypedEmployee; box.onkeyup = null;
		box.onkeydown = function(event) {
			if (event.key !== 'Enter' && event.keyCode !== 13) return;
			event.preventDefault();
			if (!chooseTypedEmployee()) {
				$(e).select2('open');
				var instance = $(e).data('select2');
				if (instance) instance.dropdown.$dropdown.find('.select2-search__field').val(box.value).trigger('input');
			}
		};
		state.updateSuggestions();
	}
	/** Keep native options bounded while preserving Select2's selection/change contract. */
	function LocalAdapter(element, options) { LocalAdapter.__super__.constructor.call(this, element, options); }
	Utils.Extend(LocalAdapter, SelectAdapter);
	/** Materialize only a chosen native option before Select2 dispatches its change event. */
	LocalAdapter.prototype.select = function(item) {
		if (!Array.prototype.some.call(e.options, function(option) { return option.value === item.id; }))
			e.add(new Option(item.text, item.id));
		LocalAdapter.__super__.select.call(this, item);
	};
	/** Search the complete authorized domain; no interaction fetch or result truncation. */
	LocalAdapter.prototype.query = function(params, callback) {
		var data = state.snapshot, raw = (params.term || '').trim();
		if (!data || Date.now() >= data.expires * 1000) {
			employeeSelectorInvalidate(e, 'Employee choices expired or unavailable. Reload the page or use form search.');
			callback({results: []}); return;
		}
		callback({results: matches(raw)});
	};
	/** A fixed-height local window; the entire filtered cohort remains in memory. */
	function WindowResults(element, options, data) {
		WindowResults.__super__.constructor.call(this, element, options, data);
		this.rows = []; this.choices = []; this.active = -1; this.start = -1;
	}
	Utils.Extend(WindowResults, Results);
	/** Release both the full filtered view and Select2's cached rendered nodes. */
	WindowResults.prototype.clear = function() {
		this.rows = []; this.choices = []; this.active = -1; this.start = -1;
		this.$results.find('li').each(function() { Utils.RemoveData(this); });
		this.$results.empty();
	};
	/** Keep legacy order and adjacent department headings in a complete logical row list. */
	WindowResults.prototype.append = function(data) {
		var self = this, department = null;
		this.clear(); this.$results.scrollTop(0);
		(data.results || []).forEach(function(item) {
			if (item.department && item.department !== department) self.rows.push({heading: item.department});
			department = item.department || null;
			self.choices.push(self.rows.length); self.rows.push({item: item, index: self.choices.length-1});
		});
		if (!this.choices.length) { this.displayMessage({message: 'noResults'}); return; }
		this.paint();
	};
	/** Replace just the viewport and overscan rows, preserving the entire scroll extent. */
	WindowResults.prototype.paint = function() {
		var height = 32, total = this.rows.length;
		var start = Math.max(0, Math.floor(this.$results.scrollTop()/height)-8);
		var end = Math.min(total, start + Math.ceil((this.$results.height() || 200)/height) + 17);
		if (start === this.start && this.end === end) return;
		this.start = start; this.end = end;
		// IDs are stable across windows: remove old cache entries before creating replacements.
		this.$results.find('li').each(function() { Utils.RemoveData(this); });
		var fragment = document.createDocumentFragment();
		/** Spacer heights preserve the full scroll range without extra option nodes. */
		function spacer(size) {
			var node = document.createElement('li'); node.setAttribute('role', 'presentation');
			node.setAttribute('aria-hidden', 'true'); node.style.cssText = 'padding:0;border:0;height:'+size+'px';
			fragment.appendChild(node);
		}
		spacer(start*height);
		for (var i = start; i < end; ++i) {
			var row = this.rows[i], node;
			if (row.heading) {
				node = document.createElement('li'); node.className = 'select2-results__group';
				node.setAttribute('role', 'presentation'); node.textContent = row.heading;
			} else {
				row.item._resultId = this.container.id+'-employee-'+row.index;
				node = this.option(row.item); node.setAttribute('data-employee-index', row.index);
				node.setAttribute('aria-setsize', this.choices.length); node.setAttribute('aria-posinset', row.index+1);
				node.setAttribute('aria-label', row.item.text+(row.item.department ? ', '+row.item.department : ''));
				node.title = row.item.text;
				var selected = row.item.id === e.value;
				node.classList.toggle('select2-results__option--selected', selected);
				node.setAttribute('aria-selected', selected ? 'true' : 'false');
				if (row.item.inactive) node.classList.add('inactive');
				if (row.index === this.active) node.classList.add('select2-results__option--highlighted');
			}
			node.style.cssText = 'height:32px;box-sizing:border-box;overflow:hidden;white-space:nowrap;text-overflow:ellipsis;padding:6px';
			fragment.appendChild(node);
		}
		this.$results.empty().append(fragment);
		var highlighted = this.getHighlightedResults();
		if (highlighted.length) this.trigger('results:focus', {data: Utils.GetData(highlighted[0], 'data'), element: highlighted});
		else this.container.$container.add(this.container.dropdown.$dropdown).find('[aria-activedescendant]').removeAttr('aria-activedescendant');
	};
	/** Navigate by logical choice index so keyboard traversal crosses every window. */
	WindowResults.prototype.focusChoice = function(index, scroll) {
		if (!this.choices.length) return;
		this.active = Math.max(0, Math.min(index, this.choices.length-1));
		var top = this.choices[this.active]*32, viewport = this.$results.height() || 200;
		if (scroll && top < this.$results.scrollTop()) this.$results.scrollTop(top);
		else if (scroll && top+32 > this.$results.scrollTop()+viewport) this.$results.scrollTop(top+32-viewport);
		this.start = -1; this.paint();
	};
	/** Remove generated option cache entries before releasing the Select2 results root. */
	WindowResults.prototype.destroy = function() {
		this.clear(); WindowResults.__super__.destroy.call(this);
	};
	/** Connect Select2's actual result events to logical keyboard and pointer navigation. */
	WindowResults.prototype.bind = function(container) {
		var self = this; this.container = container;
		this.$results.attr('id', container.id+'-results');
		container.on('results:all', function(params) {
			self.append(params.data);
			var index = self.choices.findIndex(function(row) { return self.rows[row].item.id === e.value; });
			self.focusChoice(index < 0 ? 0 : index, true);
		});
		container.on('results:next', function() { self.focusChoice(self.active+1, true); });
		container.on('results:previous', function() { self.focusChoice(self.active-1, true); });
		container.on('results:select', function() {
			if (state.snapshot && self.active >= 0 && self.choices.length)
				self.trigger('select', {data: self.rows[self.choices[self.active]].item});
		});
		container.on('results:toggle', function() { container.trigger('results:select', {}); });
		container.on('open', function() { self.$results.attr({'aria-expanded':'true', 'aria-hidden':'false'}); });
		container.on('close', function() { self.$results.attr({'aria-expanded':'false', 'aria-hidden':'true'}).removeAttr('aria-activedescendant'); });
		this.$results.on('scroll.employeeSelector', function() { self.paint(); });
		this.$results.on('mouseenter.employeeSelector', '[data-employee-index]', function() {
			self.active = Number(this.getAttribute('data-employee-index'));
			self.getHighlightedResults().removeClass('select2-results__option--highlighted');
			this.classList.add('select2-results__option--highlighted');
			self.trigger('results:focus', {data: Utils.GetData(this, 'data'), element: $(this)});
		});
		this.$results.on('mouseup.employeeSelector', '[data-employee-index]', function(event) {
			if (state.snapshot) self.trigger('select', {originalEvent: event, data: Utils.GetData(this, 'data')});
		});
		container.on('open', function() {
			// Page keys and Ctrl+Home/End traverse the complete list, including unrendered rows.
			container.dropdown.$dropdown.find('.select2-search__field').off('keydown.employeeWindow').on('keydown.employeeWindow', function(event) {
				var index = self.active;
				if (event.which === 33 || event.which === 34) index += event.which === 33 ? -6 : 6;
				else if (event.ctrlKey && (event.which === 36 || event.which === 35)) index = event.which === 36 ? 0 : self.choices.length-1;
				else return;
				event.preventDefault(); self.focusChoice(index, true);
			});
		});
	};
	$(e).off('.employeeSelector').on('change.employeeSelector', function() {
		++state.sequence;
		if (state.request) { state.request.abort(); state.request = null; }
		Array.prototype.slice.call(e.options).forEach(function(option) {
			if (!option.selected && (!state.snapshot || !state.snapshot.special || option.value !== state.snapshot.special.id)) $(option).remove();
		});
		if (box) { box.value = e.value; state.updateSuggestions(); }
	});
	return {dataAdapter: LocalAdapter, resultsAdapter: WindowResults,
		templateResult: function(item) { return $('<span>').text(item.text); }};
}

/** Install only a complete guarded response for this document, with no persistent cache. */
function employeeSelectorInstall(e, data) {
	var state = e._employeeLocal;
	if (!state) return;
	if (!data || data.complete !== true || !Array.isArray(data.results) || !data.expires || Date.now() >= data.expires*1000) {
		employeeSelectorInvalidate(e, 'Employee choices unavailable. Reload the page or use form search.'); return;
	}
	state.snapshot = data;
	e._employeeSelectorStatus.textContent = '';
	if (state.updateSuggestions) state.updateSuggestions();
	if (state.timer) clearTimeout(state.timer);
	state.timer = setTimeout(function() { employeeSelectorInvalidate(e, 'Employee choices expired. Reload the page or use form search.'); }, Math.max(1, data.expires*1000-Date.now()));
}

/** Drop all reusable choices on a learned change, expiry, failed refresh or page disposal. */
function employeeSelectorInvalidate(e, message) {
	var state = e._employeeLocal;
	if (!state) return;
	state.snapshot = null;
	if (state.datalist) state.datalist.textContent = '';
	if (state.timer) clearTimeout(state.timer);
	var instance = $(e).data('select2');
	if (instance) { instance.close(); instance.results.clear(); }
	if (e._employeeSelectorStatus) e._employeeSelectorStatus.textContent = message || '';
	employeeSelectorShowFallback(e);
}

/** Popup return obtains a complete fresh snapshot before changing the selected value. */
function employeeSelectorRefresh(e, value, onlyControl) {
	var state = e._employeeLocal, generation = state.generation;
	if (state.request) state.request.abort();
	var sequence = state.sequence = (state.sequence || 0)+1;
	// A popup can rename/change membership for every selector in this document.
	var others = [];
	if (!onlyControl) document.querySelectorAll('select[data-employee-search]').forEach(function(control) {
		if (control !== e && control._employeeLocal) others.push(control);
		employeeSelectorInvalidate(control, 'Refreshing employee choices…');
	});
	else employeeSelectorInvalidate(e, 'Refreshing employee choices…');
	var token = e.form && e.form.querySelector('input[name="_token"]');
	e._employeeSelectorStatus.textContent = 'Refreshing employees…';
	state.request = employeeSelectorRequest({url: e.getAttribute('data-employee-url'), type: 'POST', dataType: 'json',
		data: {control: e.getAttribute('data-employee-search'), snapshot: '1', _token: token ? token.value : ''}
	}).done(function(data) {
		if (!document.documentElement.contains(e) || e._employeeLocal !== state || state.generation !== generation || state.sequence !== sequence) return;
		state.request = null;
		employeeSelectorInstall(e, data);
		if (!state.snapshot) return;
		var item = data.special && data.special.id === value ? data.special
			: data.results.filter(function(row) { return row.id === value; })[0];
		if (!item) {
			if (!onlyControl) window.alert('Employee is unavailable for this control.');
			else e._employeeSelectorStatus.textContent = 'The previous employee is unavailable. Choose again.';
			return;
		}
		Array.prototype.slice.call(e.options).forEach(function(option) { if (option.value === item.id) $(option).remove(); });
		e.add(new Option(item.text, item.id, true, true));
		// Other controls keep their values without firing business submit-on-change handlers.
		$(e).trigger(onlyControl ? 'change.select2' : 'change');
	}).fail(function(xhr, reason) {
		if (reason !== 'abort' && e._employeeLocal === state && state.sequence === sequence && document.documentElement.contains(e))
			employeeSelectorInvalidate(e, 'Employee refresh failed or access expired. Reload the page or use form search.');
	});
	others.forEach(function(control) { employeeSelectorRefresh(control, control.value, true); });
	return state.request;
}

// Back/forward restoration must not resurrect a document's discarded choices.
window.addEventListener('pagehide', function() {
	document.querySelectorAll('select[data-employee-search]').forEach(function(e) {
		if (e._employeeLocal) {
			++e._employeeLocal.sequence;
			if (e._employeeLocal.request) e._employeeLocal.request.abort();
			employeeSelectorInvalidate(e, 'Reload the page to refresh employee choices.');
		}
	});
});

// Ajax replaces controls without navigating: release detached snapshots and timers promptly.
if (window.MutationObserver) new MutationObserver(function(changes) {
	changes.forEach(function(change) {
		Array.prototype.forEach.call(change.removedNodes, function(node) {
			if (node.nodeType !== 1) return;
			var controls = Array.prototype.slice.call(node.querySelectorAll('select[data-employee-search]'));
			if (node.matches('select[data-employee-search]')) controls.push(node);
			controls.forEach(function(e) {
				if (!e._employeeLocal || document.documentElement.contains(e)) return;
				++e._employeeLocal.sequence;
				if (e._employeeLocal.request) e._employeeLocal.request.abort();
				employeeSelectorInvalidate(e, '');
			});
		});
	});
}).observe(document.documentElement, {childList: true, subtree: true});
