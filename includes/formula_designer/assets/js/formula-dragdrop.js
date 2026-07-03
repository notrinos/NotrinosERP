'use strict';

(function (window, document, $) {
	var MIME = 'application/x-fd-token';
	var TOUCH_DELAY_MS = 300;
	var SEARCH_DEBOUNCE_MS = 120;

	if (!window.FormulaDesigner) {
		return;
	}

	function parseMetadata(node) {
		var raw = node.getAttribute('data-metadata');

		if (!raw) {
			return {};
		}

		try {
			return JSON.parse(raw);
		} catch (error) {
			return {};
		}
	}

	function closest(node, selector) {
		while (node && node !== document) {
			if (node.matches && node.matches(selector)) {
				return node;
			}
			node = node.parentNode;
		}

		return null;
	}

	function cloneValue(value) {
		return JSON.parse(JSON.stringify(value));
	}

	function DragManager(instance) {
		this.instance = instance;
		this.root = instance.root;
		this.activePayload = null;
		this.dragSourceNode = null;
		this.currentConnector = null;
		this.touchTimer = null;
		this.touchGhost = null;
		this.touchPayload = null;
		this.searchTimers = {};

		this.bindPaletteHeaders();
		this.bindSearch();
		this.bindDragEvents();
		this.bindTouchEvents();
		this.loadPalettes();
	}

	DragManager.prototype.bindPaletteHeaders = function () {
		this.root.addEventListener('click', function (event) {
			var header = closest(event.target, '.fd-namespace-header, .fd-category-header');
			var container;
			var items;
			var expanded;

			if (!header) {
				return;
			}

			container = header.parentNode;
			items = header.classList.contains('fd-namespace-header')
				? container.querySelector('.fd-namespace-items')
				: container.querySelector('.fd-category-items');

			if (!items) {
				return;
			}

			expanded = header.getAttribute('aria-expanded') !== 'false';
			header.setAttribute('aria-expanded', expanded ? 'false' : 'true');
			items.style.display = expanded ? 'none' : 'flex';
		});
	};

	DragManager.prototype.bindSearch = function () {
		var self = this;

		this.root.addEventListener('input', function (event) {
			var input = closest(event.target, '.fd-palette-search');
			var palette;
			var key;

			if (!input) {
				return;
			}

			palette = closest(input, '.fd-palette');
			if (!palette) {
				return;
			}

			key = palette.getAttribute('data-designer') || 'palette';
			if (self.searchTimers[key]) {
				window.clearTimeout(self.searchTimers[key]);
			}

			self.searchTimers[key] = window.setTimeout(function () {
				self.filterPalette(palette, input.value || '');
			}, SEARCH_DEBOUNCE_MS);
		});
	};

	DragManager.prototype.bindDragEvents = function () {
		var self = this;

		this.root.addEventListener('dragstart', function (event) {
			var source = closest(event.target, '.fd-palette-item, .fd-token');
			var payload;
			var dragLabel;

			if (!source || source.classList.contains('fd-palette-item--disabled')) {
				return;
			}

			payload = self.buildPayloadFromSource(source);
			if (!payload) {
				return;
			}

			self.activePayload = payload;
			self.dragSourceNode = source;
			dragLabel = source.getAttribute('data-display-label') || source.textContent.trim();
			source.classList.add(source.classList.contains('fd-token') ? 'fd-token--dragging' : 'fd-palette-item--dragging');

			if (event.dataTransfer) {
				event.dataTransfer.effectAllowed = 'move';
				event.dataTransfer.setData(MIME, JSON.stringify(payload));
				event.dataTransfer.setData('text/plain', dragLabel);
				self.setDragImage(event.dataTransfer, dragLabel);
			}
		});

		this.root.addEventListener('dragend', function () {
			self.clearDragState();
		});

		this.root.addEventListener('dragover', function (event) {
			var target = self.resolveDropTarget(event.target);
			var payload = self.readPayload(event);
			var result;

			if (!target || !payload) {
				return;
			}

			result = self.validatePayloadAt(target.position, payload);
			event.preventDefault();
			self.highlightConnector(target.connector, result.valid);
		});

		this.root.addEventListener('dragleave', function (event) {
			if (!closest(event.target, '.fd-connector')) {
				return;
			}

			self.clearConnectorState();
		}, true);

		this.root.addEventListener('drop', function (event) {
			var target = self.resolveDropTarget(event.target);
			var payload = self.readPayload(event);
			var result;

			if (!target || !payload) {
				return;
			}

			result = self.validatePayloadAt(target.position, payload);
			event.preventDefault();

			if (result.valid) {
				self.applyPayload(target.position, payload);
				self.clearDragState();
				return;
			}

			self.highlightConnector(target.connector, false);
			self.clearDragState(true);
			window.setTimeout(function () {
				self.clearConnectorState();
			}, 250);
		});
	};

	DragManager.prototype.bindTouchEvents = function () {
		var self = this;

		this.root.addEventListener('touchstart', function (event) {
			var source = closest(event.target, '.fd-palette-item, .fd-token');

			if (!source || source.classList.contains('fd-palette-item--disabled')) {
				return;
			}

			self.clearTouchTimer();
			self.touchTimer = window.setTimeout(function () {
				self.touchPayload = self.buildPayloadFromSource(source);
				self.dragSourceNode = source;
				self.createTouchGhost(source, event.touches[0]);
			}, TOUCH_DELAY_MS);
		}, { passive: true });

		this.root.addEventListener('touchmove', function (event) {
			var touch = event.touches[0];
			var target;
			var result;

			if (!self.touchPayload || !touch) {
				return;
			}

			event.preventDefault();
			self.moveTouchGhost(touch);
			target = self.resolveDropTarget(document.elementFromPoint(touch.clientX, touch.clientY));
			if (!target) {
				self.clearConnectorState();
				return;
			}

			result = self.validatePayloadAt(target.position, self.touchPayload);
			self.highlightConnector(target.connector, result.valid);
		}, { passive: false });

		this.root.addEventListener('touchend', function (event) {
			var touch = event.changedTouches[0];
			var target;
			var result;

			if (!self.touchPayload || !touch) {
				self.clearTouchTimer();
				return;
			}

			target = self.resolveDropTarget(document.elementFromPoint(touch.clientX, touch.clientY));
			if (target) {
				result = self.validatePayloadAt(target.position, self.touchPayload);
				if (result.valid) {
					self.applyPayload(target.position, self.touchPayload);
				}
			}

			self.clearDragState();
		});
	};

	DragManager.prototype.loadPalettes = function () {
		var fieldPalette = this.root.querySelector('[data-designer="field-palette"]');
		var functionPalette = this.root.querySelector('[data-designer="function-palette"]');

		if (fieldPalette) {
			this.fetchPalette(fieldPalette, 'fields');
		}

		if (functionPalette) {
			this.fetchPalette(functionPalette, 'functions');
		}
	};

	DragManager.prototype.fetchPalette = function (palette, type) {
		var self = this;
		var url = palette.getAttribute('data-api-url');

		if (!url || !$ || typeof $.getJSON !== 'function') {
			return;
		}

		$.getJSON(url).done(function (payload) {
			self.renderPalettePayload(palette, type, payload);
		});
	};

	DragManager.prototype.renderPalettePayload = function (palette, type, payload) {
		var body = palette.querySelector('.fd-palette-body');
		var html = '';

		if (!body || !payload || payload.ok !== true) {
			return;
		}

		// Build the AJAX-fetched section HTML
		if (type === 'fields') {
			(payload.namespaces || []).forEach(function (section) {
				html += renderFieldSection(section);
			});
		} else {
			(payload.categories || []).forEach(function (section) {
				html += renderFunctionSection(section);
			});
		}

		// Preserve operator and literal sections (server-rendered, not in AJAX payload)
		var operatorsSection = body.querySelector('.fd-category-section[data-category="operators"]');
		var literalsSection  = body.querySelector('.fd-category-section[data-category="literals"]');

		body.innerHTML = html;

		// Re-insert operators at the top of the palette body
		if (operatorsSection) {
			body.insertBefore(operatorsSection, body.firstChild);
		}

		// Re-insert literals after operators (or at top if no operators)
		if (literalsSection) {
			if (operatorsSection && operatorsSection.nextSibling) {
				body.insertBefore(literalsSection, operatorsSection.nextSibling);
			} else {
				body.insertBefore(literalsSection, body.firstChild);
			}
		}
	};

	DragManager.prototype.filterPalette = function (palette, query) {
		var normalized = String(query || '').toLowerCase();
		var items = palette.querySelectorAll('.fd-palette-item');
		var sections = palette.querySelectorAll('.fd-namespace-section, .fd-category-section');
		var index;

		for (index = 0; index < items.length; index += 1) {
			var text = items[index].textContent.toLowerCase();
			var match = normalized === '' || text.indexOf(normalized) !== -1;
			items[index].style.display = match ? '' : 'none';
		}

		for (index = 0; index < sections.length; index += 1) {
			// Always keep operator and literal sections visible regardless of search
			var category = sections[index].getAttribute('data-category');
			if (category === 'operators' || category === 'literals') {
				sections[index].style.display = '';
				var sectionItems = sections[index].querySelectorAll('.fd-palette-item');
				for (var j = 0; j < sectionItems.length; j += 1) {
					sectionItems[j].style.display = '';
				}
				continue;
			}
			var visibleItems = sections[index].querySelectorAll('.fd-palette-item:not([style*="display: none"])');
			sections[index].style.display = visibleItems.length ? '' : 'none';
		}
	};

	DragManager.prototype.buildPayloadFromSource = function (source) {
		if (source.classList.contains('fd-palette-item')) {
			return {
				action: 'create',
				tokenType: source.getAttribute('data-token-type'),
				tokenValue: source.getAttribute('data-token-value'),
				displayLabel: source.getAttribute('data-display-label'),
				metadata: parseMetadata(source)
			};
		}

		if (source.classList.contains('fd-token')) {
			return {
				action: 'move',
				tokenId: source.getAttribute('data-token-id'),
				sourceExpressionId: this.instance.getExpressionId(),
				sourcePosition: parseInt(source.getAttribute('data-token-index'), 10) || 0
			};
		}

		return null;
	};

	DragManager.prototype.readPayload = function (event) {
		if (this.activePayload) {
			return cloneValue(this.activePayload);
		}

		if (!event.dataTransfer) {
			return null;
		}

		try {
			return JSON.parse(event.dataTransfer.getData(MIME));
		} catch (error) {
			return null;
		}
	};

	DragManager.prototype.resolveDropTarget = function (targetNode) {
		var connector = closest(targetNode, '.fd-connector');
		var expression;

		if (connector) {
			return {
				connector: connector,
				position: parseInt(connector.getAttribute('data-connector-position'), 10) || 0
			};
		}

		expression = closest(targetNode, '.fd-expression');
		if (expression && !expression.querySelector('.fd-token')) {
			connector = expression.querySelector('.fd-connector');
			return {
				connector: connector,
				position: 0
			};
		}

		return null;
	};

	DragManager.prototype.validatePayloadAt = function (position, payload) {
		var tokens = this.instance.getTokens();
		var simulated = simulateInsertion(tokens, position, payload);

		return {
			valid: simulated !== null && sequenceIsValid(simulated)
		};
	};

	DragManager.prototype.applyPayload = function (position, payload) {
		var tokens = this.instance.getTokens();
		var simulated = simulateInsertion(tokens, position, payload);
		var inserted;
		var newTokenIds;
		var i;

		if (simulated === null || !sequenceIsValid(simulated)) {
			return;
		}

		// Build a lookup of existing token IDs so we can detect which
		// tokens in the simulated result are brand-new insertions.
		var existingIds = {};
		for (i = 0; i < tokens.length; i += 1) {
			existingIds[tokens[i].id] = true;
		}

		// The first token in simulated that does NOT have a pre-existing
		// ID is the first newly-inserted token (they appear at the insertion
		// point, not necessarily at the end).
		for (i = 0; i < simulated.length; i += 1) {
			if (!existingIds[simulated[i].id]) {
				inserted = simulated[i];
				break;
			}
		}

		this.instance.replaceTokens(simulated, {
			source: 'dragdrop',
			description: payload.action === 'move' ? 'Move' : 'Insert',
			selectedTokenId: (inserted ? inserted.id : null)
		});

		// Bug 4: If the inserted token is a literal (number), auto-start
		// inline editing so the user can type the value immediately.
		if (inserted && inserted.type === 'literal') {
			this.instance.startLiteralEdit(inserted.id, true);
		}
	};

	function sequenceIsValid(tokens) {
		if (window.FormulaDesigner && typeof window.FormulaDesigner.isTokenSequenceValid === 'function') {
			return window.FormulaDesigner.isTokenSequenceValid(tokens);
		}

		return isSequenceValid(tokens);
	}

	DragManager.prototype.highlightConnector = function (connector, valid) {
		this.clearConnectorState();
		this.currentConnector = connector;
		connector.classList.add(valid ? 'fd-connector--drop-valid' : 'fd-connector--drop-invalid');
	};

	DragManager.prototype.clearConnectorState = function () {
		var connectors = this.root.querySelectorAll('.fd-connector');
		var index;

		for (index = 0; index < connectors.length; index += 1) {
			connectors[index].classList.remove('fd-connector--drop-hint');
			connectors[index].classList.remove('fd-connector--drop-valid');
			connectors[index].classList.remove('fd-connector--drop-invalid');
		}

		this.currentConnector = null;
	};

	DragManager.prototype.clearTouchTimer = function () {
		if (this.touchTimer) {
			window.clearTimeout(this.touchTimer);
			this.touchTimer = null;
		}
	};

	DragManager.prototype.clearDragState = function (preserveConnectorState) {
		this.clearTouchTimer();
		if (!preserveConnectorState) {
			this.clearConnectorState();
		}

		if (this.dragSourceNode) {
			this.dragSourceNode.classList.remove('fd-token--dragging');
			this.dragSourceNode.classList.remove('fd-palette-item--dragging');
		}

		if (this.touchGhost && this.touchGhost.parentNode) {
			this.touchGhost.parentNode.removeChild(this.touchGhost);
		}

		this.touchGhost = null;
		this.touchPayload = null;
		this.activePayload = null;
		this.dragSourceNode = null;
	};

	DragManager.prototype.setDragImage = function (dataTransfer, label) {
		var ghost = document.createElement('div');
		ghost.className = 'fd-drag-ghost';
		ghost.textContent = label;
		ghost.style.top = '-9999px';
		ghost.style.left = '-9999px';
		document.body.appendChild(ghost);
		dataTransfer.setDragImage(ghost, 12, 12);
		window.setTimeout(function () {
			if (ghost.parentNode) {
				ghost.parentNode.removeChild(ghost);
			}
		}, 0);
	};

	DragManager.prototype.createTouchGhost = function (source, touch) {
		this.touchGhost = document.createElement('div');
		this.touchGhost.className = 'fd-drag-ghost';
		this.touchGhost.textContent = source.getAttribute('data-display-label') || source.textContent.trim();
		document.body.appendChild(this.touchGhost);
		this.moveTouchGhost(touch);
	};

	DragManager.prototype.moveTouchGhost = function (touch) {
		if (!this.touchGhost || !touch) {
			return;
		}

		this.touchGhost.style.left = (touch.clientX + 12) + 'px';
		this.touchGhost.style.top = (touch.clientY + 12) + 'px';
	};

	function renderFieldSection(section) {
		var html = '';
		var items = section.items || [];

		html += '<div class="fd-namespace-section" data-namespace="' + escapeHtml(section.namespace || '') + '">';
		html += '<div class="fd-namespace-header" role="button" aria-expanded="true" tabindex="0">';
		html += '<span class="fd-namespace-icon">' + escapeHtml((section.namespace || '').slice(0, 3)) + '</span>';
		html += '<span class="fd-namespace-label">' + escapeHtml(section.label || section.namespace || '') + '</span>';
		html += '<span class="fd-namespace-count">' + items.length + '</span>';
		html += '</div><div class="fd-namespace-items">';
		items.forEach(function (item) {
			var qualified = item.qualifiedName || ((item.namespace || '') + '.' + (item.name || ''));
			html += '<div class="fd-palette-item fd-palette-field" draggable="true" data-token-type="variable" data-token-value="'
				+ escapeAttribute(qualified) + '" data-display-label="' + escapeAttribute(item.label || qualified)
				+ '" data-metadata="' + escapeAttribute(JSON.stringify({
					namespace: item.namespace || '',
					name: item.name || '',
					dataType: item.type || 'mixed',
					description: item.description || ''
				})) + '" data-field-type="' + escapeAttribute(item.type || 'mixed')
				+ '" data-field-qualified="' + escapeAttribute(qualified) + '" data-field-label="'
				+ escapeAttribute(item.label || qualified) + '" role="listitem">'
				+ '<span class="fd-palette-item-icon fd-icon-variable">' + escapeHtml(typeAbbreviation(item.type || 'mixed')) + '</span>'
				+ '<span class="fd-palette-item-label">' + escapeHtml(item.label || qualified) + '</span>'
				+ '<span class="fd-palette-item-type">' + escapeHtml(item.type || 'mixed') + '</span>'
				+ '</div>';
		});
		html += '</div></div>';

		return html;
	}

	function renderFunctionSection(section) {
		var html = '';
		var items = section.items || [];

		html += '<div class="fd-category-section" data-category="' + escapeHtml(section.category || '') + '">';
		html += '<div class="fd-category-header" role="button" aria-expanded="true" tabindex="0">';
		html += '<span class="fd-category-icon">fx</span>';
		html += '<span class="fd-category-label">' + escapeHtml(section.label || section.category || '') + '</span>';
		html += '<span class="fd-category-count">' + items.length + '</span>';
		html += '</div><div class="fd-category-items">';
		items.forEach(function (item) {
			var disabled = item.enabled === false;
			html += '<div class="fd-palette-item fd-palette-function' + (disabled ? ' fd-palette-item--disabled' : '') + '" draggable="'
				+ (disabled ? 'false' : 'true') + '" data-token-type="function" data-token-value="'
				+ escapeAttribute(item.tokenValue || ((item.name || '') + '(')) + '" data-display-label="'
				+ escapeAttribute(item.label || item.name || '') + '" data-metadata="' + escapeAttribute(JSON.stringify({
					name: item.name || '',
					minArgs: item.minArgs || 0,
					maxArgs: item.maxArgs || 0,
					returnType: item.returnType || 'mixed',
					description: item.description || ''
				})) + '" data-function-name="' + escapeAttribute(item.name || '') + '" data-function-signature="'
				+ escapeAttribute(item.signature || '') + '" data-function-description="' + escapeAttribute(item.description || '')
				+ '" role="listitem"><span class="fd-palette-item-icon fd-icon-function">fx</span><span class="fd-palette-item-label">'
				+ escapeHtml(item.label || item.name || '') + '</span><span class="fd-palette-item-signature">'
				+ escapeHtml(item.signature || '') + '</span></div>';
		});
		html += '</div></div>';

		return html;
	}

	function typeAbbreviation(type) {
		switch (String(type || '').toLowerCase()) {
			case 'number':
			case 'decimal':
			case 'integer':
				return 'N';
			case 'text':
			case 'string':
				return 'T';
			case 'date':
				return 'D';
			case 'boolean':
				return 'B';
			default:
				return 'M';
		}
	}

	function buildTokensFromPayload(payload) {
		var metadata = payload.metadata && typeof payload.metadata === 'object' ? payload.metadata : {};
		var tokenId;

		if (payload.action === 'move') {
			return [];
		}

		tokenId = window.FormulaDesigner.createTokenId('tok');

		if (payload.tokenType === 'function') {
			return [
				{
					id: tokenId,
					type: 'function',
					value: String(payload.tokenValue || '').slice(-1) === '(' ? payload.tokenValue : String(payload.tokenValue || '') + '(',
					label: payload.displayLabel || payload.tokenValue || '',
					metadata: metadata
				},
				{
					id: window.FormulaDesigner.createTokenId('tok'),
					type: 'group',
					value: ')',
					label: '',
					metadata: { generatedBy: tokenId, autoInserted: true }
				}
			];
		}

		// For operators, use the symbol as label (not the palette display name)
		var isOp = payload.tokenType === 'operator';

		return [{
			id: tokenId,
			type: payload.tokenType || 'literal',
			value: payload.tokenValue || '',
			label: isOp ? (payload.tokenValue || '') : (payload.displayLabel || payload.tokenValue || ''),
			metadata: metadata
		}];
	}

	function simulateInsertion(tokens, position, payload) {
		var working = cloneValue(tokens || []);
		var insertTokens = buildTokensFromPayload(payload);
		var move;

		position = Math.max(0, Math.min(position, working.length));

		if (payload.action === 'move') {
			move = extractMovedTokens(working, payload);
			working = move.tokens;
			insertTokens = move.moved;
			if (!insertTokens.length) {
				return null;
			}
			if ((payload.sourcePosition || 0) < position) {
				position -= insertTokens.length;
			}
		}

		if (!insertTokens.length) {
			return null;
		}

		working.splice.apply(working, [position, 0].concat(insertTokens));
		return working;
	}

	function extractMovedTokens(tokens, payload) {
		var source = parseInt(payload.sourcePosition, 10);
		var moved = [];

		if (isNaN(source) || source < 0 || source >= tokens.length) {
			return { tokens: tokens, moved: moved };
		}

		moved.push(tokens.splice(source, 1)[0]);

		if (
			moved[0] && moved[0].type === 'function'
			&& tokens[source]
			&& tokens[source].type === 'group'
			&& tokens[source].metadata
			&& tokens[source].metadata.generatedBy === moved[0].id
		) {
			moved.push(tokens.splice(source, 1)[0]);
		}

		return { tokens: tokens, moved: moved };
	}

	function isSequenceValid(tokens) {
		var depth = 0;
		var index;

		if (!tokens.length) {
			return true;
		}

		// Only check that the first token can start a sequence.
		// Do NOT check canEnd — intermediate editing states naturally
		// end with operators (e.g. "BASIC *" while the user continues building).
		if (!canStart(tokens[0])) {
			return false;
		}

		for (index = 0; index < tokens.length; index += 1) {
			if (isFunction(tokens[index]) || isOpenGroup(tokens[index])) {
				depth += 1;
			}

			if (isCloseGroup(tokens[index])) {
				depth -= 1;
				if (depth < 0) {
					return false;
				}
			}

			if (index < tokens.length - 1 && !isTransitionValid(tokens[index], tokens[index + 1])) {
				return false;
			}
		}

		return depth === 0;
	}

	function canStart(token) {
		return isValueStarter(token) || isUnaryMinus(token);
	}

	function canEnd(token) {
		return isOperand(token) || isCloseGroup(token);
	}

	function isTransitionValid(left, right) {
		if (isFunction(left) && isCloseGroup(right)) {
			return true;
		}

		if (isFunction(left) || isOpenGroup(left) || isBinaryOperator(left) || isUnaryMinus(left)) {
			return isValueStarter(right) || isUnaryMinus(right);
		}

		if (isOperand(left) || isCloseGroup(left)) {
			return isBinaryOperator(right) || isCloseGroup(right);
		}

		return false;
	}

	function isValueStarter(token) {
		return isOperand(token) || isFunction(token) || isOpenGroup(token);
	}

	function isOperand(token) {
		return token && (token.type === 'variable' || token.type === 'literal');
	}

	function isFunction(token) {
		return token && token.type === 'function';
	}

	function isOpenGroup(token) {
		return token && token.type === 'group' && token.value === '(';
	}

	function isCloseGroup(token) {
		return token && token.type === 'group' && token.value === ')';
	}

	function isBinaryOperator(token) {
		return token && token.type === 'operator' && !isUnaryMinus(token);
	}

	function isUnaryMinus(token) {
		return token && token.type === 'operator' && token.value === '-';
	}

	function escapeHtml(value) {
		return String(value)
			.replace(/&/g, '&amp;')
			.replace(/</g, '&lt;')
			.replace(/>/g, '&gt;')
			.replace(/"/g, '&quot;')
			.replace(/'/g, '&#039;');
	}

	function escapeAttribute(value) {
		return escapeHtml(value);
	}

	function boot() {
		var key;

		for (key in window.FormulaDesigner.instances) {
			if (window.FormulaDesigner.instances.hasOwnProperty(key) && !window.FormulaDesigner.instances[key].dragManager) {
				window.FormulaDesigner.instances[key].dragManager = new DragManager(window.FormulaDesigner.instances[key]);
			}
		}
	}

	if (document.readyState === 'loading') {
		document.addEventListener('DOMContentLoaded', boot);
	} else {
		boot();
	}

	// Re-invoke boot when designer instances are created dynamically
	// (e.g. inside a modal overlay that is rendered after page load).
	// The main formula-designer.js dispatches 'fd:boot' after initialising
	// new .fd-container elements, so we pick them up here and attach a
	// DragManager to any instance that does not already have one.
	document.addEventListener('fd:boot', boot);
}(window, document, window.jQuery || null));