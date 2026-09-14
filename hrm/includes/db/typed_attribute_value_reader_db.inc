<?php
/** HRM-FND-007 permissioned exact-version decrypted Typed Attribute reader. */
if (!function_exists('hrm_typed_attribute_value_append_version'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_value_writer_db.inc');

function hrm_typed_attribute_value_read_as_of($attribute_code,$subject_type,$subject_id,$as_of)
{
    $attribute_code=hrm_typed_attribute_value_attribute_code($attribute_code);
    $subject_id=hrm_typed_attribute_value_positive_id($subject_id,false);
    $as_of=hrm_typed_attribute_value_datetime($as_of,false);
    if($attribute_code===false||$subject_id===false||$as_of===false
        ||!in_array($subject_type,hrm_typed_attribute_subject_types(),true)
        ||!hrm_typed_attribute_value_subject_exists($subject_type,$subject_id,false))return false;
    $definition=hrm_typed_attribute_value_resolve_definition($attribute_code,$as_of,false);
    if($definition===false)return null;
    if((string)$definition['allowed_subject']!==$subject_type
        ||!hrm_typed_attribute_value_user_can_view((string)$definition['sensitivity']))return false;
    $sql='SELECT d.definition_id,d.attribute_code,v.definition_version_id,v.value_type,v.allowed_subject,v.sensitivity,'
        .'v.owner_scope,v.validation_json,v.definition_status,v.effective_from definition_effective_from,'
        .'v.effective_to definition_effective_to,a.attribute_value_id,a.value_version_no,a.predecessor_id,'
        .'a.effective_from,a.effective_to,a.key_id,a.value_ciphertext,a.ciphertext_sha256,a.source '
        .'FROM '.TB_PREF.'hrm_attribute_definitions d INNER JOIN '.TB_PREF.'hrm_attribute_definition_versions v '
        .'ON v.definition_id=d.definition_id INNER JOIN '.TB_PREF.'hrm_attribute_values a '
        .'ON a.definition_version_id=v.definition_version_id WHERE d.attribute_code='.db_escape($attribute_code)
        .' AND v.definition_version_id='.(int)$definition['definition_version_id']
        .' AND a.subject_type='.db_escape($subject_type).' AND a.subject_id='.(int)$subject_id
        .' AND a.effective_from<='.db_escape($as_of).' AND (a.effective_to IS NULL OR a.effective_to>='.db_escape($as_of).') '
        .'ORDER BY a.effective_from DESC,a.value_version_no DESC,a.attribute_value_id DESC LIMIT 2';
    $r=db_query($sql,'could not read governed Typed Attribute value');
    if(!$r||db_num_rows($r)<1)return null;
    $row=db_fetch_assoc($r);
    if((int)$row['definition_version_id']!==(int)$definition['definition_version_id']
        ||(string)$row['definition_status']!=='active'||(string)$row['allowed_subject']!==$subject_type
        ||!preg_match('/^[a-f0-9]{64}$/D',(string)$row['ciphertext_sha256'])
        ||!function_exists('domain_audit_hash_equals')
        ||!domain_audit_hash_equals(hash('sha256',$row['value_ciphertext']),(string)$row['ciphertext_sha256']))return false;
    $key=hrm_person_sensitive_key((string)$row['key_id']);
    $context=hrm_typed_attribute_crypto_context((int)$row['definition_version_id'],$subject_type,$subject_id,
        (int)$row['value_version_no'],$row['predecessor_id']===null?null:(int)$row['predecessor_id']);
    $plain=$key===false||$context===false?false:hrm_person_sensitive_decrypt($row['value_ciphertext'],$context,$key);
    if(!is_string($plain)||hrm_typed_attribute_value_apply_validation($definition,$plain)!==$plain)return false;
    return array('attribute_code'=>$attribute_code,'definition_id'=>(int)$row['definition_id'],
        'definition_version_id'=>(int)$row['definition_version_id'],'attribute_value_id'=>(int)$row['attribute_value_id'],
        'subject_type'=>$subject_type,'subject_id'=>$subject_id,'value_type'=>(string)$row['value_type'],
        'sensitivity'=>(string)$row['sensitivity'],'owner_scope'=>(string)$row['owner_scope'],'value'=>$plain,
        'value_version_no'=>(int)$row['value_version_no'],'effective_from'=>(string)$row['effective_from'],
        'effective_to'=>$row['effective_to']===null?null:(string)$row['effective_to'],'source'=>(string)$row['source']);
}
