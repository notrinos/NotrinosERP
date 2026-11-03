<?php
/** Current agreement status/renewal custody through the existing shared core. */
require_once __DIR__.'/purch_agreement_dimension_db.inc';

/** Bind a native command to actor, role, company configuration and connection. */
function purch_agreement_lifecycle_context($portable = false)
{
    global $db, $db_connections;
    if (!purch_agreement_dimension_context() || !isset($_SESSION['wa_current_user']->access)) return false;
    $user = $_SESSION['wa_current_user'];
    $company = array($user->cur_con, $db_connections[$user->cur_con]);
    return hash('sha256', serialize($portable ? $company : array($company, $user->user, $user->access,
        is_object($db) ? spl_object_hash($db) : $db)));
}

/** Read exact current rows through the normal driver, without optional-store inference. */
function purch_agreement_lifecycle_rows($sql)
{
    $rows = db_query($sql);
    if (!$rows) return false;
    $result = array();
    while ($row = db_fetch_assoc($rows)) $result[] = $row;
    return $result;
}

/** Normalize intrinsic DB values across PHP/PDO runtimes, preserving null and raw JSON. */
function purch_agreement_lifecycle_canonical($source)
{
    foreach ($source as $key=>$value) {
        if (is_array($value)) $source[$key] = purch_agreement_lifecycle_canonical($value);
        elseif ($value !== null) $source[$key] = (string)$value;
    }
    return $source;
}

/** Lock this core family first, matching native draft/Entity maintenance. */
function lock_purchase_agreement_core_action($draft = null, $actor = null)
{
    if (!function_exists('is_purch_agreement_line_data_valid')) require_once __DIR__.'/purch_agreement_db.inc';
    if (!purch_agreement_lifecycle_context() || empty($GLOBALS['transaction_level'])) return false;
    if ($actor !== null && ((int)$actor['user_id'] !== (int)$_SESSION['wa_current_user']->user
        || (int)$actor['role_id'] !== (int)$_SESSION['wa_current_user']->access)) return false;
    return purch_agreement_lifecycle_rows('SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='
        .ST_PURCHAGREEMENT.' ORDER BY id FOR UPDATE') !== false;
}

/** Decode every family identity through the core; unknown custody is never absence. */
function purch_agreement_lifecycle_history($id)
{
    $rows = purch_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'approval_drafts WHERE trans_type='
        .ST_PURCHAGREEMENT.' ORDER BY id FOR UPDATE');
    if ($rows === false || !function_exists('get_approval_workflow_service')) return false;
    $history = array(); $service = get_approval_workflow_service();
    foreach ($rows as $row) {
        $data = $service->decodeStoredDraftData($row['draft_data']);
        if (!is_array($data) || !isset($data['agreement_id']) || !dimension_writer_reference_id($data['agreement_id'])) return false;
        if ((int)$data['agreement_id'] === $id) $history[] = $row;
    }
    return $history;
}

/** Lock parent, actual linked effects, then children in the established native order. */
function purch_agreement_lifecycle_source($id)
{
    $headers = purch_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'purch_agreements WHERE id='.$id.' FOR UPDATE');
    if (!$headers || count($headers) !== 1) return false;
    $effects = purch_agreement_lifecycle_rows('SELECT po.order_no,detail.* FROM '.TB_PREF.'purch_orders po LEFT JOIN '
        .TB_PREF.'purch_order_details detail ON detail.order_no=po.order_no WHERE po.agreement_id='.$id
        .' ORDER BY po.order_no,detail.po_detail_item FOR UPDATE');
    $lines = purch_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'purch_agreement_lines WHERE agreement_id='.$id.' ORDER BY id FOR UPDATE');
    // Native conversion also takes location before supplier. All owner rows
    // precede Dimension masters; review-name reads below only reacquire these.
    $header = $headers[0];
    foreach (array('SELECT loc_code FROM '.TB_PREF.'locations WHERE loc_code='.db_escape($header['delivery_location']).' FOR UPDATE',
        'SELECT supplier_id FROM '.TB_PREF.'suppliers WHERE supplier_id='.(int)$header['supplier_id'].' FOR UPDATE',
        'SELECT id FROM '.TB_PREF.'users WHERE id='.(int)$header['buyer_id'].' FOR UPDATE') as $query)
        if (purch_agreement_lifecycle_rows($query) === false) return false;
    return $effects === false || $lines === false ? false : array('header'=>$headers[0], 'lines'=>$lines, 'effects'=>$effects);
}

