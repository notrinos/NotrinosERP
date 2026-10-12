<?php
/** Reader schema proofs and immediate verified-selection handoff under transaction locks; authorization is never cached. */

/** Reuse only the immediately verified same-scope selection held by the reader's period/key/metadata locks. */
function pay_glb_001_native_reader_projection_selection(array $scope)
{
    if(!pay_glb_001_native_reader_schema_cached('finance')
        || !isset($GLOBALS['pay_glb_001_native_reader_schema_context']['projection_selection']))return false;
    $memo=$GLOBALS['pay_glb_001_native_reader_schema_context']['projection_selection'];
    return $memo['scope']===$scope && $memo['latest']['event_kind']==='select'?$memo['latest']:false;
}

/** Discard schema proofs only after the shared outer SQL transaction completes. */
function pay_glb_001_native_reader_schema_release()
{
    unset($GLOBALS['pay_glb_001_native_reader_schema_context']);
    return true;
}

/** Reuse only an exact schema proof on the same live reader transaction, connection, database and actor. */
function pay_glb_001_native_reader_schema_cached($kind)
{
    if(empty($GLOBALS['transaction_level']) || !empty($GLOBALS['transaction_resources_releasing'])
        || !isset($GLOBALS['pay_glb_001_native_reader_schema_context']))return false;
    $c=$GLOBALS['pay_glb_001_native_reader_schema_context'];
    if(!isset($c['proofs'][$kind]) || $c['proofs'][$kind]!==true || $GLOBALS['db']!==$c['db']
        || !isset($_SESSION['wa_current_user']->user) || (int)$_SESSION['wa_current_user']->user!==$c['actor']
        || (int)user_company()!==$c['company']
        || hrm_migration_active_table_prefix()!==$c['prefix']
        || !isset($GLOBALS['transaction_resource_releases']['pay_glb.native.reader_schema'])
        || $GLOBALS['transaction_resource_releases']['pay_glb.native.reader_schema']!=='pay_glb_001_native_reader_schema_release')return false;
    $row=hrm_pay_core_004_read_one('SELECT CONNECTION_ID() connection_id,DATABASE() database_name');
    return $row && (string)$row['connection_id']===$c['connection_id'] && $row['database_name']===$c['database_name'];
}

/** Return raw engine catalog evidence only under the same reader's retained metadata/transaction ownership; no schema or authorization decision is reused. */
function pay_glb_001_native_reader_engine_metadata($table)
{
    if(empty($GLOBALS['transaction_level']) || !empty($GLOBALS['transaction_resources_releasing'])
        || !isset($GLOBALS['pay_glb_001_native_reader_schema_context']))return null;
    $c=$GLOBALS['pay_glb_001_native_reader_schema_context'];
    if(!isset($c['engines']) || $GLOBALS['db']!==$c['db'] || !isset($_SESSION['wa_current_user']->user)
        || (int)$_SESSION['wa_current_user']->user!==$c['actor'] || (int)user_company()!==$c['company']
        || hrm_migration_active_table_prefix()!==$c['prefix']
        || !isset($GLOBALS['transaction_resource_releases']['pay_glb.native.reader_schema'])
        || $GLOBALS['transaction_resource_releases']['pay_glb.native.reader_schema']!=='pay_glb_001_native_reader_schema_release')return null;
    $row=hrm_pay_core_004_read_one('SELECT CONNECTION_ID() connection_id,DATABASE() database_name');
    $physical=hrm_migration_physical_table_name($table);
    if(!$row || (string)$row['connection_id']!==$c['connection_id'] || $row['database_name']!==$c['database_name']
        || $physical===false || !array_key_exists($physical,$c['engines']))return null;
    return (string)$c['engines'][$physical];
}

/** Supply actual trigger rows to the unchanged validators only while their table metadata is locked by this exact reader owner. */
function pay_glb_001_native_reader_trigger_metadata($table,$name)
{
    if(pay_glb_001_native_reader_engine_metadata($table)===null)return null;
    $c=$GLOBALS['pay_glb_001_native_reader_schema_context'];if(!isset($c['triggers']))return null;
    if(!isset($c['triggers'][$name]))return false;
    return $c['triggers'][$name];
}

