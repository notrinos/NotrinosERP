/**
 * NotrinosERP Mobile Scanner Helper
 *
 * Shared utility functions for mobile warehouse pages.
 * Provides: AJAX calls, toast notifications, step management, scanner integration.
 *
 * Depends on: barcode_scanner.js (BarcodeScanner class)
 */
(function(window) {
	'use strict';

	var AJAX_URL = '';

	/**
	 * Initialize the mobile helper.
	 * @param {Object} options  { ajaxUrl: '...' }
	 */
	function MobileHelper(options) {
		AJAX_URL = options.ajaxUrl || 'mobile_ajax.php';
		this.scanner = null;
	}

	/**
	 * Send a POST request to the mobile AJAX endpoint.
	 *
	 * @param {string}   action   Action name (e.g. 'confirm_receive')
	 * @param {Object}   data     Key-value data to post
	 * @param {Function} callback function(response) — response is parsed JSON
	 */
	MobileHelper.prototype.post = function(action, data, callback) {
		var params = 'action=' + encodeURIComponent(action);

		// Include CSRF token required by session.inc check_csrf_token()
		var csrfEl = document.getElementById('global_csrf_token');
		if (csrfEl && csrfEl.value) {
			params += '&_token=' + encodeURIComponent(csrfEl.value);
		}

		for (var key in data) {
			if (data.hasOwnProperty(key)) {
				params += '&' + encodeURIComponent(key) + '=' + encodeURIComponent(data[key]);
			}
		}

		var xhr = new XMLHttpRequest();
		xhr.open('POST', AJAX_URL, true);
		xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
		xhr.onreadystatechange = function() {
			if (xhr.readyState === 4) {
				if (xhr.status === 200) {
					try {
						var resp = JSON.parse(xhr.responseText);
						callback(resp);
					} catch (e) {
						callback({ success: false, error: 'Invalid server response' });
					}
				} else {
					callback({ success: false, error: 'Server error: HTTP ' + xhr.status });
				}
			}
		};
		xhr.send(params);
	};

	/**
	 * Show a toast notification at bottom of screen.
	 *
	 * @param {string} message  Text to display
	 * @param {string} type     'success'|'error'|'info'
	 * @param {number} duration Milliseconds (default: 3000)
	 */
	MobileHelper.prototype.toast = function(message, type, duration) {
		type = type || 'info';
		duration = duration || 3000;

		// Remove existing toast
		var existing = document.querySelector('.mobile-toast');
		if (existing) existing.remove();

		var el = document.createElement('div');
		el.className = 'mobile-toast mobile-toast-' + type;
		el.textContent = message;
		document.body.appendChild(el);

		// Trigger reflow then show
		el.offsetHeight; // force reflow
		el.classList.add('show');

		setTimeout(function() {
			el.classList.remove('show');
			setTimeout(function() { el.remove(); }, 300);
		}, duration);
	};

	/**
	 * Set the active step in a step indicator.
	 *
	 * @param {number} stepNumber  1-based step number
	 * @param {number} totalSteps  Total number of steps
	 */
	MobileHelper.prototype.setStep = function(stepNumber, totalSteps) {
		var steps = document.querySelectorAll('.mobile-step');
		for (var i = 0; i < steps.length; i++) {
			steps[i].classList.remove('active', 'done');
			if (i < stepNumber - 1)
				steps[i].classList.add('done');
			else if (i === stepNumber - 1)
				steps[i].classList.add('active');
		}
	};

	/**
	 * Show a result panel.
	 *
	 * @param {string} containerId  ID of container element
	 * @param {string} message      Message text
	 * @param {string} type         'success'|'error'|'info'
	 */
	MobileHelper.prototype.showResult = function(containerId, message, type) {
		var container = document.getElementById(containerId);
		if (!container) return;
		container.innerHTML = '<div class="mobile-result mobile-result-' + type + '">' +
			this.escapeHtml(message) + '</div>';
	};

	/**
	 * Clear a result panel.
	 *
	 * @param {string} containerId  ID of container element
	 */
	MobileHelper.prototype.clearResult = function(containerId) {
		var container = document.getElementById(containerId);
		if (container) container.innerHTML = '';
	};

	/**
	 * Show loading spinner in a container.
	 *
	 * @param {string} containerId  ID of container element
	 */
	MobileHelper.prototype.showLoading = function(containerId) {
		var container = document.getElementById(containerId);
		if (container)
			container.innerHTML = '<div class="mobile-loading"><div class="mobile-spinner"></div></div>';
	};

	/**
	 * Initialize hardware barcode scanner integration.
	 *
	 * @param {Function} onScan  Callback when scan completes: function(code)
	 */
	MobileHelper.prototype.initScanner = function(onScan) {
		if (typeof BarcodeScanner === 'undefined') return;

		this.scanner = new BarcodeScanner({
			onScan: function(result) {
				if (onScan) onScan(result.code);
			},
			onError: function() {},
			autoLookup: false,
			minLength: 3,
			maxDelay: 80
		});
		this.scanner.enable();
	};

	/**
	 * Focus a scan input field and select its content.
	 *
	 * @param {string} inputId  ID of input element
	 */
	MobileHelper.prototype.focusScan = function(inputId) {
		var el = document.getElementById(inputId);
		if (el) {
			el.focus();
			el.select();
		}
	};

	/**
	 * Escape HTML entities.
	 *
	 * @param {string} str  Raw string
	 * @return {string}     Escaped string
	 */
	MobileHelper.prototype.escapeHtml = function(str) {
		var div = document.createElement('div');
		div.appendChild(document.createTextNode(str || ''));
		return div.innerHTML;
	};

	/**
	 * Format a card row HTML.
	 *
	 * @param {string} label  Label text
	 * @param {string} value  Value text
	 * @return {string}       HTML string
	 */
	MobileHelper.prototype.cardRow = function(label, value) {
		return '<div class="mobile-card-row">' +
			'<span class="mobile-card-label">' + this.escapeHtml(label) + '</span>' +
			'<span class="mobile-card-value">' + this.escapeHtml(value || '—') + '</span>' +
			'</div>';
	};

	/**
	 * Build a status badge HTML.
	 *
	 * @param {string} status  Status string
	 * @param {string} color   Badge color class suffix (green, blue, orange, red, gray)
	 * @return {string}        HTML string
	 */
	MobileHelper.prototype.badge = function(status, color) {
		color = color || 'gray';
		return '<span class="mobile-badge mobile-badge-' + color + '">' +
			this.escapeHtml(status) + '</span>';
	};

	/**
	 * Get color class for serial/batch status.
	 *
	 * @param {string} status  Status string
	 * @return {string}        Color class suffix
	 */
	MobileHelper.prototype.statusColor = function(status) {
		var map = {
			'available': 'green',
			'active': 'green',
			'reserved': 'blue',
			'delivered': 'teal',
			'in_transit': 'orange',
			'in_production': 'orange',
			'pending': 'orange',
			'returned': 'red',
			'defective': 'red',
			'scrapped': 'red',
			'recalled': 'red',
			'expired': 'red',
			'quarantine': 'orange',
			'consumed': 'gray',
			'inactive': 'gray'
		};
		return map[status] || 'gray';
	};

	// Export
	window.MobileHelper = MobileHelper;
})(window);
