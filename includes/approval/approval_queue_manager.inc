<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

/**
 * Approval Queue Manager
 *
 * Centralized background processing queue for:
 *   - Approval escalation processing (overdue drafts)
 *   - Approval notification email dispatch
 *   - Batch job coordination with logging
 *
 * The Queue Manager provides a common execution context for cron-driven
 * and admin-triggered background operations. It wraps the existing
 * escalation timer logic and notification sender into a single,
 * consistent entry point with structured result reporting.
 *
 * PHP 5.6+ compatible.
 *
 * @package Approval
 * @since   1.0.1
 */

include_once(dirname(__FILE__) . '/approval_escalation.inc');
include_once(dirname(__FILE__) . '/approval_notify.inc');
include_once(dirname(__FILE__) . '/db/approval_db.inc');
include_once(dirname(__FILE__) . '/db/approval_history_db.inc');

/**
 * Queue Manager — orchestrates background approval tasks.
 *
 * Usage:
 *   $qm = new ApprovalQueueManager();
 *   $result = $qm->processAll();
 *   // or individually:
 *   $result = $qm->processEscalations();
 *   $result = $qm->processNotifications(100);
 */
class ApprovalQueueManager
{
    /** @var array Accumulated log messages during processing */
    private $log = array();

    /** @var int Unix timestamp when processing started */
    private $startedAt;

    /** @var int Maximum runtime in seconds before batching stops */
    private $maxRuntime;

    /**
     * Constructor.
     *
     * @param int $maxRuntime Maximum total runtime in seconds (default 120).
     */
    public function __construct($maxRuntime = 120)
    {
        $this->maxRuntime = max(10, (int)$maxRuntime);
        $this->startedAt = time();
        $this->log = array();
    }

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------

    /**
     * Process all queue items (escalations + notifications).
     *
     * @return array Structured result with counts, messages, and log.
     */
    public function processAll()
    {
        $escalationResult = $this->processEscalations();
        $notificationResult = $this->processNotifications(100);

        return array(
            'success'         => true,
            'escalations'     => $escalationResult,
            'notifications'   => $notificationResult,
            'runtime_seconds' => time() - $this->startedAt,
            'log'             => $this->log,
        );
    }

    /**
     * Process overdue approval escalations.
     *
     * Iterates through all drafts past their escalation threshold,
     * moving them to the configured escalation level, sending
     * notifications, and logging each action.
     *
     * @return array Structured escalation result.
     */
    public function processEscalations()
    {
        $this->logMessage('Starting escalation processing.');

        $summary = process_approval_escalations();

        $this->logMessage(sprintf(
            'Escalation complete: %d processed, %d escalated, %d reminded, %d skipped.',
            $summary['processed'],
            $summary['escalated'],
            $summary['reminded'],
            $summary['skipped']
        ));

        foreach ($summary['details'] as $detail) {
            $this->logMessage('  ' . $detail['message']);
        }

        return $summary;
    }

    /**
     * Process pending email notifications.
     *
     * Sends queued emails up to the batch limit or maximum runtime.
     *
     * @param int $batchSize Maximum notifications to process (default 100).
     * @return array Structured notification result.
     */
    public function processNotifications($batchSize = 100)
    {
        $this->logMessage('Starting notification processing.');

        $sent = process_approval_email_notifications((int)$batchSize);

        $this->logMessage(sprintf(
            'Notification processing complete: %d email(s) sent.',
            $sent
        ));

        return array(
            'sent' => $sent,
        );
    }

    /**
     * Determine whether there is remaining runtime budget.
     *
     * Used by external callers that want to batch-respect the time limit.
     *
     * @return bool True if more processing can be done.
     */
    public function hasRemainingRuntime()
    {
        return (time() - $this->startedAt) < $this->maxRuntime;
    }

    /**
     * Get the accumulated processing log.
     *
     * @return array Array of log message strings.
     */
    public function getLog()
    {
        return $this->log;
    }

    /**
     * Get the elapsed processing time in seconds.
     *
     * @return int
     */
    public function getElapsedSeconds()
    {
        return time() - $this->startedAt;
    }

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * Append a timestamped message to the internal log.
     *
     * @param string $message
     */
    private function logMessage($message)
    {
        $this->log[] = '[' . date('Y-m-d H:i:s') . '] ' . $message;
    }
}
