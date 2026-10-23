<?php
/** Immutable procurement-plan supplier commands through the existing PO approval workflow. */

/** Read current company rows, retaining driver types for exact source reconciliation. */
function read_plan_purchase_rows($query) {
 $result = db_query($query, 'could not verify procurement-plan purchase source');
 if (!$result) throw new RuntimeException('Procurement-plan source is unavailable.');
 $rows = array(); while ($row = db_fetch_assoc($result)) $rows[] = $row;
 return $rows;
}

/** Require current source and native destination authority before any source identity read. */
function plan_purchase_authorized() {
 if (!isset($_SESSION['wa_current_user']) || !$_SESSION['wa_current_user']->can_access('SA_PROCUREMENTPLAN')
  || !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')) return false;
 $cart = new purch_order(); return $cart->is_current_company();
}

/** Unknown core/PO custody denies; only bounded current source identities select payloads. */
function plan_purchase_has_pending($id, $line_ids, $core = false) {
 if (empty($GLOBALS['transaction_level']) || !function_exists('approval_draft_current_json_sql')) throw new RuntimeException('Plan approval custody is unavailable.');
 $json = approval_draft_current_json_sql(); $type = $core ? ST_PROCUREMENTPLAN : ST_PURCHORDER;
 if (read_plan_purchase_rows('SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.$type.' AND status=0 AND NOT JSON_VALID('.$json.') ORDER BY id FOR UPDATE')) return true;
 $path = $core ? '$.plan_id' : '$.plan_conversion.plan_id';
 $rows = read_plan_purchase_rows('SELECT id,draft_data FROM '.TB_PREF.'approval_drafts WHERE trans_type='.$type.' AND status=0 AND CASE WHEN JSON_VALID('.$json.') THEN JSON_UNQUOTE(JSON_EXTRACT('.$json.', '.db_escape($path).'))='.db_escape((string)$id).' ELSE 0 END ORDER BY id FOR UPDATE');
 if ($core) return !empty($rows);
 $actor = $_SESSION['wa_current_user']; $approval = new ApprovalWorkflow($actor->access, $actor->user);
 foreach ($rows as $row) {
  $data = $approval->decodeStoredDraftData($row['draft_data']);
  if (!is_array($data) || empty($data['plan_conversion']['line_ids']) || !is_array($data['plan_conversion']['line_ids'])) return true;
  foreach ($data['plan_conversion']['line_ids'] as $line) if (!is_int($line) || $line <= 0) return true;
  if (array_intersect($line_ids, $data['plan_conversion']['line_ids'])) return true;
 }
 return false;
}

/** Lock the plan and all lines before partitioning the selected unconsumed supplier groups. */
function plan_purchase_groups($id, $line_ids = array()) {
 if (!plan_purchase_authorized() || empty($GLOBALS['transaction_level']) || !is_scalar($id)
  || (string)(int)$id !== (string)$id || (int)$id <= 0 || !is_array($line_ids)) throw new RuntimeException('Plan purchase authority or identity is unavailable.');
 $headers = read_plan_purchase_rows('SELECT * FROM '.TB_PREF.'procurement_plan WHERE id='.(int)$id.' FOR UPDATE');
 if (count($headers) !== 1 || !in_array($headers[0]['status'], array('confirmed','in_progress','completed'), true)
  || plan_purchase_has_pending($id, array(), true)) throw new RuntimeException('Plan must have completed core approval.');
 $lines = read_plan_purchase_rows('SELECT * FROM '.TB_PREF.'procurement_plan_lines WHERE plan_id='.(int)$id.' ORDER BY id FOR UPDATE');
 $selected = array(); foreach ($line_ids as $line_id) {
  if (!is_int($line_id) || $line_id <= 0 || isset($selected[$line_id])) throw new RuntimeException('Plan line selection is invalid.');
  $selected[$line_id] = true;
 }
 $groups = array(); $used = array();
 foreach ($lines as $line) {
  if ($selected && !isset($selected[(int)$line['id']])) continue;
  if ($line['status'] !== 'approved' || (int)$line['po_number'] !== 0) continue;
  if (!is_finite((float)$line['suggested_order_qty']) || (float)$line['suggested_order_qty'] <= 0
   || !is_finite((float)$line['estimated_price']) || (float)$line['estimated_price'] < 0) throw new RuntimeException('Plan quantities or prices require reconciliation.');
  $supplier = (int)$line['supplier_id']; if ($supplier <= 0) continue;
  $groups[$supplier][] = (int)$line['id']; $used[(int)$line['id']] = true;
 }
 if (!$groups || ($selected && count($selected) !== count($used))) throw new RuntimeException('Selected plan lines are unavailable.');
 ksort($groups, SORT_NUMERIC); return $groups;
}

/** Header status is derived progress; retain every other field and opaque payload exactly. */
function plan_purchase_header_intent($header) { unset($header['status']); return $header; }

/** Build one supplier's locked native intent, retaining legacy first-line receiving location. */
function prepare_plan_purchase_order($id, $supplier_id, $line_ids = array(), $order_date = null) {
 $groups = plan_purchase_groups($id, $line_ids);
 if (!is_int($supplier_id) || $supplier_id <= 0 || count($groups) !== 1 || !isset($groups[$supplier_id])) throw new RuntimeException('One exact plan supplier group is required.');
 $used = $groups[$supplier_id]; $cart = new purch_order(); $pricing = array();
 $queries = array('header'=>'SELECT * FROM '.TB_PREF.'procurement_plan WHERE id='.(int)$id.' FOR UPDATE',
  'all_lines'=>'SELECT * FROM '.TB_PREF.'procurement_plan_lines WHERE plan_id='.(int)$id.' ORDER BY id FOR UPDATE');
 $header = read_plan_purchase_rows($queries['header']); $header = $header[0];
 $all_lines = read_plan_purchase_rows($queries['all_lines']);
 $queries['lines'] = 'SELECT line.*, item.description AS stock_description, item.units AS stock_units, item.editable AS stock_editable, item.tax_type_id AS stock_tax, tax.name AS stock_tax_name, purchase.supplier_description FROM '.TB_PREF.'procurement_plan_lines line LEFT JOIN '.TB_PREF.'stock_master item ON item.stock_id=line.stock_id LEFT JOIN '.TB_PREF.'item_tax_types tax ON tax.id=item.tax_type_id LEFT JOIN '.TB_PREF.'purch_data purchase ON purchase.stock_id=line.stock_id AND purchase.supplier_id='.$supplier_id.' WHERE line.plan_id='.(int)$id.' AND line.id IN ('.implode(',', $used).') ORDER BY line.id FOR UPDATE';
 $lines = read_plan_purchase_rows($queries['lines']);
 // The existing source line stores the native PO identity; do not infer lineage from comments.
 $queries['effects'] = 'SELECT po.*, detail.po_detail_item, detail.item_code, detail.description AS line_description, detail.quantity_ordered, detail.quantity_received, detail.qty_invoiced, detail.unit_price, detail.delivery_date FROM '.TB_PREF.'purch_orders po LEFT JOIN '.TB_PREF.'purch_order_details detail ON detail.order_no=po.order_no WHERE po.order_no IN (SELECT po_number FROM '.TB_PREF.'procurement_plan_lines WHERE plan_id='.(int)$id.' AND supplier_id='.$supplier_id.' AND po_number>0) ORDER BY po.order_no,detail.po_detail_item FOR UPDATE';
 $effects = read_plan_purchase_rows($queries['effects']);
 $queries['all_effects'] = str_replace(' AND supplier_id='.$supplier_id, '', $queries['effects']);
 $all_effects = read_plan_purchase_rows($queries['all_effects']);
 foreach ($all_lines as $line) if ((int)$line['po_number'] > 0) {
  $matched = false;
  foreach ($all_effects as $effect) if ((int)$effect['order_no'] === (int)$line['po_number']
   && (int)$effect['supplier_id'] === (int)$line['supplier_id'] && $effect['item_code'] === $line['stock_id']) $matched = true;
  if (!$matched) throw new RuntimeException('Prior plan order provenance is unavailable.');
 }
 $queries['supplier'] = 'SELECT * FROM '.TB_PREF.'suppliers WHERE supplier_id='.$supplier_id.' AND inactive=0 FOR UPDATE';
 $supplier = read_plan_purchase_rows($queries['supplier']);
 $location_id = ''; foreach ($lines as $line) if ($line['location'] !== '') { $location_id = $line['location']; break; }
 if ($location_id === '') $location_id = $cart->Location;
 // Lock all selected line locations, including secondary locations retained by the native source.
 $location_ids = array($location_id); foreach ($lines as $line) if ($line['location'] !== '') $location_ids[] = $line['location'];
 $location_ids = array_unique($location_ids); sort($location_ids, SORT_STRING); $escaped = array(); foreach ($location_ids as $location) $escaped[] = db_escape($location);
 $queries['location'] = 'SELECT * FROM '.TB_PREF.'locations WHERE loc_code IN ('.implode(',', $escaped).') AND inactive=0 ORDER BY loc_code FOR UPDATE';
 $locations = read_plan_purchase_rows($queries['location']);
 if (count($supplier) !== 1 || count($locations) !== count($location_ids)) throw new RuntimeException('Plan supplier or location is unavailable.');
 $order_date = $order_date === null ? Today() : $order_date;
 if (!is_string($order_date) || !is_date($order_date)) throw new RuntimeException('Plan purchase date is invalid.');
 $cart->trans_type = ST_PURCHORDER; $cart->orig_order_date = $order_date; $cart->due_date = $order_date;
 $cart->Location = $location_id; $cart->Comments = sprintf(_('Created from Procurement Plan %s'), (int)$id);
 get_supplier_details_to_order($cart, $supplier_id, true); $cart->reference = 'auto';
 foreach ($lines as $line) {
  $quantity = (float)$line['suggested_order_qty']; $price = (float)$line['estimated_price'];
  if ($price <= 0) {
   if (!function_exists('get_purchase_price')) include_once($GLOBALS['path_to_root'].'/purchasing/includes/purchasing_db.inc');
   $inputs = read_requisition_purchase_pricing($supplier_id, $line['stock_id'], true);
   if ($inputs !== read_requisition_purchase_pricing($supplier_id, $line['stock_id'], false)) throw new RuntimeException('Plan pricing changed; reload the source.');
   $price = (float)get_purchase_price($supplier_id, $line['stock_id'], $quantity, date2sql($order_date));
   if ($inputs !== read_requisition_purchase_pricing($supplier_id, $line['stock_id'], true)) throw new RuntimeException('Plan pricing source changed.');
   $pricing[$line['stock_id']] = $inputs;
  }
  if ($line['stock_description'] === null || !is_finite($quantity) || $quantity <= 0 || !is_finite($price) || $price < 0) throw new RuntimeException('Plan purchase item is unavailable.');
  $cart->add_to_order(count($cart->line_items), $line['stock_id'], $quantity, $line['stock_description'], $price, $line['stock_units'], $order_date, 0, 0);
  $item = $cart->line_items[count($cart->line_items)-1];
  $item->item_description = !$line['stock_editable'] && $line['supplier_description'] !== null && $line['supplier_description'] !== '' ? $line['supplier_description'] : $line['stock_description'];
  $item->descr_editable = $line['stock_editable']; $item->units = $line['stock_units']; $item->tax_type = $line['stock_tax']; $item->tax_type_name = $line['stock_tax_name'];
 }
 if (!$cart->order_has_items() || count($lines) !== count($used)) throw new RuntimeException('Plan selected lines are unavailable.');
 $tax = array(); if (!$cart->tax_included) {
  $tax = read_agreement_purchase_tax_configuration(false);
  if ($lines !== read_plan_purchase_rows(substr($queries['lines'],0,-11))
   || (string)get_company_pref('tax_algorithm') !== (string)(empty($tax['algorithm']) ? null : $tax['algorithm'][0]['value'])
   || $tax !== read_agreement_purchase_tax_configuration(true)) throw new RuntimeException('Plan tax configuration changed; retry.');
 }
 return array('cart'=>$cart,'header'=>$header,'lines'=>$lines,'all_lines'=>$all_lines,'effects'=>$effects,'all_effects'=>$all_effects,'queries'=>$queries,'used'=>$used,'tax_configuration'=>$tax,'location'=>$locations,'supplier'=>$supplier,'pricing'=>$pricing);
}

/** Bind immutable selected source/default/pricing/tax and prior supplier effects to native PO intent. */
function collect_plan_purchase_order_data($snapshot) {
 $data = collect_purchase_order_data($snapshot['cart']);
 $source = json_encode(array(plan_purchase_header_intent($snapshot['header']),$snapshot['lines'],$snapshot['effects'],$snapshot['location'],$snapshot['supplier'],$snapshot['tax_configuration'],$snapshot['pricing']));
 if ($data === false || !is_string($source) || json_last_error() !== JSON_ERROR_NONE) throw new RuntimeException('Plan purchase intent could not be encoded.');
 $data['plan_conversion'] = array('version'=>1,'plan_id'=>(int)$snapshot['header']['id'],'supplier_id'=>(int)$snapshot['cart']->supplier_id,
  'line_ids'=>$snapshot['used'],'approval_amount'=>$snapshot['cart']->get_trans_total(),'order_date'=>date2sql($snapshot['cart']->orig_order_date),'source_sha256'=>hash('sha256',$source));
 return $data;
}

/** Check exact company and source command shape before any restricted lookup. */
function plan_purchase_order_envelope_authorized($data) {
 if (!plan_purchase_authorized() || !is_array($data)) return false;
 $cart = new purch_order();
 return isset($data['company'],$data['company_prefix'],$data['trans_type'],$data['supplier_id'],$data['plan_conversion'])
  && is_scalar($data['company']) && (string)$data['company'] === (string)$cart->company && $data['company_prefix'] === $cart->company_prefix
  && is_scalar($data['trans_type']) && (string)$data['trans_type'] === (string)ST_PURCHORDER && is_array($data['plan_conversion'])
  && isset($data['plan_conversion']['version'],$data['plan_conversion']['plan_id'],$data['plan_conversion']['supplier_id'],$data['plan_conversion']['line_ids'],$data['plan_conversion']['order_date'],$data['plan_conversion']['source_sha256'])
  && $data['plan_conversion']['version'] === 1 && is_int($data['plan_conversion']['plan_id']) && $data['plan_conversion']['plan_id'] > 0
  && is_int($data['plan_conversion']['supplier_id']) && $data['plan_conversion']['supplier_id'] > 0
  && is_array($data['plan_conversion']['line_ids']) && !empty($data['plan_conversion']['line_ids'])
  && is_string($data['plan_conversion']['order_date']) && preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D',$data['plan_conversion']['order_date'])
  && is_string($data['plan_conversion']['source_sha256']) && preg_match('/^[a-f0-9]{64}$/D',$data['plan_conversion']['source_sha256']);
}

/** Reconstruct only the exact approved source; any retained intent drift requires resubmission. */
function restore_plan_purchase_order_snapshot($data) {
 if (!plan_purchase_order_envelope_authorized($data)) throw new RuntimeException('Plan purchase envelope is unavailable.');
 $source = $data['plan_conversion']; $date = sql2date($source['order_date']);
 if (!is_date($date) || date2sql($date) !== $source['order_date']) throw new RuntimeException('Plan purchase date is invalid.');
 $snapshot = prepare_plan_purchase_order($source['plan_id'],$source['supplier_id'],$source['line_ids'],$date);
 if (json_decode(json_encode(collect_plan_purchase_order_data($snapshot)),true) !== json_decode(json_encode($data),true)) throw new RuntimeException('Plan purchase source or approved intent changed.');
 return $snapshot;
}

/** Use central maker/checker/terminal controls, with current source authority at every action. */
function validate_plan_purchase_order_action($action,$data,$draft,$actor,$comments = '') {
 if (!is_array($data) || !array_key_exists('plan_conversion',$data)) return true;
 if (!plan_purchase_order_envelope_authorized($data) || (int)$actor['user_id'] !== (int)$_SESSION['wa_current_user']->user
  || (int)$actor['role_id'] !== (int)$_SESSION['wa_current_user']->access) return false;
 if ($action === 'auto_approve' || ($draft && in_array($action,array('approve','reject'),true) && (int)$draft['submitted_by'] === (int)$actor['user_id'])) return false;
 if ($action === 'reject' || $action === 'cancel') return true;
 try {
  $snapshot = restore_plan_purchase_order_snapshot($data);
  return $action !== 'submit' || !plan_purchase_has_pending($snapshot['header']['id'],$snapshot['used']);
 } catch (Exception $exception) { return false; }
}

/** Derive only legitimate progress from locked source lines, without altering retained fields. */
function complete_plan_purchase_progress($snapshot) {
 $lines = read_plan_purchase_rows($snapshot['queries']['all_lines']); $pending = false;
 foreach ($lines as $line) if (in_array($line['status'],array('pending','approved'),true)) $pending = true;
 $status = $pending ? 'in_progress' : 'completed';
 if (!db_query('UPDATE '.TB_PREF.'procurement_plan SET status='.db_escape($status).' WHERE id='.(int)$snapshot['header']['id'],'could not update procurement plan status')) throw new RuntimeException('Plan progress update failed.');
 $want = $snapshot['header']; $want['status'] = $status;
 if (read_plan_purchase_rows($snapshot['queries']['header']) !== array($want)) throw new RuntimeException('Plan derived status differs from source intent.');
}

/** Reconcile retained source and prior native effects after the domain completion hook. */
function verify_plan_purchase_completion($snapshot,$intent,$number,$expected,$all_numbers) {
 if (!plan_purchase_authorized() || (!empty($snapshot['cart']->order_no) && (int)$snapshot['cart']->order_no !== (int)$number)) throw new RuntimeException('Plan completion authority or identity changed.');
 verify_agreement_purchase_order_effect($intent,$snapshot['cart'],$number);
 foreach ($snapshot['queries'] as $key=>$query) {
  $rows = read_plan_purchase_rows($query);
  if ($key === 'effects' || $key === 'all_effects') {
   $old = array(); foreach ($rows as $row) if (!in_array((int)$row['order_no'],$all_numbers,true)) $old[] = $row; $rows = $old;
  }
  if ($rows !== $expected[$key]) throw new RuntimeException('Plan completion changed retained source or prior effects.');
 }
 foreach ($snapshot['pricing'] as $stock=>$inputs) {
  if ($inputs !== read_requisition_purchase_pricing($snapshot['cart']->supplier_id,$stock,true)) throw new RuntimeException('Plan pricing changed.');
 }
 if (!$snapshot['cart']->tax_included && $snapshot['tax_configuration'] !== read_agreement_purchase_tax_configuration(true)) throw new RuntimeException('Plan tax configuration changed.');
 if (plan_purchase_has_pending($snapshot['header']['id'],array(),true)) throw new RuntimeException('Plan core approval custody changed.');
}

/** Write exact native effects and source line linkage inside the owning approval/batch transaction. */
function write_plan_purchase_order($snapshot,$approval_draft_id = 0,$defer_completion = false) {
 global $Refs;
 if (!plan_purchase_authorized() || empty($GLOBALS['transaction_level'])) throw new RuntimeException('Plan write requires authority and an owning transaction.');
 $cart = $snapshot['cart']; $actor = $_SESSION['wa_current_user']; $user = $actor->user; $role = $actor->access;
 if (!$approval_draft_id && !purchase_order_conversion_direct_authorized($cart)) throw new RuntimeException('Plan PO approval is required.');
 $intent = json_decode(json_encode($cart),true);
 $cart->reference = $Refs->get_next(ST_PURCHORDER,null,array('supplier_id'=>$cart->supplier_id,'date'=>$cart->orig_order_date));
 $number = add_po($cart,$approval_draft_id); if (!$number) throw new RuntimeException('Plan native purchase order failed.');
 verify_agreement_purchase_order_effect($intent,$cart,$number);
 foreach (array('header','lines','all_lines','location','supplier') as $key) {
  $rows = read_plan_purchase_rows($snapshot['queries'][$key]); if ($key === 'header') $rows = $rows[0];
  if ($rows !== $snapshot[$key]) throw new RuntimeException('Plan retained source changed.');
 }
 foreach ($snapshot['used'] as $id) if (!db_query('UPDATE '.TB_PREF."procurement_plan_lines SET status='ordered',po_number=".(int)$number.' WHERE id='.(int)$id,'could not mark procurement plan line as ordered')) throw new RuntimeException('Plan source linkage failed.');
 $current = read_plan_purchase_rows($snapshot['queries']['all_lines']);
 foreach ($current as $key=>$line) {
  $want = $snapshot['all_lines'][$key];
  if (in_array((int)$line['id'],$snapshot['used'],true)) {
   if ($line['status'] !== 'ordered' || (int)$line['po_number'] !== (int)$number) throw new RuntimeException('Plan source progress differs from native identity.');
   $want['status'] = 'ordered'; $want['po_number'] = $line['po_number'];
  }
  if ($line !== $want) throw new RuntimeException('Plan retained or unselected line changed.');
 }
 if (!$defer_completion) complete_plan_purchase_progress($snapshot);
 $expected = array(); foreach ($snapshot['queries'] as $key=>$query) {
  $expected[$key] = in_array($key,array('header','lines','all_lines'),true) ? read_plan_purchase_rows($query) : $snapshot[$key];
 }
 if (!$defer_completion) { $payload = array('plan_id'=>(int)$snapshot['header']['id'],'po_numbers'=>array((int)$number)); hook_invoke_all('procurement_plan_pos_created',$payload); }
 if ($user !== $_SESSION['wa_current_user']->user || $role !== $_SESSION['wa_current_user']->access || !$cart->is_current_company()) throw new RuntimeException('Plan completion actor or company changed.');
 verify_plan_purchase_completion($snapshot,$intent,(int)$number,$expected,array((int)$number));
 if (!$approval_draft_id && !purchase_order_conversion_direct_authorized($cart)) throw new RuntimeException('Plan completion policy changed.');
 return (int)$number;
}

/** Submit every supplier atomically through current native policy, or execute direct groups together. */
function submit_plan_purchase_orders($id,$direct_only = false) {
 global $path_to_root;
 include_once($path_to_root.'/purchasing/includes/po_class.inc');
 include_once($path_to_root.'/purchasing/includes/db/po_db.inc');
 include_once($path_to_root.'/purchasing/includes/db/purch_agreement_po_approval_db.inc');
 include_once($path_to_root.'/purchasing/includes/db/purch_requisition_po_approval_db.inc');
 if (!plan_purchase_authorized() || !begin_transaction()) return false;
 try {
  if (!class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_PURCHORDER)) throw new RuntimeException('Plan PO approval authority is unavailable.');
  $groups = plan_purchase_groups($id); $results = array(); $completed = array();
  $actor = $_SESSION['wa_current_user']; $user = $actor->user; $role = $actor->access; $company_cart = new purch_order();
  foreach ($groups as $supplier=>$ids) {
   $snapshot = prepare_plan_purchase_order($id,(int)$supplier,$ids); $cart = $snapshot['cart'];
   if (plan_purchase_has_pending($id,$ids)) throw new RuntimeException('Plan purchase approval is pending.');
   $approval = new ApprovalWorkflow($role,$user);
   if ($approval->isApprovalRequired(ST_PURCHORDER,$cart->get_trans_total(),true)) {
    if ($direct_only) throw new RuntimeException('Plan purchase approval is required.');
    $data = collect_plan_purchase_order_data($snapshot);
    $result = $approval->submit(ST_PURCHORDER,$data,$cart->get_trans_total(),array('currency'=>$cart->curr_code,'person_id'=>$cart->supplier_id,'loc_code'=>$cart->Location,'summary'=>$cart->Comments),true);
    if ($result['status'] !== 'pending') throw new RuntimeException('Plan purchase submission failed.');
    restore_plan_purchase_order_snapshot($data);
   } else {
    $number = write_plan_purchase_order($snapshot,0,true);
    $completed[] = array('snapshot'=>$snapshot,'number'=>$number,'intent'=>json_decode(json_encode($cart),true));
    $result = array('status'=>'posted','trans_no'=>$number);
   }
   if (!$company_cart->is_current_company() || !plan_purchase_authorized() || $_SESSION['wa_current_user']->user !== $user || $_SESSION['wa_current_user']->access !== $role) throw new RuntimeException('Plan purchase authority changed.');
   $results[] = $result;
  }
  if ($completed) {
   $last = $completed[count($completed)-1]['snapshot']; complete_plan_purchase_progress($last);
   $numbers = array(); foreach ($completed as $receipt) $numbers[] = $receipt['number'];
   $expected = array(); foreach ($completed as $key=>$receipt) foreach ($receipt['snapshot']['queries'] as $name=>$query) {
    $rows = read_plan_purchase_rows($query);
    if ($name === 'effects' || $name === 'all_effects') { $old = array(); foreach ($rows as $row) if (!in_array((int)$row['order_no'],$numbers,true)) $old[] = $row; $rows = $old; }
    $expected[$key][$name] = $rows;
   }
   $payload = array('plan_id'=>(int)$id,'po_numbers'=>$numbers); hook_invoke_all('procurement_plan_pos_created',$payload);
   if (!$company_cart->is_current_company() || !plan_purchase_authorized() || $_SESSION['wa_current_user']->user !== $user || $_SESSION['wa_current_user']->access !== $role) throw new RuntimeException('Plan batch completion authority changed.');
   foreach ($completed as $key=>$receipt) {
    verify_plan_purchase_completion($receipt['snapshot'],$receipt['intent'],$receipt['number'],$expected[$key],$numbers);
    if (!purchase_order_conversion_direct_authorized($receipt['snapshot']['cart'])) throw new RuntimeException('Plan batch policy changed.');
   }
  }
  if (!commit_transaction()) {
   if (!empty($GLOBALS['transaction_level'])) throw new RuntimeException('Plan purchase commit failed.');
   display_error(_('Purchase orders were committed, but resource cleanup needs recovery. Reload the source before continuing.'));
  }
  return $results;
 } catch (Exception $exception) { cancel_transaction(); return false;
 } catch (Throwable $exception) { cancel_transaction(); throw $exception; }
}

/** Execute only the exact locked approved draft with its independent checker and reservation. */
function execute_plan_purchase_order($data,$reserved_trans_no,$draft_id) {
 if (empty($GLOBALS['transaction_level']) || !plan_purchase_order_envelope_authorized($data)) return false;
 $draft = get_approval_draft_for_update($draft_id); $actor = $_SESSION['wa_current_user']; $approval = new ApprovalWorkflow($actor->access,$actor->user);
 if (!$draft || (int)$draft['trans_type'] !== ST_PURCHORDER || (int)$draft['status'] !== APPROVAL_STATUS_APPROVED || !empty($draft['approved_trans_no'])
  || (int)$draft['reserved_trans_no'] !== (int)$reserved_trans_no || (int)$draft['submitted_by'] === (int)$actor->user
  || $approval->decodeStoredDraftData($draft['draft_data']) !== $data || !has_user_approved_draft($draft_id,$actor->user,(int)$draft['current_level'])) return false;
 try {
  $snapshot = restore_plan_purchase_order_snapshot($data); $user = $actor->user; $role = $actor->access;
  $number = write_plan_purchase_order($snapshot,(int)$draft_id);
  return $user === $_SESSION['wa_current_user']->user && $role === $_SESSION['wa_current_user']->access ? $number : false;
 } catch (Exception $exception) { return false; }
}
