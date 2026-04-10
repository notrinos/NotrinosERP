/**
 * NotrinosERP Barcode Scanner Library
 *
 * Provides hardware barcode scanner detection (keyboard wedge mode),
 * camera-based scanning fallback, and auto-parsing of scanned data.
 *
 * Usage:
 *   var scanner = new BarcodeScanner({
 *       onScan: function(result) { console.log(result); },
 *       ajaxUrl: 'inventory/includes/barcode_ajax.inc',
 *       minLength: 4,
 *       maxDelay: 50
 *   });
 *   scanner.enable();
 *
 * Hardware scanner detection:
 *   Most barcode scanners operate in "keyboard wedge" mode, sending
 *   scanned characters as rapid keystrokes followed by Enter.
 *   This library detects rapid keystroke sequences (< maxDelay ms apart)
 *   and captures them as scans instead of normal typing.
 *
 * Copyright (C) NotrinosERP. GPL v3.
 */
(function(window) {
	'use strict';

	/**
	 * BarcodeScanner constructor.
	 *
	 * @param {Object} options Configuration options
	 * @param {Function} options.onScan      Callback when scan completes: function({code, type, matches})
	 * @param {Function} options.onError     Callback on error: function(errorMessage)
	 * @param {string}   options.ajaxUrl     URL for barcode lookup AJAX endpoint
	 * @param {number}   options.minLength   Minimum characters for a valid scan (default: 4)
	 * @param {number}   options.maxDelay    Maximum ms between keystrokes for scanner detection (default: 50)
	 * @param {string}   options.endChar     Character code that ends a scan (default: 'Enter')
	 * @param {string}   options.targetInput CSS selector for input field to populate with scan result
	 * @param {boolean}  options.autoLookup  Whether to auto-lookup scanned data via AJAX (default: true)
	 * @param {boolean}  options.preventDefault Whether to prevent default key events during scan capture (default: true)
	 */
	function BarcodeScanner(options) {
		this.options = {
			onScan: options.onScan || function() {},
			onError: options.onError || function() {},
			ajaxUrl: options.ajaxUrl || '',
			minLength: options.minLength || 4,
			maxDelay: options.maxDelay || 50,
			endChar: options.endChar || 'Enter',
			targetInput: options.targetInput || null,
			autoLookup: options.autoLookup !== undefined ? options.autoLookup : true,
			preventDefault: options.preventDefault !== undefined ? options.preventDefault : true
		};

		this._buffer = '';
		this._lastKeyTime = 0;
		this._enabled = false;
		this._scanning = false;
		this._keyHandler = null;

		// Bind event handler
		var self = this;
		this._keyHandler = function(e) {
			self._handleKeyPress(e);
		};
	}

	/**
	 * Enable scanner detection (start listening for keyboard events).
	 */
	BarcodeScanner.prototype.enable = function() {
		if (this._enabled) return;
		this._enabled = true;
		document.addEventListener('keydown', this._keyHandler, true);
	};

	/**
	 * Disable scanner detection (stop listening for keyboard events).
	 */
	BarcodeScanner.prototype.disable = function() {
		if (!this._enabled) return;
		this._enabled = false;
		document.removeEventListener('keydown', this._keyHandler, true);
		this._resetBuffer();
	};

	/**
	 * Check if scanner detection is currently enabled.
	 * @returns {boolean}
	 */
	BarcodeScanner.prototype.isEnabled = function() {
		return this._enabled;
	};

	/**
	 * Manually process a barcode string (e.g., from a paste event or manual entry).
	 *
	 * @param {string} code Barcode data string
	 */
	BarcodeScanner.prototype.processCode = function(code) {
		code = (code || '').trim();
		if (code.length < this.options.minLength) {
			this.options.onError('Code too short: ' + code);
			return;
		}
		this._handleScan(code);
	};

	/**
	 * Handle a key press event for scanner detection.
	 * @private
	 * @param {KeyboardEvent} e
	 */
	BarcodeScanner.prototype._handleKeyPress = function(e) {
		var now = Date.now();
		var activeEl = document.activeElement;
		var tagName = activeEl ? activeEl.tagName.toLowerCase() : '';

		// Don't capture if user is typing in a textarea
		if (tagName === 'textarea') {
			return;
		}

		// Check for end character (Enter)
		if (e.key === this.options.endChar) {
			if (this._buffer.length >= this.options.minLength) {
				// This is a scan!
				if (this.options.preventDefault) {
					e.preventDefault();
					e.stopPropagation();
				}
				this._handleScan(this._buffer);
				this._resetBuffer();
				return;
			}
			this._resetBuffer();
			return;
		}

		// Only capture printable characters
		if (e.key.length !== 1) {
			return;
		}

		// Check timing — if too slow, it's manual typing
		if (this._buffer.length > 0 && (now - this._lastKeyTime) > this.options.maxDelay) {
			this._resetBuffer();
		}

		// Add to buffer
		this._buffer += e.key;
		this._lastKeyTime = now;

		// If we've accumulated enough rapid keystrokes, we're likely in a scan
		if (this._buffer.length >= 3 && this.options.preventDefault) {
			e.preventDefault();
			e.stopPropagation();
		}
	};

	/**
	 * Handle a completed scan.
	 * @private
	 * @param {string} code Scanned barcode data
	 */
	BarcodeScanner.prototype._handleScan = function(code) {
		var self = this;
		this._scanning = true;

		// Parse GS1 data locally first
		var parsed = this._parseGS1(code);

		// Populate target input if specified
		if (this.options.targetInput) {
			var input = document.querySelector(this.options.targetInput);
			if (input) {
				input.value = code;
				// Trigger change event
				var evt = document.createEvent('HTMLEvents');
				evt.initEvent('change', true, false);
				input.dispatchEvent(evt);
			}
		}

		// Perform AJAX lookup if configured
		if (this.options.autoLookup && this.options.ajaxUrl) {
			this._ajaxLookup(code, parsed);
		} else {
			// Return local parse result
			var result = {
				code: code,
				type: parsed.type || 'unknown',
				gs1: parsed,
				matches: []
			};
			this.options.onScan(result);
			this._scanning = false;
		}
	};

	/**
	 * Perform AJAX lookup for scanned barcode data.
	 * @private
	 * @param {string} code Scanned data
	 * @param {Object} parsed Local GS1 parse result
	 */
	BarcodeScanner.prototype._ajaxLookup = function(code, parsed) {
		var self = this;
		var xhr = new XMLHttpRequest();
		var url = this.options.ajaxUrl;

		xhr.open('POST', url, true);
		xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');

		xhr.onreadystatechange = function() {
			if (xhr.readyState === 4) {
				self._scanning = false;
				if (xhr.status === 200) {
					try {
						var response = JSON.parse(xhr.responseText);
						var result = {
							code: code,
							type: response.type || parsed.type || 'unknown',
							gs1: parsed,
							matches: response.matches || [],
							success: response.success || false,
							raw: response
						};
						self.options.onScan(result);
					} catch (e) {
						self.options.onError('Failed to parse scan response: ' + e.message);
					}
				} else {
					self.options.onError('Scan lookup failed: HTTP ' + xhr.status);
				}
			}
		};

		xhr.send('scan=' + encodeURIComponent(code));
	};

	/**
	 * Parse GS1 barcode data locally (client-side).
	 * Handles parenthesized format: (01)12345678901234(10)BATCH(21)SERIAL
	 * @private
	 * @param {string} data Raw barcode data
	 * @returns {Object} Parsed GS1 data with AI fields
	 */
	BarcodeScanner.prototype._parseGS1 = function(data) {
		var result = { type: 'unknown', raw: data };

		// Check for parenthesized GS1 format
		var pattern = /\((\d{2,4})\)([^()]*?)(?=\(\d{2,4}\)|$)/g;
		var match;
		var found = false;

		while ((match = pattern.exec(data)) !== null) {
			found = true;
			var ai = match[1];
			var value = match[2];

			switch (ai) {
				case '01': result.gtin = value; break;
				case '10': result.batch = value; break;
				case '11': result.production_date = this._parseGS1Date(value); break;
				case '17': result.expiry_date = this._parseGS1Date(value); break;
				case '21': result.serial = value; break;
				case '30': result.count = value; break;
				case '37': result.count = value; break;
			}
		}

		if (found) {
			result.type = 'gs1';
			return result;
		}

		// Check for EAN-13 / EAN-8 / UPC
		var numericData = data.replace(/[^0-9]/g, '');
		if (numericData === data && [8, 12, 13, 14].indexOf(data.length) !== -1) {
			result.type = 'ean';
			result.ean = data;
			return result;
		}

		return result;
	};

	/**
	 * Parse a GS1 date string (YYMMDD) to a human-readable format.
	 * @private
	 * @param {string} dateStr 6-digit YYMMDD string
	 * @returns {string} Formatted date string (YYYY-MM-DD)
	 */
	BarcodeScanner.prototype._parseGS1Date = function(dateStr) {
		if (!dateStr || dateStr.length !== 6) return dateStr;

		var yy = parseInt(dateStr.substring(0, 2), 10);
		var mm = dateStr.substring(2, 4);
		var dd = dateStr.substring(4, 6);

		var yyyy = (yy >= 51) ? (1900 + yy) : (2000 + yy);
		if (dd === '00') dd = '28'; // approximate last day

		return yyyy + '-' + mm + '-' + dd;
	};

	/**
	 * Reset the character buffer.
	 * @private
	 */
	BarcodeScanner.prototype._resetBuffer = function() {
		this._buffer = '';
		this._lastKeyTime = 0;
	};

	/**
	 * Show a visual indicator that a scan was detected (optional UI feedback).
	 *
	 * @param {string} message Text to display
	 * @param {string} type    'success', 'error', 'info'
	 */
	BarcodeScanner.prototype.showNotification = function(message, type) {
		type = type || 'info';
		var colors = {
			success: '#28a745',
			error: '#dc3545',
			info: '#17a2b8'
		};

		var div = document.createElement('div');
		div.style.cssText = 'position:fixed;top:20px;right:20px;padding:12px 20px;'
			+ 'background:' + (colors[type] || colors.info) + ';color:white;'
			+ 'border-radius:4px;z-index:99999;font-size:14px;'
			+ 'box-shadow:0 4px 12px rgba(0,0,0,0.15);'
			+ 'transition:opacity 0.5s ease;';
		div.textContent = message;
		document.body.appendChild(div);

		setTimeout(function() {
			div.style.opacity = '0';
			setTimeout(function() {
				if (div.parentNode) {
					div.parentNode.removeChild(div);
				}
			}, 500);
		}, 3000);
	};

	/**
	 * Create a scan button with a barcode icon for manual trigger.
	 *
	 * @param {string} containerId ID of the container element
	 * @param {Function} callback  Called with the manually entered code
	 * @returns {HTMLElement} The created button element
	 */
	BarcodeScanner.prototype.createScanButton = function(containerId, callback) {
		var container = document.getElementById(containerId);
		if (!container) return null;

		var self = this;
		var btn = document.createElement('button');
		btn.type = 'button';
		btn.className = 'btn btn-default';
		btn.innerHTML = '<i class="fa fa-barcode"></i> ' + (window._scanBtnText || 'Scan');
		btn.title = window._scanBtnTooltip || 'Click to enter barcode manually';
		btn.style.marginLeft = '4px';

		btn.addEventListener('click', function(e) {
			e.preventDefault();
			var code = prompt(window._scanPromptText || 'Enter or scan barcode:');
			if (code && code.trim()) {
				if (callback) {
					callback(code.trim());
				} else {
					self.processCode(code.trim());
				}
			}
		});

		container.appendChild(btn);
		return btn;
	};

	// Export to global scope
	window.BarcodeScanner = BarcodeScanner;

})(window);
