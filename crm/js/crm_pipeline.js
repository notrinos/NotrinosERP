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
            var cards = document.querySelectorAll('.kanban-card');
            var columns = document.querySelectorAll('.kanban-column');

            if (!cards.length || !columns.length) return;

            // Make cards draggable
            for (var i = 0; i < cards.length; i++) {
                cards[i].setAttribute('draggable', 'true');
                cards[i].addEventListener('dragstart', CRMPipeline.onDragStart);
                cards[i].addEventListener('dragend', CRMPipeline.onDragEnd);
            }

            // Set up stage columns as drop targets
            for (var j = 0; j < columns.length; j++) {
                columns[j].addEventListener('dragover', CRMPipeline.onDragOver);
                columns[j].addEventListener('dragenter', CRMPipeline.onDragEnter);
                columns[j].addEventListener('dragleave', CRMPipeline.onDragLeave);
                columns[j].addEventListener('drop', CRMPipeline.onDrop);
            }
        },

        /**
         * Handle drag start - store the card data.
         *
         * @param {DragEvent} e
         */
        onDragStart: function(e) {
            var card = e.target.closest('.kanban-card');
            if (!card) return;
            e.dataTransfer.setData('text/plain', card.getAttribute('data-lead-id'));
            e.dataTransfer.effectAllowed = 'move';
            card.classList.add('is-dragging');
        },

        /**
         * Handle drag end - restore appearance.
         *
         * @param {DragEvent} e
         */
        onDragEnd: function(e) {
            var card = e.target.closest('.kanban-card');
            if (card) card.classList.remove('is-dragging');
            // Remove all drag-over highlights
            var columns = document.querySelectorAll('.kanban-column');
            for (var i = 0; i < columns.length; i++) {
                columns[i].classList.remove('is-drag-over');
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
            var column = e.target.closest('.kanban-column');
            if (column) {
                column.classList.add('is-drag-over');
            }
        },

        /**
         * Remove visual feedback on drag leave.
         *
         * @param {DragEvent} e
         */
        onDragLeave: function(e) {
            var column = e.target.closest('.kanban-column');
            if (column) {
                column.classList.remove('is-drag-over');
            }
        },

        /**
         * Handle drop - update opportunity stage via AJAX.
         *
         * @param {DragEvent} e
         */
        onDrop: function(e) {
            e.preventDefault();
            var column = e.target.closest('.kanban-column');
            if (!column) return;

            column.classList.remove('is-drag-over');

            var leadId = e.dataTransfer.getData('text/plain');
            var stageId = column.getAttribute('data-stage-id');

            if (!leadId || !stageId) return;

            // Move the card DOM element into the column body
            var card = document.querySelector('.kanban-card[data-lead-id="' + leadId + '"]');
            var columnBody = column.querySelector('.kanban-column-body');
            if (card && columnBody) {
                columnBody.appendChild(card);
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