/** Transaction-local core admission, released by existing outer transaction resources. */
function purch_agreement_core_permit($operation, $value = null)
{
    static $permit = null;
    if ($operation === 'set') { $permit = $value; return true; }
    if ($operation === 'peek') return $permit;
    $old = $permit; $permit = null;
    return $operation === 'take' ? $old : true;
}

/** Clear unfinished commands after commit/rollback, without a session authorization flag. */
function release_purchase_agreement_core_permit()
{
    return purch_agreement_core_permit('clear');
}

/**
 * Own status/metadata mutation under family, source and retained master locks.
 * The callback returns the exact permitted header changes and native result.
 * No status command may alter source lines, fulfillment or linked PO effects.
 */
function purch_agreement_lifecycle_write($identity, $callback, $core = false)
{
    $id = dimension_writer_reference_id($identity); $context = purch_agreement_lifecycle_context();
    if (!$id || !$context || !is_callable($callback)) return false;
    try {
        if (!begin_transaction() || !lock_purchase_agreement_core_action()) { cancel_transaction(); return false; }
        $history = purch_agreement_lifecycle_history($id); $source = purch_agreement_lifecycle_source($id);
        if ($history === false || !$source) { cancel_transaction(); return false; }
        $permit = purch_agreement_core_permit('peek');
        if ($core && (!$permit || $permit['context'] !== $context || (int)$permit['id'] !== $id)) { cancel_transaction(); return false; }
        foreach ($history as $draft) if ((int)$draft['status'] === APPROVAL_STATUS_PENDING
            && (!$core || (int)$draft['id'] !== (int)$permit['draft_id'])) { cancel_transaction(); return false; }
        $locks = array();
        foreach (array_merge(array($source['header']),$source['lines']) as $row) {
            if (!array_key_exists('custom_data',$row)) { cancel_transaction(); return false; }
            $references = dimension_master_payload_references($row['custom_data']);
            if (is_array($references)) foreach ($references as $reference) $locks[$reference[0]] = $reference[0];
        }
        foreach (array('dimension_id','dimension2_id') as $field) {
            if (!array_key_exists($field,$source['header']) || ($axis = dimension_writer_reference_id($source['header'][$field])) === false) { cancel_transaction(); return false; }
            if ($axis) $locks[$axis] = $axis;
        }
        sort($locks,SORT_NUMERIC);
        foreach ($locks as $lock) if (purch_agreement_lifecycle_rows('SELECT id FROM '.TB_PREF.'dimensions WHERE id='.$lock.' FOR UPDATE') === false) { cancel_transaction(); return false; }
        foreach ($source['lines'] as $line)
            if (!dimension_ordinary_master_write('purch_agreement_lines', $line['id'], array(), function($admitted) { return true; }, true)) {
                cancel_transaction(); return false;
            }
        $result = dimension_ordinary_master_write('purch_agreements', $id, array(),
            function($admitted) use ($source,$history,$callback,$context,$id) {
                $outcome = call_user_func($callback, $source, $history);
                if (!is_array($outcome) || !isset($outcome['changes']) || !array_key_exists('result',$outcome)
                    || !$outcome['result'] || purch_agreement_lifecycle_context() !== $context) return false;
                $after = purch_agreement_lifecycle_source($id);
                if (!$after) return false;
                $expected = $source;
                foreach ($outcome['changes'] as $field=>$value) $expected['header'][$field] = $value;
                // The schema owns this automatic timestamp; all business fields remain exact.
                unset($expected['header']['updated_at'], $after['header']['updated_at']);
                return purch_agreement_lifecycle_canonical($after) === purch_agreement_lifecycle_canonical($expected)
                    ? $outcome['result'] : false;
            }, true);
        if (!$result || purch_agreement_lifecycle_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}

/** Persist only status and additive metadata under the existing ordinary fence. */
function purch_agreement_lifecycle_store($source, $changes)
{
    foreach ($changes as $field=>$value) if (!in_array($field,array('status','inactive','custom_data'),true)) return false;
    $id = (int)$source['header']['id'];
    return dimension_ordinary_master_write('purch_agreements',$id,$changes,function($admitted) use ($id,$changes) {
        $sets = array();
        foreach ($changes as $field=>$value) $sets[] = $field.'='.($field === 'custom_data' ? sql_purch_agreement_custom_data($value) : db_escape($value));
        return count($sets) && db_query('UPDATE '.TB_PREF.'purch_agreements SET '.implode(',',$sets).' WHERE id='.$id) !== false;
    },true);
}

/** Add command metadata without decoding opaque JSON into an empty replacement. */
function purch_agreement_lifecycle_metadata($raw, $fields)
{
    if ($raw === null || $raw === '') $data = array();
    else {
        $data = json_decode($raw,true);
        if (!is_array($data) || dimension_master_payload_references($raw) === false) return false;
    }
    foreach ($fields as $key=>$value) $data[$key] = $value;
    $next = json_encode($data);
    return is_string($next) && dimension_master_payload_transition($raw,$next) !== false ? $next : false;
}

/** Current state-machine transition; confirmation is an admitted core execution only. */
function purch_agreement_lifecycle_transition($identity, $status, $reason = '', $core = false)
{
    if (!is_string($reason) || strpos($reason,"\0") !== false) return false;
    return purch_agreement_lifecycle_write($identity,function($source,$history) use ($status,$reason,$core) {
        $header = $source['header'];
        $allowed = array('confirmed'=>array('draft'),'active'=>array('confirmed'),
            'expired'=>array('confirmed','active'),'cancelled'=>array('draft','confirmed','active'));
        if (!isset($allowed[$status]) || !in_array($header['status'],$allowed[$status],true)) return false;
        if ($status === 'confirmed') {
            $permit = purch_agreement_core_permit('peek');
            if (!$core || !$permit || $permit['action'] !== 'approve' || !empty($permit['executed'])
                || !purch_agreement_review_valid($source)) return false;
        }
        if ($status === 'active' && ($header['date_start'] > date2sql(Today())
            || ($header['date_end'] && $header['date_end'] !== '0000-00-00' && $header['date_end'] < date2sql(Today())))) return false;
        $changes = array('status'=>$status,'inactive'=>in_array($status,array('expired','cancelled'),true) ? 1 : 0);
        if ($reason !== '') {
            $json = purch_agreement_lifecycle_metadata($header['custom_data'],array('status_reason_'.$status=>$reason));
            if ($json === false) return false;
            $changes['custom_data'] = $json;
        }
        if (!purch_agreement_lifecycle_store($source,$changes)) return false;
        if (in_array($status,array('confirmed','expired'),true)) {
            $hook = array('agreement_id'=>(int)$header['id']); hook_invoke_all('purch_agreement_'.$status,$hook);
        }
        return array('changes'=>$changes,'result'=>true);
    },$core);
}

/** Validate the actual native agreement terms and authoritative committed total. */
function purch_agreement_review_valid($source)
{
    if (!$source || $source['header']['status'] !== 'draft' || !$source['lines'] || $source['effects']) return false;
    $total = 0;
    foreach ($source['lines'] as $line) {
        if (!is_purch_agreement_line_data_valid($line['committed_qty'],$line['unit_price'],$line['discount_percent'],
            $line['min_qty_per_order'],$line['price_valid_until']) || trim($line['stock_id']) === '') return false;
        $total += (float)$line['committed_qty'] * (float)$line['unit_price'] * (1-(float)$line['discount_percent']/100);
    }
    return is_finite($total) && is_numeric($source['header']['total_committed'])
        && abs($total-(float)$source['header']['total_committed']) < 0.00000001;
}

/** Immutable raw source and review fields, with stable scalar types across PHP versions. */
function purch_agreement_review_data($source)
{
    $header = $source['header']; $data = array('agreement_id'=>(int)$header['id']);
    $supplier = purch_agreement_lifecycle_rows('SELECT supp_name FROM '.TB_PREF.'suppliers WHERE supplier_id='.(int)$header['supplier_id'].' AND inactive=0 FOR UPDATE');
    if (!$supplier || count($supplier) !== 1) return false;
    $data['supplier_name'] = $supplier[0]['supp_name']; $data['buyer_name'] = ''; $data['delivery_location_name'] = '';
    if ((int)$header['buyer_id']) {
        $buyer = purch_agreement_lifecycle_rows('SELECT real_name FROM '.TB_PREF.'users WHERE id='.(int)$header['buyer_id'].' AND inactive=0 FOR UPDATE');
        if (!$buyer || count($buyer) !== 1) return false;
        $data['buyer_name'] = $buyer[0]['real_name'];
    }
    if ($header['delivery_location'] !== '') {
        $location = purch_agreement_lifecycle_rows('SELECT location_name FROM '.TB_PREF.'locations WHERE loc_code='.db_escape($header['delivery_location']).' AND inactive=0 FOR UPDATE');
        if (!$location || count($location) !== 1) return false;
        $data['delivery_location_name'] = $location[0]['location_name'];
    }
    foreach (array('reference','agreement_type','supplier_id','buyer_id','status','date_start','date_end','currency',
        'payment_terms','delivery_location','total_committed','terms_and_conditions','notes','dimension_id','dimension2_id') as $field)
        $data[$field] = $header[$field] === null ? null : (string)$header[$field];
    if ($data['currency'] === '') $data['currency'] = (string)get_company_pref('curr_default');
    $data['custom_data'] = $header['custom_data']; $data['line_items'] = array();
    foreach ($source['lines'] as $line) {
        $row = array();
        foreach ($line as $field=>$value) $row[$field] = $value === null ? null : (string)$value;
        $data['line_items'][] = $row;
    }
    $data['purchase_agreement_schema'] = 'native-v1';
    $data['agreement_company_sha256'] = purch_agreement_lifecycle_context(true);
    $data['agreement_source'] = purch_agreement_lifecycle_canonical($source);
    return $data;
}

/** Native submission always uses the registered core and its current locked policy. */
function submit_purchase_agreement_lifecycle($identity)
{
    if (!class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_PURCHAGREEMENT)
        || !function_exists('get_approval_workflow_service')) return false;
    return purch_agreement_lifecycle_write($identity,function($source,$history) {
        if (!purch_agreement_review_valid($source)) return false;
        foreach ($history as $draft) if (!in_array((int)$draft['status'],array(APPROVAL_STATUS_REJECTED,APPROVAL_STATUS_CANCELLED),true)) return false;
        $data = purch_agreement_review_data($source);
        if (!is_array($data)) return false;
        $permit = array('id'=>(int)$source['header']['id'],'action'=>'submit','context'=>purch_agreement_lifecycle_context(),
            'data'=>$data,'draft_id'=>0,'executed'=>false,'expected'=>$source);
        if (!register_transaction_resource_release('purchase-agreement-core','release_purchase_agreement_core_permit')) return false;
        purch_agreement_core_permit('set',$permit);
        $result = get_approval_workflow_service()->submit(ST_PURCHAGREEMENT,$data,(float)$source['header']['total_committed'],array(
            'summary'=>sprintf(_('Purchase Agreement %s'),$source['header']['reference']),
            'currency'=>$data['currency'],'person_type'=>PT_SUPPLIER,'person_id'=>$source['header']['supplier_id'],
            'loc_code'=>$source['header']['delivery_location'],'agreement_id'=>(int)$source['header']['id']),true);
        if (!is_array($result) || !in_array($result['status'],array('pending','auto_approved'),true)) return false;
        if (!purch_agreement_core_published($result['draft_id'],$data,
            $result['status'] === 'pending' ? APPROVAL_STATUS_PENDING : APPROVAL_STATUS_APPROVED)) return false;
        $changes = $result['status'] === 'auto_approved' ? array('status'=>'confirmed','inactive'=>0) : array();
        return array('changes'=>$changes,'result'=>$result);
    });
}

/** Source authority for submit/auto/approve and compatible legacy terminal recovery. */
function validate_purchase_agreement_core_action($action,$data,$draft,$actor,$comments)
{
    $context = purch_agreement_lifecycle_context();
    if (!$context || empty($GLOBALS['transaction_level']) || !is_array($data) || !isset($data['agreement_id'])
        || !($id = dimension_writer_reference_id($data['agreement_id']))
        || (int)$actor['user_id'] !== (int)$_SESSION['wa_current_user']->user
        || (int)$actor['role_id'] !== (int)$_SESSION['wa_current_user']->access) return false;
    if (!lock_purchase_agreement_core_action()) return false;
    $source = purch_agreement_lifecycle_source($id); $history = purch_agreement_lifecycle_history($id);
    if (!$source || $history === false || $source['header']['status'] !== 'draft') return false;
    $new = isset($data['purchase_agreement_schema']) && $data['purchase_agreement_schema'] === 'native-v1';
    if ($new && $data !== purch_agreement_review_data($source)) return false;
    if (!$new && !in_array($action,array('reject','cancel'),true)) return false;
    $permit = purch_agreement_core_permit('peek');
    if ($action === 'submit') return $new && $permit && $permit['action'] === 'submit'
        && $permit['context'] === $context && $permit['data'] === $data && purch_agreement_review_valid($source);
    if (!in_array($action,array('approve','auto_approve','reject','cancel','delegate'),true)) return false;
    $pending = array();
    foreach ($history as $row) if ((int)$row['status'] === APPROVAL_STATUS_PENDING) $pending[] = $row;
    if (count($pending) !== 1 || ($draft !== null && (int)$pending[0]['id'] !== (int)$draft['id'])) return false;
    if ($action === 'delegate') return $new;
    if (($action === 'approve' || $action === 'auto_approve') && !purch_agreement_review_valid($source)) return false;
    if (!register_transaction_resource_release('purchase-agreement-core','release_purchase_agreement_core_permit')) return false;
    return purch_agreement_core_permit('set',array('id'=>$id,'draft_id'=>(int)$pending[0]['id'],'draft'=>$pending[0],
        'action'=>$action === 'auto_approve' ? 'approve' : $action,'context'=>$context,'data'=>$data,
        'executed'=>false,'expected'=>$source));
}

/** Verify actual core publication, not a successful-looking return array. */
function purch_agreement_core_published($identity,$data,$status,$before = null)
{
    $rows = purch_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'approval_drafts WHERE id='.(int)$identity.' FOR UPDATE');
    if (!$rows || count($rows) !== 1) return false;
    $row = $rows[0];
    if ((int)$row['trans_type'] !== ST_PURCHAGREEMENT || (int)$row['status'] !== (int)$status
        || (int)$row['reserved_trans_no'] <= 0
        || get_approval_workflow_service()->decodeStoredDraftData($row['draft_data']) !== $data
        || ((int)$status === APPROVAL_STATUS_APPROVED && (int)$row['approved_trans_no'] !== (int)$data['agreement_id'])) return false;
    if ($before !== null) foreach (array('workflow_id','workflow_version','trans_type','reserved_trans_no','draft_data',
        'amount','currency','person_type_id','person_id','submitted_by','reference','loc_code') as $field)
        if ($row[$field] === null ? $before[$field] !== null : (string)$row[$field] !== (string)$before[$field]) return false;
    if (isset($data['purchase_agreement_schema'])) {
        if ((float)$row['amount'] !== (float)$data['total_committed'] || $row['currency'] !== $data['currency']
            || (int)$row['person_type_id'] !== PT_SUPPLIER || (int)$row['person_id'] !== (int)$data['supplier_id']
            || $row['reference'] !== $data['reference'] || (string)$row['loc_code'] !== (string)$data['delivery_location']) return false;
    }
    return true;
}

