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
 * CRM Common UI - Shared CRM JavaScript helpers
 *
 * Provides utility functions used across CRM pages:
 * tag rendering, badge styling, confirmation dialogs, etc.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

(function() {
    'use strict';

    var CRM = {
        /**
         * Initialize common CRM UI behaviors.
         */
        init: function() {
            CRM.addBadgeStyles();
            CRM.initConfirmButtons();
            CRM.initCardHovers();
        },

        /**
         * Inject badge CSS styles for CRM status/priority indicators.
         */
        addBadgeStyles: function() {
            if (document.getElementById('crm-badge-styles')) return;

            var css = [
                '.crm-badge { display:inline-block; padding:2px 8px; border-radius:10px; font-size:0.8em; font-weight:bold; }',
                '.crm-badge-new { background:#E3F2FD; color:#1565C0; }',
                '.crm-badge-contacted { background:#FFF3E0; color:#E65100; }',
                '.crm-badge-qualified { background:#E8F5E9; color:#2E7D32; }',
                '.crm-badge-converted { background:#F3E5F5; color:#6A1B9A; }',
                '.crm-badge-lost { background:#FFEBEE; color:#C62828; }',
                '.crm-badge-planned { background:#E3F2FD; color:#1565C0; }',
                '.crm-badge-done { background:#E8F5E9; color:#2E7D32; }',
                '.crm-badge-cancelled { background:#F5F5F5; color:#616161; }',
                '.crm-badge-overdue { background:#FFEBEE; color:#C62828; }',
                '.crm-priority-urgent { background:#F44336; color:#fff; }',
                '.crm-priority-high { background:#FF9800; color:#fff; }',
                '.crm-priority-medium { background:#2196F3; color:#fff; }',
                '.crm-priority-low { background:#9E9E9E; color:#fff; }',
                '.crm-tag { display:inline-block; padding:1px 6px; border-radius:8px; font-size:0.75em; margin-right:3px; }',
                '.crm-pipeline-card:hover { box-shadow:0 2px 6px rgba(0,0,0,0.15); transform:translateY(-1px); transition:all 0.2s; }',
                '.crm-pipeline-stage { transition:background 0.2s; }',
                '.crm-probability-bar { display:inline-block; height:14px; border-radius:3px; background:#e0e0e0; overflow:hidden; width:80px; vertical-align:middle; }',
                '.crm-probability-fill { height:100%; border-radius:3px; background:#4CAF50; transition:width 0.3s; }'
            ].join('\n');

            var style = document.createElement('style');
            style.id = 'crm-badge-styles';
            style.textContent = css;
            document.head.appendChild(style);
        },

        /**
         * Add confirmation to delete/destructive buttons.
         */
        initConfirmButtons: function() {
            var buttons = document.querySelectorAll('[name="Delete"], [name="MarkLost"]');
            for (var i = 0; i < buttons.length; i++) {
                buttons[i].addEventListener('click', function(e) {
                    if (!confirm('Are you sure you want to perform this action?')) {
                        e.preventDefault();
                        return false;
                    }
                });
            }
        },

        /**
         * Add hover effects to pipeline cards.
         */
        initCardHovers: function() {
            var cards = document.querySelectorAll('.crm-pipeline-card');
            for (var i = 0; i < cards.length; i++) {
                cards[i].addEventListener('mouseenter', function() {
                    this.style.boxShadow = '0 2px 6px rgba(0,0,0,0.15)';
                });
                cards[i].addEventListener('mouseleave', function() {
                    this.style.boxShadow = 'none';
                });
            }
        },

        /**
         * Format a number as currency (uses the page's currency symbol if available).
         *
         * @param {number} value
         * @returns {string}
         */
        formatCurrency: function(value) {
            if (typeof value !== 'number') value = parseFloat(value) || 0;
            return value.toLocaleString(undefined, {minimumFractionDigits: 2, maximumFractionDigits: 2});
        },

        /**
         * Toggle visibility of an element by ID.
         *
         * @param {string} elementId
         */
        toggle: function(elementId) {
            var el = document.getElementById(elementId);
            if (el) {
                el.style.display = (el.style.display === 'none') ? '' : 'none';
            }
        }
    };

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', CRM.init);
    } else {
        CRM.init();
    }

    window.CRM = CRM;
})();
