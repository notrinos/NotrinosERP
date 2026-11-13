<?php
/** Source-bound Sales agreement conversions through the existing Sales order core. */
require_once __DIR__.'/sales_agreement_lifecycle_db.inc';

/** Bind conversion authority to the actual current company and native actor. */
function agreement_sales_order_context($portable=false)
{
    $user=isset($_SESSION['wa_current_user'])?$_SESSION['wa_current_user']:null;
    return $user && $user->can_access('SA_SALESORDER') ? sales_agreement_lifecycle_context($portable) : false;
}

/** Read current native tax configuration; stale snapshot caches cannot price a reviewed command. */
function agreement_sales_order_tax($lock)
{
    $result=array();
    foreach(array('tax_types'=>'id','tax_groups'=>'id','tax_group_items'=>'tax_group_id,tax_type_id',
        'item_tax_types'=>'id','item_tax_type_exemptions'=>'item_tax_type_id,tax_type_id') as $table=>$order){
        $rows=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.$table.' ORDER BY '.$order.($lock?' FOR UPDATE':''));
        if($rows===false)throw new RuntimeException('Sales tax configuration is unavailable.');$result[$table]=$rows;
    }
    $result['algorithm']=sales_agreement_lifecycle_rows("SELECT name,value FROM ".TB_PREF."sys_prefs WHERE name='tax_algorithm' ORDER BY name".($lock?' FOR UPDATE':''));
    if($result['algorithm']===false)throw new RuntimeException('Sales tax policy is unavailable.');
    return $result;
}

