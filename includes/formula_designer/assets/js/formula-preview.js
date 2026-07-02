/**
 * NotrinosERP Visual Formula Designer — Preview & Explain Mode
 *
 * Client-side preview and explain functionality.
 * All interaction is initiated by toolbar toggles; no automatic eval
 * fires without user intent.
 *
 * @package   FormulaDesigner
 * @phase     7
 * @since     2.0.0
 * @requires  jQuery, formula-designer.js, JsHttpRequest
 */

(function ($, window, undefined) {
    'use strict';

    // -------------------------------------------------------------------
    // 0.  Module-private state
    // -------------------------------------------------------------------

    /** @type {boolean}  Track whether the explain panel is visible. */
    var explainVisible = false;

    /** @type {number|null}  Active debounce timer for preview requests. */
    var previewDebounce = null;

    /**
     * Sample context values keyed by canonical variable path.
     * Persisted per module in localStorage.
     * @type {Object.<string, string>}
     */
    var sampleValues = {};

    /** @const {number}  Debounce window for preview broadcasts (ms). */
    var PREVIEW_DEBOUNCE_MS = 600;

    /** @const {string}  localStorage key template. */
    var STORAGE_KEY_PREFIX = 'fd-preview-vars-';

    // -------------------------------------------------------------------
    // 1.  Public initialisation — called by formula-designer.js
    // -------------------------------------------------------------------

    /**
     * Wire preview and explain toolbar buttons into a live designer
     * instance.  This function is idempotent within the container.
     *
     * @param {HTMLElement} container  The `.fd-container` element.
     */
    function initPreviewPanel(container) {
        var $container = $(container);
        if ($container.data('fd-preview-initialised')) {
            return;
        }
        $container.data('fd-preview-initialised', true);

        loadSampleValues($container);

        // ---- toolbar toggle buttons ----
        $container.on('click', '[data-action="toggle-preview"]', function () {
            togglePanel($container, 'preview');
        });

        $container.on('click', '[data-action="toggle-explain"]', function () {
            togglePanel($container, 'explain');
        });

        $container.on('click', '[data-action="preview-refresh"]', function () {
            requestPreview($container, true);
        });

        $container.on('click', '[data-action="sample-reset"]', function () {
            resetSampleValues($container);
        });

        // ---- sample value input changes ----
        $container.on('input', '.fd-preview-sample-input', function () {
            var $input = $(this);
            var key = $input.attr('data-fd-var-key') || '';
            sampleValues[key] = $input.val();
            persistSampleValues($container);
            schedulePreviewRequest($container);
        });

        // ---- listen for formula-change events from the main editor ----
        $container.on('fd:textareasynced fd:canvasrendered', function () {
            schedulePreviewRequest($container);
        });

        // ---- initial render of sample panel if previously cached ----
        if (Object.keys(sampleValues).length > 0) {
            renderSamplePanel($container);
        }
    }

    // -------------------------------------------------------------------
    // 2.  Panel visibility helpers
    // -------------------------------------------------------------------

    /**
     * Toggle a bottom panel, hide the other.
     * @param {jQuery} $container
     * @param {string}  panel  'preview' or 'explain'
     */
    function togglePanel($container, panel) {
        var $preview = $container.find('.fd-preview-panel');
        var $explain = $container.find('.fd-explain-panel');

        if (panel === 'preview') {
            if ($preview.hasClass('fd-panel--open')) {
                $preview.removeClass('fd-panel--open');
                return;
            }
            $preview.addClass('fd-panel--open');
            $explain.removeClass('fd-panel--open');
            explainVisible = false;
            requestPreview($container, true);
        } else {
            if ($explain.hasClass('fd-panel--open')) {
                $explain.removeClass('fd-panel--open');
                explainVisible = false;
                return;
            }
            $explain.addClass('fd-panel--open');
            $preview.removeClass('fd-panel--open');
            explainVisible = true;
            requestExplain($container);
        }
    }

    // -------------------------------------------------------------------
    // 3.  AJAX — Preview
    // -------------------------------------------------------------------

    /**
     * Queue a debounced preview request.
     * @param {jQuery} $container
     */
    function schedulePreviewRequest($container) {
        if (previewDebounce !== null) {
            clearTimeout(previewDebounce);
        }
        previewDebounce = setTimeout(function () {
            previewDebounce = null;
            requestPreview($container, false);
        }, PREVIEW_DEBOUNCE_MS);
    }

    /**
     * Send the current formula + sample values to the server and
     * render the result in the preview panel.
     *
     * @param {jQuery}   $container
     * @param {boolean}  immediate  True = skip debounce (user click)
     */
    function requestPreview($container, immediate) {
        var $panel = $container.find('.fd-preview-panel');
        if (!$panel.hasClass('fd-panel--open') && !immediate) {
            return;
        }

        var formula = readFormula($container);
        var module = $container.attr('data-module') || 'hrm';

        if (formula === '') {
            renderPreviewResult($panel, null, 'Enter a formula to preview.');
            return;
        }

        renderPreviewLoading($panel);

        var payload = {
            action: 'preview',
            formula: formula,
            module: module,
            sampleValues: sampleValues
        };

        JsHttpRequest.query(
            window.FORMULA_DESIGNER_EXPLAIN_URL || getDefaultPreviewUrl($container),
            payload,
            function (result) { handlePreviewResponse($panel, result); },
            function (err) { handlePreviewError($panel, err); },
            true
        );
    }

    /**
     * Handle the preview AJAX response.
     */
    function handlePreviewResponse($panel, data) {
        if (!data || data.ok !== true) {
            renderPreviewResult($panel, null, data && data.error ? data.error : 'Preview failed.');
            return;
        }

        renderPreviewResult($panel, data.result, data.note || null);
        renderSamplePanelFromResponse($panel, data);
    }

    /**
     * Handle a preview network / server error.
     */
    function handlePreviewError($panel /*, err */) {
        renderPreviewResult($panel, null, 'Preview request failed. Verify the server is available.');
    }

    // -------------------------------------------------------------------
    // 4.  AJAX — Explain
    // -------------------------------------------------------------------

    /**
     * Request a step-by-step explanation of the current formula.
     * @param {jQuery} $container
     */
    function requestExplain($container) {
        var $panel = $container.find('.fd-explain-panel');
        if (!explainVisible) {
            return;
        }

        var formula = readFormula($container);
        var module = $container.attr('data-module') || 'hrm';

        if (formula === '') {
            renderExplainResult($panel, null, 'Enter a formula to explain.');
            return;
        }

        renderExplainLoading($panel);

        var payload = {
            action: 'explain',
            formula: formula,
            module: module,
            sampleValues: sampleValues
        };

        JsHttpRequest.query(
            window.FORMULA_DESIGNER_EXPLAIN_URL || getDefaultPreviewUrl($container),
            payload,
            function (result) { handleExplainResponse($panel, result); },
            function (err) { handleExplainError($panel, err); },
            true
        );
    }

    /**
     * Handle the explain AJAX response.
     */
    function handleExplainResponse($panel, data) {
        if (!data || data.ok !== true) {
            renderExplainResult($panel, null, data && data.error ? data.error : 'Explain failed.');
            return;
        }

        renderExplainResult($panel, data.steps || [], data.result, data.durationMs || 0);
    }

    /**
     * Handle an explain network / server error.
     */
    function handleExplainError($panel /*, err */) {
        renderExplainResult($panel, null, 'Explain request failed. Verify the server is available.');
    }

    // -------------------------------------------------------------------
    // 5.  DOM rendering — Preview panel
    // -------------------------------------------------------------------

    function renderPreviewLoading($panel) {
        var $body = $panel.find('.fd-preview-body');
        $body.html('<div class="fd-preview-loading">Evaluating…</div>');
    }

    /**
     * Render the preview result.
     * @param {jQuery} $panel
     * @param {*}      result   Computed result value.
     * @param {string} note     Optional context message (empty, error, etc.).
     */
    function renderPreviewResult($panel, result, note) {
        var $body = $panel.find('.fd-preview-body');
        var html = '';

        if (note) {
            html += '<div class="fd-preview-note">' + escapeHtml(note) + '</div>';
        }

        if (result !== null && result !== undefined) {
            html += '<div class="fd-preview-value">' + escapeHtml(formatResult(result)) + '</div>';
        }

        $body.html(html || '<div class="fd-preview-note">No result.</div>');
    }

    // -------------------------------------------------------------------
    // 6.  DOM rendering — Explain panel
    // -------------------------------------------------------------------

    function renderExplainLoading($panel) {
        var $body = $panel.find('.fd-explain-body');
        $body.html('<div class="fd-explain-loading">Tracing steps…</div>');
    }

    /**
     * Render the step-by-step trace.
     */
    function renderExplainResult($panel, steps, finalResult, durationMs) {
        var $body = $panel.find('.fd-explain-body');
        var html = '';

        if (steps === null || steps === undefined) {
            html += '<div class="fd-explain-note">' + escapeHtml(finalResult || '') + '</div>';
            $body.html(html);
            return;
        }

        html += '<ol class="fd-explain-steps">';
        for (var i = 0; i < steps.length; i++) {
            var step = steps[i];
            html += '<li class="fd-explain-step">';
            html += '<span class="fd-explain-step-label">' + escapeHtml(step.label || ('Step ' + (i + 1))) + '</span>';
            html += '<span class="fd-explain-step-detail">' + escapeHtml(step.detail || '') + '</span>';
            if (step.result !== undefined) {
                html += ' <span class="fd-explain-step-result">→ ' + escapeHtml(formatResult(step.result)) + '</span>';
            }
            html += '</li>';
        }
        html += '</ol>';

        if (finalResult !== null && finalResult !== undefined) {
            html += '<div class="fd-explain-final">';
            html += '<span class="fd-explain-final-label">Final result:</span> ';
            html += '<span class="fd-explain-final-value">' + escapeHtml(formatResult(finalResult)) + '</span>';
            html += '</div>';
        }

        if (durationMs) {
            html += '<div class="fd-explain-timing">Duration: ' + Number(durationMs).toFixed(1) + ' ms</div>';
        }

        $body.html(html);
    }

    // -------------------------------------------------------------------
    // 7.  Sample value management
    // -------------------------------------------------------------------

    /**
     * Build / refresh the sample-value editor inside the preview panel.
     * Called after a successful preview response so we can seed values
     * that the server discovered (e.g. variable references).
     */
    function renderSamplePanelFromResponse($panel, data) {
        var $sampleBody = $panel.find('.fd-preview-samples-body');
        if ($sampleBody.length === 0) {
            return;
        }

        if (data && data.variables && typeof data.variables === 'object') {
            // Seed any new variable keys the server reported
            var keys = Object.keys(data.variables);
            for (var i = 0; i < keys.length; i++) {
                var k = keys[i];
                if (sampleValues[k] === undefined) {
                    sampleValues[k] = data.variables[k] !== undefined && data.variables[k] !== null
                        ? String(data.variables[k])
                        : '';
                }
            }
            persistSampleValues($panel.closest('.fd-container'));
        }

        renderSampleInputs($sampleBody);
    }

    /**
     * Render the full sample panel from stored values.
     */
    function renderSamplePanel($container) {
        var $body = $container.find('.fd-preview-samples-body');
        if ($body.length === 0) {
            return;
        }
        renderSampleInputs($body);
    }

    function renderSampleInputs($body) {
        var keys = Object.keys(sampleValues).sort();
        if (keys.length === 0) {
            $body.html('<div class="fd-preview-samples-empty">No variables detected. Type a formula to see sample inputs.</div>');
            return;
        }

        var html = '';
        for (var i = 0; i < keys.length; i++) {
            var key = keys[i];
            var val = sampleValues[key] || '';
            html += '<div class="fd-preview-sample-row">';
            html += '<label class="fd-preview-sample-label">' + escapeHtml(key) + '</label>';
            html += '<input class="fd-preview-sample-input" type="text" value="' + escapeHtml(val) + '" data-fd-var-key="' + escapeHtml(key) + '" placeholder="0">';
            html += '</div>';
        }
        $body.html(html);
    }

    function resetSampleValues($container) {
        sampleValues = {};
        persistSampleValues($container);
        var $body = $container.find('.fd-preview-samples-body');
        if ($body.length) {
            $body.html('<div class="fd-preview-samples-empty">No variables detected. Type a formula to see sample inputs.</div>');
        }
    }

    // -------------------------------------------------------------------
    // 8.  localStorage persistence
    // -------------------------------------------------------------------

    function getStorageKey($container) {
        var module = $container.attr('data-module') || 'hrm';
        return STORAGE_KEY_PREFIX + module;
    }

    function loadSampleValues($container) {
        try {
            var raw = window.localStorage.getItem(getStorageKey($container));
            if (raw) {
                sampleValues = JSON.parse(raw);
                if (typeof sampleValues !== 'object' || sampleValues === null) {
                    sampleValues = {};
                }
            }
        } catch (ignore) {
            sampleValues = {};
        }
    }

    function persistSampleValues($container) {
        try {
            window.localStorage.setItem(getStorageKey($container), JSON.stringify(sampleValues));
        } catch (ignore) {
            // Storage full or unavailable — silently degrade.
        }
    }

    // -------------------------------------------------------------------
    // 9.  Utility helpers
    // -------------------------------------------------------------------

    /**
     * Read the current formula from the hidden source textarea.
     * @param {jQuery} $container
     * @returns {string}
     */
    function readFormula($container) {
        var textareaId = $container.attr('data-textarea-id');
        if (textareaId) {
            var $ta = $('#' + textareaId);
            if ($ta.length) {
                return ($ta.val() || '').trim();
            }
        }
        return '';
    }

    /**
     * Format a preview result value for display.
     * @param {*} value
     * @returns {string}
     */
    function formatResult(value) {
        if (typeof value === 'number') {
            return (Math.round(value * 1000000) / 1000000).toString();
        }
        return String(value);
    }

    /**
     * Escape HTML entities.
     * @param {string} str
     * @returns {string}
     */
    function escapeHtml(str) {
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#039;');
    }

    /**
     * Derive the default explain/preview endpoint URL from container metadata.
     * @param {jQuery} $container
     * @returns {string}
     */
    function getDefaultPreviewUrl($container) {
        var url = $container.attr('data-validate-api-url') || '';
        return url.replace(/DesignerValidateAPI\.php$/i, 'DesignerExplainAPI.php');
    }

    // -------------------------------------------------------------------
    // 10.  Public API exposed under window.FormulaDesigner
    // -------------------------------------------------------------------

    var DesignerPreview = {
        init: initPreviewPanel,
        togglePreview: function (container) {
            togglePanel($(container), 'preview');
        },
        toggleExplain: function (container) {
            togglePanel($(container), 'explain');
        },
        refreshPreview: function (container) {
            requestPreview($(container), true);
        },
        getSampleValues: function () {
            return $.extend({}, sampleValues);
        },
        setSampleValue: function (key, value) {
            sampleValues[key] = String(value);
        }
    };

    // Attach to the shared designer namespace.
    if (!window.FormulaDesigner) {
        window.FormulaDesigner = {};
    }
    window.FormulaDesigner.preview = DesignerPreview;

})(jQuery, window);
