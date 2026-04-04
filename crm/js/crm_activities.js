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
 * CRM Activities - Activity scheduling and management helpers
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

(function() {
    'use strict';

    var CRMActivities = {
        /**
         * Initialize activity UI enhancements.
         */
        init: function() {
            CRMActivities.highlightOverdue();
            CRMActivities.setupQuickActions();
        },

        /**
         * Highlight overdue activity rows with a subtle red background.
         */
        highlightOverdue: function() {
            var badges = document.querySelectorAll('.crm-badge-overdue, .crm-activity-overdue');
            for (var i = 0; i < badges.length; i++) {
                var row = badges[i].closest('tr');
                if (row) {
                    row.style.backgroundColor = '#fff3f3';
                }
            }
        },

        /**
         * Setup quick action buttons for activities (mark done, reschedule).
         */
        setupQuickActions: function() {
            var completeButtons = document.querySelectorAll('[data-action="complete-activity"]');
            for (var i = 0; i < completeButtons.length; i++) {
                completeButtons[i].addEventListener('click', function(e) {
                    var activityId = this.getAttribute('data-activity-id');
                    if (activityId && confirm('Mark this activity as done?')) {
                        CRMActivities.completeActivity(activityId);
                    }
                });
            }
        },

        /**
         * Mark an activity as done via AJAX.
         *
         * @param {string} activityId
         */
        completeActivity: function(activityId) {
            var csrfToken = '';
            var tokenEl = document.querySelector('input[name="_token"]');
            if (tokenEl) csrfToken = tokenEl.value;

            var xhr = new XMLHttpRequest();
            xhr.open('POST', 'activity_action.php', true);
            xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
            xhr.onreadystatechange = function() {
                if (xhr.readyState === 4 && xhr.status === 200) {
                    window.location.reload();
                }
            };
            xhr.send('action=complete&activity_id=' + encodeURIComponent(activityId) + '&_token=' + encodeURIComponent(csrfToken));
        },

        /**
         * Count activities due today and display a notification badge.
         *
         * @param {number} count
         */
        showDueTodayBadge: function(count) {
            if (count <= 0) return;
            var badge = document.createElement('span');
            badge.className = 'crm-due-today-badge';
            badge.textContent = count + ' due today';
            badge.style.cssText = 'background:#ff9800;color:#fff;padding:2px 8px;border-radius:10px;font-size:0.8em;margin-left:10px;';
            var heading = document.querySelector('h2, .display_heading');
            if (heading) {
                heading.appendChild(badge);
            }
        }
    };

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', CRMActivities.init);
    } else {
        CRMActivities.init();
    }

    window.CRMActivities = CRMActivities;
})();
