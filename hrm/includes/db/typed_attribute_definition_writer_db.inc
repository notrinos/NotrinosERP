<?php
/** HRM-FND-007 immutable Typed Attribute definition draft writer. */
if (!function_exists('hrm_typed_attribute_definition_canonical_command'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_definition_command_db.inc');
if (!function_exists('hrm_person_worker_actor_id'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_worker_db.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

function hrm_typed_attribute_definition_writer_row_matches($row, $command, $actor_id)
{
    return is_array($row) && is_array($command)
        && (int)$row['created_by'] === (int)$actor_id
        && (string)$row['value_type'] === $command['value_type']
        && (string)$row['allowed_subject'] === $command['allowed_subject']
        && (string)$row['sensitivity'] === $command['sensitivity']
        && (string)$row['owner_scope'] === $command['owner_scope']
        && (string)$row['validation_json'] === $command['validation_json']
        && (string)$row['definition_status'] === 'draft'
        && (string)$row['effective_from'] === $command['effective_from']
        && (($row['effective_to'] === null ? null : (string)$row['effective_to']) === $command['effective_to'])
        && (($row['predecessor_id'] === null ? null : (int)$row['predecessor_id']) === $command['predecessor_id']);
}

function hrm_typed_attribute_definition_append_draft($input)
{
    $command = hrm_typed_attribute_definition_canonical_command($input);
    $actor_id = function_exists('hrm_person_worker_actor_id') ? hrm_person_worker_actor_id() : 0;
    if ($command === false || $actor_id <= 0 || !hrm_typed_attribute_definition_user_can_manage()) return false;
    $digest = hrm_typed_attribute_definition_command_digest($command);
    if ($digest === false) return false;

    begin_transaction();
    $ok = true; $return_id = false; $definition_id = 0; $created_definition = false;
    $code = db_escape($command['attribute_code']);
    $definition_result = db_query('SELECT definition_id,attribute_code,created_by FROM '.TB_PREF.'hrm_attribute_definitions '
        .'WHERE attribute_code='.$code.' LIMIT 2 FOR UPDATE', 'could not lock Typed Attribute definition');
    if (!$definition_result) $ok = false;
    if ($ok && db_num_rows($definition_result) > 1) $ok = false;
    if ($ok && db_num_rows($definition_result) === 1) {
        $definition = db_fetch_assoc($definition_result);
        $definition_id = (int)$definition['definition_id'];
    } elseif ($ok && $command['predecessor_id'] !== null) {
        $ok = false;
    } elseif ($ok) {
        $ok = db_query('INSERT INTO '.TB_PREF.'hrm_attribute_definitions (attribute_code,created_by) VALUES ('
            .$code.','.(int)$actor_id.')', 'could not create Typed Attribute definition identity') !== false;
        if ($ok) { $definition_id = (int)db_insert_id(); $created_definition = true; }
    }

    $rows = array(); $successor = array(); $max_version = 0; $leaf_ids = array();
    if ($ok) {
        $r = db_query('SELECT definition_version_id,definition_id,version_no,predecessor_id,value_type,allowed_subject,'
            .'sensitivity,owner_scope,validation_json,definition_status,effective_from,effective_to,created_by '
            .'FROM '.TB_PREF.'hrm_attribute_definition_versions WHERE definition_id='.(int)$definition_id
            .' ORDER BY version_no,definition_version_id FOR UPDATE', 'could not lock Typed Attribute definition versions');
        if (!$r) $ok = false;
        while ($ok && ($row = db_fetch_assoc($r))) {
            $id = (int)$row['definition_version_id']; $rows[$id] = $row;
            $max_version = max($max_version, (int)$row['version_no']);
            if ($row['predecessor_id'] !== null) {
                $pred = (int)$row['predecessor_id'];
                if (isset($successor[$pred])) { $ok = false; break; }
                $successor[$pred] = $id;
            }
        }
        foreach ($rows as $id=>$row) if (!isset($successor[$id])) $leaf_ids[] = $id;
        if (count($leaf_ids) > 1) $ok = false;
    }

    $predecessor = $command['predecessor_id'];
    if ($ok && $predecessor !== null) {
        if (!isset($rows[$predecessor])) $ok = false;
        elseif (isset($successor[$predecessor])) {
            $candidate = $rows[$successor[$predecessor]];
            if (count($leaf_ids) === 1 && (int)$leaf_ids[0] === (int)$candidate['definition_version_id']
                && hrm_typed_attribute_definition_writer_row_matches($candidate, $command, $actor_id))
                $return_id = (int)$candidate['definition_version_id'];
            else $ok = false;
        } elseif (count($leaf_ids) !== 1 || (int)$leaf_ids[0] !== (int)$predecessor) {
            $ok = false;
        } else {
            $target = $rows[$predecessor];
            if (!in_array((string)$target['definition_status'], array('draft','active'), true)) $ok = false;
            if ($ok && (string)$target['definition_status'] === 'draft'
                && (int)$target['created_by'] !== (int)$actor_id) $ok = false;
            if ($ok && (string)$target['definition_status'] === 'active'
                && strcmp($command['effective_from'], (string)$target['effective_from']) <= 0) $ok = false;
        }
    } elseif ($ok && count($rows) > 0) {
        if (count($leaf_ids) === 1
            && hrm_typed_attribute_definition_writer_row_matches($rows[$leaf_ids[0]], $command, $actor_id))
            $return_id = (int)$leaf_ids[0];
        else $ok = false;
    } elseif ($ok && !$created_definition) {
        $ok = false;
    }

    if ($ok && $return_id === false) {
        $version_no = $max_version + 1;
        $sql = 'INSERT INTO '.TB_PREF.'hrm_attribute_definition_versions '
            .'(definition_id,version_no,predecessor_id,value_type,allowed_subject,sensitivity,owner_scope,validation_json,'
            .'definition_status,effective_from,effective_to,created_by) VALUES ('
            .(int)$definition_id.','.(int)$version_no.','.($predecessor===null?'NULL':(int)$predecessor).','
            .db_escape($command['value_type']).','.db_escape($command['allowed_subject']).','
            .db_escape($command['sensitivity']).','.db_escape($command['owner_scope']).','
            .db_escape($command['validation_json']).",'draft',".db_escape($command['effective_from']).','
            .($command['effective_to']===null?'NULL':db_escape($command['effective_to'])).','.(int)$actor_id.')';
        $ok = db_query($sql, 'could not append Typed Attribute draft definition version') !== false;
        if ($ok) {
            $return_id = (int)db_insert_id();
            $receipt = append_domain_audit_event(array(
                'company_id'=>function_exists('user_company')?(int)user_company():0,
                'legal_entity_id'=>0,'entity_type'=>'hrm_typed_attribute_definition',
                'entity_key'=>array('definition_id'=>$definition_id,'definition_version_id'=>$return_id,
                    'version_no'=>$version_no,'command_digest'=>$digest),
                'action'=>'hrm.typed_attribute.definition_draft_created',
                'reason_code'=>'hrm_fnd_007_typed_attribute_definition_writer',
                'before'=>$predecessor===null?null:array('predecessor_id'=>(int)$predecessor),
                'after'=>array('definition_version_id'=>$return_id,'definition_status'=>'draft'),
                'restricted_diff'=>array('immutable_successor'=>true,'stable_attribute_code'=>true,
                    'owner_scope_is_classification_not_role_grant'=>true,'attribute_value_rows_mutated'=>0,
                    'legacy_custom_data_mapped'=>false,'payroll_consumer_activated'=>false)
            ));
            if ($receipt === false) { $ok = false; $return_id = false; }
        }
    }
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $return_id : false;
}
