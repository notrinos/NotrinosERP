<?php
/** PAY-CTRY-007 in-app regulatory update notification provider. */
include_once(dirname(__FILE__).'/notification_manager.inc');
include_once(dirname(__FILE__).'/../hrm/includes/db/pay_ctry_007_notification_db.inc');

class PayCtry007NotificationProvider implements NotificationProvider
{
    private $user_id;
    private $count_cache=null;
    public function __construct($user_id){$this->user_id=(int)$user_id;}
    public function getSectionId(){return 'pay_ctry_update';}
    public function getSectionLabel(){return _('Regulatory Updates');}
    public function getSectionIcon(){return 'alert-triangle';}
    public function getCount(){if($this->count_cache!==null)return $this->count_cache;$this->count_cache=count(pay_ctry_007_get_visible_deliveries($this->user_id,100));return $this->count_cache;}
    public function getItems($limit=5){global $path_to_root;$limit=(int)$limit;if($limit<1)$limit=1;if($limit>20)$limit=20;$rows=pay_ctry_007_get_visible_deliveries($this->user_id,$limit);$items=array();foreach($rows as $row){$title=sprintf(_('Country pack update: %s'),(string)$row['pack_code']);$summary=sprintf(_('%s → %s · %s · action: %s'),(string)$row['current_pack_version'],(string)$row['successor_pack_version'],(string)$row['severity'],(string)$row['required_action']);$items[]=array('id'=>(int)$row['delivery_outbox_id'],'title'=>$title,'summary'=>$summary,'time'=>(string)$row['deadline_at'],'url'=>$path_to_root.'/hrm/inquiry/pay_ctry_update_notices.php','type'=>(string)$row['severity'],'is_read'=>false); }return $items;}
    public function getViewAllUrl(){global $path_to_root;return $path_to_root.'/hrm/inquiry/pay_ctry_update_notices.php';}
}
?>