/** Lock every validated table against DDL, then run the original complete schema validators once per page. */
function pay_glb_001_native_reader_schema_begin(array $principal,$started,&$error=null)
{
    if(!isset($GLOBALS['transaction_level']) || $GLOBALS['transaction_level']!==1
        || isset($GLOBALS['pay_glb_001_native_reader_schema_context'])
        || !isset($GLOBALS['pay_glb_001_native_key_lock_company'])
        || (int)$GLOBALS['pay_glb_001_native_key_lock_company']!==(int)$principal['company_id']){
        $error='native_reader_schema_owner_invalid';return false;
    }
    $row=hrm_pay_core_004_read_one('SELECT CONNECTION_ID() connection_id,DATABASE() database_name');
    $prefix=hrm_migration_active_table_prefix();
    if(!$row || !is_string($row['database_name']) || $row['database_name']==='' || $prefix===false
        || !register_transaction_resource_release('pay_glb.native.reader_schema','pay_glb_001_native_reader_schema_release')){
        $error='native_reader_schema_connection_unavailable';return false;
    }
    $GLOBALS['pay_glb_001_native_reader_schema_context']=array('db'=>$GLOBALS['db'],
        'connection_id'=>(string)$row['connection_id'],'database_name'=>$row['database_name'],
        'company'=>(int)$principal['company_id'],'actor'=>(int)$principal['actor_user_id'],'prefix'=>$prefix,'proofs'=>array(),'started'=>$started);
    $tables=array();
    foreach(pay_glb_001_native_schema_definitions() as $model){
        $tables[$model['name']]=true;foreach($model['fks'] as $fk)$tables[$fk['table']]=true;
    }
    foreach(hrm_pay_core_010_columns() as $name=>$columns)$tables[$name]=true;
    foreach(array('hrm_persons','hrm_person_names','hrm_workers','hrm_employee_worker_map') as $name)$tables[$name]=true;
    foreach(pay_glb_001_native_calendar_tables() as $name=>$code)$tables[$name]=true;
    foreach(pay_gl_001_completion_base_columns() as $name=>$columns)$tables[$name]=true;
    $groups=pay_gl_001_completion_groups();
    foreach(array('mapping_governance','writer_reconciliation','posting_bridge','reversal_recovery','legacy_cutover') as $group)
        foreach($groups[$group]['tables'] as $name=>$columns)$tables[$name]=true;
    ksort($tables,SORT_STRING);
    foreach($tables as $name=>$unused){
        $physical=hrm_migration_physical_table_name($name);
        if($physical===false || !db_query('SELECT 1 FROM `'.$physical.'` LIMIT 1') || microtime(true)-$started>60){
            $error='native_reader_schema_metadata_lock_unavailable';return false;
        }
    }
    $physical_names=array();foreach($tables as $name=>$unused){$physical=hrm_migration_physical_table_name($name);if($physical===false)return false;$physical_names[$physical]=true;}
    $literals=array();foreach($physical_names as $name=>$unused)$literals[]=db_escape($name);
    $q=db_query('SELECT TABLE_NAME,ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('.implode(',',$literals).')');
    if(!$q || db_num_rows($q)!==count($physical_names)){$error='native_reader_engine_metadata_unavailable';return false;}
    $engines=array();while($r=db_fetch_assoc($q)){
        if(!isset($physical_names[$r['TABLE_NAME']]) || array_key_exists($r['TABLE_NAME'],$engines)){$error='native_reader_engine_metadata_unavailable';return false;}
        $engines[$r['TABLE_NAME']]=$r['ENGINE'];
    }
    $GLOBALS['pay_glb_001_native_reader_schema_context']['engines']=$engines;
    $q=db_query('SELECT TRIGGER_NAME,EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND EVENT_OBJECT_TABLE IN ('.implode(',',$literals).')');
    if(!$q){$error='native_reader_trigger_metadata_unavailable';return false;}$triggers=array();
    while($r=db_fetch_assoc($q)){
        if(!isset($physical_names[$r['EVENT_OBJECT_TABLE']]) || isset($triggers[$r['TRIGGER_NAME']])){$error='native_reader_trigger_metadata_unavailable';return false;}
        $triggers[$r['TRIGGER_NAME']]=$r;
    }
    $GLOBALS['pay_glb_001_native_reader_schema_context']['triggers']=$triggers;

    // Proof flags stay empty until every original validator has succeeded under retained MDL.
    if(!pay_glb_001_native_finance_require_schema($error) || !hrm_person_worker_tables_ready()
        || !pay_glb_001_native_calendar_require_schema($error) || microtime(true)-$started>60)return false;
    $GLOBALS['pay_glb_001_native_reader_schema_context']['proofs']=array('native'=>true,'core010'=>true,'projection'=>true,'finance'=>true,'person_worker'=>true);
    return true;
}
