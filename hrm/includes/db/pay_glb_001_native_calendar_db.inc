<?php
/** Read existing CORE002 governed native calendar custody; no calendar writer or statutory authority. */
require_once dirname(dirname(dirname(dirname(__FILE__)))).'/includes/pay_glb_001_native_calendar_view.inc';
require_once dirname(__FILE__).'/pay_core_002_governance_schema_db.inc';

/** Enumerate the existing immutable tables needed for a calendar provenance proof. */
function pay_glb_001_native_calendar_tables()
{
    return array('hrm_payroll_calendars'=>'cal','hrm_payroll_calendar_versions'=>'calv','hrm_payroll_calendar_periods'=>'per',
        'hrm_payroll_groups'=>'grp','hrm_payroll_group_versions'=>'grpv','hrm_payroll_seed_proposals'=>'sdp',
        'hrm_payroll_seed_approvals'=>'sda','hrm_payroll_seed_executions'=>'sdx');
}

/** Revalidate original columns/engine and exact denial bodies without installing or repairing history. */
function pay_glb_001_native_calendar_require_schema(&$error=null)
{
    $columns=hrm_pay_core_002_expected_columns()+hrm_pay_core_002_successor_expected_columns();
    $expected_triggers=array();$physical_tables=array();
    foreach(pay_glb_001_native_calendar_tables() as $table=>$code){
        $physical=hrm_migration_physical_table_name($table);if($physical===false || isset($physical_tables[$physical])){$error='native_calendar_schema_invalid';return false;}
        $physical_tables[$physical]=$columns[$table];
        foreach(array('UPDATE','DELETE') as $operation){
            $name=hrm_pay_core_002_trigger_name($code,$operation);
            $expected_triggers[$name]=array('operation'=>$operation,'physical'=>$physical,
                'body'=>"SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies ".strtolower($operation).' on '.$table."'");
        }
    }
    // Exact same ordered column/engine evidence, fetched once for this fixed table set.
    $table_names=array();foreach($physical_tables as $physical=>$expected)$table_names[]=db_escape($physical);
    $filter='TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('.implode(',',$table_names).')';
    $q=db_query('SELECT TABLE_NAME,ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE '.$filter);
    if(!$q || db_num_rows($q)!==count($physical_tables)){$error='native_calendar_schema_invalid';return false;}
    $seen=array();while($r=db_fetch_assoc($q)){
        if(!isset($physical_tables[$r['TABLE_NAME']]) || isset($seen[$r['TABLE_NAME']]) || strcasecmp((string)$r['ENGINE'],'InnoDB')!==0){$error='native_calendar_schema_invalid';return false;}
        $seen[$r['TABLE_NAME']]=true;
    }
    $q=db_query('SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION FROM INFORMATION_SCHEMA.COLUMNS WHERE '.$filter.' ORDER BY TABLE_NAME,ORDINAL_POSITION');
    if(!$q){$error='native_calendar_schema_invalid';return false;}$actual=array();
    while($r=db_fetch_assoc($q)){
        $physical=$r['TABLE_NAME'];if(!isset($physical_tables[$physical])){$error='native_calendar_schema_invalid';return false;}
        if(!isset($actual[$physical]))$actual[$physical]=array();
        if((int)$r['ORDINAL_POSITION']!==count($actual[$physical])+1){$error='native_calendar_schema_invalid';return false;}
        $actual[$physical][]=(string)$r['COLUMN_NAME'];
    }
    foreach($physical_tables as $physical=>$expected)if(!isset($actual[$physical]) || $actual[$physical]!==$expected){$error='native_calendar_schema_invalid';return false;}
    // One scoped catalog scan retains every original exact name/event/table/timing/body invariant.
    $names=array();foreach($expected_triggers as $name=>$expected)$names[]=db_escape($name);
    $q=db_query('SELECT TRIGGER_NAME,EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS '
        .'WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME IN ('.implode(',',$names).')');
    if(!$q || db_num_rows($q)!==count($expected_triggers)){$error='native_calendar_immutable_trigger_invalid';return false;}
    $seen=array();
    while($r=db_fetch_assoc($q)){
        $name=$r['TRIGGER_NAME'];if(!isset($expected_triggers[$name]) || isset($seen[$name])){$error='native_calendar_immutable_trigger_invalid';return false;}
        $seen[$name]=true;$expected=$expected_triggers[$name];
        $body=is_string($r['ACTION_STATEMENT'])?preg_replace('/\s+/',' ',trim($r['ACTION_STATEMENT'])):false;
        if(strtoupper($r['EVENT_MANIPULATION'])!==$expected['operation'] || $r['EVENT_OBJECT_TABLE']!==$expected['physical']
            || strtoupper($r['ACTION_TIMING'])!=='BEFORE' || !is_string($body)
            || strtolower(preg_replace('/\s*=\s*/','=',$body))!==strtolower($expected['body'])){
            $error='native_calendar_immutable_trigger_invalid';return false;
        }
    }
    return true;
}

