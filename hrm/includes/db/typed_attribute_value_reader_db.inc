<?php
/** HRM-FND-007 permissioned exact-version decrypted Typed Attribute reader. */
if (!function_exists('hrm_typed_attribute_value_append_version'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_value_writer_db.inc');

/**
 * Lifecycle rows are state-only immutable successors. Existing ciphertext stays
 * bound to the exact active definition version that originally governed it.
 * The bound metadata must still match the current lifecycle state's copied
 * metadata before that ciphertext is eligible for return.
 */
function hrm_typed_attribute_value_definition_metadata_matches($left,$right)
{
    if(!is_array($left)||!is_array($right)
        ||(int)$left['definition_id']!==(int)$right['definition_id'])return false;
    foreach(array('value_type','allowed_subject','sensitivity','owner_scope','validation_json') as $field)
        if((string)$left[$field]!==(string)$right[$field])return false;
    return ($left['effective_to']===null?null:(string)$left['effective_to'])
        ===($right['effective_to']===null?null:(string)$right['effective_to']);
}

function hrm_typed_attribute_value_definition_version_row($definition_version_id)
{
    $definition_version_id=hrm_typed_attribute_value_positive_id($definition_version_id,false);
    if($definition_version_id===false)return false;
    $r=db_query('SELECT definition_version_id,definition_id,predecessor_id,value_type,allowed_subject,sensitivity,'
        .'owner_scope,validation_json,definition_status,effective_from,effective_to FROM '
        .TB_PREF.'hrm_attribute_definition_versions WHERE definition_version_id='.(int)$definition_version_id.' LIMIT 2',
        'could not validate Typed Attribute lifecycle definition lineage');
    return $r&&db_num_rows($r)===1?db_fetch_assoc($r):false;
}

function hrm_typed_attribute_value_lifecycle_edge_is_state_only($child,$parent)
{
    if(!hrm_typed_attribute_value_definition_metadata_matches($child,$parent))return false;
    $child_status=(string)$child['definition_status'];
    $parent_status=(string)$parent['definition_status'];
    if($child_status==='deprecating')return $parent_status==='active';
    if($child_status==='retiring')return in_array($parent_status,array('active','deprecated'),true);
    if($child_status==='reactivating')return $parent_status==='deprecated';
    if($child_status==='deprecated')return $parent_status==='deprecating';
    if($child_status==='retired')return $parent_status==='retiring';
    if($child_status==='active')return $parent_status==='reactivating';
    return false;
}

/**
 * Only state-only lifecycle successors may inherit ciphertext from an older
 * active definition version. A newly published/effective active definition is
 * a new semantic binding boundary even when its metadata happens to compare
 * equal byte-for-byte.
 */
function hrm_typed_attribute_value_bound_definition_matches_state($bound,$state)
{
    if(!is_array($bound)||!is_array($state)
        ||(int)$bound['definition_id']!==(int)$state['definition_id']
        ||(string)$bound['definition_status']!=='active')return false;
    foreach(array('value_type','allowed_subject','sensitivity','owner_scope','validation_json') as $field)
        if((string)$bound[$field]!==(string)$state[$field])return false;
    $bound_id=(int)$bound['definition_version_id'];
    $state_id=(int)$state['definition_version_id'];
    if($bound_id<=0||$state_id<=0)return false;
    if($bound_id===$state_id)return true;

    $current=hrm_typed_attribute_value_definition_version_row($state_id);
    for($depth=0;$depth<64&&is_array($current);++$depth){
        if($current['predecessor_id']===null)return false;
        $parent=hrm_typed_attribute_value_definition_version_row((int)$current['predecessor_id']);
        if(!is_array($parent)||!hrm_typed_attribute_value_lifecycle_edge_is_state_only($current,$parent))return false;
        if((int)$parent['definition_version_id']===$bound_id)return true;
        $current=$parent;
    }
    return false;
}

function hrm_typed_attribute_value_read_as_of($attribute_code,$subject_type,$subject_id,$as_of)
{
    $attribute_code=hrm_typed_attribute_value_attribute_code($attribute_code);
    $subject_id=hrm_typed_attribute_value_positive_id($subject_id,false);
    $as_of=hrm_typed_attribute_value_datetime($as_of,false);
    if($attribute_code===false||$subject_id===false||$as_of===false
        ||!in_array($subject_type,hrm_typed_attribute_subject_types(),true)
        ||!hrm_typed_attribute_value_subject_exists($subject_type,$subject_id,false))return false;
    $definition=hrm_typed_attribute_value_resolve_definition($attribute_code,$as_of,false,'read');
    if($definition===false)return null;
    if((string)$definition['allowed_subject']!==$subject_type
        ||!hrm_typed_attribute_value_user_can_view((string)$definition['sensitivity'])
        ||!hrm_typed_attribute_value_user_can_access_object_scope((string)$definition['owner_scope']))return false;
    $sql='SELECT d.definition_id,d.attribute_code,v.definition_version_id,v.value_type,v.allowed_subject,v.sensitivity,'
        .'v.owner_scope,v.validation_json,v.definition_status,v.effective_from definition_effective_from,'
        .'v.effective_to definition_effective_to,a.attribute_value_id,a.value_version_no,a.predecessor_id,'
        .'a.effective_from,a.effective_to,a.key_id,a.value_ciphertext,a.ciphertext_sha256,a.source '
        .'FROM '.TB_PREF.'hrm_attribute_definitions d INNER JOIN '.TB_PREF.'hrm_attribute_definition_versions v '
        .'ON v.definition_id=d.definition_id INNER JOIN '.TB_PREF.'hrm_attribute_values a '
        .'ON a.definition_version_id=v.definition_version_id WHERE d.definition_id='.(int)$definition['definition_id']
        .' AND d.attribute_code='.db_escape($attribute_code)
        .' AND a.subject_type='.db_escape($subject_type).' AND a.subject_id='.(int)$subject_id
        .' AND a.effective_from<='.db_escape($as_of).' AND (a.effective_to IS NULL OR a.effective_to>='.db_escape($as_of).') '
        .'ORDER BY a.effective_from DESC,a.value_version_no DESC,a.attribute_value_id DESC LIMIT 2';
    $r=db_query($sql,'could not read governed Typed Attribute value');
    if(!$r||db_num_rows($r)<1)return null;
    $row=db_fetch_assoc($r);
    if(!hrm_typed_attribute_value_bound_definition_matches_state($row,$definition))return null;
    if((string)$row['allowed_subject']!==$subject_type
        ||!preg_match('/^[a-f0-9]{64}$/D',(string)$row['ciphertext_sha256'])
        ||!function_exists('domain_audit_hash_equals')
        ||!domain_audit_hash_equals(hash('sha256',$row['value_ciphertext']),(string)$row['ciphertext_sha256']))return false;
    $key=hrm_person_sensitive_key((string)$row['key_id']);
    $context=hrm_typed_attribute_crypto_context((int)$row['definition_version_id'],$subject_type,$subject_id,
        (int)$row['value_version_no'],$row['predecessor_id']===null?null:(int)$row['predecessor_id']);
    $plain=$key===false||$context===false?false:hrm_person_sensitive_decrypt($row['value_ciphertext'],$context,$key);
    $bound_definition=array(
        'definition_id'=>(int)$row['definition_id'],'attribute_code'=>(string)$row['attribute_code'],
        'definition_version_id'=>(int)$row['definition_version_id'],'definition_status'=>(string)$row['definition_status'],
        'allowed_subject'=>(string)$row['allowed_subject'],'sensitivity'=>(string)$row['sensitivity'],
        'owner_scope'=>(string)$row['owner_scope'],'effective_from'=>(string)$row['definition_effective_from'],
        'effective_to'=>$row['definition_effective_to']===null?null:(string)$row['definition_effective_to'],
        'value_type'=>(string)$row['value_type'],'validation_json'=>(string)$row['validation_json']);
    if(!is_string($plain)||hrm_typed_attribute_value_apply_validation($bound_definition,$plain)!==$plain)return false;
    return array('attribute_code'=>$attribute_code,'definition_id'=>(int)$row['definition_id'],
        'definition_version_id'=>(int)$row['definition_version_id'],'attribute_value_id'=>(int)$row['attribute_value_id'],
        'subject_type'=>$subject_type,'subject_id'=>$subject_id,'value_type'=>(string)$row['value_type'],
        'sensitivity'=>(string)$row['sensitivity'],'owner_scope'=>(string)$row['owner_scope'],'value'=>$plain,
        'value_version_no'=>(int)$row['value_version_no'],'effective_from'=>(string)$row['effective_from'],
        'effective_to'=>$row['effective_to']===null?null:(string)$row['effective_to'],'source'=>(string)$row['source']);
}
