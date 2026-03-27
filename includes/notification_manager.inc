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

/**********************************************************************
	NotrinosERP-1.0 Notification System
	Core Notification Manager

	Provides a pluggable notification framework that modules can hook
	into by registering NotificationProvider implementations. Themes
	call the manager to retrieve aggregated notification data for
	rendering (e.g., the bell icon dropdown in the topbar).

	Usage:
	  $manager = get_notification_manager();
	  $manager->registerProvider(new MyNotificationProvider());
	  $total   = $manager->getTotalCount();
	  $sections = $manager->getSections();

	PHP 5.6+ compatible (no return type declarations, no scalar type hints).
***********************************************************************/

/**
 * Interface that all notification providers must implement.
 *
 * Each provider represents one category of notifications (e.g., approvals,
 * leave requests, material requests). The NotificationManager aggregates
 * data from all registered providers.
 */
interface NotificationProvider
{
	/**
	 * Unique section identifier for this provider.
	 *
	 * @return string e.g., 'approval', 'leave_request', 'material_request'
	 */
	public function getSectionId();

	/**
	 * Human-readable label for this notification section.
	 *
	 * @return string e.g., 'Pending Approvals', 'Leave Requests'
	 */
	public function getSectionLabel();

	/**
	 * Icon name for this section (matches theme icon system).
	 *
	 * @return string e.g., 'check-circle', 'calendar', 'box'
	 */
	public function getSectionIcon();

	/**
	 * Count of unread/pending notifications for current user.
	 *
	 * This should be a fast query (COUNT only) since it's called
	 * on every page load for the badge number.
	 *
	 * @return int
	 */
	public function getCount();

	/**
	 * Get notification items for the dropdown display.
	 *
	 * Each item is an associative array with keys:
	 *   'id'       => int|string   Unique identifier
	 *   'title'    => string       Short title (e.g., 'Delivery Note #DN-001')
	 *   'summary'  => string       Brief description
	 *   'time'     => string       Formatted time string (e.g., '2 hours ago')
	 *   'url'      => string       Link to view the item
	 *   'type'     => string       Notification type for styling (e.g., 'pending', 'rejected')
	 *   'is_read'  => bool         Whether this notification has been read
	 *
	 * @param int $limit Maximum number of items to return
	 * @return array     Array of notification item arrays
	 */
	public function getItems($limit);

	/**
	 * URL to view all notifications for this section.
	 *
	 * @return string Full or relative URL to the inquiry/list page
	 */
	public function getViewAllUrl();
}

/**
 * Central notification manager that collects data from all providers.
 *
 * Registered providers are queried for counts and items. The manager
 * provides aggregated totals and per-section data that themes use
 * to render notification UI (bell icon badge, dropdown panel, etc.).
 */
class NotificationManager
{
	/** @var NotificationProvider[] Registered notification providers */
	private $providers = array();

	/** @var int|null Cached total count (reset per request) */
	private $total_count_cache = null;

	/**
	 * Register a notification provider.
	 *
	 * @param NotificationProvider $provider Provider instance
	 * @return void
	 */
	public function registerProvider($provider)
	{
		$section_id = $provider->getSectionId();
		$this->providers[$section_id] = $provider;
		$this->total_count_cache = null;
	}

	/**
	 * Remove a registered provider by section ID.
	 *
	 * @param string $section_id Provider section identifier
	 * @return void
	 */
	public function removeProvider($section_id)
	{
		unset($this->providers[$section_id]);
		$this->total_count_cache = null;
	}

	/**
	 * Check if a provider is registered.
	 *
	 * @param string $section_id Provider section identifier
	 * @return bool
	 */
	public function hasProvider($section_id)
	{
		return isset($this->providers[$section_id]);
	}

	/**
	 * Get all registered providers.
	 *
	 * @return NotificationProvider[]
	 */
	public function getProviders()
	{
		return $this->providers;
	}

	/**
	 * Get total notification count across all providers.
	 *
	 * Result is cached for the current request to avoid repeated queries.
	 *
	 * @return int
	 */
	public function getTotalCount()
	{
		if ($this->total_count_cache !== null)
			return $this->total_count_cache;

		$total = 0;
		foreach ($this->providers as $provider) {
			$total += (int)$provider->getCount();
		}

		$this->total_count_cache = $total;
		return $total;
	}

	/**
	 * Get notification sections with items for dropdown rendering.
	 *
	 * Returns an array of section arrays, each with:
	 *   'id'            => string
	 *   'label'         => string
	 *   'icon'          => string
	 *   'count'         => int
	 *   'items'         => array of notification items
	 *   'view_all_url'  => string
	 *
	 * Sections with zero count are included but with empty items.
	 *
	 * @param int $items_per_section Maximum items per section
	 * @return array
	 */
	public function getSections($items_per_section = 5)
	{
		$sections = array();

		foreach ($this->providers as $provider) {
			$count = (int)$provider->getCount();
			$items = array();

			if ($count > 0) {
				$items = $provider->getItems($items_per_section);
			}

			$sections[] = array(
				'id'           => $provider->getSectionId(),
				'label'        => $provider->getSectionLabel(),
				'icon'         => $provider->getSectionIcon(),
				'count'        => $count,
				'items'        => $items,
				'view_all_url' => $provider->getViewAllUrl(),
			);
		}

		return $sections;
	}

	/**
	 * Get a single section by provider ID.
	 *
	 * @param string $section_id Provider section identifier
	 * @param int    $limit      Maximum items to return
	 * @return array|null        Section data array or null
	 */
	public function getSection($section_id, $limit = 10)
	{
		if (!isset($this->providers[$section_id]))
			return null;

		$provider = $this->providers[$section_id];
		$count = (int)$provider->getCount();
		$items = $count > 0 ? $provider->getItems($limit) : array();

		return array(
			'id'           => $provider->getSectionId(),
			'label'        => $provider->getSectionLabel(),
			'icon'         => $provider->getSectionIcon(),
			'count'        => $count,
			'items'        => $items,
			'view_all_url' => $provider->getViewAllUrl(),
		);
	}

	/**
	 * Reset the cached total count.
	 *
	 * Call after marking notifications as read or when data changes.
	 *
	 * @return void
	 */
	public function resetCache()
	{
		$this->total_count_cache = null;
	}
}

/**
 * Get the singleton NotificationManager instance.
 *
 * @return NotificationManager
 */
function get_notification_manager()
{
	static $instance = null;
	if ($instance === null) {
		$instance = new NotificationManager();
	}
	return $instance;
}
