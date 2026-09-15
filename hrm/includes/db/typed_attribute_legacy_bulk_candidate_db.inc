<?php
/**
 * HRM-FND-007 bounded read-only bulk-conversion candidate inventory.
 *
 * This boundary never returns legacy values or decrypted Typed Attribute values.
 * It scans at most 100 Employee/Worker bindings per request and returns at most
 * 25 unconverted candidates for one exact approved mapping version. The future
 * batch command must revalidate every candidate under transaction locks.
 */
if (!function_exists('hrm_typed_attribute_legacy_custom_data_parity'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_custom_data_inventory_db.inc');
if (!function_exists('hrm_typed_attribute_legacy_conversion_extract_scalar'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_conversion_db.inc');
if (!function_exists('hrm_typed_attribute_value_resolve_definition'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_value_command_db.inc');

function hrm_typed_attribute_legacy_bulk_candidate_user_can_view()
{
    return function_exists('user_check_access')
        && user_check_access('SA_EMPLOYEE')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_MAPPING')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_CANDIDATES');
}

function hrm_typed_attribute_legacy_bulk_candidate_request($input)
{
    $allowed=array('mapping_version_id','after_employee_number','limit','as_of');
    if(!is_array($input)||array_diff(array_keys($input),$allowed)||!array_key_exists('mapping_version_id',$input))return false;
    $mapping=hrm_typed_attribute_legacy_mapping_positive_id($input['mapping_version_id'],false);
    $after=array_key_exists('after_employee_number',$input)?$input['after_employee_number']:0;
    if(is_string($after)&&preg_match('/^(0|[1-9][0-9]*)$/D',$after))$after=(int)$after;
    if(!is_int($after)||$after<0)return false;
    $limit=array_key_exists('limit',$input)?$input['limit']:25;
    if(is_string($limit)&&preg_match('/^[1-9][0-9]*$/D',$limit))$limit=(int)$limit;
    if(!is_int($limit)||$limit<1||$limit>25)return false;
    $as_of=array_key_exists('as_of',$input)?$input['as_of']:gmdate('Y-m-d');
    $as_of=hrm_typed_attribute_legacy_mapping_date($as_of,false);
    return $mapping===false||$as_of===false?false:array('mapping_version_id'=>$mapping,'after_employee_number'=>$after,'limit'=>$limit,'as_of'=>$as_of);
}

function hrm_typed_attribute_legacy_bulk_candidate_digest($mapping_version_id,$worker_id,$profile_version_id,$definition_version_id)
{
    foreach(array($mapping_version_id,$worker_id,$profile_version_id,$definition_version_id) as $id)if((int)$id<=0)return false;
    return hash('sha256','hrm-fnd-007-legacy-bulk-candidate-v1|mapping='.(int)$mapping_version_id
        .'|worker='.(int)$worker_id.'|profile='.(int)$profile_version_id.'|definition='.(int)$definition_version_id);
}

/** Return a bounded, value-free candidate page for one exact approved mapping. */
function get_hrm_typed_attribute_legacy_bulk_conversion_candidates($input)
{
    if(!hrm_typed_attribute_legacy_bulk_candidate_user_can_view())return false;
    $request=hrm_typed_attribute_legacy_bulk_candidate_request($input);if($request===false)return false;
    $q=db_query('SELECT mapping_version_id,legacy_key,version_no,predecessor_id,attribute_code,subject_type,mapping_status,'
        .'effective_from,effective_to,qualified_review_ref,approved_at,approved_by FROM '.TB_PREF.'hrm_attribute_legacy_mapping_versions '
        .'WHERE mapping_version_id='.(int)$request['mapping_version_id'].' LIMIT 2','could not read approved legacy mapping for bulk candidate inventory');
    $mapping=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$mapping||!hrm_typed_attribute_legacy_mapping_validate_metadata($mapping)||(string)$mapping['mapping_status']!=='approved'
        ||(string)$mapping['subject_type']!=='worker'||$request['as_of']<(string)$mapping['effective_from']
        ||($mapping['effective_to']!==null&&$request['as_of']>(string)$mapping['effective_to']))return false;
    $resolved=get_hrm_typed_attribute_legacy_mapping_as_of((string)$mapping['legacy_key'],$request['as_of']);
    if(!is_array($resolved)||(int)$resolved['mapping_version_id']!==(int)$mapping['mapping_version_id'])return false;
    $definition=hrm_typed_attribute_value_resolve_definition((string)$mapping['attribute_code'],$request['as_of'].' 00:00:00',false,'write');
    if(!is_array($definition)||(string)$definition['allowed_subject']!=='worker'
        ||(int)$definition['definition_version_id']!==(int)$resolved['definition_version_id']
        ||!hrm_typed_attribute_value_user_can_view((string)$definition['sensitivity'])
        ||!hrm_typed_attribute_value_user_can_access_object_scope((string)$definition['owner_scope']))return false;

    $scan_limit=100;
    $r=db_query('SELECT e.employee_number,e.custom_data,m.worker_id FROM '.TB_PREF.'employees e '
        .'INNER JOIN '.TB_PREF.'hrm_employee_worker_map m ON m.employee_number=e.employee_number AND m.retired_at IS NULL '
        .'WHERE e.employee_number>'.(int)$request['after_employee_number'].' ORDER BY e.employee_number ASC LIMIT '.$scan_limit,
        'could not scan bounded legacy Typed Attribute bulk candidates');
    if(!$r)return false;
    $rows=array();$scanned=0;$last=(int)$request['after_employee_number'];
    while(($row=db_fetch_assoc($r))!==false){
        $scanned++;$last=(int)$row['employee_number'];
        $profile=hrm_compatibility_profile_current((int)$row['worker_id']);
        $retained=$profile?hrm_compatibility_profile_open($profile):false;
        if($retained===false||!array_key_exists('custom_data',$retained)
            ||!hrm_typed_attribute_legacy_custom_data_parity($row['custom_data'],$retained['custom_data']))continue;
        $extracted=hrm_typed_attribute_legacy_conversion_extract_scalar($retained['custom_data'],(string)$mapping['legacy_key']);
        if($extracted===false||hrm_typed_attribute_value_apply_validation($definition,$extracted['value'])===false)continue;
        $receipt=db_query('SELECT conversion_id FROM '.TB_PREF.'hrm_attribute_legacy_conversion_receipts WHERE mapping_version_id='.(int)$mapping['mapping_version_id']
            .' AND worker_id='.(int)$row['worker_id'].' LIMIT 2','could not inspect legacy conversion receipt for bulk candidate inventory');
        if(!$receipt||db_num_rows($receipt)>1)return false;
        if(db_num_rows($receipt)===1)continue;
        $values=db_query('SELECT a.attribute_value_id FROM '.TB_PREF.'hrm_attribute_values a INNER JOIN '
            .TB_PREF.'hrm_attribute_definition_versions v ON v.definition_version_id=a.definition_version_id '
            .'WHERE v.definition_id='.(int)$definition['definition_id']." AND a.subject_type='worker' AND a.subject_id=".(int)$row['worker_id']
            .' ORDER BY a.value_version_no,a.attribute_value_id LIMIT 1','could not inspect existing Typed Attribute values for bulk candidate inventory');
        if(!$values)return false;
        if(db_num_rows($values)!==0)continue;
        $digest=hrm_typed_attribute_legacy_bulk_candidate_digest((int)$mapping['mapping_version_id'],(int)$row['worker_id'],
            (int)$profile['profile_version_id'],(int)$definition['definition_version_id']);
        if($digest===false)return false;
        $rows[]=array('employee_number'=>(int)$row['employee_number'],'worker_id'=>(int)$row['worker_id'],
            'profile_version_id'=>(int)$profile['profile_version_id'],'mapping_version_id'=>(int)$mapping['mapping_version_id'],
            'definition_version_id'=>(int)$definition['definition_version_id'],'candidate_digest'=>$digest,
            'plaintext_exposed'=>false,'legacy_key_retained'=>true,'requires_transactional_revalidation'=>true);
        if(count($rows)>=$request['limit'])break;
    }
    return array('mapping_version_id'=>(int)$mapping['mapping_version_id'],'legacy_key'=>(string)$mapping['legacy_key'],
        'attribute_code'=>(string)$mapping['attribute_code'],'candidate_count'=>count($rows),'scan_count'=>$scanned,
        'scan_cap'=>$scan_limit,'page_limit'=>$request['limit'],'after_employee_number'=>$request['after_employee_number'],
        'next_after_employee_number'=>$last,'plaintext_exposed'=>false,'read_only'=>true,'rows'=>$rows);
}