/** Build the existing native cart from current locked source and actual linked quantities. */
function prepare_agreement_sales_order($identity,$selections=array(),$order_date=null)
{
    if(!agreement_sales_order_context() || empty($GLOBALS['transaction_level'])
        || !($id=dimension_writer_reference_id($identity)) || !is_array($selections))throw new RuntimeException('Sales agreement conversion authority is unavailable.');
    $date=$order_date===null?Today():$order_date;if(!is_date($date))throw new RuntimeException('Sales agreement order date is invalid.');
    if(!lock_sales_agreement_core_action())throw new RuntimeException('Sales agreement source custody is unavailable.');
    $source=sales_agreement_lifecycle_source($id);$a=$source?$source['header']:null;
    if(!$a || $a['status']!=='active' || !empty($a['inactive']) || $a['date_start']>date2sql($date)
        || ($a['date_end'] && $a['date_end']!=='0000-00-00' && $a['date_end']<date2sql($date)))throw new RuntimeException('Sales agreement is unavailable for ordering.');
    $customers=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'debtors_master WHERE debtor_no='.(int)$a['debtor_no'].' AND inactive=0 FOR UPDATE');
    $branches=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'cust_branch WHERE debtor_no='.(int)$a['debtor_no'].' AND inactive=0'
        .((int)$a['branch_code']?' AND branch_code='.(int)$a['branch_code']:'').' ORDER BY branch_code FOR UPDATE');
    if(!$customers || count($customers)!==1 || !$branches)throw new RuntimeException('Current Sales customer/branch is unavailable.');
    $customer=$customers[0];$branch=$branches[0];
    if($a['currency']!=='' && $a['currency']!==$customer['curr_code'])throw new RuntimeException('Agreement/customer currency differs.');
    $locations=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'locations WHERE loc_code='.db_escape($branch['default_location']).' AND inactive=0 FOR UPDATE');
    $types=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'sales_types WHERE id='.(int)$customer['sales_type'].' AND inactive=0 FOR UPDATE');
    $payment=(int)$a['payment_terms']?(int)$a['payment_terms']:(int)$customer['payment_terms'];
    $terms=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'payment_terms WHERE terms_indicator='.$payment.' FOR UPDATE');
    if(!$locations || !$types || !$terms)throw new RuntimeException('Current Sales defaults are unavailable.');
    $cart=new Cart(ST_SALESORDER);$cart->set_customer($a['debtor_no'],$customer['name'],$customer['curr_code'],$customer['discount'],$payment);
    $cart->set_branch($branch['branch_code'],$branch['tax_group_id'],'','','');
    $cart->set_sales_type($types[0]['id'],$types[0]['sales_type'],$types[0]['tax_included'],$types[0]['factor']);
    $cart->set_location($locations[0]['loc_code'],$locations[0]['location_name']);
    $cart->ship_via=$branch['default_ship_via'];$cart->deliver_to=$branch['br_name'];
    $cart->delivery_address=$branch['br_post_address']!==''?$branch['br_post_address']:$branch['br_address'];
    $cart->cust_ref=$branch['branch_ref'];$cart->salesman=$a['salesman_id'];$cart->agreement_id=$id;
    $cart->dimension_id=(int)$a['dimension_id'];$cart->dimension2_id=(int)$a['dimension2_id'];
    $cart->document_date=$date;$cart->due_date=add_days($date,1);$cart->reference='auto';
    $cart->Comments=_('From Agreement: ').$a['reference'];
    $ordered=array();foreach($source['effects'] as $effect)if($effect['stk_code']!==null)
        $ordered[$effect['stk_code']]=(isset($ordered[$effect['stk_code']])?$ordered[$effect['stk_code']]:0)+(float)$effect['quantity'];
    $selected=$selections;$items=array();$payloads=array();$line_map=array();
    foreach($source['lines'] as $line){
        $key=(int)$line['id'];$requested=isset($selections[$key])?$selections[$key]:null;unset($selected[$key]);
        if($requested!==null && (!is_numeric($requested) || !is_finite((float)$requested) || (float)$requested<0))throw new RuntimeException('Sales agreement quantity is invalid.');
        if(!is_sales_agreement_line_data_valid($line['committed_qty'],$line['unit_price'],$line['discount_percent'],$line['min_qty_per_order'],$line['price_valid_until']))throw new RuntimeException('Sales agreement line is invalid.');
        if($line['price_valid_until'] && $line['price_valid_until']!=='0000-00-00' && $line['price_valid_until']<date2sql($date))continue;
        $remaining=max(0,(float)$line['committed_qty']-(isset($ordered[$line['stock_id']])?$ordered[$line['stock_id']]:0));
        $qty=$requested===null?$remaining:min((float)$requested,$remaining);if($qty<=0)continue;
        if($qty<(float)$line['min_qty_per_order'])throw new RuntimeException('Sales agreement minimum quantity is not met.');
        $current=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'stock_master WHERE stock_id='.db_escape($line['stock_id']).' AND inactive=0 FOR UPDATE');
        if(!$current || count($current)!==1)throw new RuntimeException('Sales agreement item is unavailable.');$items[]=$current[0];
        $index=count($cart->line_items);$cart->add_to_cart($index,$line['stock_id'],$qty,$line['unit_price'],(float)$line['discount_percent']/100,0,0,$line['description']!==''?$line['description']:$current[0]['description']);
        $payloads[$index]=$line['custom_data']===null || $line['custom_data']===''?'{}':$line['custom_data'];
        $line_map[(int)$line['id']]=$index;
    }
    if($selected || !$cart->line_items)throw new RuntimeException('No current Sales agreement lines can be ordered.');
    sales_classification_apply_agreement_conversion($cart,$source,$line_map);
    $tax=agreement_sales_order_tax(false);$cart->get_trans_total();
    $algorithm=empty($tax['algorithm'])?null:$tax['algorithm'][0]['value'];
    if((string)get_company_pref('tax_algorithm')!==(string)$algorithm || $tax!==agreement_sales_order_tax(true))throw new RuntimeException('Sales tax snapshot changed; retry a fresh command.');
    // Native constructors use consistent reads: current locked defaults must match them.
    foreach(array('debtors_master'=>array('debtor_no',$a['debtor_no'],$customer), 'cust_branch'=>array('branch_code',$branch['branch_code'],$branch),
        'sales_types'=>array('id',$types[0]['id'],$types[0]),'payment_terms'=>array('terms_indicator',$payment,$terms[0])) as $table=>$row){
        $read=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.$table.' WHERE '.$row[0].'='.db_escape($row[1]));
        if(!$read || count($read)!==1 || $read[0]!==$row[2])throw new RuntimeException('Sales default snapshot changed; retry a fresh command.');
    }
    foreach($items as $item){$read=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'stock_master WHERE stock_id='.db_escape($item['stock_id']));if(!$read || $read[0]!==$item)throw new RuntimeException('Sales item snapshot changed; retry a fresh command.');}
    if(dimension_master_payload_references($a['custom_data'])===false)throw new RuntimeException('Agreement classification requires owner recovery.');
    $retained=$a['custom_data']===null || $a['custom_data']===''?array():json_decode($a['custom_data'],true);
    $payload=json_encode(array('dimension_id'=>(int)$a['dimension_id'],'dimension2_id'=>(int)$a['dimension2_id'],'agreement_data'=>$retained));
    $masters=array();foreach(array_merge(array($payload),array_values($payloads)) as $raw){
        $transition=dimension_master_payload_transition(null,$raw);if($transition===false)throw new RuntimeException('Agreement classification is unavailable.');
        foreach($transition['new'] as $ref){if(isset($masters[$ref[0]]) && $masters[$ref[0]][1]!==$ref[1])throw new RuntimeException('Conflicting agreement classification axes.');$masters[$ref[0]]=$ref;}
    }
    $dimension_rows=array();ksort($masters,SORT_NUMERIC);foreach($masters as $ref){$rows=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'dimensions WHERE id='.$ref[0].' FOR UPDATE');if(!$rows || (int)$rows[0]['closed'] || (int)$rows[0]['type_']!==$ref[1])throw new RuntimeException('Fresh agreement classification is unavailable.');$dimension_rows[]=$rows[0];}
    return array('cart'=>$cart,'source'=>$source,'masters'=>array($customer,$branch,$locations,$types,$terms,$items,$tax),
        'header_payload'=>$payload,'line_payloads'=>$payloads,'dimensions'=>$dimension_rows,'selections'=>$selections);
}

