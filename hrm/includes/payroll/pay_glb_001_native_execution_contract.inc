<?php
/**
 * PAY-GLB-001 S39 pure typed receipt contract, v1.
 *
 * Hashes establish byte identity only: these helpers grant no producer,
 * approval, financial, transport or closure authority. Old CORE hash domains
 * are never rewritten. PHP 5.6 compatible; new arithmetic never uses floats.
 */

/** Return the eight exact ordered receipt field lists. */
function pay_glb_001_native_receipt_fields()
{
    return array(
        'execution' => explode(',', 'company_id,legal_entity_id,payroll_group_id,group_version_id,payroll_period_id,calendar_period_id,source_kind,producer_contract_version,producer_event_sha256,request_sha256,native_execution_key,revision_no,predecessor_execution_sha256,joint_core009_run_id,joint_core009_state_version,joint_core009_state_sha256,joint_producer_sha256,member_count,member_set_sha256,core010_result_set_sha256,gross_8,deduction_8,net_8,currency_code,maker_id,produced_at_utc'),
        'member' => explode(',', 'native_execution_key,ordinal,payroll_relationship_id,assignment_id,core004_snapshot_id,core004_result_id,snapshot_sha256,result_sha256,line_set_sha256,trace_set_sha256,joint_core009_run_item_id,joint_core009_document_sha256,cross_domain_parity_sha256'),
        'members' => explode(',', 'native_execution_key,member_count,members'),
        'selection' => explode(',', 'company_id,legal_entity_id,payroll_group_id,group_version_id,payroll_period_id,calendar_period_id,sequence_no,predecessor_selection_sha256,event_kind,native_execution_sha256,reason_code,maker_id,checker_id,command_key_sha256,occurred_at_utc'),
        'approval_binding' => explode(',', 'selection_sha256,native_execution_sha256,approval_intent_id,approval_id,intent_sha256,approval_sha256,result_set_sha256,projection_set_sha256,cutover_transition_sha256,maker_id,approver_id,binder_id,command_key_sha256'),
        'gl_binding' => explode(',', 'approval_binding_sha256,journal_intent_id,reconciliation_id,review_id,posting_authority_id,posting_id,journal_sha256,approval_sha256,posting_sha256,reconciliation_sha256,posting_kind,predecessor_posting_id,native_gl_effect_sha256,command_key_sha256,bound_by'),
        'command' => explode(',', 'company_id,command_domain,scope_sha256,command_key_sha256,expected_predecessor_sha256,request_sha256,effect_kind,effect_id,effect_sha256,actor_id,purpose_code'),
        'joint_producer' => explode(',', 'company_id,legal_entity_id,payroll_group_id,group_version_id,payroll_period_id,calendar_period_id,native_execution_key,producer_contract_version,producer_event_sha256,joint_core009_run_id,joint_core009_run_key,joint_core009_request_sha256,joint_core009_state_version,joint_core009_state_sha256,joint_core009_checkpoint_no,joint_core009_item_set_sha256,mapping_contract_version,mapping_sha256,cross_domain_parity_sha256,maker_id,produced_at_utc')
    );
}

/** Compare canonical unsigned decimal strings without native integer casts. */
function pay_glb_001_native_unsigned_compare($left, $right)
{
    if (strlen($left) !== strlen($right))
        return strlen($left) < strlen($right) ? -1 : 1;
    return strcmp($left, $right);
}

/** Validate an unsigned decimal string against a declared SQL bound. */
function pay_glb_001_native_unsigned($value, $maximum, $allow_zero=false)
{
    return is_string($value) && preg_match('/^(0|[1-9][0-9]*)$/D', $value) === 1
        && ($allow_zero || $value !== '0')
        && pay_glb_001_native_unsigned_compare($value, $maximum) <= 0;
}

/** Explicitly deny native-integer overflow before any existing CORE helper. */
function pay_glb_001_native_helper_id($value, &$error=null)
{
    $error = null;
    if (!pay_glb_001_native_unsigned($value, (string)PHP_INT_MAX)) {
        $error = 'native_helper_id_out_of_range';
        return false;
    }
    return (int)$value;
}

/** Validate one exact scale-eight DECIMAL(38,8) string. */
function pay_glb_001_native_decimal($value)
{
    return is_string($value) && strlen($value) <= 40
        && preg_match('/^-?(0|[1-9][0-9]{0,29})\.[0-9]{8}$/D', $value) === 1
        && $value !== '-0.00000000';
}