/** Load one current selected scope's complete seed provenance under the reader-owned transaction. */
function pay_glb_001_native_calendar_collect(array $scope,&$error=null)
{
    $error=null;
    if(empty($GLOBALS['transaction_level']) || $GLOBALS['transaction_level']!==1
        || !isset($scope['group_version_id'],$scope['payroll_group_id'],$scope['calendar_period_id'],$scope['legal_entity_id'],$scope['currency_code'])){
        $error='native_calendar_owner_or_scope_invalid';return false;
    }
    foreach(array('group_version_id','payroll_group_id','calendar_period_id','legal_entity_id') as $field)
        if(!is_string($scope[$field]) || !ctype_digit($scope[$field]) || (int)$scope[$field]<1){$error='native_calendar_scope_invalid';return false;}
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_payroll_seed_executions WHERE group_version_id='.(int)$scope['group_version_id'].' LIMIT 2 FOR UPDATE');
    if(!$q || db_num_rows($q)>1){$error='native_calendar_execution_ambiguous';return false;}
    if(db_num_rows($q)===0)return pay_glb_001_native_calendar_unknown();
    $execution=pay_glb_001_native_producer_sql_strings(db_fetch_assoc($q),$error);
    if(!$execution || (string)$execution['payroll_group_id']!==$scope['payroll_group_id'] || $execution['currency_code']!==$scope['currency_code']){
        $error='native_calendar_execution_scope_mismatch';return false;
    }
    $bundle=array('execution'=>$execution);
    $sources=array('proposal'=>array('hrm_payroll_seed_proposals','proposal_id',$execution['proposal_id']),
        'approval'=>array('hrm_payroll_seed_approvals','approval_id',$execution['approval_id']),
        'calendar'=>array('hrm_payroll_calendars','calendar_id',$execution['calendar_id']),
        'version'=>array('hrm_payroll_calendar_versions','calendar_version_id',$execution['calendar_version_id']),
        'group'=>array('hrm_payroll_groups','payroll_group_id',$execution['payroll_group_id']),
        'group_version'=>array('hrm_payroll_group_versions','group_version_id',$execution['group_version_id']));
    foreach($sources as $kind=>$source){
        $q=db_query('SELECT * FROM '.TB_PREF.$source[0].' WHERE '.$source[1].'='.(int)$source[2].' LIMIT 2 FOR UPDATE');
        if(!$q || db_num_rows($q)!==1){$error='native_calendar_provenance_missing';return false;}
        $bundle[$kind]=pay_glb_001_native_producer_sql_strings(db_fetch_assoc($q),$error);if($bundle[$kind]===false)return false;
    }
    if((string)$bundle['proposal']['legal_entity_id']!==$scope['legal_entity_id']){$error='native_calendar_entity_mismatch';return false;}
    $bundle['periods']=array();$last='0';
    $started=isset($GLOBALS['pay_glb_001_native_reader_schema_context']['started'])?$GLOBALS['pay_glb_001_native_reader_schema_context']['started']:microtime(true);
    do{
        $q=db_query('SELECT * FROM '.TB_PREF.'hrm_payroll_calendar_periods WHERE calendar_version_id='.(int)$execution['calendar_version_id']
            .' AND calendar_period_id>'.db_escape($last).' ORDER BY calendar_period_id LIMIT 512 FOR UPDATE');
        if(!$q || microtime(true)-$started>60){$error='native_calendar_periods_unavailable';return false;}$count=db_num_rows($q);
        while($row=db_fetch_assoc($q)){
            $row=pay_glb_001_native_producer_sql_strings($row,$error);if($row===false)return false;
            $bundle['periods'][]=$row;$last=$row['calendar_period_id'];
        }
    }while($count===512);
    return pay_glb_001_native_calendar_verify($bundle,$scope['calendar_period_id'],$error);
}
