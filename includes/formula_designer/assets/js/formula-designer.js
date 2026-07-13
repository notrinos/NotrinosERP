'use strict';

(function (window, document) {
	var DEFAULT_ZOOM = 1;
	var MIN_ZOOM = 0.5;
	var MAX_ZOOM = 2;
	var ZOOM_STEP = 0.25;
	var TEXT_SYNC_DEBOUNCE_MS = 100;
	var TEXTAREA_PARSE_DEBOUNCE_MS = 300;
	var VALIDATION_DEBOUNCE_MS = 500;
	var tokenIdCounter = 0;

	// ---- Undo / Redo Manager ---------------------------------
	/**
	 * UndoManager — unlimited undo/redo stack with configurable entry cap.
	 * Each entry stores complete before/after token snapshots for reliability.
	 */
	function UndoManager(maxEntries) {
		this.undoStack = [];
		this.redoStack = [];
		this.maxEntries = typeof maxEntries === 'number' && maxEntries > 0 ? maxEntries : 200;
		this.enabled = true;
	}

	UndoManager.prototype.push = function (type, description, reverseTokens, forwardTokens, options) {
		if (!this.enabled) {
			return;
		}

		this.redoStack = [];

		this.undoStack.push({
			type: type,
			description: description || type,
			timestamp: Date.now(),
			reverseTokens: cloneExpression({ tokens: reverseTokens }).tokens,
			forwardTokens: cloneExpression({ tokens: forwardTokens }).tokens,
			selectionId: (options && options.selectionId) || null
		});

		while (this.undoStack.length > this.maxEntries) {
			this.undoStack.shift();
		}
	};

	UndoManager.prototype.undo = function (currentTokens) {
		var entry;

		if (!this.enabled || this.undoStack.length === 0) {
			return null;
		}

		entry = this.undoStack.pop();
		this.redoStack.push(entry);

		return {
			tokens: entry.reverseTokens,
			selectionId: entry.selectionId
		};
	};

	UndoManager.prototype.redo = function (currentTokens) {
		var entry;

		if (!this.enabled || this.redoStack.length === 0) {
			return null;
		}

		entry = this.redoStack.pop();
		this.undoStack.push(entry);

		return {
			tokens: entry.forwardTokens,
			selectionId: entry.selectionId
		};
	};

	UndoManager.prototype.canUndo = function () {
		return this.enabled && this.undoStack.length > 0;
	};

	UndoManager.prototype.canRedo = function () {
		return this.enabled && this.redoStack.length > 0;
	};

	UndoManager.prototype.getLastActionLabel = function () {
		if (!this.canUndo()) {
			return '';
		}

		return this.undoStack[this.undoStack.length - 1].description;
	};

	UndoManager.prototype.clear = function () {
		this.undoStack = [];
		this.redoStack = [];
	};

	UndoManager.prototype.setEnabled = function (value) {
		this.enabled = !!value;
	};

	// ---- Clipboard Manager -----------------------------------
	/**
	 * ClipboardManager — internal token clipboard (no system clipboard access).
	 */
	function ClipboardManager() {
		this.buffer = [];
		this.sourcePosition = -1;
	}

	ClipboardManager.prototype.copy = function (tokens, sourcePosition) {
		if (!tokens || !tokens.length) {
			return false;
		}

		this.buffer = cloneExpression({ tokens: tokens }).tokens;
		this.sourcePosition = typeof sourcePosition === 'number' ? sourcePosition : -1;
		return true;
	};

	ClipboardManager.prototype.cut = function (tokens, sourcePosition) {
		var copied = this.copy(tokens, sourcePosition);

		return copied;
	};

	ClipboardManager.prototype.getPasteTokens = function () {
		if (!this.buffer.length) {
			return null;
		}

		return cloneExpression({ tokens: this.buffer }).tokens;
	};

	ClipboardManager.prototype.hasContent = function () {
		return this.buffer.length > 0;
	};

	ClipboardManager.prototype.clear = function () {
		this.buffer = [];
		this.sourcePosition = -1;
	};

	ClipboardManager.prototype.getTokenCount = function () {
		return this.buffer.length;
	};

	// ---- Shared Undo/Clipboard instances --------------------
	var sharedUndoManager = new UndoManager(200);
	var sharedClipboard = new ClipboardManager();

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

	function closest(node, selector) {
		while (node && node !== document) {
			if (node.matches && node.matches(selector)) {
				return node;
			}
			node = node.parentNode;
		}

		return null;
	}

	function createDesignerEvent(name, detail) {
		var event;

		if (typeof window.CustomEvent === 'function') {
			return new window.CustomEvent(name, {
				bubbles: true,
				detail: detail || {}
			});
		}

		event = document.createEvent('CustomEvent');
		event.initCustomEvent(name, true, false, detail || {});
		return event;
	}

	function dispatchDesignerEvent(root, name, detail) {
		root.dispatchEvent(createDesignerEvent(name, detail));
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
			return normalizeToken(token, index);
		});

		return normalized;
	}

	function normalizeToken(token, index) {
			var metadata = token.metadata && typeof token.metadata === 'object' ? token.metadata : {};
			var type = token.type || 'literal';
			var value = token.value || '';
			var label = token.label || buildDefaultLabel(type, value, metadata);

			return {
				id: token.id || ('tok-' + index),
				type: type,
				value: value,
				label: label,
				metadata: metadata
			};
	}

	function buildDefaultLabel(type, value, metadata) {
		if (type === 'function') {
			return String(value || '').replace(/\($/, '').toUpperCase();
		}

		if (type === 'literal' && metadata && String(metadata.dataType || '').toLowerCase() === 'string') {
			return typeof metadata.rawValue === 'string' ? metadata.rawValue : stripLiteralQuotes(value);
		}

		return value || '';
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
		var isEditing = token.type === 'literal' && token.metadata && token.metadata.editing === true;

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
			if (isEditing) {
				classes += ' fd-token--editing';
				inner = '<input type="text" class="fd-literal-editor" value="'
					+ escapeAttribute(getEditableLiteralValue(token)) + '" aria-label="Edit literal value">';
			} else {
				inner = '<span class="fd-token-value">' + escapeHtml(label) + '</span>';
			}
			ariaLabel = 'Literal: ' + label;
		} else if (token.type === 'operator') {
			inner = '<span class="fd-token-symbol">' + escapeHtml(label || token.value) + '</span>';
			ariaLabel = 'Operator: ' + label;
		} else {
			inner = '<span class="fd-token-symbol">' + escapeHtml(token.value || ')') + '</span>';
			ariaLabel = 'Group: ' + (token.value || ')');
		}

		return '<span class="' + classes + '" draggable="' + (isEditing ? 'false' : 'true') + '" data-token-id="' + escapeAttribute(token.id)
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

	function stripLiteralQuotes(value) {
		var stringValue = String(value || '');
		if (stringValue.length >= 2 && (stringValue.charAt(0) === '"' || stringValue.charAt(0) === '\'')) {
			if (stringValue.charAt(stringValue.length - 1) === stringValue.charAt(0)) {
				return stringValue.substring(1, stringValue.length - 1);
			}
		}

		return stringValue;
	}

	function buildFunctionValue(token) {
		var metadata = token.metadata && typeof token.metadata === 'object' ? token.metadata : {};
		var name = metadata.name || String(token.value || '').replace(/\($/, '');
		return String(name || '').toUpperCase() + '(';
	}

	function buildLiteralValue(token) {
		var metadata = token.metadata && typeof token.metadata === 'object' ? token.metadata : {};
		var dataType = String(metadata.dataType || '').toLowerCase();
		var quote;
		var rawValue;

		if (dataType === 'string') {
			quote = metadata.quote || '"';
			rawValue = typeof metadata.rawValue === 'string' ? metadata.rawValue : stripLiteralQuotes(token.value || token.label || '');
			return quote + rawValue + quote;
		}

		if (token.value !== undefined && token.value !== null && token.value !== '') {
			return String(token.value);
		}

		if (metadata.rawValue === null) {
			return 'NULL';
		}

		return String(token.label || '');
	}

	function getEditableLiteralValue(token) {
		var metadata = token.metadata && typeof token.metadata === 'object' ? token.metadata : {};
		if (typeof metadata.editingValue === 'string') {
			return metadata.editingValue;
		}

		if (String(metadata.dataType || '').toLowerCase() === 'string') {
			return typeof metadata.rawValue === 'string' ? metadata.rawValue : stripLiteralQuotes(token.value || token.label || '');
		}

		return String(token.label || token.value || '');
	}

	function isOperatorToken(token) {
		return token && token.type === 'operator';
	}

	function isFunctionToken(token) {
		return token && token.type === 'function';
	}

	function isOpenGroupToken(token) {
		return token && token.type === 'group' && token.value === '(';
	}

	function isCloseGroupToken(token) {
		return token && token.type === 'group' && token.value === ')';
	}

	function isOperandToken(token) {
		return token && (token.type === 'variable' || token.type === 'literal');
	}

	function isSeparatorToken(token) {
		return isOperatorToken(token) && token.value === ',';
	}

	function isUnaryOperator(tokens, index) {
		var token = tokens[index];
		var metadata;
		var previous;
		var value;

		if (!isOperatorToken(token)) {
			return false;
		}

		metadata = token.metadata && typeof token.metadata === 'object' ? token.metadata : {};
		value = String(token.value || '').toUpperCase();

		if (metadata.operatorKind === 'unary' || value === 'NOT') {
			return true;
		}

		if (token.value !== '+' && token.value !== '-') {
			return false;
		}

		if (index === 0) {
			return true;
		}

		previous = tokens[index - 1];
		return isOperatorToken(previous) || isOpenGroupToken(previous) || isFunctionToken(previous);
	}

	function isBinaryOperator(tokens, index) {
		var token = tokens[index];
		return isOperatorToken(token) && !isSeparatorToken(token) && !isUnaryOperator(tokens, index);
	}

	function isValueStarter(tokens, index) {
		var token = tokens[index];
		return isOperandToken(token) || isFunctionToken(token) || isOpenGroupToken(token);
	}

	function canStart(tokens) {
		return isValueStarter(tokens, 0) || isUnaryOperator(tokens, 0);
	}

	function canEnd(tokens) {
		var token = tokens[tokens.length - 1];
		return isOperandToken(token) || isCloseGroupToken(token);
	}

	function isTokenSequenceValid(tokens) {
		var depth = 0;
		var index;
		var token;
		var next;

		if (!tokens.length) {
			return true;
		}

		// Only check that the first token can start a sequence.
		// Do NOT check canEnd — intermediate editing states naturally
		// end with operators (e.g. "BASIC *" while user continues building).
		if (!canStart(tokens)) {
			return false;
		}

		for (index = 0; index < tokens.length; index += 1) {
			token = tokens[index];

			if (isFunctionToken(token) || isOpenGroupToken(token)) {
				depth += 1;
			}

			if (isCloseGroupToken(token)) {
				depth -= 1;
				if (depth < 0) {
					return false;
				}
			}

			if (index === tokens.length - 1) {
				continue;
			}

			next = tokens[index + 1];
			if (isFunctionToken(token) && isCloseGroupToken(next)) {
				continue;
			}

			if (isFunctionToken(token) || isOpenGroupToken(token) || isBinaryOperator(tokens, index) || isUnaryOperator(tokens, index) || isSeparatorToken(token)) {
				if (!(isValueStarter(tokens, index + 1) || isUnaryOperator(tokens, index + 1))) {
					return false;
				}
				continue;
			}

			if (isOperandToken(token) || isCloseGroupToken(token)) {
				if (!(isBinaryOperator(tokens, index + 1) || isCloseGroupToken(next) || isSeparatorToken(next))) {
					return false;
				}
			}
		}

		return depth === 0;
	}

	function describeTokenFragment(tokens, index) {
		var token = tokens[index];
		var value = token ? String(token.value || '') : '';
		var upper = value.toUpperCase();

		if (!token) {
			return { text: '', lexemeOffset: 0, lexemeLength: 0 };
		}

		if (token.type === 'function') {
			value = buildFunctionValue(token);
			return { text: value, lexemeOffset: 0, lexemeLength: value.length };
		}

		if (token.type === 'group') {
			value = value || ')';
			return { text: value, lexemeOffset: 0, lexemeLength: value.length };
		}

		if (token.type === 'literal') {
			value = buildLiteralValue(token);
			return { text: value, lexemeOffset: 0, lexemeLength: value.length };
		}

		if (token.type === 'operator') {
			if (value === ',') {
				return { text: ', ', lexemeOffset: 0, lexemeLength: 1 };
			}

			if (isUnaryOperator(tokens, index)) {
				if (/^[A-Z]+$/.test(upper)) {
					return { text: upper + ' ', lexemeOffset: 0, lexemeLength: upper.length };
				}
				return { text: value, lexemeOffset: 0, lexemeLength: value.length };
			}

			return { text: ' ' + value + ' ', lexemeOffset: 1, lexemeLength: value.length };
		}

		return { text: value, lexemeOffset: 0, lexemeLength: value.length };
	}

	function serializeTokens(tokens) {
		var formula = '';
		var spans = [];
		var index;
		var fragment;
		var start;

		for (index = 0; index < tokens.length; index += 1) {
			fragment = describeTokenFragment(tokens, index);
			start = formula.length + fragment.lexemeOffset + 1;
			formula += fragment.text;
			spans.push({
				tokenId: tokens[index].id,
				start: start,
				end: start + Math.max(fragment.lexemeLength - 1, 0)
			});
		}

		formula = formula.replace(/\s+$/g, '');

		return {
			formula: formula,
			spans: spans
		};
	}

	function buildVariableMetadata(value) {
		var segments = String(value || '').split('.');
		if (segments.length > 1) {
			return {
				namespace: segments[0],
				name: segments.slice(1).join('.')
			};
		}

		return {
			name: String(value || '')
		};
	}

	function buildKeywordLiteralToken(keyword) {
		var metadata = {};

		if (keyword === 'NULL') {
			metadata.dataType = 'null';
			metadata.rawValue = null;
		} else {
			metadata.dataType = 'boolean';
			metadata.rawValue = keyword === 'TRUE';
		}

		return normalizeToken({
			id: nextTokenId('tok'),
			type: 'literal',
			value: keyword,
			label: keyword,
			metadata: metadata
		});
	}

	function buildOperatorToken(value) {
		var metadata = {};
		if (String(value || '').toUpperCase() === 'NOT') {
			metadata.operatorKind = 'unary';
		}
		if (value === ',') {
			metadata.operatorKind = 'separator';
		}

		return normalizeToken({
			id: nextTokenId('tok'),
			type: value === '(' || value === ')' ? 'group' : 'operator',
			value: value,
			label: value,
			metadata: metadata
		});
	}

	function readStringToken(formula, cursor) {
		var quote = formula.charAt(cursor.index);
		var value = '';
		cursor.index += 1;

		while (cursor.index < formula.length) {
			if (formula.charAt(cursor.index) === quote) {
				cursor.index += 1;
				break;
			}
			value += formula.charAt(cursor.index);
			cursor.index += 1;
		}

		return normalizeToken({
			id: nextTokenId('tok'),
			type: 'literal',
			value: quote + value + quote,
			label: value,
			metadata: {
				dataType: 'string',
				rawValue: value,
				quote: quote
			}
		});
	}

	function readNumberToken(formula, cursor) {
		var value = '';
		var hasDecimal = false;
		var character;

		while (cursor.index < formula.length) {
			character = formula.charAt(cursor.index);
			if (character === '.') {
				if (hasDecimal) {
					break;
				}
				hasDecimal = true;
				value += character;
				cursor.index += 1;
				continue;
			}

			if (!/[0-9]/.test(character)) {
				break;
			}

			value += character;
			cursor.index += 1;
		}

		return normalizeToken({
			id: nextTokenId('tok'),
			type: 'literal',
			value: value,
			label: value,
			metadata: {
				dataType: 'number',
				rawValue: hasDecimal ? parseFloat(value) : parseInt(value, 10)
			}
		});
	}

	function consumeWhitespace(formula, cursor) {
		while (cursor.index < formula.length && /\s/.test(formula.charAt(cursor.index))) {
			cursor.index += 1;
		}
	}

	function peekNextNonWhitespaceCharacter(formula, position) {
		while (position < formula.length) {
			if (!/\s/.test(formula.charAt(position))) {
				return formula.charAt(position);
			}
			position += 1;
		}

		return null;
	}

	function readIdentifierToken(formula, cursor) {
		var value = '';
		var character;
		var keyword;

		while (cursor.index < formula.length) {
			character = formula.charAt(cursor.index);
			if (/[A-Za-z0-9_]/.test(character)) {
				value += character;
				cursor.index += 1;
				continue;
			}

			if (character === '.' && cursor.index + 1 < formula.length && /[A-Za-z_]/.test(formula.charAt(cursor.index + 1))) {
				value += character;
				cursor.index += 1;
				continue;
			}

			break;
		}

		keyword = value.toUpperCase();
		if (keyword === 'TRUE' || keyword === 'FALSE' || keyword === 'NULL') {
			return buildKeywordLiteralToken(keyword);
		}

		if (keyword === 'AND' || keyword === 'OR' || keyword === 'NOT' || keyword === 'XOR') {
			return buildOperatorToken(keyword);
		}

		if (peekNextNonWhitespaceCharacter(formula, cursor.index) === '(') {
			consumeWhitespace(formula, cursor);
			if (formula.charAt(cursor.index) === '(') {
				cursor.index += 1;
			}

			return normalizeToken({
				id: nextTokenId('tok'),
				type: 'function',
				value: keyword + '(',
				label: keyword,
				metadata: {
					name: keyword
				}
			});
		}

		return normalizeToken({
			id: nextTokenId('tok'),
			type: 'variable',
			value: value,
			label: value,
			metadata: buildVariableMetadata(value)
		});
	}

	function matchOperator(formula, cursor) {
		var operators = ['??', '<=', '>=', '==', '!=', '<>'];
		var index;
		var character;

		for (index = 0; index < operators.length; index += 1) {
			if (formula.substr(cursor.index, operators[index].length) === operators[index]) {
				cursor.index += operators[index].length;
				return operators[index];
			}
		}

		character = formula.charAt(cursor.index);
		if ('+-*/%^(),:!<>=$'.indexOf(character) !== -1) {
			cursor.index += 1;
			return character;
		}

		return null;
	}

	function tokenizeFormula(formula) {
		var cursor = { index: 0 };
		var tokens = [];
		var character;
		var operator;

		while (cursor.index < formula.length) {
			character = formula.charAt(cursor.index);

			if (/\s/.test(character)) {
				cursor.index += 1;
				continue;
			}

			if (character === '"' || character === '\'') {
				tokens.push(readStringToken(formula, cursor));
				continue;
			}

			if (/[0-9]/.test(character) || (character === '.' && cursor.index + 1 < formula.length && /[0-9]/.test(formula.charAt(cursor.index + 1)))) {
				tokens.push(readNumberToken(formula, cursor));
				continue;
			}

			if (/[A-Za-z_]/.test(character)) {
				tokens.push(readIdentifierToken(formula, cursor));
				continue;
			}

			operator = matchOperator(formula, cursor);
			if (operator !== null) {
				tokens.push(buildOperatorToken(operator));
				continue;
			}

			cursor.index += 1;
		}

		return tokens;
	}

	function createEmptyValidationState(formula) {
		return {
			formula: formula || '',
			isValid: true,
			errors: [],
			warnings: []
		};
	}

	function normalizeValidationState(formula, payload) {
		var validation = payload && payload.validation ? payload.validation : payload;
		validation = validation && typeof validation === 'object' ? validation : {};

		return {
			formula: formula || '',
			isValid: validation.isValid !== false,
			errors: Array.isArray(validation.errors) ? validation.errors : [],
			warnings: Array.isArray(validation.warnings) ? validation.warnings : []
		};
	}

	function findTokenIdForColumn(spans, column) {
		var nearest = null;
		var nearestDistance = Number.MAX_VALUE;
		var index;
		var span;
		var distance;

		if (!spans.length) {
			return null;
		}

		if (!column || column < 1) {
			return spans[0].tokenId;
		}

		for (index = 0; index < spans.length; index += 1) {
			span = spans[index];
			if (column >= span.start && column <= span.end) {
				return span.tokenId;
			}

			distance = column < span.start ? span.start - column : column - span.end;
			if (distance < nearestDistance) {
				nearestDistance = distance;
				nearest = span.tokenId;
			}
		}

		return nearest;
	}

	function mapFindingsToTokenIds(serialized, findings) {
		var tokenIds = {};

		(findings || []).forEach(function (finding) {
			var tokenId = findTokenIdForColumn(serialized.spans || [], parseInt(finding.column, 10) || 0);
			if (tokenId) {
				tokenIds[tokenId] = true;
			}
		});

		return tokenIds;
	}

	function DesignerInstance(root) {
		this.root = root;
		this.canvas = root.querySelector('.fd-canvas');
		this.expressionNode = root.querySelector('.fd-expression');
		this.gridNode = root.querySelector('.fd-grid-background');
		this.zoomValueNode = root.querySelector('.fd-zoom-value');
		this.textarea = root.querySelector('.fd-source');
		this.errorPanel = root.querySelector('[data-designer="error-panel"]');
		this.errorListNode = root.querySelector('.fd-error-list');
		this.validateButton = root.querySelector('[data-action="validate"]');
		this.validationCountNodes = root.querySelectorAll('[data-role="validation-count"]');
		this.form = closest(root, 'form');
		this.expression = parseExpression(root);
		this.zoomLevel = DEFAULT_ZOOM;
		this.selectedTokenId = null;
		this.renderQueued = false;
		this.syncTimer = null;
		this.textareaParseTimer = null;
		this.validationTimer = null;
		this.validationRequest = null;
		this.lastSerialized = serializeTokens(this.expression.tokens);
		this.validationState = createEmptyValidationState(this.lastSerialized.formula);
		this.parseError = null;
		this.activeLiteralEditorId = null;
		this.skipTextareaInput = false;
		this.errorPanelDismissed = false;
		this.lastErrorSignature = '';
		this.lastValidatedFormula = '';
		this.allowSubmitOnce = false;
		this.pendingSubmitter = null;
		this.undoManager = sharedUndoManager;
		this.clipboard = sharedClipboard;
		this.disableUndoSnapshot = false;
		this.propertyPanel = root.querySelector('[data-designer="property-panel"]');
		this.propertyPanelBody = root.querySelector('[data-designer="property-panel-body"]');

		this.bindToolbar();
		this.bindCanvas();
		this.bindTextarea();
		this.bindForm();
		this.bindKeyboard();
		this.render();
		this.renderGrid();
		this.bindResize();
		this.initializeFormulaSource();
	}

	DesignerInstance.prototype.bindToolbar = function () {
		var self = this;
		this.root.addEventListener('click', function (event) {
			var trigger = event.target.closest('.fd-toolbar-action');
			var dismiss = event.target.closest('[data-action="dismiss-errors"]');

			if (dismiss) {
				self.errorPanelDismissed = true;
				self.applyValidationPresentation();
				return;
			}

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
			} else if (action === 'validate') {
				self.runValidation(true);
			} else if (action === 'preview') {
				// Show/hide preview panel
				if (window.FormulaDesigner.preview) {
					var previewPanel = self.root.querySelector('[data-designer="preview-panel"]');
					if (previewPanel) {
						previewPanel.hidden = !previewPanel.hidden;
						self.announceToScreenReader(
							previewPanel.hidden ? 'Preview panel closed' : 'Preview panel opened',
							'polite'
						);
					}
				}
			} else if (action === 'explain') {
				// Show/hide explain panel
				if (window.FormulaDesigner.preview) {
					var explainPanel = self.root.querySelector('[data-designer="explain-panel"]');
					if (explainPanel) {
						explainPanel.hidden = !explainPanel.hidden;
						self.announceToScreenReader(
							explainPanel.hidden ? 'Explain panel closed' : 'Explain panel opened',
							'polite'
						);
					}
				}
			} else if (action === 'template') {
				// Show/hide template browser
				var templateBrowser = self.root.querySelector('[data-designer="template-browser"]');
				if (templateBrowser) {
					templateBrowser.hidden = !templateBrowser.hidden;
					self.announceToScreenReader(
						templateBrowser.hidden ? 'Template browser closed' : 'Template browser opened',
						'polite'
					);
				}
			} else if (action === 'undo') {
				self.performUndo();
			} else if (action === 'redo') {
				self.performRedo();
			}
		});
	};

	DesignerInstance.prototype.bindCanvas = function () {
		var self = this;

		this.root.addEventListener('click', function (event) {
			// Find the closest token -- use our custom `closest` helper which
			// walks ancestors to locate the .fd-token wrapper.  Then determine
			// whether the token is a literal *and* the click landed on (or
			// inside) its value-rendering element.
			var hitToken = closest(event.target, '.fd-token');
			var token = hitToken ? hitToken : null;
			var isLiteralToken = token ? token.classList.contains('fd-token--literal') : false;
			var hitLiteralValue = isLiteralToken
				? closest(event.target, '.fd-token-value') !== null
				: false;
			var closePropPanel = event.target.closest('[data-action="close-property-panel"]');
			var insidePropertyPanel = closest(event.target, '.fd-property-panel');
			var insideToolbar = closest(event.target, '.fd-toolbar');

			// Clicking on an active literal editor (the <input> itself) must
			// be a no-op — the browser handles the input focus natively.
			// Stealing focus via canvas.focus() or re-triggering startLiteralEdit
			// would cause the input to blur (commitLiteralEdit) and collapse.
			var insideLiteralEditor = closest(event.target, '.fd-literal-editor') !== null;

			if (closePropPanel) {
				self.selectedTokenId = null;
				self.applySelection();
				self.renderPropertyPanel();
				self.announceToScreenReader('Property panel closed');
				return;
			}

			// Bug 3: Clicking inside the property panel or toolbar should
			// never deselect the current token or close the property panel.
			if (insidePropertyPanel || insideToolbar || insideLiteralEditor) {
				return;
			}

			if (!token) {
				if (self.activeLiteralEditorId) {
					self.commitLiteralEdit();
				}
				self.selectedTokenId = null;
				self.applySelection();
				self.renderPropertyPanel();
				return;
			}

			self.selectedTokenId = token.getAttribute('data-token-id');
			self.applySelection();
			self.renderPropertyPanel();
			// Only steal focus for the canvas when we are NOT inside an
			// active literal editor — otherwise the input loses focus and
			// the focusout handler commits the edit prematurely.
			if (!insideLiteralEditor) {
				self.canvas.focus();
			}

			// Announce token selection to screen reader
			self.announceTokenToScreenReader(self.selectedTokenId);

			if (hitLiteralValue || (isLiteralToken && token === hitToken && event.target === hitToken)) {
				self.startLiteralEdit(self.selectedTokenId);
			}
		});

		this.root.addEventListener('keydown', function (event) {
			var input = closest(event.target, '.fd-literal-editor');
			if (!input) {
				return;
			}

			if (event.key === 'Enter') {
				event.preventDefault();
				self.commitLiteralEdit();
			} else if (event.key === 'Escape') {
				event.preventDefault();
				self.cancelLiteralEdit();
			}
		}, true);

		this.root.addEventListener('focusout', function (event) {
			if (closest(event.target, '.fd-literal-editor')) {
				self.commitLiteralEdit();
			}
		}, true);

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

	DesignerInstance.prototype.bindTextarea = function () {
		var self = this;

		if (!this.textarea) {
			return;
		}

		this.textarea.addEventListener('input', function () {
			if (self.skipTextareaInput) {
				return;
			}

			self.queueTextareaParse();
			self.scheduleValidation('textarea');
		});
	};

	DesignerInstance.prototype.bindForm = function () {
		var self = this;

		if (!this.form) {
			return;
		}

		this.form.addEventListener('click', function (event) {
			var submitter = closest(event.target, 'button[type="submit"], input[type="submit"]');
			if (submitter) {
				self.pendingSubmitter = submitter;
			}
		});

		this.form.addEventListener('submit', function (event) {
			var currentFormula;

			if (self.allowSubmitOnce) {
				self.allowSubmitOnce = false;
				return;
			}

			self.flushTextareaSync('submit');
			currentFormula = self.getFormulaText();

			if (self.parseError === null && self.lastValidatedFormula === currentFormula && self.validationState.isValid) {
				return;
			}

			event.preventDefault();
			self.pendingSubmitter = event.submitter || self.pendingSubmitter;
			self.runValidation(true, function (validationState) {
				if (self.parseError === null && validationState.isValid) {
					self.allowSubmitOnce = true;
					if (typeof self.form.requestSubmit === 'function') {
						self.form.requestSubmit(self.pendingSubmitter || undefined);
						return;
					}

					if (self.pendingSubmitter && typeof self.pendingSubmitter.click === 'function') {
						self.pendingSubmitter.click();
						return;
					}

					self.form.submit();
					return;
				}

				window.alert(self.root.getAttribute('data-submit-block-message') || 'Fix validation errors before saving this formula.');
			});
		});
	};

	DesignerInstance.prototype.bindKeyboard = function () {
		var self = this;

		this.root.addEventListener('keydown', function (event) {
			var target = event.target;
			var operatorKey = event.key;

			// Escape — cancel literal edit, close panels, dismiss modals
			if (event.key === 'Escape') {
				var activePanel = self.root.querySelector('.fd-palette-panel:not([hidden]), .fd-template-browser:not([hidden])');
				var activeLiteral = self.root.querySelector('.fd-literal-editor');

				if (activeLiteral) {
					event.preventDefault();
					self.cancelLiteralEdit();
					return;
				}

				if (self.activeLiteralEditorId) {
					event.preventDefault();
					self.cancelLiteralEdit();
					return;
				}

				if (activePanel) {
					event.preventDefault();
					activePanel.hidden = true;
					self.canvas.focus();
					return;
				}

				if (self.selectedTokenId) {
					event.preventDefault();
					self.selectedTokenId = null;
					self.applySelection();
					self.renderPropertyPanel();
					self.canvas.focus();
					return;
				}

				// Allow Escape to bubble for modal close
				return;
			}

			if (target && (target.tagName === 'TEXTAREA' || target.tagName === 'INPUT' || target.isContentEditable)) {
				return;
			}

			// Ctrl+Shift shortcuts
			if (event.ctrlKey || event.metaKey) {
				// Ctrl+S — trigger save (submit form via pre-submit validation flow)
				if (event.key === 's' || event.key === 'S') {
					if (event.shiftKey) {
						return; // Allow Ctrl+Shift+S (browser save as)
					}
					event.preventDefault();
					self.flushTextareaSync('keyboard-save');
					if (self.form && typeof self.form.requestSubmit === 'function') {
						self.form.requestSubmit();
					} else if (self.form) {
						self.pendingSubmitter = null;
						self.allowSubmitOnce = true;
						self.form.submit();
					}
					return;
				}

				// Ctrl+Shift+V — trigger validation
				if (event.key === 'V' && event.shiftKey) {
					event.preventDefault();
					self.runValidation(true);
					return;
				}

				// Ctrl+Shift+F — focus function search
				if (event.key === 'F' && event.shiftKey) {
					event.preventDefault();
					self.focusPaletteSearch('function');
					return;
				}

				// Ctrl+Shift+B — focus field/variable search
				if (event.key === 'B' && event.shiftKey) {
					event.preventDefault();
					self.focusPaletteSearch('field');
					return;
				}

				// Ctrl+Z / Ctrl+Y / Ctrl+C / Ctrl+X / Ctrl+V
				if (event.key === 'z' || event.key === 'Z') {
					event.preventDefault();
					if (event.shiftKey) {
						self.performRedo();
					} else {
						self.performUndo();
					}
					return;
				}

				if (event.key === 'y' || event.key === 'Y') {
					event.preventDefault();
					self.performRedo();
					return;
				}

				if (event.key === 'c' || event.key === 'C') {
					event.preventDefault();
					self.copySelection();
					return;
				}

				if (event.key === 'x' || event.key === 'X') {
					event.preventDefault();
					self.cutSelection();
					return;
				}

				if (event.key === 'v' || event.key === 'V') {
					event.preventDefault();
					self.pasteAtSelection();
					return;
				}

				return;
			}

			// Alt+Left — return focus to module page
			if (event.altKey && event.key === 'ArrowLeft') {
				var moduleTrigger = document.querySelector('.fd-modal-trigger-btn');
				if (moduleTrigger) {
					event.preventDefault();
					moduleTrigger.focus();
				}
				return;
			}

			// Arrow keys — navigate between tokens on canvas
			if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
				if (target && target.closest && target.closest('.fd-expression')) {
					event.preventDefault();
					self.navigateToolbarTokens(event.key === 'ArrowRight' ? 1 : -1);
					return;
				}
				var focusedToken = document.activeElement && document.activeElement.closest && document.activeElement.closest('.fd-token');
				if (focusedToken && focusedToken.closest('.fd-expression')) {
					event.preventDefault();
					self.navigateToolbarTokens(event.key === 'ArrowRight' ? 1 : -1);
					return;
				}
			}

			// Enter / Space on focused token — select it
			if ((event.key === 'Enter' || event.key === ' ') && target) {
				var activeToken = target.closest ? target.closest('.fd-token') : null;
				if (activeToken && activeToken.closest('.fd-expression')) {
					event.preventDefault();
					var tid = activeToken.getAttribute('data-token-id');
					self.selectedTokenId = tid;
					self.applySelection();
					self.renderPropertyPanel();
					self.canvas.focus();
					return;
				}
			}

			// Alt modifier shortcuts — ignore remaining
			if (event.altKey) {
				return;
			}

			if ((event.key === 'Delete' || event.key === 'Backspace') && self.selectedTokenId) {
				event.preventDefault();
				self.deleteSelectedToken();
				return;
			}

			// Direct number entry: insert or replace literal when user types a digit
			if (/^[0-9.]$/.test(event.key)) {
				event.preventDefault();
				self.handleNumberEntry(event.key);
				return;
			}

			if (operatorKey === '+' || operatorKey === '-' || operatorKey === '*' || operatorKey === '/') {
				event.preventDefault();
				self.insertOperator(operatorKey);
			}
		});

		// Tab navigation — manage focus between tokens in the expression
		this.canvas.addEventListener('keydown', function (event) {
			if (event.key === 'Tab' && !event.ctrlKey && !event.altKey) {
				event.preventDefault();
				var direction = event.shiftKey ? -1 : 1;
				self.navigateToolbarTokens(direction);
			}
		});
	};

	/**
	 * Navigate focus between expression tokens.
	 *
	 * Moves focus to the next or previous token. If no token is currently
	 * focused, focuses the first (or last) token in the expression.
	 *
	 * @param {number} direction  1 for next, -1 for previous
	 * @return {void}
	 */
	DesignerInstance.prototype.navigateToolbarTokens = function (direction) {
		var tokens = this.expressionNode.querySelectorAll('.fd-token');
		var tokenCount = tokens.length;
		var activeIndex = -1;
		var newIndex;
		var index;
		var focused;

		if (!tokenCount) {
			this.canvas.focus();
			return;
		}

		// Find currently focused token
		focused = document.activeElement;
		for (index = 0; index < tokenCount; index += 1) {
			tokens[index].setAttribute('tabindex', '-1');
			if (focused && tokens[index] === focused) {
				activeIndex = index;
			}
		}

		// Compute target index
		if (activeIndex === -1) {
			if (direction >= 0) {
				newIndex = 0;
			} else {
				newIndex = tokenCount - 1;
			}
		} else {
			newIndex = activeIndex + direction;
			if (newIndex < 0) {
				newIndex = tokenCount - 1;
			}
			if (newIndex >= tokenCount) {
				newIndex = 0;
			}
		}

		// Apply focus + select
		tokens[newIndex].setAttribute('tabindex', '0');
		tokens[newIndex].focus();
		this.selectedTokenId = tokens[newIndex].getAttribute('data-token-id');
		this.applySelection();
		this.renderPropertyPanel();
	};

	/**
	 * Focus a palette search input by type.
	 *
	 * @param {'field'|'function'} paletteType
	 * @return {void}
	 */
	DesignerInstance.prototype.focusPaletteSearch = function (paletteType) {
		var selector = paletteType === 'field'
			? '[data-designer="field-palette"] .fd-palette-search'
			: '[data-designer="function-palette"] .fd-palette-search';
		var input = this.root.querySelector(selector);

		if (input) {
			input.focus();
			input.select();
		}
	};

	/**
	 * Announce a message to screen readers via an aria-live region.
	 *
	 * Creates a temporary live region if a permanent one is not available,
	 * sets the message, and removes it after a short delay.
	 *
	 * @param {string} message  The message to announce
	 * @param {'polite'|'assertive'} [priority='polite']
	 * @return {void}
	 */
	DesignerInstance.prototype.announceToScreenReader = function (message, priority) {
		var liveRegion = this.root.querySelector('[data-designer="live-region"]');
		var tempRegion;
		var clearTimer;

		priority = priority || 'polite';

		if (!liveRegion) {
			tempRegion = document.createElement('div');
			tempRegion.setAttribute('data-designer', 'live-region');
			tempRegion.setAttribute('aria-live', priority);
			tempRegion.setAttribute('aria-atomic', 'true');
			tempRegion.className = 'fd-sr-only';
			this.root.appendChild(tempRegion);
			liveRegion = tempRegion;
		}

		// Clear to trigger re-announcement even for identical messages
		liveRegion.textContent = '';
		clearTimer = setTimeout(function () {
			liveRegion.textContent = message;
		}, 50);

		// Clean up temp region
		if (tempRegion) {
			setTimeout(function () {
				if (tempRegion.parentNode) {
					tempRegion.parentNode.removeChild(tempRegion);
				}
			}, 5000);
		}
	};

	/**
	 * Create a focus trap within a container element.
	 *
	 * Traps Tab/Shift+Tab within the container so that keyboard focus
	 * cannot escape into the background page.
	 *
	 * @param {Element} container  The container element to trap focus in
	 * @return {Function}  A cleanup function to remove the trap
	 */
	DesignerInstance.prototype.createFocusTrap = function (container) {
		var self = this;
		var focusableSelector = 'a[href], button:not([disabled]), [tabindex]:not([tabindex="-1"]), '
			+ 'input:not([disabled]), select:not([disabled]), textarea:not([disabled]), '
			+ '[contenteditable="true"]';

		function getFocusableNodes() {
			var nodes = container.querySelectorAll(focusableSelector);
			var result = [];
			var index;
			for (index = 0; index < nodes.length; index += 1) {
				if (nodes[index].offsetParent !== null) {
					result.push(nodes[index]);
				}
			}
			return result;
		}

		function handleTrap(event) {
			if (event.key !== 'Tab') {
				return;
			}

			if (!container.contains(document.activeElement) && !container.isSameNode(document.activeElement)) {
				return;
			}

			var nodes = getFocusableNodes();
			if (!nodes.length) {
				event.preventDefault();
				return;
			}

			var first = nodes[0];
			var last = nodes[nodes.length - 1];

			if (event.shiftKey) {
				if (document.activeElement === first || !container.contains(document.activeElement)) {
					event.preventDefault();
					last.focus();
				}
			} else {
				if (document.activeElement === last || !container.contains(document.activeElement)) {
					event.preventDefault();
					first.focus();
				}
			}
		}

		document.addEventListener('keydown', handleTrap);
		return function cleanup() {
			document.removeEventListener('keydown', handleTrap);
		};
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

	DesignerInstance.prototype.replaceTokens = function (tokens, options) {
		var normalized = normalizeExpression({
			id: this.expression.id,
			type: this.expression.type,
			tokens: tokens
		});

		var previousTokens = this.expression.tokens;

		this.expression = normalized;

		if (options && typeof options.selectedTokenId !== 'undefined') {
			this.selectedTokenId = options.selectedTokenId;
		} else if (this.selectedTokenId && !this.findTokenById(this.selectedTokenId)) {
			this.selectedTokenId = null;
		}

		// auto-snapshot for undo (unless suppressed)
		if (options && options.source && !this.disableUndoSnapshot) {
			this.snapshotForUndo(options.source, options.description || options.source, previousTokens, normalized.tokens, { selectionId: this.selectedTokenId });
		}

		this.render();
		this.scheduleTextareaSync(options && options.source ? options.source : 'canvas');
		this.scheduleValidation(options && options.source ? options.source : 'canvas');
		this.syncUndoRedoButtons();
	};

	/**
	 * Record an undo snapshot for the current mutation.
	 */
	DesignerInstance.prototype.snapshotForUndo = function (type, description, reverseTokens, forwardTokens, options) {
		if (!this.undoManager) {
			return;
		}

		this.undoManager.push(type, description, reverseTokens, forwardTokens, options);
	};

	/**
	 * Undo — revert to the previous token snapshot.
	 */
	DesignerInstance.prototype.performUndo = function () {
		var currentTokens = this.expression.tokens;
		var result;

		if (!this.undoManager || !this.undoManager.canUndo()) {
			return;
		}

		this.disableUndoSnapshot = true;
		result = this.undoManager.undo(currentTokens);

		if (result && result.tokens) {
			this.replaceTokens(result.tokens, {
				source: 'undo',
				description: 'Undo',
				selectedTokenId: result.selectionId || null
			});
		}
		this.disableUndoSnapshot = false;
	};

	/**
	 * Redo — restore the previously undone mutation.
	 */
	DesignerInstance.prototype.performRedo = function () {
		var currentTokens = this.expression.tokens;
		var result;

		if (!this.undoManager || !this.undoManager.canRedo()) {
			return;
		}

		this.disableUndoSnapshot = true;
		result = this.undoManager.redo(currentTokens);

		if (result && result.tokens) {
			this.replaceTokens(result.tokens, {
				source: 'redo',
				description: 'Redo',
				selectedTokenId: result.selectionId || null
			});
		}
		this.disableUndoSnapshot = false;
	};

	/**
	 * Copy selected token(s) to internal clipboard.
	 */
	DesignerInstance.prototype.copySelection = function () {
		var tokens = this.expression.tokens;
		var index;
		var token;

		if (!this.selectedTokenId || !this.clipboard) {
			return;
		}

		index = this.findTokenIndex(this.selectedTokenId);
		if (index === -1) {
			return;
		}

		token = tokens[index];

		// Copy function + paired closing group token together
		if (token.type === 'function' && tokens[index + 1] && tokens[index + 1].type === 'group' && tokens[index + 1].metadata && tokens[index + 1].metadata.generatedBy === token.id) {
			this.clipboard.copy([token, tokens[index + 1]], index);
			return;
		}

		this.clipboard.copy([token], index);
	};

	/**
	 * Cut selected token(s) to internal clipboard, removing them.
	 */
	DesignerInstance.prototype.cutSelection = function () {
		var tokens = this.getTokens();
		var index;
		var token;
		var ids;

		if (!this.selectedTokenId || !this.clipboard) {
			return;
		}

		index = this.findTokenIndex(this.selectedTokenId);
		if (index === -1) {
			return;
		}

		token = tokens[index];
		ids = [];

		if (token.type === 'function' && tokens[index + 1] && tokens[index + 1].type === 'group' && tokens[index + 1].metadata && tokens[index + 1].metadata.generatedBy === token.id) {
			ids = [token.id, tokens[index + 1].id];
			this.clipboard.cut([token, tokens[index + 1]], index);
			tokens.splice(index, 2);
		} else {
			ids = [token.id];
			this.clipboard.cut([token], index);
			tokens.splice(index, 1);
		}

		this.replaceTokens(tokens, {
			source: 'cut',
			description: 'Cut',
			selectedTokenId: tokens[index] ? tokens[index].id : (tokens[index - 1] ? tokens[index - 1].id : null)
		});
	};

	/**
	 * Paste clipboard content at the current selection or cursor position.
	 */
	DesignerInstance.prototype.pasteAtSelection = function () {
		var tokens = this.getTokens();
		var pasteTokens;
		var insertIndex;

		if (!this.clipboard || !this.clipboard.hasContent()) {
			return;
		}

		pasteTokens = this.clipboard.getPasteTokens();
		if (!pasteTokens || !pasteTokens.length) {
			return;
		}

		insertIndex = this.selectedTokenId ? this.findTokenIndex(this.selectedTokenId) + 1 : tokens.length;

		pasteTokens.forEach(function (token, offset) {
			tokens.splice(insertIndex + offset, 0, token);
		});

		this.replaceTokens(tokens, {
			source: 'paste',
			description: 'Paste (' + pasteTokens.length + ' token' + (pasteTokens.length !== 1 ? 's' : '') + ')',
			selectedTokenId: pasteTokens[0].id
		});
	};

	/**
	 * Synchronize undo/redo toolbar button enabled states.
	 */
	DesignerInstance.prototype.syncUndoRedoButtons = function () {
		var undoButton = this.root.querySelector('[data-action="undo"]');
		var redoButton = this.root.querySelector('[data-action="redo"]');

		if (undoButton) {
			if (this.undoManager && this.undoManager.canUndo()) {
				undoButton.removeAttribute('disabled');
				undoButton.setAttribute('title', 'Undo — ' + escapeHtml(this.undoManager.getLastActionLabel()));
			} else {
				undoButton.setAttribute('disabled', 'disabled');
				undoButton.setAttribute('title', 'Undo');
			}
		}

		if (redoButton) {
			if (this.undoManager && this.undoManager.canRedo()) {
				redoButton.removeAttribute('disabled');
				redoButton.setAttribute('title', 'Redo');
			} else {
				redoButton.setAttribute('disabled', 'disabled');
				redoButton.setAttribute('title', 'Redo');
			}
		}
	};

	DesignerInstance.prototype.findTokenById = function (tokenId) {
		var index;
		for (index = 0; index < this.expression.tokens.length; index += 1) {
			if (this.expression.tokens[index].id === tokenId) {
				return this.expression.tokens[index];
			}
		}

		return null;
	};

	DesignerInstance.prototype.findTokenIndex = function (tokenId) {
		var index;
		for (index = 0; index < this.expression.tokens.length; index += 1) {
			if (this.expression.tokens[index].id === tokenId) {
				return index;
			}
		}

		return -1;
	};

	DesignerInstance.prototype.getFormulaText = function () {
		if (this.textarea) {
			return this.textarea.value || '';
		}

		return this.lastSerialized.formula || '';
	};

	DesignerInstance.prototype.initializeFormulaSource = function () {
		if (!this.textarea) {
			return;
		}

		if (!this.textarea.value && this.lastSerialized.formula) {
			this.skipTextareaInput = true;
			this.textarea.value = this.lastSerialized.formula;
			this.skipTextareaInput = false;
		}

		this.lastSerialized = serializeTokens(this.expression.tokens);
		if (this.textarea.value) {
			this.lastSerialized.formula = this.textarea.value;
		}

		if (this.getFormulaText()) {
			this.scheduleValidation('initial');
		}
	};

	DesignerInstance.prototype.scheduleTextareaSync = function (source) {
		var self = this;

		if (this.syncTimer) {
			window.clearTimeout(this.syncTimer);
		}

		this.syncTimer = window.setTimeout(function () {
			self.flushTextareaSync(source);
		}, TEXT_SYNC_DEBOUNCE_MS);
	};

	DesignerInstance.prototype.flushTextareaSync = function (source) {
		this.lastSerialized = serializeTokens(this.expression.tokens);

		if (this.textarea) {
			this.skipTextareaInput = true;
			this.textarea.value = this.lastSerialized.formula;
			this.skipTextareaInput = false;
		}

		dispatchDesignerEvent(this.root, 'fd:textareasynced', {
			formula: this.lastSerialized.formula,
			source: source || 'canvas'
		});

		if (this.syncTimer) {
			window.clearTimeout(this.syncTimer);
			this.syncTimer = null;
		}

		return this.lastSerialized;
	};

	DesignerInstance.prototype.queueTextareaParse = function () {
		var self = this;

		if (this.textareaParseTimer) {
			window.clearTimeout(this.textareaParseTimer);
		}

		this.textareaParseTimer = window.setTimeout(function () {
			self.textareaParseTimer = null;
			self.parseTextareaValue();
		}, TEXTAREA_PARSE_DEBOUNCE_MS);
	};

	DesignerInstance.prototype.parseTextareaValue = function () {
		var formula = this.getFormulaText();
		var tokens;
		var sequenceSerialized;

		if (formula.replace(/\s+/g, '') === '') {
			this.applyParseError(null);
			this.replaceTokens([], { source: 'textarea', description: 'Clear formula', selectedTokenId: null });
			return;
		}

		try {
			tokens = tokenizeFormula(formula);
		} catch (error) {
			this.applyParseError({
				message: error && error.message ? error.message : 'Formula text could not be parsed.',
				line: 1,
				column: 1
			});
			return;
		}

		if (!isTokenSequenceValid(tokens)) {
			sequenceSerialized = serializeTokens(tokens);
			this.applyParseError({
				message: 'Formula text is incomplete or cannot be rendered yet.',
				line: 1,
				column: Math.max(sequenceSerialized.formula.length, 1)
			});
			return;
		}

		this.applyParseError(null);
		this.replaceTokens(tokens, { source: 'textarea', description: 'Edit formula text', selectedTokenId: null });
	};

	DesignerInstance.prototype.applyParseError = function (error) {
		this.parseError = error || null;
		if (error) {
			this.errorPanelDismissed = false;
		}
		this.applyValidationPresentation();
	};

	DesignerInstance.prototype.scheduleValidation = function (source) {
		var self = this;

		if (this.validationTimer) {
			window.clearTimeout(this.validationTimer);
		}

		this.validationTimer = window.setTimeout(function () {
			self.runValidation(false, null, source);
		}, VALIDATION_DEBOUNCE_MS);
	};

	DesignerInstance.prototype.buildValidationRequestData = function (formula) {
		var payload = {
			formula: formula,
			module: this.root.getAttribute('data-module') || ''
		};
		var csrfInput = this.form ? this.form.querySelector('input[name="_token"]') : null;

		if (csrfInput && csrfInput.value) {
			payload._token = csrfInput.value;
		}

		return payload;
	};

	DesignerInstance.prototype.runValidation = function (immediate, callback) {
		var self = this;
		var formula;

		if (this.validationTimer) {
			window.clearTimeout(this.validationTimer);
			this.validationTimer = null;
		}

		if (immediate) {
			this.flushTextareaSync('validate');
		}

		formula = this.getFormulaText();
		if (!this.root.getAttribute('data-validate-api-url') || !window.jQuery || typeof window.jQuery.ajax !== 'function') {
			this.validationState = createEmptyValidationState(formula);
			this.lastValidatedFormula = formula;
			this.applyValidationPresentation();
			if (typeof callback === 'function') {
				callback(this.validationState);
			}
			return;
		}

		if (this.validationRequest && typeof this.validationRequest.abort === 'function') {
			this.validationRequest.abort();
		}

		if (this.validateButton) {
			this.validateButton.classList.add('is-busy');
		}

		this.validationRequest = window.jQuery.ajax({
			url: this.root.getAttribute('data-validate-api-url'),
			method: 'POST',
			dataType: 'json',
			data: this.buildValidationRequestData(formula)
		}).done(function (payload) {
			self.validationState = normalizeValidationState(formula, payload || {});
			self.lastValidatedFormula = formula;
			self.applyValidationPresentation();
			if (typeof callback === 'function') {
				callback(self.validationState);
			}
		}).fail(function (xhr) {
			if (xhr && xhr.statusText === 'abort') {
				return;
			}

			self.validationState = normalizeValidationState(formula, {
				isValid: false,
				errors: [{
					message: 'Validation request failed. Try again.',
					line: 0,
					column: 0
				}],
				warnings: []
			});
			self.lastValidatedFormula = formula;
			self.applyValidationPresentation();
			if (typeof callback === 'function') {
				callback(self.validationState);
			}
		}).always(function () {
			self.validationRequest = null;
			if (self.validateButton) {
				self.validateButton.classList.remove('is-busy');
			}
		});
	};

	DesignerInstance.prototype.applyValidationPresentation = function () {
		var serialized = this.lastSerialized || serializeTokens(this.expression.tokens);
		var errors = (this.validationState && this.validationState.errors ? this.validationState.errors.slice() : []);
		var warnings = this.validationState && this.validationState.warnings ? this.validationState.warnings : [];
		var errorCountNodes = this.validationCountNodes;
		var errorTokenIds;
		var warningTokenIds;
		var signature;
		var shouldShowPanel;
		var index;

		if (this.parseError) {
			errors.unshift(this.parseError);
		}

		errorTokenIds = mapFindingsToTokenIds(serialized, errors);
		warningTokenIds = mapFindingsToTokenIds(serialized, warnings);
		signature = JSON.stringify({
			errors: errors,
			warnings: warnings
		});

		if (errors.length > 0 && signature !== this.lastErrorSignature) {
			this.errorPanelDismissed = false;
		}
		this.lastErrorSignature = signature;

		for (index = 0; index < errorCountNodes.length; index += 1) {
			errorCountNodes[index].textContent = String(errors.length);
		}

		if (this.canvas) {
			this.canvas.classList.toggle('fd-canvas--invalid', errors.length > 0);
		}

		this.applyTokenValidationClasses(errorTokenIds, warningTokenIds);
		this.renderErrorList(errors, warnings);

		shouldShowPanel = errors.length > 0 && !this.errorPanelDismissed;
		if (this.errorPanel) {
			this.errorPanel.hidden = !shouldShowPanel;
		}

		// Announce validation result to screen reader via aria-live
		if (errors.length > 0) {
			this.announceToScreenReader(
				errors.length + ' validation error' + (errors.length !== 1 ? 's' : '') + '. ' +
				errors.length + ' error' + (errors.length !== 1 ? 's' : '') + ' found.',
				'assertive'
			);
		} else if (warnings.length > 0) {
			this.announceToScreenReader(
				'Validation passed with ' + warnings.length + ' warning' + (warnings.length !== 1 ? 's' : '') + '.',
				'polite'
			);
		}
	};

	DesignerInstance.prototype.applyTokenValidationClasses = function (errorTokenIds, warningTokenIds) {
		var tokens = this.expressionNode.querySelectorAll('.fd-token');
		var index;
		var tokenId;

		for (index = 0; index < tokens.length; index += 1) {
			tokenId = tokens[index].getAttribute('data-token-id');
			tokens[index].classList.toggle('fd-token--error', !!errorTokenIds[tokenId]);
			tokens[index].classList.toggle('fd-token--warning', !!warningTokenIds[tokenId]);
		}
	};

	DesignerInstance.prototype.renderErrorList = function (errors, warnings) {
		var html = '';

		if (!this.errorListNode) {
			return;
		}

		errors.forEach(function (error) {
			html += '<li class="fd-error-item fd-error-item--error">';
			html += '<span class="fd-error-location">Line ' + escapeHtml(String(error.line || 1)) + ', Col ' + escapeHtml(String(error.column || 1)) + '</span>';
			html += '<span class="fd-error-message">' + escapeHtml(String(error.message || 'Validation error.')) + '</span>';
			html += '</li>';
		});

		warnings.forEach(function (warning) {
			html += '<li class="fd-error-item fd-error-item--warning">';
			html += '<span class="fd-error-location">Line ' + escapeHtml(String(warning.line || 1)) + ', Col ' + escapeHtml(String(warning.column || 1)) + '</span>';
			html += '<span class="fd-error-message">' + escapeHtml(String(warning.message || 'Warning.')) + '</span>';
			html += '</li>';
		});

		this.errorListNode.innerHTML = html;
	};

	DesignerInstance.prototype.startLiteralEdit = function (tokenId, selectAll) {
		var token = this.findTokenById(tokenId);

		if (!token || token.type !== 'literal') {
			return;
		}

		if (this.activeLiteralEditorId && this.activeLiteralEditorId !== tokenId) {
			this.commitLiteralEdit();
		}

		token.metadata = token.metadata || {};
		token.metadata.editing = true;
		token.metadata.editingValue = selectAll ? '' : getEditableLiteralValue(token);
		this.activeLiteralEditorId = tokenId;
		this.render();
	};

	DesignerInstance.prototype.commitLiteralEdit = function () {
		var tokenId = this.activeLiteralEditorId;
		var input = this.root.querySelector('.fd-literal-editor');
		var value = input ? input.value : null;
		var tokens;
		var index;
		var updated;

		if (!tokenId) {
			return;
		}

		index = this.findTokenIndex(tokenId);
		if (index === -1) {
			this.activeLiteralEditorId = null;
			return;
		}

		tokens = this.getTokens();
		updated = tokens[index];
		updated.metadata = updated.metadata || {};

		if (value !== null) {
			this.applyLiteralValue(updated, value);
		}

		delete updated.metadata.editing;
		delete updated.metadata.editingValue;
		this.activeLiteralEditorId = null;
		this.replaceTokens(tokens, { source: 'literal-edit', description: 'Edit literal', selectedTokenId: tokenId });
	};

	DesignerInstance.prototype.cancelLiteralEdit = function () {
		var token = this.findTokenById(this.activeLiteralEditorId);

		if (token && token.metadata) {
			delete token.metadata.editing;
			delete token.metadata.editingValue;
		}

		this.activeLiteralEditorId = null;
		this.render();
	};

	DesignerInstance.prototype.applyLiteralValue = function (token, value) {
		var metadata = token.metadata && typeof token.metadata === 'object' ? token.metadata : {};
		var dataType = String(metadata.dataType || 'number').toLowerCase();
		var trimmed = String(value || '');
		var numericValue;

		if (dataType === 'string') {
			metadata.rawValue = trimmed;
			metadata.quote = metadata.quote || '"';
			token.label = trimmed;
			token.value = metadata.quote + trimmed + metadata.quote;
			token.metadata = metadata;
			return;
		}

		numericValue = /^-?\d+$/.test(trimmed) ? parseInt(trimmed, 10) : parseFloat(trimmed);
		if (!isNaN(numericValue)) {
			metadata.rawValue = numericValue;
		}
		metadata.dataType = dataType || 'number';
		token.label = trimmed;
		token.value = trimmed;
		token.metadata = metadata;
	};

	DesignerInstance.prototype.deleteSelectedToken = function () {
		var tokens = this.getTokens();
		var index = this.findTokenIndex(this.selectedTokenId);
		var token;
		var nextToken;
		var previousToken;
		var nextSelectionId = null;

		if (index === -1) {
			return;
		}

		token = tokens[index];
		nextToken = tokens[index + 1];
		previousToken = tokens[index - 1];

		if (token.type === 'function' && nextToken && nextToken.type === 'group' && nextToken.metadata && nextToken.metadata.generatedBy === token.id) {
			tokens.splice(index, 2);
		} else if (token.type === 'group' && token.metadata && token.metadata.generatedBy && previousToken && previousToken.id === token.metadata.generatedBy) {
			tokens.splice(index - 1, 2);
			index -= 1;
		} else {
			tokens.splice(index, 1);
		}

		if (tokens[index]) {
			nextSelectionId = tokens[index].id;
		} else if (tokens[index - 1]) {
			nextSelectionId = tokens[index - 1].id;
		}

		this.replaceTokens(tokens, { source: 'delete', description: 'Delete', selectedTokenId: nextSelectionId });
	};

	DesignerInstance.prototype.insertOperator = function (operatorValue) {
		var tokens = this.getTokens();
		var index = this.selectedTokenId ? this.findTokenIndex(this.selectedTokenId) + 1 : tokens.length;
		var operatorToken = buildOperatorToken(operatorValue);
		var previousToken = tokens[index - 1];

		operatorToken.metadata = operatorToken.metadata || {};
		if ((operatorValue === '+' || operatorValue === '-') && (!previousToken || isOperatorToken(previousToken) || isOpenGroupToken(previousToken) || isFunctionToken(previousToken))) {
			operatorToken.metadata.operatorKind = 'unary';
		}

		tokens.splice(index, 0, operatorToken);
		this.replaceTokens(tokens, { source: 'keyboard', description: 'Insert operator', selectedTokenId: operatorToken.id });
	};

	/**
	 * Handle direct number entry via keyboard.
	 *
	 * When the user types a digit (0-9) or decimal point while the canvas
	 * is focused, this method either:
	 *  - Starts an inline edit on a selected literal token (appending), or
	 *  - Creates a new literal number token and starts editing it inline
	 *
	 * @param {string} digit  The character typed (0-9 or .)
	 * @return {void}
	 */
	DesignerInstance.prototype.handleNumberEntry = function (digit) {
		var tokens = this.getTokens();
		var token = this.selectedTokenId ? this.findTokenById(this.selectedTokenId) : null;
		var index;
		var newToken;
		var editingToken;

		// If a literal is selected and not already editing, start editing with the typed digit
		if (token && token.type === 'literal' && token.metadata && token.metadata.dataType === 'number') {
			index = this.findTokenIndex(this.selectedTokenId);
			// Start editing with just the digit (replacing the current value)
			this.selectedTokenId = token.id;
			this.startLiteralEdit(token.id, true); // true = edit mode
			// Queue the digit insertion after the editor renders
			var self = this;
			var char = digit;
			window.setTimeout(function () {
				self.queueLiteralEditorInsert(char);
			}, 50);
			return;
		}

		// If a literal is already being edited, insert the digit
		if (token && token.type === 'literal' && token.metadata && token.metadata.editing) {
			this.queueLiteralEditorInsert(digit);
			return;
		}

		// Create a new literal number token
		index = this.selectedTokenId ? this.findTokenIndex(this.selectedTokenId) + 1 : tokens.length;
		newToken = normalizeToken({
			id: nextTokenId('tok'),
			type: 'literal',
			value: digit,
			label: digit,
			metadata: { dataType: 'number', rawValue: parseFloat(digit), editing: true, editingValue: digit }
		});

		tokens.splice(index, 0, newToken);
		this.replaceTokens(tokens, { source: 'keyboard', description: 'Insert number literal', selectedTokenId: newToken.id });

		// Immediately start inline editing
		var self2 = this;
		window.setTimeout(function () {
			self2.startLiteralEdit(newToken.id);
		}, 50);
	};

	/**
	 * Insert a character into an active literal editor input.
	 *
	 * @param {string} character  Single character to insert at cursor position
	 * @return {void}
	 */
	DesignerInstance.prototype.queueLiteralEditorInsert = function (character) {
		var input = this.expressionNode.querySelector('.fd-literal-editor');
		if (!input) {
			return;
		}

		var start = input.selectionStart || 0;
		var end = input.selectionEnd || 0;
		var value = input.value;
		input.value = value.substring(0, start) + character + value.substring(end);
		var newPos = start + character.length;
		input.setSelectionRange(newPos, newPos);
		input.focus();
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

	/**
	 * Announce token selection to screen readers.
	 *
	 * Uses the ARIA label from the token element to build a descriptive
	 * announcement for assistive technologies.
	 *
	 * @param {string} tokenId  The ID of the selected token
	 * @return {void}
	 */
	DesignerInstance.prototype.announceTokenToScreenReader = function (tokenId) {
		var tokenEl = this.expressionNode.querySelector('[data-token-id="' + tokenId + '"]');
		var label;

		if (!tokenEl) {
			return;
		}

		label = tokenEl.getAttribute('aria-label') || '';
		if (label) {
			this.announceToScreenReader('Selected: ' + label, 'polite');
		}
	};

	/**
	 * Render the expression canvas using DOM recycling.
	 *
	 * optimization — existing token DOM nodes are reused when
	 * the token array changes, avoiding full innerHTML replacement.  Only
	 * tokens that differ (by id, type, or position) are created or removed.
	 * This eliminates layout thrashing and reduces GC pressure for
	 * large formulas (100+ tokens).
	 *
	 * @return {void}
	 */
	DesignerInstance.prototype.render = function () {
		var self = this;

		if (this.renderQueued) {
			return;
		}

		this.renderQueued = true;
		window.requestAnimationFrame(function () {
			self.renderTokensPatch();
			self.renderQueued = false;
			self.applyZoom();
			self.applySelection();
			self.applyValidationPresentation();
			self.focusLiteralEditor();
			dispatchDesignerEvent(self.root, 'fd:canvasrendered', {
				expressionId: self.expression.id,
				tokenCount: self.expression.tokens.length
			});
		});
	};

	/**
	 * Patch-based token rendering using DOM recycling.
	 *
	 * Compares the current DOM child list with the desired token array
	 * and applies the minimal set of insertions, removals, and in-place
	 * updates needed.  Falls back to full innerHTML rebuild when the
	 * token count changes by more than DOM_RECYCLE_THRESHOLD (set to
	 * 50 to avoid N^2 diff for huge batches like template insertion).
	 *
	 * @return {void}
	 */
	DesignerInstance.prototype.renderTokensPatch = function () {
		var desiredTokens = this.expression.tokens;
		var children = this.expressionNode.children;
		var desiredLen = desiredTokens.length;
		var currentLen = children.length;
		var DOM_RECYCLE_THRESHOLD = 50;
		var index;
		var desired;
		var existing;
		var html;
		var fragment;
		var i;

		// Build a count of connectors + tokens from children.  The
		// DOM always interleaves: connector(0), token(0), connector(1),
		// token(1), ... token(N-1), connector(N).  So:
		//   extistingTokenCount = (children.length - 1) / 2
		var existingTokenCount = currentLen > 0 ? (currentLen - 1) / 2 : 0;

		// ----- Full rebuild: empty canvas, delta overload, or first render -----
		if (
			currentLen === 0
			|| desiredLen === 0
			|| existingTokenCount < 0
			|| Math.abs(desiredLen - existingTokenCount) > DOM_RECYCLE_THRESHOLD
		) {
			this.renderTokensFull(desiredTokens);
			return;
		}

		// ----- Fast path: token count unchanged, update in place -----
		if (desiredLen === existingTokenCount) {
			for (index = 0; index < desiredLen; index += 1) {
				desired = desiredTokens[index];

				// DOM position: each token at index maps to children[2*index + 1]
				existing = children[2 * index + 1];

				var existingId = existing ? existing.getAttribute('data-token-id') : null;
				var existingType = existing ? existing.getAttribute('data-token-type') : null;

				if (existingId === desired.id && existingType === desired.type) {
					// Same token — update label / DOM only if the editing mode
					// did NOT change (input ↔ span transition requires a full
					// node replacement because the inner structure differs).
					var wasEditing = existing.querySelector('.fd-literal-editor') !== null;
					var isEditing = desired.metadata && desired.metadata.editing === true;

					if (desired.type === 'literal' && wasEditing !== isEditing) {
						// Editing state changed — replace the whole node
						var replacement = this.createTokenElement(desired, index);
						if (replacement && existing.parentNode) {
							existing.parentNode.replaceChild(replacement, existing);
						}
					} else if (desired.type === 'literal' && !isEditing) {
						var labelNode = existing.querySelector('.fd-token-value');
						if (labelNode) {
							labelNode.textContent = desired.label || desired.value || '';
						}
					}
					// Update index attribute
					existing.setAttribute('data-token-index', index);
					existing.setAttribute('data-token-value', escapeAttribute(desired.value || ''));
				} else {
					// Different token — replace the DOM node
					var replacement = this.createTokenElement(desired, index);
					if (replacement && existing.parentNode) {
						existing.parentNode.replaceChild(replacement, existing);
					}
				}
			}

			// Re-apply selection (may have been dropped during replace)
			this.applySelection();
			return;
		}

		// ----- General case: token count changed, patch with fragment -----
		fragment = document.createDocumentFragment();
		fragment.appendChild(this.createConnectorElement(0));

		for (index = 0; index < desiredLen; index += 1) {
			desired = desiredTokens[index];
			if (index < existingTokenCount) {
				existing = children[2 * index + 1];
				var eId = existing ? existing.getAttribute('data-token-id') : null;
				var eType = existing ? existing.getAttribute('data-token-type') : null;

				if (eId === desired.id && eType === desired.type) {
					// Reuse existing DOM
					existing.setAttribute('data-token-index', index);
					fragment.appendChild(existing);
				} else {
					fragment.appendChild(this.createTokenElement(desired, index));
				}
			} else {
				fragment.appendChild(this.createTokenElement(desired, index));
			}

			fragment.appendChild(this.createConnectorElement(index + 1));
		}

		// Replace all children at once — single layout operation
		this.expressionNode.innerHTML = '';
		this.expressionNode.appendChild(fragment);
	};

	/**
	 * Full innerHTML-based render (fallback for large delta or first render).
	 *
	 * @param {Array} tokens
	 * @return {void}
	 */
	DesignerInstance.prototype.renderTokensFull = function (tokens) {
		var html = connectorMarkup(0);

		tokens.forEach(function (token, index) {
			html += tokenMarkup(token, index);
			html += connectorMarkup(index + 1);
		});

		this.expressionNode.innerHTML = html;
	};

	/**
	 * Create a single token DOM element from a token data object.
	 *
	 * Used during patched (non-full) renders to create new token nodes
	 * without rebuilding the entire canvas HTML.
	 *
	 * @param {Object}  token  Normalized token data
	 * @param {number}  index  Position within the expression
	 * @return {Element}
	 */
	DesignerInstance.prototype.createTokenElement = function (token, index) {
		var span = document.createElement('span');
		var classes = 'fd-token fd-token--' + token.type + ' fd-token-' + token.type;
		var label = token.label || token.value || '';
		var isEditing = token.type === 'literal' && token.metadata && token.metadata.editing === true;
		var ariaLabel = label;

		if (token.type === 'variable') {
			span.innerHTML = '<span class="fd-token-badge fd-token-badge--namespace fd-token-badge-namespace">'
				+ namespaceAbbreviation(token.metadata.namespace) + '</span>'
				+ '<span class="fd-token-label">' + escapeHtml(label) + '</span>';
			ariaLabel = 'Variable: ' + label;
		} else if (token.type === 'function') {
			span.innerHTML = '<span class="fd-token-prefix">fx</span>'
				+ '<span class="fd-token-label">' + escapeHtml(label) + '</span>'
				+ '<span class="fd-token-suffix">(</span>';
			ariaLabel = 'Function: ' + label;
		} else if (token.type === 'literal') {
			if (isEditing) {
				classes += ' fd-token--editing';
				span.innerHTML = '<input type="text" class="fd-literal-editor" value="'
					+ escapeAttribute(getEditableLiteralValue(token)) + '" aria-label="Edit literal value">';
			} else {
				span.innerHTML = '<span class="fd-token-value">' + escapeHtml(label) + '</span>';
			}
			ariaLabel = 'Literal: ' + label;
		} else if (token.type === 'operator') {
			span.innerHTML = '<span class="fd-token-symbol">' + escapeHtml(label || token.value) + '</span>';
			ariaLabel = 'Operator: ' + label;
		} else {
			span.innerHTML = '<span class="fd-token-symbol">' + escapeHtml(token.value || ')') + '</span>';
			ariaLabel = 'Group: ' + (token.value || ')');
		}

		span.className = classes;
		span.setAttribute('draggable', isEditing ? 'false' : 'true');
		span.setAttribute('data-token-id', token.id);
		span.setAttribute('data-token-index', index);
		span.setAttribute('data-token-type', token.type);
		span.setAttribute('data-token-value', token.value || '');
		span.setAttribute('tabindex', '-1');
		span.setAttribute('role', 'button');
		span.setAttribute('aria-label', ariaLabel);

		return span;
	};

	/**
	 * Create a connector drop-zone element.
	 *
	 * @param {number} position  Connector position index
	 * @return {Element}
	 */
	DesignerInstance.prototype.createConnectorElement = function (position) {
		var span = document.createElement('span');
		span.className = 'fd-connector';
		span.setAttribute('data-connector-position', position);
		span.setAttribute('data-designer', 'connector');
		span.setAttribute('aria-hidden', 'true');
		return span;
	};

	DesignerInstance.prototype.focusLiteralEditor = function () {
		var input;

		if (!this.activeLiteralEditorId) {
			return;
		}

		input = this.root.querySelector('.fd-literal-editor');
		if (!input) {
			return;
		}

		input.focus();
		input.select();
	};

	DesignerInstance.prototype.renderPropertyPanel = function () {
		var tokenId = this.selectedTokenId;
		var token;
		var html = '';

		if (!this.propertyPanel || !this.propertyPanelBody) {
			return;
		}

		if (!tokenId) {
			this.propertyPanel.hidden = true;
			this.propertyPanelBody.innerHTML = '<div class="fd-property-panel-empty">Select a token to view its properties</div>';
			return;
		}

		token = this.findTokenById(tokenId);
		if (!token) {
			this.propertyPanel.hidden = true;
			this.propertyPanelBody.innerHTML = '<div class="fd-property-panel-empty">Select a token to view its properties</div>';
			return;
		}

		html = buildPropertyPanelContent(token);
		this.propertyPanelBody.innerHTML = html;
		this.propertyPanel.hidden = false;

		// Bind property panel input listeners
		this.bindPropertyPanelInputs();
	};

	DesignerInstance.prototype.bindPropertyPanelInputs = function () {
		var self = this;
		var input = this.propertyPanelBody.querySelector('.fd-property-input');
		if (!input) {
			return;
		}

		input.addEventListener('keydown', function (event) {
			if (event.key === 'Enter') {
				event.preventDefault();
				self.commitPropertyPanelEdit(input);
			} else if (event.key === 'Escape') {
				event.preventDefault();
				self.cancelPropertyPanelEdit(input);
			}
		});

		input.addEventListener('blur', function () {
			self.commitPropertyPanelEdit(input);
		});

		input.focus();
		input.select();
	};

	DesignerInstance.prototype.commitPropertyPanelEdit = function (input) {
		var token = this.findTokenById(this.selectedTokenId);
		var newValue = input ? input.value : '';
		var tokens;
		var index;

		if (!token || token.type !== 'literal') {
			this.renderPropertyPanel();
			return;
		}

		index = this.findTokenIndex(this.selectedTokenId);
		if (index === -1) {
			this.renderPropertyPanel();
			return;
		}

		tokens = this.getTokens();
		this.applyLiteralValue(tokens[index], newValue);
		this.replaceTokens(tokens, { source: 'property-panel', description: 'Edit literal via properties', selectedTokenId: this.selectedTokenId });
	};

	DesignerInstance.prototype.cancelPropertyPanelEdit = function (input) {
		this.renderPropertyPanel();
	};

	/**
	 * Render the SVG dot-grid background on the canvas.
	 * Called on init, resize, and zoom changes.
	 *
	 * @return {void}
	 */
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

	function buildPropertyPanelContent(token) {
		var metadata = (token.metadata && typeof token.metadata === 'object') ? token.metadata : {};
		var typeLabel = escapeHtml(token.type.charAt(0).toUpperCase() + token.type.slice(1));
		var html = '';

		html += '<div class="fd-property-group">';
		html += '<div class="fd-property-group-header">General</div>';
		html += '<div class="fd-property-row">';
		html += '<span class="fd-property-label">Type</span>';
		html += '<span class="fd-property-badge fd-property-badge--type">' + typeLabel + '</span>';
		html += '</div>';
		html += '</div>';

		if (token.type === 'variable') {
			html += buildVariableProperties(metadata);
		} else if (token.type === 'function') {
			html += buildFunctionProperties(token, metadata);
		} else if (token.type === 'literal') {
			html += buildLiteralProperties(token, metadata);
		} else if (token.type === 'operator') {
			html += buildOperatorProperties(token, metadata);
		} else if (token.type === 'group') {
			html += buildGroupProperties(token, metadata);
		}

		if (metadata.description) {
			html += '<div class="fd-property-group">';
			html += '<div class="fd-property-group-header">Description</div>';
			html += '<p class="fd-property-description">' + escapeHtml(String(metadata.description)) + '</p>';
			html += '</div>';
		}

		if (metadata.permission) {
			html += '<div class="fd-property-group">';
			html += '<div class="fd-property-row">';
			html += '<span class="fd-property-label">Permission</span>';
			html += '<span class="fd-property-badge fd-property-badge--permission">' + escapeHtml(String(metadata.permission)) + '</span>';
			html += '</div>';
			html += '</div>';
		}

		return html;
	}

	function buildVariableProperties(metadata) {
		var html = '';
		html += '<div class="fd-property-group">';
		html += '<div class="fd-property-group-header">Field</div>';
		if (metadata.namespace) {
			html += '<div class="fd-property-row">';
			html += '<span class="fd-property-label">Namespace</span>';
			html += '<span class="fd-property-value">' + escapeHtml(String(metadata.namespace)) + '</span>';
			html += '</div>';
		}
		html += '<div class="fd-property-row">';
		html += '<span class="fd-property-label">Field Name</span>';
		html += '<span class="fd-property-value">' + escapeHtml(String(metadata.name || '')) + '</span>';
		html += '</div>';
		html += '<div class="fd-property-row">';
		html += '<span class="fd-property-label">Data Type</span>';
		html += '<span class="fd-property-value">' + escapeHtml(String(metadata.dataType || 'mixed')) + '</span>';
		html += '</div>';
		html += '</div>';
		return html;
	}

	function buildFunctionProperties(token, metadata) {
		var html = '';
		html += '<div class="fd-property-group">';
		html += '<div class="fd-property-group-header">Function</div>';
		html += '<div class="fd-property-row">';
		html += '<span class="fd-property-label">Name</span>';
		html += '<span class="fd-property-value">' + escapeHtml(String(metadata.name || token.label || '')) + '</span>';
		html += '</div>';
		if (typeof metadata.minArgs !== 'undefined' || typeof metadata.maxArgs !== 'undefined') {
			html += '<div class="fd-property-row">';
			html += '<span class="fd-property-label">Arguments</span>';
			if (typeof metadata.minArgs !== 'undefined' && typeof metadata.maxArgs !== 'undefined') {
				html += '<span class="fd-property-value">' + String(metadata.minArgs) + ' – ' + String(metadata.maxArgs) + '</span>';
			} else if (typeof metadata.minArgs !== 'undefined') {
				html += '<span class="fd-property-value">≥ ' + String(metadata.minArgs) + '</span>';
			} else {
				html += '<span class="fd-property-value">≤ ' + String(metadata.maxArgs) + '</span>';
			}
			html += '</div>';
		}
		if (metadata.returnType) {
			html += '<div class="fd-property-row">';
			html += '<span class="fd-property-label">Returns</span>';
			html += '<span class="fd-property-value">' + escapeHtml(String(metadata.returnType)) + '</span>';
			html += '</div>';
		}
		html += '</div>';
		return html;
	}

	function buildLiteralProperties(token, metadata) {
		var html = '';
		var editableValue = getEditableLiteralValue(token);
		var dataType = String(metadata.dataType || 'number').toLowerCase();

		html += '<div class="fd-property-group">';
		html += '<div class="fd-property-group-header">Value</div>';
		html += '<div class="fd-property-row">';
		html += '<span class="fd-property-label">Value</span>';
		html += '<input type="text" class="fd-property-input" value="' + escapeAttribute(editableValue) + '" data-property="literal-value" aria-label="Literal value">';
		html += '</div>';
		html += '<div class="fd-property-row">';
		html += '<span class="fd-property-label">Data Type</span>';
		html += '<span class="fd-property-value">' + escapeHtml(String(metadata.dataType || 'number')) + '</span>';
		html += '</div>';
		html += '</div>';
		return html;
	}

	function buildOperatorProperties(token, metadata) {
		var html = '';
		var kind = metadata.operatorKind || 'binary';

		html += '<div class="fd-property-group">';
		html += '<div class="fd-property-group-header">Operator</div>';
		html += '<div class="fd-property-row">';
		html += '<span class="fd-property-label">Operator</span>';
		html += '<span class="fd-property-value">' + escapeHtml(String(token.value || token.label || '')) + '</span>';
		html += '</div>';
		html += '<div class="fd-property-row">';
		html += '<span class="fd-property-label">Kind</span>';
		html += '<span class="fd-property-value">' + escapeHtml(kind.charAt(0).toUpperCase() + kind.slice(1)) + '</span>';
		html += '</div>';
		html += '</div>';
		return html;
	}

	function buildGroupProperties(token, metadata) {
		var html = '';
		var groupRole = (token.value === '(') ? 'Opening parenthesis' : 'Closing parenthesis';

		html += '<div class="fd-property-group">';
		html += '<div class="fd-property-group-header">Group</div>';
		html += '<div class="fd-property-row">';
		html += '<span class="fd-property-label">Role</span>';
		html += '<span class="fd-property-value">' + escapeHtml(groupRole) + '</span>';
		html += '</div>';
		html += '</div>';
		return html;
	}

	function initDesigner(root) {
		if (root.getAttribute('data-designer-initialized') === '1') {
			return;
		}
		var instanceId = root.getAttribute('data-instance-id') || ('fd-' + Object.keys(window.FormulaDesigner.instances).length);

		// Ensure a permanent screen-reader live region exists
		if (!root.querySelector('[data-designer="live-region"]')) {
			var live = document.createElement('div');
			live.setAttribute('data-designer', 'live-region');
			live.setAttribute('aria-live', 'polite');
			live.setAttribute('aria-atomic', 'true');
			live.className = 'fd-sr-only';
			root.appendChild(live);
		}

		window.FormulaDesigner.instances[instanceId] = new DesignerInstance(root);
		root.setAttribute('data-designer-initialized', '1');
	}

	window.FormulaDesigner = window.FormulaDesigner || {
		instances: {},
		phaseTwoExpression: cloneExpression(testExpression)
	};
	window.FormulaDesigner.cloneExpression = cloneExpression;
	window.FormulaDesigner.normalizeExpression = normalizeExpression;
	window.FormulaDesigner.createTokenId = nextTokenId;
	window.FormulaDesigner.serializeTokens = serializeTokens;
	window.FormulaDesigner.tokenizeFormula = tokenizeFormula;
	window.FormulaDesigner.isTokenSequenceValid = isTokenSequenceValid;
	window.FormulaDesigner.UndoManager = UndoManager;
	window.FormulaDesigner.ClipboardManager = ClipboardManager;
	window.FormulaDesigner.sharedUndoManager = sharedUndoManager;
	window.FormulaDesigner.sharedClipboard = sharedClipboard;

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

	// Support for dynamically-added designer instances (e.g. modal overlays).
	// When renderModal() finishes injecting the designer HTML, it dispatches
	// 'fd:boot' on the document so we can initialise any containers that were
	// added after the initial boot.
	document.addEventListener('fd:boot', boot);
}(window, document));