/** Execute or terminate only a core-admitted current command, never a direct callback. */
function execute_purchase_agreement_core_action($action,$data,$draft_id,$comments = '')
{
    $permit = purch_agreement_core_permit('peek');
    if (!$permit || $permit['action'] !== $action || $permit['data'] !== $data || !empty($permit['executed'])
        || $permit['context'] !== purch_agreement_lifecycle_context() || (int)$permit['draft_id'] !== (int)$draft_id
        || empty($GLOBALS['transaction_level'])) return false;
    $id = $permit['id'];
    if ($action === 'approve') $result = purch_agreement_lifecycle_transition($id,'confirmed','',true);
    else $result = purch_agreement_lifecycle_write($id,function($source,$history) use ($action,$comments,$draft_id) {
        if ($source['header']['status'] !== 'draft') return false;
        $fields = $action === 'reject' ? array('approval_rejection_reason'=>$comments,'approval_rejected_draft_id'=>(int)$draft_id)
            : array('approval_cancel_reason'=>$comments,'approval_cancelled_draft_id'=>(int)$draft_id);
        $json = purch_agreement_lifecycle_metadata($source['header']['custom_data'],$fields);
        if ($json === false || !purch_agreement_lifecycle_store($source,array('custom_data'=>$json))) return false;
        return array('changes'=>array('custom_data'=>$json),'result'=>true);
    },true);
    if (!$result) return false;
    $permit['executed'] = true; $permit['expected'] = purch_agreement_lifecycle_source($id);
    purch_agreement_core_permit('set',$permit);
    return $action === 'approve' ? $id : true;
}