/** Add two unsigned digit strings exactly. */
function pay_glb_001_native_digit_add($left, $right)
{
    $carry = 0; $out = ''; $a = strlen($left)-1; $b = strlen($right)-1;
    while ($a >= 0 || $b >= 0 || $carry) {
        $digit = ($a >= 0 ? (int)$left[$a--] : 0)
            + ($b >= 0 ? (int)$right[$b--] : 0) + $carry;
        $out = (string)($digit % 10).$out;
        $carry = $digit >= 10 ? 1 : 0;
    }
    return $out === '' ? '0' : $out;
}

/** Subtract right from left unsigned digits, requiring left >= right. */
function pay_glb_001_native_digit_subtract($left, $right)
{
    $borrow = 0; $out = ''; $b = strlen($right)-1;
    for ($a = strlen($left)-1; $a >= 0; $a--) {
        $digit = (int)$left[$a] - ($b >= 0 ? (int)$right[$b--] : 0) - $borrow;
        $borrow = $digit < 0 ? 1 : 0;
        $out = (string)($digit < 0 ? $digit+10 : $digit).$out;
    }
    $out = ltrim($out, '0');
    return $out === '' ? '0' : $out;
}

/** Add bounded signed fixed decimals using digit-string arithmetic. */
function pay_glb_001_native_decimal_add($left, $right, &$error=null)
{
    $error = null;
    if (!pay_glb_001_native_decimal($left) || !pay_glb_001_native_decimal($right)) {
        $error = 'decimal_noncanonical';
        return false;
    }
    $negative_left = $left[0] === '-'; $negative_right = $right[0] === '-';
    $a = ltrim(str_replace(array('-', '.'), '', $left), '0');
    $b = ltrim(str_replace(array('-', '.'), '', $right), '0');
    if ($a === '') $a = '0';
    if ($b === '') $b = '0';
    if ($negative_left === $negative_right) {
        $digits = pay_glb_001_native_digit_add($a, $b);
        $negative = $negative_left;
    } else {
        $comparison = pay_glb_001_native_unsigned_compare($a, $b);
        $digits = $comparison >= 0 ? pay_glb_001_native_digit_subtract($a, $b)
            : pay_glb_001_native_digit_subtract($b, $a);
        $negative = $comparison >= 0 ? $negative_left : $negative_right;
    }
    if (strlen($digits) > 38) { $error = 'decimal_sum_out_of_range'; return false; }
    $digits = str_pad($digits, 9, '0', STR_PAD_LEFT);
    return ($negative && trim($digits, '0') !== '' ? '-' : '')
        .substr($digits, 0, -8).'.'.substr($digits, -8);
}

/** Verify gross minus deductions equals net without float conversion. */
function pay_glb_001_native_totals_match($gross, $deduction, $net)
{
    if (!pay_glb_001_native_decimal($deduction) || !pay_glb_001_native_decimal($net))
        return false;
    $inverse = $deduction === '0.00000000' ? $deduction
        : ($deduction[0] === '-' ? substr($deduction, 1) : '-'.$deduction);
    return pay_glb_001_native_decimal_add($gross, $inverse, $error) === $net;
}

/** Validate strict UTC seconds including real calendar dates and years. */
function pay_glb_001_native_timestamp($value)
{
    if (!is_string($value)
        || preg_match('/^[1-9][0-9]{3}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$/D', $value) !== 1)
        return false;
    $date = DateTimeImmutable::createFromFormat('!Y-m-d\TH:i:s\Z', $value, new DateTimeZone('UTC'));
    return $date !== false && $date->format('Y-m-d\TH:i:s\Z') === $value;
}

/** Identify the explicitly nullable fields for one receipt kind. */
function pay_glb_001_native_nullable_fields($kind)
{
    $fields = array(
        'execution'=>array('predecessor_execution_sha256','joint_core009_run_id',
            'joint_core009_state_version','joint_core009_state_sha256','joint_producer_sha256'),
        'member'=>array('assignment_id','joint_core009_run_item_id',
            'joint_core009_document_sha256','cross_domain_parity_sha256'),
        'selection'=>array('predecessor_selection_sha256','native_execution_sha256'),
        'gl_binding'=>array('predecessor_posting_id'),
        'command'=>array('expected_predecessor_sha256')
    );
    return isset($fields[$kind]) ? $fields[$kind] : array();
}

