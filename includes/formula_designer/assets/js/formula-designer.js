'use strict';

(function (window, document) {
	var DEFAULT_ZOOM = 1;
	var MIN_ZOOM = 0.5;
	var MAX_ZOOM = 2;
	var ZOOM_STEP = 0.25;
	var tokenIdCounter = 0;

	var testExpression = {
		id: 'expr-root',
		type: 'expression',
		tokens: [
			{
				id: 't1',
				type: 'variable',
				value: 'Employee.BasicSalary',
				label: 'Basic Salary',
				metadata: { namespace: 'Employee', name: 'BasicSalary', dataType: 'number' }
			},
			{ id: 't2', type: 'operator', value: '*', label: '×' },
			{
				id: 't3',
				type: 'literal',
				value: '0.12',
				label: '0.12',
				metadata: { dataType: 'number', rawValue: 0.12 }
			},
			{ id: 't4', type: 'operator', value: '+', label: '+' },
			{
				id: 't5',
				type: 'function',
				value: 'ROUND(',
				label: 'ROUND',
				metadata: { name: 'ROUND', minArgs: 1, maxArgs: 2 }
			},
			{
				id: 't6',
				type: 'variable',
				value: 'Employee.Overtime',
				label: 'Overtime',
				metadata: { namespace: 'Employee', name: 'Overtime', dataType: 'number' }
			},
			{
				id: 't7',
				type: 'literal',
				value: '2',
				label: '2',
				metadata: { dataType: 'number', rawValue: 2 }
			},
			{ id: 't8', type: 'group', value: ')', label: '' }
		]
	};

	function clamp(value, min, max) {
		return Math.max(min, Math.min(max, value));
	}

	function parseExpression(root) {
		var dataNode = root.querySelector('.fd-expression-data');
		var parsed;
		if (!dataNode) {
			return cloneExpression(testExpression);
		}

		try {
			parsed = JSON.parse(dataNode.textContent || '{}');
			if (parsed && parsed.expression) {
				parsed = parsed.expression;
			}

			return normalizeExpression(parsed);
		} catch (error) {
			return cloneExpression(testExpression);
		}
	}

	function cloneExpression(expression) {
		return normalizeExpression(JSON.parse(JSON.stringify(expression)));
	}

	function nextTokenId(prefix) {
		tokenIdCounter += 1;
		return (prefix || 'tok') + '-' + new Date().getTime() + '-' + tokenIdCounter;
	}

	function normalizeExpression(expression) {
		var normalized = expression && typeof expression === 'object' ? expression : {};
		normalized.id = normalized.id || 'expr-root';
		normalized.type = normalized.type || 'expression';
		normalized.tokens = Array.isArray(normalized.tokens) ? normalized.tokens : [];

		normalized.tokens = normalized.tokens.map(function (token, index) {
			var metadata = token.metadata && typeof token.metadata === 'object' ? token.metadata : {};

			return {
				id: token.id || ('tok-' + index),
				type: token.type || 'literal',
				value: token.value || '',
				label: token.label || token.value || '',
				metadata: metadata
			};
		});

		return normalized;
	}

	function namespaceAbbreviation(namespaceValue) {
		var cleaned = String(namespaceValue || '').replace(/[^A-Za-z]/g, '');
		return cleaned ? cleaned.slice(0, 3) : 'Var';
	}

	function tokenMarkup(token, index) {
		var classes = 'fd-token fd-token--' + token.type + ' fd-token-' + token.type;
		var label = token.label || token.value || '';
		var inner = '';
		var ariaLabel = label;

		if (token.type === 'variable') {
			inner = '<span class="fd-token-badge fd-token-badge--namespace fd-token-badge-namespace">'
				+ namespaceAbbreviation(token.metadata.namespace) + '</span>'
				+ '<span class="fd-token-label">' + escapeHtml(label) + '</span>';
			ariaLabel = 'Variable: ' + label;
		} else if (token.type === 'function') {
			inner = '<span class="fd-token-prefix">fx</span>'
				+ '<span class="fd-token-label">' + escapeHtml(label) + '</span>'
				+ '<span class="fd-token-suffix">(</span>';
			ariaLabel = 'Function: ' + label;
		} else if (token.type === 'literal') {
			inner = '<span class="fd-token-value">' + escapeHtml(label) + '</span>';
			ariaLabel = 'Literal: ' + label;
		} else if (token.type === 'operator') {
			inner = '<span class="fd-token-symbol">' + escapeHtml(label || token.value) + '</span>';
			ariaLabel = 'Operator: ' + label;
		} else {
			inner = '<span class="fd-token-symbol">' + escapeHtml(token.value || ')') + '</span>';
			ariaLabel = 'Group: ' + (token.value || ')');
		}

		return '<span class="' + classes + '" draggable="true" data-token-id="' + escapeAttribute(token.id)
			+ '" data-token-index="' + index + '" data-token-type="' + escapeAttribute(token.type)
			+ '" data-token-value="' + escapeAttribute(token.value || '') + '" tabindex="-1" role="button"'
			+ ' aria-label="' + escapeAttribute(ariaLabel) + '">' + inner + '</span>';
	}

	function connectorMarkup(position) {
		return '<span class="fd-connector" data-connector-position="' + position + '" data-designer="connector" aria-hidden="true"></span>';
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

	function DesignerInstance(root) {
		this.root = root;
		this.canvas = root.querySelector('.fd-canvas');
		this.expressionNode = root.querySelector('.fd-expression');
		this.gridNode = root.querySelector('.fd-grid-background');
		this.zoomValueNode = root.querySelector('.fd-zoom-value');
		this.expression = parseExpression(root);
		this.zoomLevel = DEFAULT_ZOOM;
		this.selectedTokenId = null;
		this.renderQueued = false;

		this.bindToolbar();
		this.bindCanvas();
		this.render();
		this.renderGrid();
		this.bindResize();
	}

	DesignerInstance.prototype.bindToolbar = function () {
		var self = this;
		this.root.addEventListener('click', function (event) {
			var trigger = event.target.closest('.fd-toolbar-action');
			if (!trigger) {
				return;
			}

			var action = trigger.getAttribute('data-action');
			if (action === 'zoom-in') {
				self.setZoom(self.zoomLevel + ZOOM_STEP);
			} else if (action === 'zoom-out') {
				self.setZoom(self.zoomLevel - ZOOM_STEP);
			} else if (action === 'reset-zoom') {
				self.setZoom(DEFAULT_ZOOM);
			}
		});
	};

	DesignerInstance.prototype.bindCanvas = function () {
		var self = this;

		this.root.addEventListener('click', function (event) {
			var token = event.target.closest('.fd-token');
			if (!token) {
				self.selectedTokenId = null;
				self.applySelection();
				return;
			}

			self.selectedTokenId = token.getAttribute('data-token-id');
			self.applySelection();
		});

		this.root.addEventListener('mouseenter', function (event) {
			var token = event.target.closest('.fd-token');
			if (token) {
				token.classList.add('fd-token--hover');
			}
		}, true);

		this.root.addEventListener('mouseleave', function (event) {
			var token = event.target.closest('.fd-token');
			if (token) {
				token.classList.remove('fd-token--hover');
			}
		}, true);
	};

	DesignerInstance.prototype.bindResize = function () {
		var self = this;
		window.addEventListener('resize', function () {
			self.renderGrid();
		});
	};

	DesignerInstance.prototype.setZoom = function (value) {
		this.zoomLevel = clamp(value, MIN_ZOOM, MAX_ZOOM);
		this.canvas.setAttribute('data-zoom-level', this.zoomLevel.toFixed(2));
		this.applyZoom();
	};

	DesignerInstance.prototype.getExpressionId = function () {
		return this.expression.id;
	};

	DesignerInstance.prototype.getTokens = function () {
		return cloneExpression(this.expression).tokens;
	};

	DesignerInstance.prototype.replaceTokens = function (tokens) {
		this.expression = normalizeExpression({
			id: this.expression.id,
			type: this.expression.type,
			tokens: tokens
		});
		this.render();
	};

	DesignerInstance.prototype.applyZoom = function () {
		this.expressionNode.style.transform = 'scale(' + this.zoomLevel + ')';
		this.expressionNode.style.transformOrigin = '0 0';

		if (this.zoomValueNode) {
			this.zoomValueNode.textContent = Math.round(this.zoomLevel * 100) + '%';
		}
	};

	DesignerInstance.prototype.applySelection = function () {
		var tokens = this.expressionNode.querySelectorAll('.fd-token');
		var index;

		for (index = 0; index < tokens.length; index += 1) {
			tokens[index].classList.toggle(
				'fd-token--selected',
				this.selectedTokenId !== null && tokens[index].getAttribute('data-token-id') === this.selectedTokenId
			);
		}
	};

	DesignerInstance.prototype.render = function () {
		var self = this;
		if (this.renderQueued) {
			return;
		}

		this.renderQueued = true;
		window.requestAnimationFrame(function () {
			var html = connectorMarkup(0);

			self.expression.tokens.forEach(function (token, index) {
				html += tokenMarkup(token, index);
				html += connectorMarkup(index + 1);
			});

			self.expressionNode.innerHTML = html;
			self.renderQueued = false;
			self.applyZoom();
			self.applySelection();
		});
	};

	DesignerInstance.prototype.renderGrid = function () {
		var width = Math.max(this.canvas.clientWidth, 640);
		var height = Math.max(this.canvas.clientHeight, 260);
		var step = parseInt(this.canvas.getAttribute('data-grid-size'), 10) || 20;
		var circles = '';
		var x;
		var y;

		this.gridNode.setAttribute('viewBox', '0 0 ' + width + ' ' + height);
		this.gridNode.setAttribute('width', width);
		this.gridNode.setAttribute('height', height);

		for (y = step / 2; y < height; y += step) {
			for (x = step / 2; x < width; x += step) {
				circles += '<circle cx="' + x + '" cy="' + y + '" r="1.2"></circle>';
			}
		}

		this.gridNode.innerHTML = circles;
	};

	function initDesigner(root) {
		var instanceId = root.getAttribute('data-instance-id') || ('fd-' + Object.keys(window.FormulaDesigner.instances).length);
		window.FormulaDesigner.instances[instanceId] = new DesignerInstance(root);
	}

	window.FormulaDesigner = window.FormulaDesigner || {
		instances: {},
		phaseTwoExpression: cloneExpression(testExpression)
	};
	window.FormulaDesigner.cloneExpression = cloneExpression;
	window.FormulaDesigner.normalizeExpression = normalizeExpression;
	window.FormulaDesigner.createTokenId = nextTokenId;

	function boot() {
		var roots = document.querySelectorAll('.fd-container[data-designer="root"]');
		var index;

		for (index = 0; index < roots.length; index += 1) {
			initDesigner(roots[index]);
		}
	}

	if (document.readyState === 'loading') {
		document.addEventListener('DOMContentLoaded', boot);
	} else {
		boot();
	}
}(window, document));