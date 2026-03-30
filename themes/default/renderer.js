(function () {
	var sidebarCollapsedStorageKey = 'modernSidebarCollapsed';

	function isMobileLayout() {
		return window.matchMedia('(max-width: 1024px)').matches;
	}

	function getAppShell() {
		return document.getElementById('modern-app-shell');
	}

	function persistSidebarCollapsedState(appShell) {
		if (!appShell) {
			return;
		}
		if (appShell.classList.contains('modern-sidebar-collapsed')) {
			window.localStorage.setItem(sidebarCollapsedStorageKey, '1');
		} else {
			window.localStorage.removeItem(sidebarCollapsedStorageKey);
		}
	}

	function applyStoredSidebarCollapsedState() {
		var appShell = getAppShell();
		if (!appShell || isMobileLayout()) {
			return;
		}
		if (window.localStorage.getItem(sidebarCollapsedStorageKey) === '1') {
			appShell.classList.add('modern-sidebar-collapsed');
		} else {
			appShell.classList.remove('modern-sidebar-collapsed');
		}
	}

	function toggleSidebar() {
		var appShell = getAppShell();
		if (!appShell) {
			return;
		}

		if (isMobileLayout()) {
			appShell.classList.toggle('modern-sidebar-open');
			appShell.classList.remove('modern-sidebar-collapsed');
		} else {
			appShell.classList.toggle('modern-sidebar-collapsed');
			appShell.classList.remove('modern-sidebar-open');
			persistSidebarCollapsedState(appShell);
		}
	}

	function bindSidebarToggle() {
		var toggleButton = document.getElementById('modern-sidebar-toggle');
		if (toggleButton) {
			toggleButton.addEventListener('click', function () {
				toggleSidebar();
			});
		}

		var overlay = document.getElementById('modern-sidebar-overlay');
		if (overlay) {
			overlay.addEventListener('click', function () {
				var appShell = getAppShell();
				if (appShell) {
					appShell.classList.remove('modern-sidebar-open');
				}
			});
		}

		applyStoredSidebarCollapsedState();

		window.addEventListener('resize', function () {
			var appShell = getAppShell();
			if (!appShell) {
				return;
			}

			if (isMobileLayout()) {
				appShell.classList.remove('modern-sidebar-collapsed');
				appShell.classList.remove('modern-sidebar-open');
			} else {
				appShell.classList.remove('modern-sidebar-open');
				applyStoredSidebarCollapsedState();
			}
		});

		var appLinks = document.querySelectorAll('.modern-app-link');
		for (var i = 0; i < appLinks.length; i++) {
			appLinks[i].addEventListener('click', function () {
				var appShell = getAppShell();
				if (!appShell || isMobileLayout()) {
					return;
				}

				if (appShell.classList.contains('modern-sidebar-collapsed')) {
					appShell.classList.remove('modern-sidebar-collapsed');
					persistSidebarCollapsedState(appShell);
				}
			});
		}
	}

	function bindModuleGroups() {
		var groupToggles = document.querySelectorAll('.modern-module-toggle');
		for (var i = 0; i < groupToggles.length; i++) {
			groupToggles[i].addEventListener('click', function () {
				var parentNode = this.parentNode;
				if (parentNode) {
					parentNode.classList.toggle('is-expanded');
				}
			});
		}
	}

	function normalizePath(pathname) {
		return (pathname || '').replace(/\/+$/, '').toLowerCase();
	}

	function canonicalizeLocation(pathname, search) {
		var normalizedPath = normalizePath(pathname);
		var params = new URLSearchParams(search || '');
		params.delete('sel_app');

		var sortedKeys = [];
		params.forEach(function (_, key) {
			sortedKeys.push(key);
		});
		sortedKeys.sort();

		var canonicalPairs = [];
		for (var i = 0; i < sortedKeys.length; i++) {
			var key = sortedKeys[i];
			var values = params.getAll(key);
			for (var j = 0; j < values.length; j++) {
				canonicalPairs.push(encodeURIComponent(key) + '=' + encodeURIComponent(values[j]));
			}
		}

		return canonicalPairs.length ? (normalizedPath + '?' + canonicalPairs.join('&')) : normalizedPath;
	}

	function bindSidebarModuleActiveLinks() {
		var moduleLinks = document.querySelectorAll('.modern-module-link a');
		if (!moduleLinks.length) {
			return;
		}

		var currentCanonicalUrl = canonicalizeLocation(window.location.pathname, window.location.search);
		var pathMatches = [];
		var exactMatches = [];

		for (var i = 0; i < moduleLinks.length; i++) {
			var linkCanonicalUrl = canonicalizeLocation(moduleLinks[i].pathname, moduleLinks[i].search);
			if (linkCanonicalUrl && linkCanonicalUrl === currentCanonicalUrl) {
				exactMatches.push(moduleLinks[i]);
			} else if (normalizePath(moduleLinks[i].pathname) === normalizePath(window.location.pathname)) {
				pathMatches.push(moduleLinks[i]);
			}

			moduleLinks[i].addEventListener('click', function () {
				for (var j = 0; j < moduleLinks.length; j++) {
					moduleLinks[j].classList.remove('is-current');
				}
				this.classList.add('is-current');
			});
		}

		var activeLinks = exactMatches.length ? exactMatches : pathMatches.slice(0, 1);
		for (var k = 0; k < activeLinks.length; k++) {
			activeLinks[k].classList.add('is-current');
		}
	}

	function bindUserDropdown() {
		var trigger = document.getElementById('modern-user-trigger');
		var menu = document.getElementById('modern-user-menu');
		if (!trigger || !menu) {
			return;
		}

		trigger.addEventListener('click', function (e) {
			e.stopPropagation();
			var isOpen = menu.classList.toggle('is-open');
			trigger.setAttribute('aria-expanded', isOpen ? 'true' : 'false');

			// Close notification panel when user dropdown opens
			if (isOpen) {
				var notifPanel = document.getElementById('modern-notification-panel');
				var notifTrigger = document.getElementById('modern-notification-trigger');
				if (notifPanel) {
					notifPanel.classList.remove('is-open');
				}
				if (notifTrigger) {
					notifTrigger.setAttribute('aria-expanded', 'false');
				}
			}
		});

		document.addEventListener('click', function (e) {
			if (!trigger.contains(e.target) && !menu.contains(e.target)) {
				menu.classList.remove('is-open');
				trigger.setAttribute('aria-expanded', 'false');
			}
		});
	}

	function bindClickableCheckboxRows() {
		var checkboxRows = document.querySelectorAll('.form-section .form-group, .form-table:not(.filter-bar) .form-check-row');
		for (var i = 0; i < checkboxRows.length; i++) {
			(function (row) {
				var checkbox = row.querySelector("input[type='checkbox']");
				if (!checkbox || row.getAttribute('data-checkbox-row-bound') === '1') {
					return;
				}

				row.setAttribute('data-checkbox-row-bound', '1');

				row.addEventListener('click', function (e) {
					var interactiveTarget = e.target.closest("a, button, select, textarea, input:not([type='checkbox']), .select2-container, .search_btn_container");
					if (interactiveTarget) {
						return;
					}

					if (e.target === checkbox) {
						return;
					}

					checkbox.checked = !checkbox.checked;
					checkbox.dispatchEvent(new Event('change', { bubbles: true }));
					checkbox.dispatchEvent(new Event('input', { bubbles: true }));
				});

				row.addEventListener('keydown', function (e) {
					if (e.target !== row) {
						return;
					}

					if (e.key === ' ' || e.key === 'Enter') {
						e.preventDefault();
						checkbox.checked = !checkbox.checked;
						checkbox.dispatchEvent(new Event('change', { bubbles: true }));
						checkbox.dispatchEvent(new Event('input', { bubbles: true }));
					}
				});

				if (!row.hasAttribute('tabindex')) {
					row.setAttribute('tabindex', '0');
				}
			})(checkboxRows[i]);
		}
	}

	function bindSearchToggle() {
		var appShell = getAppShell();
		var searchToggle = document.getElementById('modern-search-toggle');
		var searchInput = document.querySelector('.modern-header-search-input');
		if (!appShell || !searchToggle || !searchInput) {
			return;
		}

		searchToggle.addEventListener('click', function (e) {
			e.stopPropagation();
			var isOpen = appShell.classList.toggle('modern-search-open');
			if (isOpen) {
				setTimeout(function () {
					searchInput.focus();
				}, 10);
			}
		});

		document.addEventListener('click', function (e) {
			if (!appShell.classList.contains('modern-search-open')) {
				return;
			}

			var searchCenter = document.querySelector('.modern-topbar-center');
			if (searchCenter && !searchCenter.contains(e.target) && e.target !== searchToggle && !searchToggle.contains(e.target)) {
				appShell.classList.remove('modern-search-open');
			}
		});

		window.addEventListener('resize', function () {
			if (!window.matchMedia('(max-width: 768px)').matches) {
				appShell.classList.remove('modern-search-open');
			}
		});
	}

	/**
	 * Fast client-side menu search using embedded JSON index.
	 *
	 * Searches the pre-built window.__searchIndex array, renders a dropdown
	 * with highlighted results, and supports full keyboard navigation.
	 */
	function bindMenuSearch() {
		var searchInput = document.querySelector('.modern-header-search-input');
		var resultsContainer = document.getElementById('modern-search-results');
		var searchIndex = window.__searchIndex;
		var searchShortcut = document.querySelector('.modern-header-search-shortcut');
		if (!searchInput || !resultsContainer || !searchIndex) {
			return;
		}

		var activeIndex = -1;
		var currentResults = [];
		var maxResults = 12;
		var debounceTimer = null;
		var searchBar = document.querySelector('.modern-header-search');

		// Pre-compute lowercase labels and keywords for faster matching
		var indexLength = searchIndex.length;
		for (var i = 0; i < indexLength; i++) {
			searchIndex[i]._lower = (searchIndex[i].l + ' ' + searchIndex[i].a + ' ' + searchIndex[i].m).toLowerCase();
		}

		function positionResults() {
			if (!searchBar) return;
			var rect = searchBar.getBoundingClientRect();
			resultsContainer.style.top = Math.round(rect.bottom + 4) + 'px';
			resultsContainer.style.left = Math.round(rect.left) + 'px';
			resultsContainer.style.width = Math.round(rect.width) + 'px';
		}

		function escapeHtml(text) {
			var div = document.createElement('div');
			div.appendChild(document.createTextNode(text));
			return div.innerHTML;
		}

		function highlightMatch(text, query) {
			if (!query) return escapeHtml(text);
			var lowerText = text.toLowerCase();
			var lowerQuery = query.toLowerCase();
			var startIndex = lowerText.indexOf(lowerQuery);
			if (startIndex === -1) return escapeHtml(text);
			var before = text.substring(0, startIndex);
			var match = text.substring(startIndex, startIndex + query.length);
			var after = text.substring(startIndex + query.length);
			return escapeHtml(before) + '<mark>' + escapeHtml(match) + '</mark>' + escapeHtml(after);
		}

		function performSearch(query) {
			if (!query || query.length < 1) {
				hideResults();
				return;
			}

			var lowerQuery = query.toLowerCase();
			var queryWords = lowerQuery.split(/\s+/);
			var scored = [];

			for (var i = 0; i < indexLength; i++) {
				var item = searchIndex[i];
				var haystack = item._lower;
				var allMatch = true;
				var score = 0;

				for (var w = 0; w < queryWords.length; w++) {
					var wordIndex = haystack.indexOf(queryWords[w]);
					if (wordIndex === -1) {
						allMatch = false;
						break;
					}
					// Bonus for matching at word start
					if (wordIndex === 0 || haystack.charAt(wordIndex - 1) === ' ') {
						score += 10;
					}
					score += 1;
				}

				if (!allMatch) continue;

				// Bonus for label-only matches (more relevant)
				var labelLower = item.l.toLowerCase();
				if (labelLower.indexOf(lowerQuery) !== -1) {
					score += 20;
					// Starts-with bonus
					if (labelLower.indexOf(lowerQuery) === 0) {
						score += 15;
					}
				}

				scored.push({ item: item, score: score });
			}

			// Sort by score descending
			scored.sort(function (a, b) { return b.score - a.score; });

			currentResults = [];
			var limit = Math.min(scored.length, maxResults);
			for (var j = 0; j < limit; j++) {
				currentResults.push(scored[j].item);
			}

			renderResults(query);
		}

		function renderResults(query) {
			if (currentResults.length === 0) {
				resultsContainer.innerHTML = '<div class="modern-search-empty">No results found</div>';
				positionResults();
				resultsContainer.style.display = 'block';
				activeIndex = -1;
				return;
			}

			var html = '';
			for (var i = 0; i < currentResults.length; i++) {
				var item = currentResults[i];
				html += '<a class="modern-search-item' + (i === activeIndex ? ' is-active' : '') + '" href="' + escapeHtml(item.u) + '" data-index="' + i + '">';
				html += '<div class="modern-search-item-label">' + highlightMatch(item.l, query) + '</div>';
				html += '<div class="modern-search-item-meta">' + escapeHtml(item.a) + ' &rsaquo; ' + escapeHtml(item.m) + '</div>';
				html += '</a>';
			}

			resultsContainer.innerHTML = html;
			positionResults();
			resultsContainer.style.display = 'block';
			activeIndex = -1;
		}

		function hideResults() {
			resultsContainer.style.display = 'none';
			resultsContainer.innerHTML = '';
			currentResults = [];
			activeIndex = -1;
		}

		function setActiveItem(newIndex) {
			var items = resultsContainer.querySelectorAll('.modern-search-item');
			if (items.length === 0) return;

			// Remove current active
			if (activeIndex >= 0 && activeIndex < items.length) {
				items[activeIndex].classList.remove('is-active');
			}

			activeIndex = newIndex;
			if (activeIndex < 0) activeIndex = items.length - 1;
			if (activeIndex >= items.length) activeIndex = 0;

			items[activeIndex].classList.add('is-active');
			items[activeIndex].scrollIntoView({ block: 'nearest' });
		}

		function navigateToResult(index) {
			if (index >= 0 && index < currentResults.length) {
				window.location.href = currentResults[index].u;
			}
		}

		function openSearchInput() {
			var appShell = getAppShell();
			if (appShell && window.matchMedia('(max-width: 768px)').matches) {
				appShell.classList.add('modern-search-open');
			}

			searchInput.focus();
			searchInput.select();
		}

		function isEditableTarget(target) {
			if (!target || !target.tagName) {
				return false;
			}

			var tagName = target.tagName.toLowerCase();
			return tagName === 'input' || tagName === 'textarea' || tagName === 'select' || target.isContentEditable;
		}

		// Debounced search on input
		searchInput.addEventListener('input', function () {
			var query = this.value.trim();
			if (searchShortcut) {
				searchShortcut.style.visibility = query ? 'hidden' : 'visible';
			}
			if (debounceTimer) clearTimeout(debounceTimer);
			debounceTimer = setTimeout(function () {
				performSearch(query);
			}, 60);
		});

		// Keyboard navigation
		searchInput.addEventListener('keydown', function (e) {
			if (resultsContainer.style.display === 'none') return;

			if (e.keyCode === 40) { // Arrow Down
				e.preventDefault();
				setActiveItem(activeIndex + 1);
			} else if (e.keyCode === 38) { // Arrow Up
				e.preventDefault();
				setActiveItem(activeIndex - 1);
			} else if (e.keyCode === 13) { // Enter
				e.preventDefault();
				if (activeIndex >= 0) {
					navigateToResult(activeIndex);
				} else if (currentResults.length > 0) {
					navigateToResult(0);
				}
			} else if (e.keyCode === 27) { // Escape
				e.preventDefault();
				hideResults();
				searchInput.value = '';
				if (searchShortcut) {
					searchShortcut.style.visibility = 'visible';
				}
				searchInput.blur();
			}
		});

		// Hover to highlight
		resultsContainer.addEventListener('mouseover', function (e) {
			var target = e.target.closest('.modern-search-item');
			if (target) {
				var idx = parseInt(target.getAttribute('data-index'), 10);
				if (!isNaN(idx)) setActiveItem(idx);
			}
		});

		// Close when clicking outside
		document.addEventListener('click', function (e) {
			var searchArea = document.querySelector('.modern-topbar-center');
			if (searchArea && !searchArea.contains(e.target) && !resultsContainer.contains(e.target)) {
				hideResults();
			}
		});

		// Ctrl+K / Cmd+K shortcut to focus search
		document.addEventListener('keydown', function (e) {
			if (e.key === '/' && !e.ctrlKey && !e.metaKey && !e.altKey && !isEditableTarget(e.target)) {
				e.preventDefault();
				openSearchInput();
				return;
			}

			if ((e.ctrlKey || e.metaKey) && e.keyCode === 75) {
				e.preventDefault();
				openSearchInput();
			}
		});

		if (searchShortcut) {
			searchShortcut.setAttribute('title', 'Press / or Ctrl/Cmd+K to focus search');
		}
	}

	function bindCollapsedSidebarTooltips() {
		var appShell = getAppShell();
		if (!appShell) {
			return;
		}

		var tooltipElement = document.createElement('div');
		tooltipElement.id = 'modern-sidebar-tooltip';
		tooltipElement.style.position = 'fixed';
		tooltipElement.style.zIndex = '4000';
		tooltipElement.style.pointerEvents = 'none';
		tooltipElement.style.padding = '6px 10px';
		tooltipElement.style.borderRadius = '6px';
		tooltipElement.style.background = '#0f172a';
		tooltipElement.style.color = '#ffffff';
		tooltipElement.style.fontSize = '11px';
		tooltipElement.style.fontWeight = '600';
		tooltipElement.style.whiteSpace = 'nowrap';
		tooltipElement.style.opacity = '0';
		tooltipElement.style.transition = 'opacity 0.12s ease';
		document.body.appendChild(tooltipElement);

		function hideTooltip() {
			tooltipElement.style.opacity = '0';
		}

		function showTooltip(linkElement) {
			if (!appShell.classList.contains('modern-sidebar-collapsed') || isMobileLayout()) {
				hideTooltip();
				return;
			}

			var text = linkElement.getAttribute('data-tooltip');
			if (!text) {
				hideTooltip();
				return;
			}

			tooltipElement.textContent = text;
			var rect = linkElement.getBoundingClientRect();
			tooltipElement.style.left = Math.round(rect.right + 10) + 'px';
			tooltipElement.style.top = Math.round(rect.top + rect.height / 2) + 'px';
			tooltipElement.style.transform = 'translateY(-50%)';
			tooltipElement.style.opacity = '1';
		}

		var appLinks = document.querySelectorAll('.modern-app-link');
		for (var i = 0; i < appLinks.length; i++) {
			appLinks[i].addEventListener('mouseenter', function () {
				showTooltip(this);
			});
			appLinks[i].addEventListener('mouseleave', function () {
				hideTooltip();
			});
		}

		window.addEventListener('scroll', hideTooltip, true);
		window.addEventListener('resize', hideTooltip);
	}

	function bindNotificationDropdown() {
		var trigger = document.getElementById('modern-notification-trigger');
		var panel = document.getElementById('modern-notification-panel');
		if (!trigger || !panel) {
			return;
		}

		trigger.addEventListener('click', function (e) {
			e.stopPropagation();
			var isOpen = panel.classList.toggle('is-open');
			trigger.setAttribute('aria-expanded', isOpen ? 'true' : 'false');

			// Close user dropdown when notification panel opens
			if (isOpen) {
				var userMenu = document.getElementById('modern-user-menu');
				var userTrigger = document.getElementById('modern-user-trigger');
				if (userMenu) {
					userMenu.classList.remove('is-open');
				}
				if (userTrigger) {
					userTrigger.setAttribute('aria-expanded', 'false');
				}
			}
		});

		document.addEventListener('click', function (e) {
			if (!trigger.contains(e.target) && !panel.contains(e.target)) {
				panel.classList.remove('is-open');
				trigger.setAttribute('aria-expanded', 'false');
			}
		});
	}

	if (document.readyState === 'loading') {
		document.addEventListener('DOMContentLoaded', function () {
			bindSidebarToggle();
			bindModuleGroups();
			bindSidebarModuleActiveLinks();
			bindUserDropdown();
			bindNotificationDropdown();
			bindClickableCheckboxRows();
			bindCollapsedSidebarTooltips();
			bindSearchToggle();
			bindMenuSearch();
		});
	} else {
		bindSidebarToggle();
		bindModuleGroups();
		bindSidebarModuleActiveLinks();
		bindUserDropdown();
		bindNotificationDropdown();
			bindClickableCheckboxRows();
		bindCollapsedSidebarTooltips();
		bindSearchToggle();
		bindMenuSearch();
	}
})();