/** Validate a primitive by its declared field role, bounds and exact type. */
function pay_glb_001_native_field_valid($kind, $field, $value)
{
    if ($value === null)
        return in_array($field, pay_glb_001_native_nullable_fields($kind), true);
    if (substr($field, -7) === '_sha256')
        return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
    if (substr($field, -3) === '_id' || in_array($field, array('ordinal','member_count',
        'revision_no','sequence_no','joint_core009_state_version','joint_core009_checkpoint_no','bound_by'), true)) {
        $small = in_array($field, array('company_id','legal_entity_id','payroll_period_id',
            'maker_id','checker_id','approver_id','binder_id','actor_id','bound_by','ordinal','member_count'), true);
        return pay_glb_001_native_unsigned($value, $small ? '4294967295' : '18446744073709551615',
            in_array($field, array('company_id','joint_core009_checkpoint_no'), true));
    }
    if (in_array($field, array('gross_8','deduction_8','net_8'), true))
        return pay_glb_001_native_decimal($value);
    if (in_array($field, array('produced_at_utc','occurred_at_utc'), true))
        return pay_glb_001_native_timestamp($value);
    if ($field === 'currency_code')
        return is_string($value) && preg_match('/^[A-Z]{3}$/D', $value) === 1;
    if ($field === 'source_kind')
        return in_array($value, array('GOVERNED_NATIVE_V1','JOINT_CORE009_V1'), true);
    if ($field === 'event_kind')
        return in_array($value, array('select','suspend','revoke'), true);
    if ($field === 'posting_kind')
        return in_array($value, array('post','reverse','repost'), true);
    $max = in_array($field, array('native_execution_key','joint_core009_run_key'), true) ? 128
        : (in_array($field, array('producer_contract_version','mapping_contract_version','command_domain','effect_kind'), true) ? 48 : 64);
    return is_string($value) && strlen($value) <= $max
        && preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:-]*$/D', $value) === 1;
}

/** Verify a canonical ordered membership summary without arbitrary subsets. */
function pay_glb_001_native_member_summary_valid(array $descriptor, &$error=null)
{
    $rows = $descriptor['members'];
    if (!is_array($rows) || count($rows) < 1 || count($rows) > 4096
        || array_keys($rows) !== range(0, count($rows)-1)
        || (string)count($rows) !== $descriptor['member_count']) {
        $error = 'member_count_or_list_invalid'; return false;
    }
    $seen_relationships = array(); $seen_results = array(); $previous = null;
    foreach ($rows as $index=>$row) {
        if (!is_array($row) || array_keys($row) !== array('ordinal','payroll_relationship_id','core004_result_id','member_sha256')) {
            $error = 'member_summary_fields_invalid'; return false;
        }
        foreach ($row as $field=>$value)
            if (!pay_glb_001_native_field_valid('members', $field, $value)) {
                $error = 'member_summary_type_invalid'; return false;
            }
        if ($row['ordinal'] !== (string)($index+1)
            || isset($seen_relationships[$row['payroll_relationship_id']])
            || isset($seen_results[$row['core004_result_id']])
            || ($previous !== null && pay_glb_001_native_unsigned_compare($previous, $row['payroll_relationship_id']) >= 0)) {
            $error = 'member_summary_order_or_duplicate'; return false;
        }
        $seen_relationships[$row['payroll_relationship_id']] = true;
        $seen_results[$row['core004_result_id']] = true;
        $previous = $row['payroll_relationship_id'];
    }
    return true;
}

/** Validate a complete descriptor; source authenticity must be checked elsewhere. */
function pay_glb_001_native_receipt_validate($kind, $descriptor, &$error=null)
{
    $error = null; $fields = pay_glb_001_native_receipt_fields();
    if (!is_string($kind) || !isset($fields[$kind]) || !is_array($descriptor)
        || array_keys($descriptor) !== $fields[$kind]) {
        $error = 'receipt_kind_or_exact_fields_invalid'; return false;
    }
    foreach ($descriptor as $field=>$value) {
        if ($field === 'members') continue;
        if (!pay_glb_001_native_field_valid($kind, $field, $value)) {
            $error = 'receipt_field_invalid:'.$field; return false;
        }
    }
    if ($kind === 'execution') {
        $joint = $descriptor['source_kind'] === 'JOINT_CORE009_V1';
        foreach (array('joint_core009_run_id','joint_core009_state_version','joint_core009_state_sha256','joint_producer_sha256') as $field)
            if (($descriptor[$field] !== null) !== $joint) { $error = 'joint_quartet_invalid'; return false; }
        if (($descriptor['revision_no'] === '1') !== ($descriptor['predecessor_execution_sha256'] === null)) {
            $error = 'execution_predecessor_invalid'; return false;
        }
        if (!pay_glb_001_native_totals_match($descriptor['gross_8'], $descriptor['deduction_8'], $descriptor['net_8'])) {
            $error = 'execution_totals_invalid'; return false;
        }
    }
    if ($kind === 'member') {
        $joint_count = 0;
        foreach (array('joint_core009_run_item_id','joint_core009_document_sha256','cross_domain_parity_sha256') as $field)
            if ($descriptor[$field] !== null) $joint_count++;
        if ($joint_count !== 0 && $joint_count !== 3) { $error = 'member_joint_triple_invalid'; return false; }
    }
    if ($kind === 'members' && !pay_glb_001_native_member_summary_valid($descriptor, $error))
        return false;
    if ($kind === 'selection') {
        if (($descriptor['sequence_no'] === '1') !== ($descriptor['predecessor_selection_sha256'] === null)
            || (($descriptor['event_kind'] === 'select') !== ($descriptor['native_execution_sha256'] !== null))
            || $descriptor['maker_id'] === $descriptor['checker_id']) {
            $error = 'selection_predecessor_target_or_sod_invalid'; return false;
        }
    }
    if ($kind === 'approval_binding' && $descriptor['maker_id'] === $descriptor['approver_id']) {
        $error = 'approval_sod_invalid'; return false;
    }
    if ($kind === 'gl_binding'
        && (($descriptor['posting_kind'] === 'post') !== ($descriptor['predecessor_posting_id'] === null))) {
        $error = 'posting_predecessor_invalid'; return false;
    }
    return true;
}