/** Freeze the native intent and exact raw source under current company custody. */
function collect_agreement_sales_order_data($snapshot,$selections=array())
{
    $data=collect_sales_cart_data($snapshot['cart'],true);$data['agreement_id']=(int)$snapshot['source']['header']['id'];
    $data['agreement_conversion']=array('version'=>1,'agreement_id'=>$data['agreement_id'],'line_selections'=>$selections,
        'order_date'=>date2sql($snapshot['cart']->document_date),'company_sha256'=>agreement_sales_order_context(true),
        'source_sha256'=>hash('sha256',json_encode(array($snapshot['source'],$snapshot['masters']))),
        'header_payload'=>$snapshot['header_payload'],'line_payloads'=>$snapshot['line_payloads'],'dimensions'=>$snapshot['dimensions']);
    return $data;
}

/** Require native source/destination authorization before looking up an envelope's identity. */
function agreement_sales_order_envelope_authorized($data)
{
    return agreement_sales_order_context() && is_array($data) && isset($data['agreement_conversion'],$data['agreement_id'],$data['trans_type'])
        && (int)$data['trans_type']===ST_SALESORDER && is_array($data['agreement_conversion'])
        && isset($data['agreement_conversion']['version'],$data['agreement_conversion']['agreement_id'],$data['agreement_conversion']['line_selections'],
            $data['agreement_conversion']['order_date'],$data['agreement_conversion']['company_sha256'],$data['agreement_conversion']['source_sha256'])
        && $data['agreement_conversion']['version']===1 && is_int($data['agreement_id']) && $data['agreement_id']>0
        && $data['agreement_conversion']['agreement_id']===$data['agreement_id'] && is_array($data['agreement_conversion']['line_selections'])
        && is_string($data['agreement_conversion']['order_date']) && preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D',$data['agreement_conversion']['order_date'])
        && $data['agreement_conversion']['company_sha256']===agreement_sales_order_context(true);
}

/** Rebuild the exact original priced source; drift needs withdrawal and a fresh decision. */
function restore_agreement_sales_order_snapshot($data)
{
    global $path_to_root;
    if(!class_exists('Cart'))include_once($path_to_root.'/sales/includes/cart_class.inc');
    if(!function_exists('add_sales_order'))include_once($path_to_root.'/sales/includes/sales_db.inc');
    if(!agreement_sales_order_envelope_authorized($data))throw new RuntimeException('Sales agreement order envelope is unavailable.');
    $c=$data['agreement_conversion'];$date=sql2date($c['order_date']);if(!is_date($date) || date2sql($date)!==$c['order_date'])throw new RuntimeException('Sales agreement order date is invalid.');
    $snapshot=prepare_agreement_sales_order($data['agreement_id'],$c['line_selections'],$date);
    if(json_decode(json_encode(collect_agreement_sales_order_data($snapshot,$c['line_selections'])),true)!==json_decode(json_encode($data),true))throw new RuntimeException('Sales agreement source or reviewed intent changed.');
    return $snapshot;
}

