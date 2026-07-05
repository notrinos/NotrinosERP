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
		var clearButton = document.querySelector('.modern-header-search-clear');
		if (!searchInput || !resultsContainer || !searchIndex) {
			return;
		}

		var activeIndex = -1;
		var currentResults = [];
		var maxResults = 12;
		var debounceTimer = null;
		var searchBar = document.querySelector('.modern-header-search');

		function updateSearchChrome(query) {
			var hasQuery = !!query;
			if (searchShortcut) {
				searchShortcut.hidden = hasQuery;
				searchShortcut.style.visibility = hasQuery ? 'hidden' : 'visible';
			}
			if (clearButton) {
				clearButton.hidden = !hasQuery;
			}
		}

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
			updateSearchChrome(query);
			if (debounceTimer) clearTimeout(debounceTimer);
			debounceTimer = setTimeout(function () {
				performSearch(query);
			}, 60);
		});

		if (clearButton) {
			clearButton.addEventListener('click', function () {
				searchInput.value = '';
				updateSearchChrome('');
				hideResults();
				searchInput.focus();
			});
		}

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
				updateSearchChrome('');
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

		updateSearchChrome(searchInput.value.trim());
	}

	function bindCollapsedSidebarDropmenus() {
		var appShell = getAppShell();
		if (!appShell) {
			return;
		}

		var activeDropmenu = null;
		var activeAppLink = null;
		var hideTimer = null;

		function showDropmenu(appLink, appId) {
			if (!appShell.classList.contains('modern-sidebar-collapsed') || isMobileLayout()) {
				return;
			}

			// Hide any currently open dropmenu
			if (activeDropmenu && activeDropmenu !== null) {
				activeDropmenu.classList.remove('is-open');
			}

			// Clear hover from previously active link
			if (activeAppLink && activeAppLink !== appLink) {
				activeAppLink.classList.remove('is-hover');
			}

			var dropmenu = document.getElementById('modern-dropmenu-' + appId);
			if (!dropmenu) {
				return;
			}

			// Position dropmenu right of the sidebar, aligned with the hovered icon minus one link height
			var sidebarRect = document.getElementById('modern-sidebar').getBoundingClientRect();
			var linkRect = appLink.getBoundingClientRect();
			var dropTop = Math.round(linkRect.top) - 30;
			// Clamp top so the menu never goes above the header
			if (dropTop < 56) {
				dropTop = 56;
			}
			dropmenu.style.position = 'fixed';
			dropmenu.style.top = dropTop + 'px';
			dropmenu.style.left = Math.round(sidebarRect.right) + 'px';
			dropmenu.style.maxHeight = (window.innerHeight - dropTop) + 'px';

			dropmenu.classList.add('is-open');
			activeDropmenu = dropmenu;
			activeAppLink = appLink;

			if (appLink) {
				appLink.classList.add('is-hover');
			}
		}

		function hideDropmenu() {
			if (activeDropmenu) {
				activeDropmenu.classList.remove('is-open');
				activeDropmenu = null;
			}
			if (activeAppLink) {
				activeAppLink.classList.remove('is-hover');
				activeAppLink = null;
			}
		}

		// Attach hover listeners to each app link
		var appLinks = document.querySelectorAll('.modern-app-link');
		for (var i = 0; i < appLinks.length; i++) {
			(function (link) {
				var appId = link.getAttribute('data-app-id');
				if (!appId) {
					return;
				}

				link.addEventListener('mouseenter', function () {
					if (hideTimer) {
						clearTimeout(hideTimer);
						hideTimer = null;
					}
					showDropmenu(link, appId);
				});

				link.addEventListener('mouseleave', function () {
					// Short delay so user can move mouse to the dropmenu
					hideTimer = setTimeout(function () {
						hideDropmenu();
					}, 150);
				});
			})(appLinks[i]);
		}

		// Keep dropmenu open when hovering over it
		var dropmenuContainer = document.getElementById('modern-dropmenu-container');
		if (dropmenuContainer) {
			dropmenuContainer.addEventListener('mouseenter', function () {
				if (hideTimer) {
					clearTimeout(hideTimer);
					hideTimer = null;
				}
			});

			dropmenuContainer.addEventListener('mouseleave', function () {
				hideTimer = setTimeout(function () {
					hideDropmenu();
				}, 150);
			});
		}

		// Hide on sidebar expand
		var observer = new MutationObserver(function () {
			if (!appShell.classList.contains('modern-sidebar-collapsed')) {
				hideDropmenu();
			}
		});
		observer.observe(appShell, { attributes: true, attributeFilter: ['class'] });
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

	/**
	 * Scroll to top of the main content area when messages (errors, warnings,
	 * notifications) are present on page load.  Also observe the msgbox for
	 * dynamic AJAX-injected messages and scroll then too.
	 */
	function scrollToMessagesOnLoad() {
		var msgbox = document.getElementById('msgbox');
		if (!msgbox) return;

		var mainContent = document.getElementById('modern-main-content');
		var scrollTarget = mainContent || window;

		function doScroll() {
			if (mainContent) {
				mainContent.scrollTop = 0;
			}
			window.scrollTo(0, 0);
		}

		// Scroll on initial page load if msgbox has content
		if (msgbox.innerHTML && msgbox.innerHTML.trim().length > 0) {
			doScroll();
		}

		// Observe dynamic changes (AJAX updates) to msgbox
		if (typeof MutationObserver !== 'undefined') {
			var observer = new MutationObserver(function () {
				if (msgbox.innerHTML && msgbox.innerHTML.trim().length > 0) {
					doScroll();
				}
			});
			observer.observe(msgbox, { childList: true, subtree: true, characterData: true });
		}
	}

	if (document.readyState === 'loading') {
		document.addEventListener('DOMContentLoaded', function () {
			bindSidebarToggle();
			bindModuleGroups();
			bindSidebarModuleActiveLinks();
			bindUserDropdown();
			bindNotificationDropdown();
			bindClickableCheckboxRows();
			bindCollapsedSidebarDropmenus();
			bindSearchToggle();
			bindMenuSearch();
			scrollToMessagesOnLoad();
			bindDataTableScrollHints();
		});
	} else {
		bindSidebarToggle();
		bindModuleGroups();
		bindSidebarModuleActiveLinks();
		bindUserDropdown();
		bindNotificationDropdown();
			bindClickableCheckboxRows();
		bindCollapsedSidebarDropmenus();
		bindSearchToggle();
		bindMenuSearch();
		scrollToMessagesOnLoad();
		bindDataTableScrollHints();
	}

	/**
	 * Initialize custom JS scrollbars for all erp-data-table wrappers.
	 *
	 * Replaces native scrollbar with a slim custom track visible only
	 * during scrolling, positioned outside the table on the right.
	 *
	 * Idempotent — wrappers already marked data-scrollbar-bound="1" are
	 * skipped.  Also cleans up orphaned tracks whose wrapper was removed
	 * from the DOM (e.g. by an AJAX refresh).
	 */
	function bindDataTableScrollHints() {
		// ── Clean up orphaned tracks whose wrapper is gone ──
		var orphanTracks = document.querySelectorAll('.erp-custom-scrollbar-track');
		for (var t = 0; t < orphanTracks.length; t++) {
			var prev = orphanTracks[t].previousElementSibling;
			if (!prev || !prev.classList.contains('erp-data-table') || prev.getAttribute('data-scrollbar-bound') !== '1') {
				orphanTracks[t].parentNode.removeChild(orphanTracks[t]);
			}
		}

		var tables = document.querySelectorAll('.erp-data-table:not([data-scrollbar-bound])');
		for (var i = 0; i < tables.length; i++) {
			(function (table) {
				if (table.scrollHeight <= table.clientHeight + 2) {
					return;
				}

				var hintScrolled = false;
				var scrollHideTimer = null;

				// ── Scroll hint overlay ──
				var hint = document.createElement('div');
				hint.className = 'erp-data-table-hint';
				hint.innerHTML = '<span class="erp-data-table-hint-text">&#8595; Scroll down &#8595;</span>';
				table.appendChild(hint);

				table.addEventListener('mouseenter', function () {
					if (!hintScrolled) hint.classList.add('is-visible');
				});
				table.addEventListener('mouseleave', function () {
					hint.classList.remove('is-visible');
				});

				// ── Custom scrollbar (positioned outside wrapper) ──
			var wrapper = table.closest('.erp-data-table');
			if (!wrapper) return;
			wrapper.setAttribute('data-scrollbar-bound', '1');

			var track = document.createElement('div');
			track.className = 'erp-custom-scrollbar-track';

			var thumb = document.createElement('div');
			thumb.className = 'erp-custom-scrollbar-thumb';
			track.appendChild(thumb);

			// Insert track as a sibling after the wrapper, not inside it
			wrapper.parentNode.insertBefore(track, wrapper.nextSibling);

			function updateThumb() {
				if (!wrapper) return;
				var viewH = wrapper.clientHeight;
				var totalH = wrapper.scrollHeight;
				if (totalH <= viewH) return;

				var ratio = viewH / totalH;
				var thumbHeight = Math.max(ratio * viewH, 24);
				var maxScroll = totalH - viewH;
				var scrollRatio = wrapper.scrollTop / maxScroll;
				var trackSpace = viewH - thumbHeight;

				var wrapperRect = wrapper.getBoundingClientRect();
				track.style.height = viewH + 'px';
				track.style.top = wrapperRect.top + 'px';
				track.style.left = (wrapperRect.right - 6) + 'px';

				thumb.style.height = thumbHeight + 'px';
				thumb.style.top = Math.round(scrollRatio * trackSpace) + 'px';
			}

			function showScrollbar() {
				track.classList.add('is-visible');
				updateThumb();
				if (scrollHideTimer) clearTimeout(scrollHideTimer);
				scrollHideTimer = setTimeout(function () {
					track.classList.remove('is-visible');
				}, 1200);
			}

			var onScroll = function () {
				showScrollbar();
				updateThumb();
				if (!hintScrolled) {
					hintScrolled = true;
					hint.classList.remove('is-visible');
				}
			};

			wrapper.addEventListener('scroll', onScroll);

			updateThumb();
			window.addEventListener('scroll', updateThumb);
			window.addEventListener('resize', updateThumb);
			})(tables[i]);
		}
	}

	/**
	 * MutationObserver that re-runs bindDataTableScrollHints whenever
	 * new .erp-data-table wrappers are inserted into the DOM (e.g. by
	 * AJAX page updates that replace table content).
	 */
	function observeDataTableScrollbars() {
		if (typeof MutationObserver === 'undefined') return;

		var observer = new MutationObserver(function (mutations) {
			var needsRefresh = false;
			for (var i = 0; i < mutations.length; i++) {
				var addedNodes = mutations[i].addedNodes;
				for (var j = 0; j < addedNodes.length; j++) {
					if (addedNodes[j].nodeType !== 1) continue;
					// Check the node itself or any descendant
					if (addedNodes[j].classList && addedNodes[j].classList.contains('erp-data-table')) {
						needsRefresh = true;
						break;
					}
					if (addedNodes[j].querySelectorAll) {
						var nested = addedNodes[j].querySelectorAll('.erp-data-table');
						if (nested.length > 0) {
							needsRefresh = true;
							break;
						}
					}
				}
				if (needsRefresh) break;
			}
			if (needsRefresh) bindDataTableScrollHints();
		});

		observer.observe(document.body, { childList: true, subtree: true });
	}

	// Start watching for DOM changes to re-bind scrollbars
	observeDataTableScrollbars();

	/**
	* Observe DOM mutations and automatically bind click handlers for
	* checkbox rows inserted after the initial page load.
	*
	* Many ERP screens replace portions of the form via AJAX, creating new
	* `.form-group` / `.form-check-row` elements after
	* `bindClickableCheckboxRows()` has already executed. Without additional
	* initialization, these dynamically inserted rows would not receive the
	* click-to-toggle behavior.
	*
	* This observer watches for newly added DOM nodes and re-runs
	* `bindClickableCheckboxRows()`. The binding function is idempotent and
	* skips rows already marked with `data-checkbox-row-bound="1"`, so only
	* newly inserted checkbox rows are initialized.
	*
	* This approach provides AJAX compatibility while preserving the existing
	* event-binding implementation. It serves as an intermediate solution
	* until the renderer is fully refactored to use delegated event handling,
	* at which point this observer can be removed.
	*/
	function observeCheckboxRows() {
		if (typeof MutationObserver === 'undefined') {
			return;
		}

		var observer = new MutationObserver(function (mutations) {
			var needsRefresh = false;

			for (var i = 0; i < mutations.length; i++) {
				var addedNodes = mutations[i].addedNodes;

				for (var j = 0; j < addedNodes.length; j++) {
					if (addedNodes[j].nodeType !== 1) {
						continue;
					}

					if (
						(addedNodes[j].classList &&
							(addedNodes[j].classList.contains('form-group') ||
							addedNodes[j].classList.contains('form-check-row'))) ||
						(addedNodes[j].querySelector &&
							addedNodes[j].querySelector('.form-group, .form-check-row'))
					) {
						needsRefresh = true;
						break;
					}
				}

				if (needsRefresh) {
					break;
				}
			}

			if (needsRefresh) {
				bindClickableCheckboxRows();
			}
		});

		observer.observe(document.body, {
			childList: true,
			subtree: true
		});
	}

	// Start watching for AJAX-inserted checkbox rows.
	observeCheckboxRows();
})();