/** Produce exact ordered UTF-8 JSON with bounded in-memory size. */
function pay_glb_001_native_receipt_canonical_v1($kind, $descriptor, &$error=null)
{
    if (!pay_glb_001_native_receipt_validate($kind, $descriptor, $error))
        return false;
    $json = json_encode($descriptor, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    if (!is_string($json) || strlen($json) > 2097152) {
        $error = 'receipt_encoding_or_size_invalid'; return false;
    }
    return $json;
}

/** Hash the allowlisted domain, one LF byte and canonical JSON. */
function pay_glb_001_native_receipt_sha256_v1($kind, $descriptor, &$error=null)
{
    $json = pay_glb_001_native_receipt_canonical_v1($kind, $descriptor, $error);
    return $json === false ? false : hash('sha256', 'PAY-GLB-001|'.$kind."|v1\n".$json);
}

/** Decode only exact canonical bytes; duplicate keys and spelling changes deny. */
function pay_glb_001_native_receipt_decode_v1($kind, $json, &$error=null)
{
    $error = null;
    if (!is_string($json) || strlen($json) > 2097152) {
        $error = 'receipt_bytes_invalid'; return false;
    }
    $descriptor = json_decode($json, true, 64);
    if (json_last_error() !== JSON_ERROR_NONE) { $error = 'receipt_json_invalid'; return false; }
    $canonical = pay_glb_001_native_receipt_canonical_v1($kind, $descriptor, $error);
    if ($canonical === false) return false;
    if ($canonical !== $json) { $error = 'receipt_noncanonical_bytes'; return false; }
    return $descriptor;
}

/** Sort complete member descriptors and derive the deterministic member-set seal. */
function pay_glb_001_native_member_set_v1($members, &$error=null)
{
    $error = null;
    if (!is_array($members) || count($members) < 1 || count($members) > 4096) {
        $error = 'member_population_invalid'; return false;
    }
    foreach ($members as $member)
        if (!pay_glb_001_native_receipt_validate('member', $member, $error)) return false;
    usort($members, function($left, $right) {
        $relationship = pay_glb_001_native_unsigned_compare($left['payroll_relationship_id'], $right['payroll_relationship_id']);
        return $relationship !== 0 ? $relationship
            : pay_glb_001_native_unsigned_compare($left['core004_result_id'], $right['core004_result_id']);
    });
    $summary = array(); $snapshots = array(); $items = array(); $key = $members[0]['native_execution_key'];
    foreach ($members as $index=>$member) {
        if ($member['native_execution_key'] !== $key || $member['ordinal'] !== (string)($index+1)
            || isset($snapshots[$member['core004_snapshot_id']])
            || ($member['joint_core009_run_item_id'] !== null && isset($items[$member['joint_core009_run_item_id']]))) {
            $error = 'member_key_ordinal_or_source_duplicate'; return false;
        }
        $snapshots[$member['core004_snapshot_id']] = true;
        if ($member['joint_core009_run_item_id'] !== null) $items[$member['joint_core009_run_item_id']] = true;
        $summary[] = array('ordinal'=>$member['ordinal'],'payroll_relationship_id'=>$member['payroll_relationship_id'],
            'core004_result_id'=>$member['core004_result_id'],
            'member_sha256'=>pay_glb_001_native_receipt_sha256_v1('member', $member, $error));
    }
    $descriptor = array('native_execution_key'=>$key, 'member_count'=>(string)count($members), 'members'=>$summary);
    $digest = pay_glb_001_native_receipt_sha256_v1('members', $descriptor, $error);
    return $digest === false ? false : array('descriptor'=>$descriptor, 'member_set_sha256'=>$digest, 'members'=>$members);
}