/** Serialize duplicate pending conversions under the locked source; unknown stored JSON denies. */
function agreement_sales_order_has_pending($id)
{
    $json=approval_draft_current_json_sql();$sql='SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_SALESORDER
        .' AND status=0 AND CASE WHEN JSON_VALID('.$json.') THEN JSON_UNQUOTE(JSON_EXTRACT('.$json.", '$.agreement_conversion.agreement_id'))=".db_escape((string)$id).' ELSE 1 END ORDER BY id FOR UPDATE';
    $rows=db_query($sql);if(!$rows)throw new RuntimeException('Pending Sales conversion custody is unavailable.');return db_num_rows($rows)>0;
}

/** Existing transaction resource releases the conversion's native write admission. */
function agreement_sales_order_permit($value=null,$set=false)
{
    static $permit=null;if($set)$permit=$value;return $permit;
}

/** Release only this conversion's request-local write admission. */
function release_agreement_sales_order_permit(){agreement_sales_order_permit(null,true);return true;}

/** Native reference writer receives metadata only from its actual source-bound command. */
function agreement_sales_order_native_metadata($cart)
{
    $permit=agreement_sales_order_permit();if(!$permit || !empty($permit['executed']))return array();
    if($permit['context']!==agreement_sales_order_context() || $permit['cart']!==spl_object_hash($cart))return false;
    return $permit;
}

/** Submit the required independent core decision, or honor the existing native direct policy. */
function submit_agreement_sales_order($identity,$selections=array())
{
    global $path_to_root;
    if(!class_exists('Cart'))include_once($path_to_root.'/sales/includes/cart_class.inc');if(!function_exists('add_sales_order'))include_once($path_to_root.'/sales/includes/sales_db.inc');
    if(!class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_SALESORDER))return false;
    try{
        if(!begin_transaction())return false;$snapshot=prepare_agreement_sales_order($identity,$selections);$context=agreement_sales_order_context();
        if(agreement_sales_order_has_pending((int)$identity))throw new RuntimeException('Sales agreement order is already pending.');
        $actor=$_SESSION['wa_current_user'];$service=new ApprovalWorkflow($actor->access,$actor->user);$data=collect_agreement_sales_order_data($snapshot,$selections);
        if($service->isApprovalRequired(ST_SALESORDER,$snapshot['cart']->get_items_total(),true)){
            $result=$service->submit(ST_SALESORDER,$data,$snapshot['cart']->get_items_total(),array('summary'=>$snapshot['cart']->Comments,
                'currency'=>$snapshot['cart']->customer_currency,'person_type'=>PT_CUSTOMER,'person_id'=>$snapshot['cart']->customer_id,'loc_code'=>$snapshot['cart']->Location),true);
            if(!is_array($result) || $result['status']!=='pending')throw new RuntimeException('Independent Sales order submission failed.');
            $published=get_approval_draft_for_update($result['draft_id']);
            if(!$published || (int)$published['trans_type']!==ST_SALESORDER || (int)$published['status']!==APPROVAL_STATUS_PENDING
                || (int)$published['submitted_by']!==(int)$actor->user || !empty($published['approved_trans_no'])
                || (int)$published['reserved_trans_no']!==(int)$result['trans_no'] || $published['reference']!==$result['reference']
                || $service->decodeStoredDraftData($published['draft_data'])!==json_decode(json_encode($data),true))throw new RuntimeException('Published Sales order intent changed.');
            restore_agreement_sales_order_snapshot($data);
        }else{$number=write_agreement_sales_order($snapshot);$result=array('status'=>'posted','trans_no'=>$number);}
        if(agreement_sales_order_context()!==$context || !commit_transaction())throw new RuntimeException('Sales agreement order commit failed.');return $result;
    }catch(Exception $exception){cancel_transaction();return false;}catch(Throwable $exception){cancel_transaction();return false;}
}

