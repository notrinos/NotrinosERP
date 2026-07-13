/**
 * Formula Ai Panel — AI assistant chat interface for the Visual Formula Designer.
 *
 * Provides the browser-side UI for AI formula assistance features:
 *  - Natural Language → NFX conversion
 *  - Excel formula import
 *  - Chat-style formula assistance
 *  - Formula explanation
 *  - Duplicate detection
 *
 * This module requires formula-designer.js (core) and communicates with
 * the server via the AIAssistantAdapter through AJAX endpoints.
 *
 * @package FormulaDesigner\Assets\JS
 * @since   2.0.0
 */
(function (FD, $) {
    'use strict';

    /**
     * AiPanel — Manages the AI assistant panel in the designer UI.
     *
     * @constructor
     * @param {DesignerInstance} designer The parent designer instance
     */
    function AIPanel(designer) {
        this.designer = designer;
        this.panel = null;
        this.chatHistory = [];
        this.isProcessing = false;
        this.endpointUrl = '';
    }

    AIPanel.prototype = {

        /**
         * Initialize the AI panel and bind event handlers.
         *
         * @param {string} endpointUrl The AJAX endpoint for AI requests
         */
        init: function (endpointUrl) {
            this.endpointUrl = endpointUrl || '';
            this.panel = document.querySelector('.fd-ai-panel');

            if (!this.panel) {
                return;
            }

            // Chat input handlers
            var self = this;
            var chatInput = this.panel.querySelector('.fd-ai-chat-input');
            if (chatInput) {
                chatInput.addEventListener('keydown', function (e) {
                    if (e.key === 'Enter' && !e.shiftKey) {
                        e.preventDefault();
                        self.sendChatMessage();
                    }
                });
            }

            // Send button
            var sendBtn = this.panel.querySelector('.fd-ai-send-btn');
            if (sendBtn) {
                sendBtn.addEventListener('click', function () {
                    self.sendChatMessage();
                });
            }

            // Excel import button
            var excelBtn = this.panel.querySelector('.fd-ai-import-btn');
            if (excelBtn) {
                excelBtn.addEventListener('click', function () {
                    self.importExcelFormula();
                });
            }

            // Clear chat button
            var clearBtn = this.panel.querySelector('.fd-ai-clear-btn');
            if (clearBtn) {
                clearBtn.addEventListener('click', function () {
                    self.clearChat();
                });
            }

            // Apply formula button (delegated — buttons are added dynamically)
            this.panel.addEventListener('click', function (e) {
                var applyBtn = e.target.closest('.fd-ai-apply-btn');
                if (applyBtn) {
                    var formula = applyBtn.getAttribute('data-formula') || '';
                    if (formula) {
                        self.applyFormula(formula);
                    }
                }
            });
        },

        /**
         * Send a chat message to the AI provider.
         */
        sendChatMessage: function () {
            if (this.isProcessing) {
                return;
            }

            var chatInput = this.panel.querySelector('.fd-ai-chat-input');
            if (!chatInput) {
                return;
            }

            var message = chatInput.value.trim();
            if (message === '') {
                return;
            }

            // Add user message to chat
            this.addChatBubble('user', message);

            // Clear input
            chatInput.value = '';

            // Process with AI
            this.processNaturalLanguage(message);
        },

        /**
         * Process a natural language message through the AI provider.
         *
         * @param {string} description Natural language description
         */
        processNaturalLanguage: function (description) {
            var self = this;
            this.isProcessing = true;
            this.showTyping();

            var module = this.designer.config ? this.designer.config.module : 'hrm';

            // Collect available variables for context
            var availableVariables = [];
            var fieldSections = this.designer.fieldSections || [];
            for (var i = 0; i < fieldSections.length; i++) {
                var items = fieldSections[i].items || [];
                for (var j = 0; j < items.length; j++) {
                    if (items[j].qualifiedName) {
                        availableVariables.push(items[j].qualifiedName);
                    }
                }
            }

            // Send AJAX request
            $.ajax({
                url: this.endpointUrl,
                type: 'POST',
                data: {
                    action: 'nl_to_formula',
                    description: description,
                    module: module,
                    availableVariables: availableVariables,
                    availableFunctions: []
                },
                success: function (response) {
                    self.hideTyping();
                    self.isProcessing = false;

                    var data = response;
                    if (typeof response === 'string') {
                        try {
                            data = JSON.parse(response);
                        } catch (e) {
                            self.addChatBubble('assistant', 'Sorry, received an unexpected response.');
                            return;
                        }
                    }

                    if (data.success && data.result) {
                        self.addFormulaBubble(data.result, data.explanation || '', data.confidence || 0.8);
                    } else {
                        var errorMsg = (data.errorMessage) ? data.errorMessage : 'Could not generate a formula. Please rephrase your request.';
                        self.addChatBubble('assistant', errorMsg);
                    }
                },
                error: function () {
                    self.hideTyping();
                    self.isProcessing = false;
                    self.addChatBubble('assistant', 'Sorry, the AI service is currently unavailable. Please try again later.');
                }
            });
        },

        /**
         * Import an Excel formula and convert it to NFX.
         */
        importExcelFormula: function () {
            var self = this;
            var excelFormula = prompt('Paste your Excel formula (e.g., =IF(A1>100, A1*0.8, A1)*B1):');

            if (!excelFormula || excelFormula.trim() === '') {
                return;
            }

            this.isProcessing = true;
            this.showTyping();

            $.ajax({
                url: this.endpointUrl,
                type: 'POST',
                data: {
                    action: 'convert_excel',
                    excel_formula: excelFormula
                },
                success: function (response) {
                    self.hideTyping();
                    self.isProcessing = false;

                    var data = response;
                    if (typeof response === 'string') {
                        try {
                            data = JSON.parse(response);
                        } catch (e) {
                            self.addChatBubble('assistant', 'Sorry, received an unexpected response.');
                            return;
                        }
                    }

                    if (data.success && data.result) {
                        self.addFormulaBubble(data.result, 'Converted from Excel formula: ' + excelFormula + '\n\n' + (data.explanation || ''), data.confidence || 0.85);
                    } else {
                        self.addChatBubble('assistant', 'Could not convert that Excel formula. Please check the syntax and try again.');
                    }
                },
                error: function () {
                    self.hideTyping();
                    self.isProcessing = false;
                    self.addChatBubble('assistant', 'Sorry, the conversion service is unavailable.');
                }
            });
        },

        /**
         * Add a chat bubble to the conversation.
         *
         * @param {string} role    'user' or 'assistant'
         * @param {string} message The message text
         */
        addChatBubble: function (role, message) {
            var chatBody = this.panel.querySelector('.fd-ai-chat-body');
            if (!chatBody) {
                return;
            }

            var bubble = document.createElement('div');
            bubble.className = 'fd-ai-bubble fd-ai-bubble--' + role;

            var content = document.createElement('div');
            content.className = 'fd-ai-bubble-content';
            content.textContent = message;

            bubble.appendChild(content);
            chatBody.appendChild(bubble);

            // Scroll to bottom
            chatBody.scrollTop = chatBody.scrollHeight;

            // Store in history (max 50 entries)
            this.chatHistory.push({ role: role, message: message, timestamp: Date.now() });
            if (this.chatHistory.length > 50) {
                this.chatHistory.shift();
            }
        },

        /**
         * Add a formula suggestion bubble with an "Apply" button.
         *
         * @param {string} formula     The NFX formula text
         * @param {string} explanation Human-readable explanation
         * @param {number} confidence  Confidence score (0.0–1.0)
         */
        addFormulaBubble: function (formula, explanation, confidence) {
            var chatBody = this.panel.querySelector('.fd-ai-chat-body');
            if (!chatBody) {
                return;
            }

            var bubble = document.createElement('div');
            bubble.className = 'fd-ai-bubble fd-ai-bubble--assistant';

            var content = document.createElement('div');
            content.className = 'fd-ai-bubble-content';

            // Formula code block
            var codeBlock = document.createElement('code');
            codeBlock.className = 'fd-ai-formula-code';
            codeBlock.textContent = formula;
            content.appendChild(codeBlock);

            // Explanation
            if (explanation) {
                var expl = document.createElement('p');
                expl.className = 'fd-ai-explanation';
                expl.textContent = explanation;
                content.appendChild(expl);
            }

            // Confidence indicator
            var confidencePct = Math.round(confidence * 100);
            var confLabel = confidencePct >= 80 ? 'High confidence' : (confidencePct >= 50 ? 'Medium confidence' : 'Low confidence');
            var confSpan = document.createElement('span');
            confSpan.className = 'fd-ai-confidence fd-ai-confidence--' + (confidencePct >= 80 ? 'high' : (confidencePct >= 50 ? 'medium' : 'low'));
            confSpan.textContent = confLabel + ' (' + confidencePct + '%)';
            content.appendChild(confSpan);

            // Apply button
            var applyBtn = document.createElement('button');
            applyBtn.type = 'button';
            applyBtn.className = 'fd-ai-apply-btn';
            applyBtn.setAttribute('data-formula', formula);
            applyBtn.textContent = 'Apply This Formula';
            content.appendChild(applyBtn);

            bubble.appendChild(content);
            chatBody.appendChild(bubble);

            // Scroll to bottom
            chatBody.scrollTop = chatBody.scrollHeight;

            // Flash the formula into the canvas for preview
            this.flashPreview(formula);
        },

        /**
         * Flash the formula text into the canvas temporarily.
         * Does NOT replace the working formula — only the hidden textarea.
         *
         * @param {string} formula NFX formula text
         */
        flashPreview: function (formula) {
            var textarea = document.getElementById(
                this.designer.textareaId || ''
            );

            if (textarea) {
                // Store current value temporarily
                var currentValue = textarea.value;
                textarea.value = formula;

                // Trigger textarea sync so canvas shows the preview
                if (typeof this.designer.syncCanvasFromTextarea === 'function') {
                    this.designer.syncCanvasFromTextarea();
                }

                // Restore after 3 seconds (unless user clicks "Apply")
                var self = this;
                this.previewTimeout = setTimeout(function () {
                    if (textarea.value === formula) {
                        textarea.value = currentValue;
                        if (typeof self.designer.syncCanvasFromTextarea === 'function') {
                            self.designer.syncCanvasFromTextarea();
                        }
                    }
                }, 3000);
            }
        },

        /**
         * Apply an AI-generated formula to the designer canvas.
         *
         * @param {string} formula The NFX formula text
         */
        applyFormula: function (formula) {
            var textarea = document.getElementById(
                this.designer.textareaId || ''
            );

            if (textarea) {
                // Clear any pending preview timeout
                if (this.previewTimeout) {
                    clearTimeout(this.previewTimeout);
                    this.previewTimeout = null;
                }

                textarea.value = formula;
                if (typeof this.designer.syncCanvasFromTextarea === 'function') {
                    this.designer.syncCanvasFromTextarea();
                }

                // Mark as dirty for save
                if (typeof this.designer.markDirty === 'function') {
                    this.designer.markDirty();
                }

                // Announce to screen reader
                if (typeof this.designer.announceToScreenReader === 'function') {
                    this.designer.announceToScreenReader('AI-generated formula applied to canvas.');
                }
            }
        },

        /**
         * Show the typing indicator in the chat.
         */
        showTyping: function () {
            var chatBody = this.panel.querySelector('.fd-ai-chat-body');
            if (!chatBody) {
                return;
            }

            var typing = document.createElement('div');
            typing.className = 'fd-ai-typing';
            typing.innerHTML = '<span></span><span></span><span></span>';
            typing.setAttribute('data-typing', 'true');
            chatBody.appendChild(typing);
            chatBody.scrollTop = chatBody.scrollHeight;
        },

        /**
         * Hide the typing indicator.
         */
        hideTyping: function () {
            var typing = this.panel.querySelector('[data-typing]');
            if (typing) {
                typing.parentNode.removeChild(typing);
            }
        },

        /**
         * Clear all chat messages.
         */
        clearChat: function () {
            var chatBody = this.panel.querySelector('.fd-ai-chat-body');
            if (!chatBody) {
                return;
            }

            chatBody.innerHTML = '';
            this.chatHistory = [];

            // Add back the welcome message
            var welcome = document.createElement('div');
            welcome.className = 'fd-ai-bubble fd-ai-bubble--assistant';
            var welcomeContent = document.createElement('div');
            welcomeContent.className = 'fd-ai-bubble-content';
            welcomeContent.textContent = 'Hello! I can help you create formulas. Try describing what you want to calculate in plain language, or paste an Excel formula.';
            welcome.appendChild(welcomeContent);
            chatBody.appendChild(welcome);
        },

        /**
         * Show the AI panel.
         */
        show: function () {
            if (this.panel) {
                this.panel.style.display = '';
            }
        },

        /**
         * Hide the AI panel.
         */
        hide: function () {
            if (this.panel) {
                this.panel.style.display = 'none';
            }
        },

        /**
         * Toggle the AI panel visibility.
         */
        toggle: function () {
            if (this.panel) {
                if (this.panel.style.display === 'none') {
                    this.show();
                } else {
                    this.hide();
                }
            }
        },

        /**
         * Check if the panel is visible.
         *
         * @return {boolean}
         */
        isVisible: function () {
            return this.panel && this.panel.style.display !== 'none';
        }
    };

    // Register with the global FormulaDesigner namespace
    FD.AIPanel = AIPanel;

})(window.FormulaDesigner || {}, jQuery);