/** Verify native source after core audit/status/notification publication, before SQL completion. */
function verify_purchase_agreement_core_completion($outcome,$draft,$actor,$comments)
{
    $permit = purch_agreement_core_permit('peek');
    if (!$permit || $permit['context'] !== purch_agreement_lifecycle_context()
        || (int)$permit['draft_id'] !== (int)$draft['id']) return false;
    $source = purch_agreement_lifecycle_source($permit['id']);
    if (!$source || purch_agreement_lifecycle_canonical($source) !== purch_agreement_lifecycle_canonical($permit['expected'])) return false;
    $partial = in_array($outcome,array('pending_next_level','pending_more_approvers'),true);
    $statuses = array('approved'=>APPROVAL_STATUS_APPROVED,'rejected'=>APPROVAL_STATUS_REJECTED,'cancelled'=>APPROVAL_STATUS_CANCELLED);
    $status = $partial ? APPROVAL_STATUS_PENDING : (isset($statuses[$outcome]) ? $statuses[$outcome] : false);
    return $status !== false && ($partial || !empty($permit['executed']))
        && purch_agreement_core_published($draft['id'],$permit['data'],$status,$permit['draft']);
}

/** Atomically copy one expired recurrence, submit its own approval, and retain both links. */
function renew_purchase_agreement($identity)
{
    return purch_agreement_lifecycle_write($identity,function($source,$history) {
        $header = $source['header'];
        if (empty($header['auto_renew']) || !in_array($header['status'],array('confirmed','active'),true)
            || !$header['date_end'] || $header['date_end'] === '0000-00-00' || $header['date_end'] >= date2sql(Today())
            || (int)$header['renewal_period_months'] < 1 || !$source['lines']) return false;
        $metadata = purch_agreement_lifecycle_metadata($header['custom_data'],array());
        if ($metadata === false) return false;
        $existing = json_decode($metadata,true);
        if (!empty($existing['renewed_to_agreement_id'])) return false;
        unset($existing['renewed_to_agreement_id']);
        $existing['renewed_from_agreement_id'] = (int)$header['id'];
        $start = add_days(sql2date($header['date_end']),1);
        $end = add_days(add_months($start,(int)$header['renewal_period_months']),-1);
        $new = add_purch_agreement($header['agreement_type'],$header['supplier_id'],date2sql($start),date2sql($end),
            $header['currency'],$header['payment_terms'],$header['delivery_location'],1,$header['renewal_period_months'],
            $header['terms_and_conditions'],$header['notes'],$header['rfq_id'],$header['dimension_id'],$header['dimension2_id'],
            '',$header['buyer_id'],$existing);
        if (!$new) return false;
        foreach ($source['lines'] as $line) if (!add_agreement_line($new,$line['stock_id'],$line['committed_qty'],$line['unit_price'],
            $line['discount_percent'],$line['description'],$line['min_qty_per_order'],$line['price_valid_until'],$line['custom_data'])) return false;
        $result = submit_purchase_agreement_lifecycle($new);
        if (!$result) return false;
        if ($result['status'] === 'auto_approved' && date2sql($start) <= date2sql(Today()) && date2sql($end) >= date2sql(Today())
            && !activate_agreement($new)) return false;
        $json = purch_agreement_lifecycle_metadata($header['custom_data'],array('renewed_to_agreement_id'=>(int)$new));
        $changes = array('custom_data'=>$json,'status'=>'expired','inactive'=>1);
        if ($json === false || !purch_agreement_lifecycle_store($source,$changes)) return false;
        $hook = array('old_id'=>(int)$header['id'],'new_id'=>(int)$new);hook_invoke_all('purch_agreement_renewed',$hook);
        return array('changes'=>$changes,'result'=>(int)$new);
    });
}