/** Bind only converted Sales drafts; ordinary/legacy orders retain their existing registration. */
function validate_agreement_sales_order_action($action,$data,$draft,$actor,$comments='')
{
    if(!is_array($data) || !array_key_exists('agreement_conversion',$data))return validate_native_sales_order_action($action,$data,$draft,$actor,$comments);
    if(!agreement_sales_order_envelope_authorized($data) || empty($GLOBALS['transaction_level'])
        || (int)$actor['user_id']!==(int)$_SESSION['wa_current_user']->user || (int)$actor['role_id']!==(int)$_SESSION['wa_current_user']->access)return false;
    if($action==='auto_approve' || ($draft && in_array($action,array('approve','reject'),true) && (int)$draft['submitted_by']===(int)$actor['user_id']))return false;
    if(in_array($action,array('reject','cancel'),true)){
        $source=sales_agreement_lifecycle_source((int)$data['agreement_id']);
        if(!$source || !register_transaction_resource_release('sales-agreement-order','release_agreement_sales_order_permit'))return false;
        agreement_sales_order_permit(array('executed'=>true,'context'=>agreement_sales_order_context(),'action_source'=>$source),true);return true;
    }
    try{
        $snapshot=restore_agreement_sales_order_snapshot($data);
        if($action==='approve'){
            if(!register_transaction_resource_release('sales-agreement-order','release_agreement_sales_order_permit'))return false;
            agreement_sales_order_permit(array('executed'=>true,'context'=>agreement_sales_order_context(),'action_source'=>$snapshot['source']),true);
        }
        return $action!=='submit' || !agreement_sales_order_has_pending((int)$snapshot['source']['header']['id']);
    }
    catch(Exception $e){return false;}catch(Throwable $e){return false;}
}

/** Execute only the locked independently approved original core command and reservation. */
function execute_agreement_sales_order($data,$number,$draft_id)
{
    if(empty($GLOBALS['transaction_level']) || !agreement_sales_order_envelope_authorized($data))return false;
    $draft=get_approval_draft_for_update($draft_id);$actor=$_SESSION['wa_current_user'];$service=new ApprovalWorkflow($actor->access,$actor->user);
    if(!$draft || (int)$draft['trans_type']!==ST_SALESORDER || (int)$draft['status']!==APPROVAL_STATUS_APPROVED || !empty($draft['approved_trans_no'])
        || (int)$draft['reserved_trans_no']!==(int)$number || (int)$number<=0 || (int)$draft['submitted_by']===(int)$actor->user
        || $service->decodeStoredDraftData($draft['draft_data'])!==$data || !has_user_approved_draft($draft_id,$actor->user,(int)$draft['current_level']))return false;
    try{$snapshot=restore_agreement_sales_order_snapshot($data);return write_agreement_sales_order($snapshot,(int)$draft_id);}
    catch(Exception $e){return false;}catch(Throwable $e){return false;}
}

