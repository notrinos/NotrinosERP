/**********************************************************************
Copyright (C) NotrinosERP.
Released under the terms of the GNU General Public License, GPL,
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************//**
 * CRM Pipeline Board - Drag-and-drop stage changes
 *
 * Provides drag-and-drop functionality for the pipeline board.
 * Cards can be dragged between stage columns to update the opportunity stage.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

(function() {
    'use strict';

    var CRMPipeline = {
        /**
         * Initialize the pipeline board drag-and-drop.
         */
        init: function() {
            var cards = document.querySelectorAll('.crm-pipeline-card');
            var stages = document.querySelectorAll('.crm-pipeline-stage');

            if (!cards.length || !stages.length) return;

            // Make cards draggable
            for (var i = 0; i < cards.length; i++) {
                cards[i].setAttribute('draggable', 'true');
                cards[i].addEventListener('dragstart', CRMPipeline.onDragStart);
                cards[i].addEventListener('dragend', CRMPipeline.onDragEnd);
            }

            // Set up stage columns as drop targets
            for (var j = 0; j < stages.length; j++) {
                stages[j].addEventListener('dragover', CRMPipeline.onDragOver);
                stages[j].addEventListener('dragenter', CRMPipeline.onDragEnter);
                stages[j].addEventListener('dragleave', CRMPipeline.onDragLeave);
                stages[j].addEventListener('drop', CRMPipeline.onDrop);
            }
        },

        /**
         * Handle drag start - store the card data.
         *
         * @param {DragEvent} e
         */
        onDragStart: function(e) {
            var card = e.target.closest('.crm-pipeline-card');
            if (!card) return;
            e.dataTransfer.setData('text/plain', card.getAttribute('data-lead-id'));
            e.dataTransfer.effectAllowed = 'move';
            card.style.opacity = '0.5';
        },

        /**
         * Handle drag end - restore appearance.
         *
         * @param {DragEvent} e
         */
        onDragEnd: function(e) {
            e.target.style.opacity = '1';
            // Remove all drag-over highlights
            var stages = document.querySelectorAll('.crm-pipeline-stage');
            for (var i = 0; i < stages.length; i++) {
                stages[i].classList.remove('crm-drag-over');
            }
        },

        /**
         * Allow drop on stage columns.
         *
         * @param {DragEvent} e
         */
        onDragOver: function(e) {
            e.preventDefault();
            e.dataTransfer.dropEffect = 'move';
        },

        /**
         * Visual feedback on drag enter.
         *
         * @param {DragEvent} e
         */
        onDragEnter: function(e) {
            e.preventDefault();
            var stage = e.target.closest('.crm-pipeline-stage');
            if (stage) {
                stage.style.background = '#e8f5e9';
            }
        },

        /**
         * Remove visual feedback on drag leave.
         *
         * @param {DragEvent} e
         */
        onDragLeave: function(e) {
            var stage = e.target.closest('.crm-pipeline-stage');
            if (stage) {
                stage.style.background = '#f5f5f5';
            }
        },

        /**
         * Handle drop - update opportunity stage via AJAX.
         *
         * @param {DragEvent} e
         */
        onDrop: function(e) {
            e.preventDefault();
            var stage = e.target.closest('.crm-pipeline-stage');
            if (!stage) return;

            stage.style.background = '#f5f5f5';

            var leadId = e.dataTransfer.getData('text/plain');
            var stageId = stage.getAttribute('data-stage-id');

            if (!leadId || !stageId) return;

            // Move the card DOM element
            var card = document.querySelector('.crm-pipeline-card[data-lead-id="' + leadId + '"]');
            if (card) {
                stage.appendChild(card);
            }

            // AJAX update (uses NotrinosERP's JsHttpRequest if available)
            CRMPipeline.updateStage(leadId, stageId);
        },

        /**
         * Send AJAX request to update opportunity stage.
         *
         * @param {string} leadId
         * @param {string} stageId
         */
        updateStage: function(leadId, stageId) {
            var xhr = new XMLHttpRequest();
            xhr.open('POST', 'pipeline_update.php', true);
            xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
            xhr.onreadystatechange = function() {
                if (xhr.readyState === 4) {
                    if (xhr.status === 200) {
                        // Optionally refresh totals
                        try {
                            var resp = JSON.parse(xhr.responseText);
                            if (resp.error) {
                                alert(resp.error);
                                window.location.reload();
                            }
                        } catch(ex) {
                            // silent
                        }
                    }
                }
            };
            // Include CSRF token from the page's hidden input
            var token = '';
            var tokenInput = document.querySelector('input[name="_token"]');
            if (tokenInput) token = tokenInput.value;
            xhr.send('lead_id=' + encodeURIComponent(leadId) +
                     '&stage_id=' + encodeURIComponent(stageId) +
                     '&_token=' + encodeURIComponent(token));
        }
    };

    // Initialize when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', CRMPipeline.init);
    } else {
        CRMPipeline.init();
    }

    window.CRMPipeline = CRMPipeline;
})();