/** Post one exact native order while retaining its source, previous effects and actor. */
function write_agreement_sales_order($snapshot,$draft_id=0)
{
    global $Refs;
    if(!agreement_sales_order_context() || empty($GLOBALS['transaction_level']) || !is_array($snapshot) || !isset($snapshot['cart'],$snapshot['selections']))throw new RuntimeException('Sales agreement native source custody is unavailable.');
    $snapshot=restore_agreement_sales_order_snapshot(collect_agreement_sales_order_data($snapshot,$snapshot['selections']));
    $cart=$snapshot['cart'];$context=agreement_sales_order_context();$intent=collect_sales_cart_data($cart);$number=0;$reference=null;
    $actor=$_SESSION['wa_current_user'];$service=new ApprovalWorkflow($actor->access,$actor->user);
    if($draft_id!==0){
        if(!is_int($draft_id) || $draft_id<=0 || empty($GLOBALS['transaction_level']))throw new RuntimeException('Sales order core execution is unavailable.');
        $draft=get_approval_draft_for_update($draft_id);$data=$draft?$service->decodeStoredDraftData($draft['draft_data']):null;
        if(!$draft || (int)$draft['trans_type']!==ST_SALESORDER || (int)$draft['status']!==APPROVAL_STATUS_APPROVED || !empty($draft['approved_trans_no'])
            || (int)$draft['reserved_trans_no']<=0 || (int)$draft['submitted_by']===(int)$actor->user
            || !has_user_approved_draft($draft_id,$actor->user,(int)$draft['current_level']) || !agreement_sales_order_envelope_authorized($data)
            || json_decode(json_encode(collect_agreement_sales_order_data($snapshot,$data['agreement_conversion']['line_selections'])),true)!==$data)
            throw new RuntimeException('Sales order native write requires the actual independent core command.');
        $number=(int)$draft['reserved_trans_no'];$reference=$draft['reference'];
    }elseif($service->isApprovalRequired(ST_SALESORDER,$cart->get_items_total(),true))throw new RuntimeException('Sales order independent approval is required.');
    if(!$context || empty($GLOBALS['transaction_level']) || !register_transaction_resource_release('sales-agreement-order','release_agreement_sales_order_permit'))throw new RuntimeException('Sales agreement execution custody is unavailable.');
    $cart->reference=$reference===null?$Refs->get_next(ST_SALESORDER,null,array('date'=>$cart->document_date,'customer'=>$cart->customer_id,'branch'=>$cart->Branch)):$reference;
    $resolved_reference=$cart->reference;
    $expected_header=agreement_sales_order_header_intent($cart);
    agreement_sales_order_permit(array('context'=>$context,'cart'=>spl_object_hash($cart),'header_payload'=>$snapshot['header_payload'],
        'line_payloads'=>$snapshot['line_payloads'],'reserved_number'=>$number,'draft_id'=>$draft_id),true);
    $result=$cart->write(1);if(!$result || $result<=0 || ($number && (int)$result!==$number))throw new RuntimeException('Sales agreement order execution failed.');
    hook_invoke_all('sales_agreement_order_created',$snapshot['source']['header']['id'],$result);
    if(agreement_sales_order_context()!==$context)throw new RuntimeException('Sales agreement execution authority changed.');
    $actual=collect_sales_cart_data($cart);if($actual['reference']!==html_entity_decode($resolved_reference,ENT_QUOTES))throw new RuntimeException('Sales agreement native reference changed.');$actual['reference']=$intent['reference'];
    $intent['Comments']=html_entity_decode($intent['Comments'],ENT_QUOTES);foreach($intent['line_items'] as $index=>$line){$intent['line_items'][$index]['stock_id']=html_entity_decode($line['stock_id'],ENT_QUOTES);$intent['line_items'][$index]['item_description']=html_entity_decode($line['item_description'],ENT_QUOTES);}$actual['trans_no']=$intent['trans_no'];
    foreach($actual['line_items'] as $key=>$line)$actual['line_items'][$key]['id']=$intent['line_items'][$key]['id'];
    if(sales_classification_without_volatile_metadata(json_decode(json_encode($actual),true))
        !==sales_classification_without_volatile_metadata(json_decode(json_encode($intent),true)))
        throw new RuntimeException('Sales agreement native cart changed.');
    $headers=sales_order_dimension_rows('sales_orders',$result,ST_SALESORDER);$lines=sales_order_dimension_rows('sales_order_details',$result,ST_SALESORDER);
    $expected_payload=json_encode(get_discount_custom_data_for_save(json_decode($snapshot['header_payload'],true),$cart));
    if(!$headers || count($headers)!==1 || count($lines)!==count($intent['line_items']) || $headers[0]['custom_data']!==$expected_payload
        || (int)$headers[0]['agreement_id']!==(int)$snapshot['source']['header']['id'] || $headers[0]['reference']!==html_specials_encode(html_entity_decode($resolved_reference,ENT_QUOTES,'UTF-8'))
        || (int)$headers[0]['debtor_no']!==(int)$cart->customer_id || (int)$headers[0]['branch_code']!==(int)$cart->Branch
        || abs((float)$headers[0]['total']-$cart->get_trans_total())>0.000001)throw new RuntimeException('Sales agreement native header differs from reviewed intent.');
    foreach($lines as $index=>$line){$planned=$intent['line_items'][$index];if($line['stk_code']!==html_specials_encode(html_entity_decode($planned['stock_id'],ENT_QUOTES,'UTF-8'))
        || (float)$line['quantity']!=(float)$planned['quantity'] || (float)$line['unit_price']!=(float)$planned['price'] || (float)$line['discount_percent']!=(float)$planned['discount_percent']
        || $line['custom_data']!==$snapshot['line_payloads'][$index] || (float)$line['qty_sent'] || (float)$line['invoiced'])throw new RuntimeException('Sales agreement native line differs from reviewed intent.');}
    verify_agreement_sales_order_header($expected_header,(int)$result);
    $source=sales_agreement_lifecycle_source((int)$snapshot['source']['header']['id']);$previous=$snapshot['source'];
    foreach(array('total_committed','total_ordered','total_delivered','total_invoiced') as $field){unset($source['header'][$field],$previous['header'][$field]);}
    foreach($source['lines'] as $index=>$line)foreach(array('ordered_qty','delivered_qty','invoiced_qty') as $field)unset($source['lines'][$index][$field]);
    foreach($previous['lines'] as $index=>$line)foreach(array('ordered_qty','delivered_qty','invoiced_qty') as $field)unset($previous['lines'][$index][$field]);
    $source['effects']=array_values(array_filter($source['effects'],function($row)use($result){return (int)$row['order_no']!==(int)$result;}));
    if($source!==$previous)throw new RuntimeException('Sales agreement source or previous effects changed.');
    verify_agreement_sales_order_masters($snapshot);
    if(!$number){$actor=$_SESSION['wa_current_user'];$service=new ApprovalWorkflow($actor->access,$actor->user);if($service->isApprovalRequired(ST_SALESORDER,$cart->get_items_total(),true))throw new RuntimeException('Sales order policy changed during direct execution.');}
    $permit=agreement_sales_order_permit();$permit['executed']=true;$permit['receipt']=array('number'=>(int)$result,'headers'=>$headers,'lines'=>$lines,'source'=>sales_agreement_lifecycle_source((int)$cart->agreement_id),'snapshot'=>array('masters'=>$snapshot['masters'],'dimensions'=>$snapshot['dimensions']));agreement_sales_order_permit($permit,true);return (int)$result;
}

/** Recheck source/effects after the core has published its terminal metadata. */
function verify_agreement_sales_order_completion($outcome,$draft,$actor,$comments='')
{
    $service=new ApprovalWorkflow($actor['role_id'],$actor['user_id']);$data=$service->decodeStoredDraftData($draft['draft_data']);
    if(!is_array($data) || !array_key_exists('agreement_conversion',$data))return verify_native_sales_order_completion($outcome,$draft,$actor,$comments);
    if(!agreement_sales_order_envelope_authorized($data))return false;
    $current=get_approval_draft_for_update($draft['id']);
    if(!$current || $service->decodeStoredDraftData($current['draft_data'])!==$data)return false;
    $permit=agreement_sales_order_permit();
    if($outcome!=='approved')return $permit && isset($permit['action_source'])
        && $permit['context']===agreement_sales_order_context()
        && sales_agreement_lifecycle_source((int)$data['agreement_id'])===$permit['action_source'];$receipt=$permit && isset($permit['receipt'])?$permit['receipt']:false;
    if($receipt)verify_agreement_sales_order_masters($receipt['snapshot']);
    return $receipt && $permit['context']===agreement_sales_order_context()
        && (int)$current['approved_trans_no']===$receipt['number'] && (int)$current['reserved_trans_no']===$receipt['number']
        && sales_order_dimension_rows('sales_orders',$receipt['number'],ST_SALESORDER)===$receipt['headers']
        && sales_order_dimension_rows('sales_order_details',$receipt['number'],ST_SALESORDER)===$receipt['lines']
        && sales_agreement_lifecycle_source((int)$data['agreement_id'])===$receipt['source'];
}

/** Render the immutable native order and inherited classifications for the actual checker. */
function render_agreement_sales_order_draft($data)
{
    $html=render_sales_draft($data);$source=$data['agreement_conversion'];
    $html.='<h3>'.htmlspecialchars(_('Agreement classifications')).'</h3><table class="tablestyle">';
    foreach($source['dimensions'] as $dimension)$html.='<tr><td>'.htmlspecialchars($dimension['name']).'</td><td>'.htmlspecialchars($dimension['id']).'</td></tr>';
    $html.='</table><pre>'.htmlspecialchars($source['header_payload']).'</pre>';
    foreach($source['line_payloads'] as $payload)$html.='<pre>'.htmlspecialchars($payload).'</pre>';
    return $html;
}

/** Retain every master/configuration row actually included in the immutable source review. */
function verify_agreement_sales_order_masters($snapshot)
{
    list($customer,$branch,$locations,$types,$terms,$items,$tax)=$snapshot['masters'];
    $expected=array('debtors_master'=>array('debtor_no',$customer['debtor_no'],array($customer)),
        'cust_branch'=>array('branch_code',$branch['branch_code'],array($branch)),
        'locations'=>array('loc_code',$locations[0]['loc_code'],$locations),
        'sales_types'=>array('id',$types[0]['id'],$types),
        'payment_terms'=>array('terms_indicator',$terms[0]['terms_indicator'],$terms));
    foreach($expected as $table=>$row)
        if(sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.$table.' WHERE '.$row[0].'='.db_escape($row[1]).' FOR UPDATE')!==$row[2])throw new RuntimeException('Sales agreement reviewed master changed during execution.');
    foreach($items as $item)
        if(sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'stock_master WHERE stock_id='.db_escape($item['stock_id']).' FOR UPDATE')!==array($item))throw new RuntimeException('Sales agreement reviewed item changed during execution.');
    foreach($snapshot['dimensions'] as $dimension)
        if(sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'dimensions WHERE id='.(int)$dimension['id'].' FOR UPDATE')!==array($dimension))throw new RuntimeException('Sales agreement reviewed classification changed during execution.');
    if(agreement_sales_order_tax(true)!==$tax)throw new RuntimeException('Sales agreement reviewed tax configuration changed during execution.');
}

/** Capture the actual native header SQL contract before any persistence hook runs. */
function agreement_sales_order_header_intent($cart)
{
    return array('type'=>0,'version'=>0,'debtor_no'=>$cart->customer_id,'branch_code'=>$cart->Branch,
        'salesman_code'=>$cart->salesman,'customer_ref'=>$cart->cust_ref,'reference'=>html_entity_decode($cart->reference,ENT_QUOTES),
        'comments'=>html_entity_decode($cart->Comments,ENT_QUOTES),'ord_date'=>date2sql($cart->document_date),
        'order_type'=>$cart->sales_type,'ship_via'=>$cart->ship_via,'deliver_to'=>$cart->deliver_to,
        'delivery_address'=>$cart->delivery_address,'contact_phone'=>$cart->phone,'freight_cost'=>$cart->freight_cost,
        'from_stk_loc'=>$cart->Location,'delivery_date'=>date2sql($cart->due_date),'payment_terms'=>$cart->payment,
        'total'=>$cart->get_trans_total(),'prep_amount'=>$cart->prep_amount,'template_id'=>(int)$cart->template_id,
        'validity_date'=>empty($cart->validity_date)?null:date2sql($cart->validity_date),'opportunity_id'=>(int)$cart->opportunity_id,
        'terms_and_conditions'=>$cart->terms_and_conditions,'margin_total'=>(float)$cart->margin_total,
        'cost_total'=>(float)$cart->cost_total,'agreement_id'=>(int)$cart->agreement_id);
}

/** Compare every native header field with the frozen intent, using native SQL escaping. */
function verify_agreement_sales_order_header($fields,$number)
{
    $numeric=array('type','version','debtor_no','branch_code','salesman_code','order_type','ship_via','freight_cost',
        'payment_terms','total','prep_amount','template_id','opportunity_id','margin_total','cost_total','agreement_id');
    $where='order_no='.$number.' AND trans_type='.ST_SALESORDER;
    foreach($fields as $field=>$value){
        if($field==='validity_date' && $value===null)$where.=' AND validity_date IS NULL';
        else $where.=' AND '.(in_array($field,$numeric,true)?'':'BINARY ').$field.'='.db_escape($value);
    }
    $rows=db_query('SELECT order_no FROM '.TB_PREF.'sales_orders WHERE '.$where.' FOR UPDATE');
    if(!$rows || db_num_rows($rows)!==1)throw new RuntimeException('Sales agreement native header differs from reviewed fields.');
}
