<?php
/** Current ordinary Sales order source custody through the existing shared core. */
require_once __DIR__.'/sales_agreement_order_db.inc';
require_once __DIR__.'/sales_classification_db.inc';

/** Ordinary entry requires Sales order authority, without agreement-management rights. */
function sales_order_source_context($portable=false)
{
    global $db,$db_connections;
    $user=isset($_SESSION['wa_current_user'])?$_SESSION['wa_current_user']:null;
    if(!is_object($user) || !method_exists($user,'can_access') || !$user->can_access('SA_SALESORDER')
        || !isset($user->cur_con,$user->user,$user->access,$db_connections[$user->cur_con]) || !$db)return false;
    $company=array($user->cur_con,$db_connections[$user->cur_con]);
    return hash('sha256',serialize($portable?$company:array($company,$user->user,$user->access,is_object($db)?spl_object_hash($db):$db)));
}

/** Read a fixed current row; absence or an unavailable owner never establishes authority. */
function sales_order_source_row($table,$field,$value)
{
    $keys=array('debtors_master'=>'debtor_no','cust_branch'=>'branch_code','stock_master'=>'stock_id',
        'locations'=>'loc_code','payment_terms'=>'terms_indicator','sales_types'=>'id','dimensions'=>'id');
    if(!isset($keys[$table]) || $keys[$table]!==$field)throw new RuntimeException('Sales source identity is unavailable.');
    $rows=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.$table.' WHERE '.$field.'='.db_escape($value).' FOR UPDATE');
    if(!$rows || count($rows)!==1)throw new RuntimeException('Sales source owner is unavailable.');return $rows[0];
}

/** Capture current source/defaults while preserving the native user's prices and line choices. */
function prepare_native_sales_order($cart)
{
    if(!sales_order_source_context() || empty($GLOBALS['transaction_level']) || !is_object($cart)
        || !method_exists($cart,'is_current_company') || !$cart->is_current_company() || (int)$cart->trans_type!==ST_SALESORDER
        || $cart->trans_no!=0 || !is_array($cart->line_items) || !$cart->line_items)throw new RuntimeException('Current Sales order entry is unavailable.');
    $id=dimension_writer_reference_id($cart->agreement_id);if($id===false)throw new RuntimeException('Sales agreement identity is invalid.');$cart->agreement_id=$id;
    $source=null;
    if($id){
        // Match the agreement family before its header/current linked effects.
        if(sales_agreement_lifecycle_rows('SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_SALESAGREEMENT.' ORDER BY id FOR UPDATE')===false)throw new RuntimeException('Agreement source custody is unavailable.');
        $source=sales_agreement_lifecycle_source($id);$header=$source?$source['header']:null;
        if(!$header || $header['status']!=='active' || !empty($header['inactive']) || !is_date($cart->document_date)
            || $header['date_start']>date2sql($cart->document_date) || ($header['date_end'] && $header['date_end']!=='0000-00-00' && $header['date_end']<date2sql($cart->document_date))
            || (int)$header['debtor_no']!==(int)$cart->customer_id || ((int)$header['branch_code'] && (int)$header['branch_code']!==(int)$cart->Branch)
            || ($header['currency']!=='' && $header['currency']!==$cart->customer_currency))throw new RuntimeException('Selected agreement does not belong to this current order.');
        $cart->dimension_id=(int)$header['dimension_id'];$cart->dimension2_id=(int)$header['dimension2_id'];
    }
    $customer=sales_order_source_row('debtors_master','debtor_no',$cart->customer_id);
    $branch=sales_order_source_row('cust_branch','branch_code',$cart->Branch);
    $location=sales_order_source_row('locations','loc_code',$cart->Location);
    $type=sales_order_source_row('sales_types','id',$cart->sales_type);
    $terms=sales_order_source_row('payment_terms','terms_indicator',$cart->payment);
    if($customer['inactive'] || $branch['inactive'] || $location['inactive'] || $type['inactive']
        || (int)$branch['debtor_no']!==(int)$cart->customer_id || $customer['curr_code']!==$cart->customer_currency
        || (int)$branch['tax_group_id']!==(int)$cart->tax_group_id || (int)$type['tax_included']!==(int)$cart->tax_included)
        throw new RuntimeException('Current Sales order defaults changed.');
    $d1=dimension_writer_reference_id($cart->dimension_id);$d2=dimension_writer_reference_id($cart->dimension2_id);if($d1===false || $d2===false)throw new RuntimeException('Native order axes are invalid.');$cart->dimension_id=$d1;$cart->dimension2_id=$d2;
    $base=array('dimension_id'=>$d1,'dimension2_id'=>$d2);
    if($source){if(dimension_master_payload_references($header['custom_data'])===false)throw new RuntimeException('Agreement metadata requires owner recovery.');
        $base['agreement_data']=$header['custom_data']===null || $header['custom_data']===''?array():json_decode($header['custom_data'],true);}
    $payload=json_encode($base);$line_payloads=array();$items=array();$parents=array();
    foreach($cart->line_items as $index=>$line){
        $item=sales_order_source_row('stock_master','stock_id',$line->stock_id);if($item['inactive'])throw new RuntimeException('Current order item is inactive.');$items[]=$item;
        $raw='{}';if($source)foreach($source['lines'] as $agreement_line)if($agreement_line['stock_id']===$line->stock_id){$raw=$agreement_line['custom_data']===null || $agreement_line['custom_data']===''?'{}':$agreement_line['custom_data'];break;}
        $line_payloads[$index]=$raw;
        $parent_id=dimension_writer_reference_id(isset($line->src_id)?$line->src_id:0);if($parent_id===false)throw new RuntimeException('Quotation identity is invalid.');$line->src_id=$parent_id;
        if($parent_id){
            $rows=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'sales_order_details WHERE id='.db_escape($line->src_id).' AND trans_type='.ST_SALESQUOTE.' FOR UPDATE');
            if(!$rows || count($rows)!==1 || $rows[0]['stk_code']!==$line->stock_id)throw new RuntimeException('Current quotation line is unavailable.');
            $quote=sales_order_dimension_rows('sales_orders',(int)$rows[0]['order_no'],ST_SALESQUOTE);
            if(!$quote || (int)$quote[0]['debtor_no']!==(int)$cart->customer_id || (int)$quote[0]['branch_code']!==(int)$cart->Branch)throw new RuntimeException('Quotation source belongs to another customer.');
            $parents[]=array($quote,$rows);
        }
    }
    $tax=agreement_sales_order_tax(false);$cart->get_trans_total();$algorithm=empty($tax['algorithm'])?null:$tax['algorithm'][0]['value'];
    if($tax!==agreement_sales_order_tax(true) || (string)get_company_pref('tax_algorithm')!==(string)$algorithm)throw new RuntimeException('Sales tax snapshot changed.');
    $dimensions=array();$refs=array();foreach(array_merge(array($payload),array_values($line_payloads)) as $raw){
        $transition=dimension_master_payload_transition(null,$raw);if($transition===false)throw new RuntimeException('Order classification is unavailable.');
        foreach($transition['new'] as $ref){if(isset($refs[$ref[0]]) && $refs[$ref[0]][1]!==$ref[1])throw new RuntimeException('Conflicting order axes.');$refs[$ref[0]]=$ref;}
    }
    ksort($refs,SORT_NUMERIC);foreach($refs as $ref){$dimension=sales_order_source_row('dimensions','id',$ref[0]);if($dimension['closed'] || (int)$dimension['type_']!==$ref[1])throw new RuntimeException('Fresh order classification is closed or on the wrong axis.');$dimensions[]=$dimension;}
    return array('cart'=>$cart,'source'=>$source,'masters'=>array($customer,$branch,array($location),array($type),array($terms),$items,$tax),
        'dimensions'=>$dimensions,'parents'=>$parents,'header_payload'=>$payload,'line_payloads'=>$line_payloads);
}

/** Freeze current company/source and the actual native user's commercial intent. */
function collect_native_sales_order_data($snapshot)
{
    $data=collect_sales_cart_data($snapshot['cart']);
    $components=array($snapshot['source'],$snapshot['masters'],$snapshot['dimensions'],$snapshot['parents']);
    $data['native_sales_order']=array('version'=>1,'company_sha256'=>sales_order_source_context(true),
        'source_sha256'=>hash('sha256',json_encode($components)),
        'header_payload'=>$snapshot['header_payload'],'line_payloads'=>$snapshot['line_payloads'],'dimensions'=>$snapshot['dimensions']);return $data;
}

/** Validate the company before any customer, order, agreement or quotation identity lookup. */
function native_sales_order_envelope_authorized($data)
{
    global $db_connections;
    $user=isset($_SESSION['wa_current_user'])?$_SESSION['wa_current_user']:null;
    return sales_order_source_context() && is_array($data) && isset($data['company'],$data['company_prefix'],$data['company_sha256'],$data['native_sales_order'],$data['agreement_id'])
        && (string)$data['company']===(string)$user->cur_con && $data['company_prefix']===$db_connections[$user->cur_con]['tbpref']
        && $data['company_sha256']===sales_order_source_context(true) && is_array($data['native_sales_order'])
        && isset($data['native_sales_order']['version'],$data['native_sales_order']['company_sha256']) && $data['native_sales_order']['version']===1
        && $data['native_sales_order']['company_sha256']===sales_order_source_context(true) && (int)$data['trans_type']===ST_SALESORDER;
}

/** Restore only the frozen native intent whose current source still matches exactly. */
function restore_native_sales_order_snapshot($data)
{
    global $path_to_root;
    if(!native_sales_order_envelope_authorized($data))throw new RuntimeException('Sales order company/source envelope is unavailable.');
    if(!class_exists('Cart'))include_once($path_to_root.'/sales/includes/cart_class.inc');
    if(!function_exists('add_sales_order'))include_once($path_to_root.'/sales/includes/sales_db.inc');
    $cart=restore_sales_cart_from_data($data);$snapshot=prepare_native_sales_order($cart);
    $expected=sales_classification_without_approval_envelope($data);
    $actual=json_decode(json_encode(collect_native_sales_order_data($snapshot)),true);
    $expected=json_decode(json_encode($expected),true);
    if($actual!==$expected){
        throw new RuntimeException('Native Sales intent or current source changed.');
    }
    return $snapshot;
}

/** Request-local admission/receipt, released by the existing outer transaction manager. */
function native_sales_order_permit($value=null,$set=false){static $permit=null;if($set)$permit=$value;return $permit;}

/** Release no persistent metadata and no other module's transaction resource. */
function release_native_sales_order_permit(){native_sales_order_permit(null,true);return true;}

/** Read inheritance only for the actual admitted Cart object and current actor/company. */
function native_sales_order_metadata($cart)
{
    $permit=native_sales_order_permit();if(!$permit || !empty($permit['executed']) || !isset($permit['cart']))return array();
    return $permit['context']===sales_order_source_context() && $permit['cart']===spl_object_hash($cart)?$permit:false;
}

/** Submit the existing core policy or post the same native direct-policy command. */
function submit_native_sales_order($cart)
{
    if(!class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_SALESORDER))return false;
    try{
        if(!begin_transaction())return false;$snapshot=prepare_native_sales_order($cart);
        $classification=sales_classification_capture_cart($cart);
        // Re-read the native intent with the resolved source pair so the retained header/line payloads
        // describe the same decision that was sealed above.
        $snapshot=prepare_native_sales_order($cart);
        $snapshot['classification']=$classification;
        $context=sales_order_source_context();$data=collect_native_sales_order_data($snapshot);
        $actor=$_SESSION['wa_current_user'];$service=new ApprovalWorkflow($actor->access,$actor->user);
        if($service->isApprovalRequired(ST_SALESORDER,$cart->get_items_total(),true)){
            $result=$service->submit(ST_SALESORDER,$data,$cart->get_items_total(),array('summary'=>sprintf(_('Sales Order to %s'),$cart->customer_name),
                'date'=>$cart->document_date,'customer'=>$cart->customer_id,'branch'=>$cart->Branch,'currency'=>$cart->customer_currency,'person_type'=>PT_CUSTOMER,'person_id'=>$cart->customer_id,'loc_code'=>$cart->Location,
                'native_sales_order_reference'=>$cart->reference,
                'dimension_snapshot'=>sales_classification_approval_request($classification)),true);
            if(!is_array($result) || !in_array($result['status'],array('pending','auto_approved'),true))
                throw new RuntimeException('Native Sales order submission failed.');
            $published=get_approval_draft_for_update($result['draft_id']);
            $published_data=$published?$service->decodeStoredDraftData($published['draft_data']):null;
            if(!$published || sales_classification_without_approval_envelope($published_data)!==json_decode(json_encode($data),true)
                || (int)$published['reserved_trans_no']!==(int)$result['trans_no'] || $published['reference']!==$result['reference'])throw new RuntimeException('Published native Sales intent changed.');
            if($result['status']==='pending'){
                if((int)$published['status']!==APPROVAL_STATUS_PENDING || !empty($published['approved_trans_no']))throw new RuntimeException('Pending Sales publication changed.');
                restore_native_sales_order_snapshot($data);
            }elseif((int)$published['status']!==APPROVAL_STATUS_APPROVED || !verify_native_sales_order_completion('approved',$published,array('user_id'=>$actor->user,'role_id'=>$actor->access)))throw new RuntimeException('Automatic Sales publication changed.');
        }else{$number=write_native_sales_order($snapshot);$result=array('status'=>'posted','trans_no'=>$number);}
        if(sales_order_source_context()!==$context || !commit_transaction())throw new RuntimeException('Sales order source commit failed.');return $result;
    }catch(Exception $e){cancel_transaction();return false;}catch(Throwable $e){cancel_transaction();return false;}
}

/** Legacy unknown-company commands may withdraw/reject without inventing source identity. */
function validate_native_sales_order_action($action,$data,$draft,$actor,$comments='')
{
    if(!sales_order_source_context() || !is_array($data) || empty($GLOBALS['transaction_level'])
        || (int)$actor['user_id']!==(int)$_SESSION['wa_current_user']->user || (int)$actor['role_id']!==(int)$_SESSION['wa_current_user']->access)return false;
    if(!isset($data['native_sales_order']))return in_array($action,array('reject','cancel'),true);
    if(!native_sales_order_envelope_authorized($data))return false;
    if($action==='auto_approve' && !empty($data['agreement_id']))return false;
    if(!empty($data['agreement_id']) && $draft && in_array($action,array('approve','reject'),true) && (int)$draft['submitted_by']===(int)$actor['user_id'])return false;
    try{
        if(in_array($action,array('reject','cancel'),true))$source=empty($data['agreement_id'])?null:sales_agreement_lifecycle_source((int)$data['agreement_id']);
        else $source=restore_native_sales_order_snapshot($data)['source'];
        if(!empty($data['agreement_id']) && !$source)return false;
        if(!register_transaction_resource_release('native-sales-order','release_native_sales_order_permit'))return false;
        native_sales_order_permit(array('executed'=>true,'context'=>sales_order_source_context(),'action_source'=>$source,'data'=>$data,'auto'=>$action==='auto_approve'),true);return true;
    }catch(Exception $e){return false;}catch(Throwable $e){return false;}
}

/** Execute only the actual core draft and its current decision/reservation ownership. */
function execute_native_sales_order($data,$number,$draft_id)
{
    if(!native_sales_order_envelope_authorized($data) || empty($GLOBALS['transaction_level']))return false;
    try{$snapshot=restore_native_sales_order_snapshot($data);$posted=write_native_sales_order($snapshot,(int)$draft_id);return $posted===(int)$number?$posted:false;}
    catch(Exception $e){return false;}catch(Throwable $e){return false;}
}

/** Post one current native Cart, checking the real independent/system core decision when required. */
function write_native_sales_order($snapshot,$draft_id=0)
{
    if(!sales_order_source_context() || empty($GLOBALS['transaction_level']))throw new RuntimeException('Sales order write authority is unavailable.');
    $snapshot=restore_native_sales_order_snapshot(collect_native_sales_order_data($snapshot));$cart=$snapshot['cart'];
    $actor=$_SESSION['wa_current_user'];$service=new ApprovalWorkflow($actor->access,$actor->user);$context=sales_order_source_context();$number=0;
    if($draft_id){
        $draft=get_approval_draft_for_update($draft_id);$permit=native_sales_order_permit();$data=$draft?$service->decodeStoredDraftData($draft['draft_data']):null;
        $business_data=sales_classification_without_approval_envelope($data);
        $actual_data=collect_native_sales_order_data($snapshot);
        $human=$draft && has_user_approved_draft($draft_id,$actor->user,(int)$draft['current_level']);
        $automatic=$draft && $permit && !empty($permit['auto']) && $permit['context']===$context && $permit['data']===$data && empty($data['agreement_id']) && has_user_approved_draft($draft_id,0,(int)$draft['current_level']);
        if(!$draft || (int)$draft['trans_type']!==ST_SALESORDER || (int)$draft['status']!==APPROVAL_STATUS_APPROVED || !empty($draft['approved_trans_no'])
            || (int)$draft['reserved_trans_no']<=0 || (!$human && !$automatic) || !native_sales_order_envelope_authorized($data)
            || sales_classification_without_intent_carriers(json_decode(json_encode($actual_data),true))
                !==sales_classification_without_intent_carriers($business_data))
            throw new RuntimeException('Actual Sales order core decision is unavailable.');
        $number=(int)$draft['reserved_trans_no'];$cart->reference=$draft['reference'];
    }elseif($service->isApprovalRequired(ST_SALESORDER,$cart->get_items_total(),true))throw new RuntimeException('Sales order core approval is required.');
    $intent=collect_sales_cart_data($cart);$fields=agreement_sales_order_header_intent($cart);
    if(!register_transaction_resource_release('native-sales-order','release_native_sales_order_permit'))throw new RuntimeException('Sales order write admission is unavailable.');
    native_sales_order_permit(array('context'=>$context,'cart'=>spl_object_hash($cart),'header_payload'=>$snapshot['header_payload'],
        'line_payloads'=>$snapshot['line_payloads'],'reserved_number'=>$number,'draft_id'=>$draft_id),true);
    $posted=$cart->write(1);if(!$posted || $posted<=0 || ($number && $posted!==$number))throw new RuntimeException('Native Sales order write failed.');
    verify_native_sales_order_effect($snapshot,$intent,$fields,(int)$posted);
    $receipt=array('number'=>(int)$posted,'headers'=>sales_order_dimension_rows('sales_orders',$posted,ST_SALESORDER),'lines'=>sales_order_dimension_rows('sales_order_details',$posted,ST_SALESORDER),
        'source'=>empty($cart->agreement_id)?null:sales_agreement_lifecycle_source((int)$cart->agreement_id),'snapshot'=>$snapshot);
    if(sales_order_source_context()!==$context || (!$number && $service->isApprovalRequired(ST_SALESORDER,$cart->get_items_total(),true)))throw new RuntimeException('Sales order actor/company/policy changed.');
    native_sales_order_permit(array('executed'=>true,'context'=>$context,'receipt'=>$receipt),true);return (int)$posted;
}

/** Native hook output must retain every reviewed field, classification and earlier source effect. */
function verify_native_sales_order_effect($snapshot,$intent,$fields,$number,$decoded=false)
{
    $cart=$snapshot['cart'];$actual=collect_sales_cart_data($cart);$actual['trans_no']=$intent['trans_no'];
    if(!$decoded){foreach(array('reference','Comments') as $field)$intent[$field]=html_entity_decode($intent[$field],ENT_QUOTES);
    foreach($intent['line_items'] as $index=>$line)foreach(array('stock_id','item_description') as $field)$intent['line_items'][$index][$field]=html_entity_decode($line[$field],ENT_QUOTES);}
    foreach($actual['line_items'] as $index=>$line)$actual['line_items'][$index]['id']=$intent['line_items'][$index]['id'];
    $compare=function($value){
        if(!is_array($value))return $value;
        unset($value['classification_revision_id'],$value['classification_lineage_state']);
        if(isset($value['line_items'])&&is_array($value['line_items']))foreach($value['line_items'] as $index=>$line)
            if(is_array($line))unset($value['line_items'][$index]['classification_revision_id'],$value['line_items'][$index]['classification_lineage_state']);
        return $value;
    };
    if(json_decode(json_encode($compare($actual)),true)!==json_decode(json_encode($compare($intent)),true))throw new RuntimeException('Reviewed native Sales cart changed.');
    verify_agreement_sales_order_header($fields,$number);verify_agreement_sales_order_masters($snapshot);
    $header=sales_order_dimension_rows('sales_orders',$number,ST_SALESORDER);$lines=sales_order_dimension_rows('sales_order_details',$number,ST_SALESORDER);
    $payload=json_encode(get_discount_custom_data_for_save(json_decode($snapshot['header_payload'],true),$cart));
    if(!$header || $header[0]['custom_data']!==$payload || count($lines)!==count($intent['line_items']))throw new RuntimeException('Sales order retained output differs from review.');
    foreach($lines as $index=>$line){$expected=$intent['line_items'][$index];
        if($line['stk_code']!==html_specials_encode(html_entity_decode($expected['stock_id'],ENT_QUOTES)) || (float)$line['quantity']!=(float)$expected['quantity'] || (float)$line['unit_price']!=(float)$expected['price']
            || (float)$line['discount_percent']!=(float)$expected['discount_percent'] || $line['custom_data']!==$snapshot['line_payloads'][$index])throw new RuntimeException('Sales order line differs from review.');}
    if($snapshot['source']){
        $current=sales_agreement_lifecycle_source((int)$cart->agreement_id);$before=$snapshot['source'];if(!$current)throw new RuntimeException('Sales agreement source disappeared.');
        foreach(array('total_committed','total_ordered','total_delivered','total_invoiced') as $field)unset($current['header'][$field],$before['header'][$field]);
        foreach($current['lines'] as $index=>$line)foreach(array('ordered_qty','delivered_qty','invoiced_qty') as $field)unset($current['lines'][$index][$field]);
        foreach($before['lines'] as $index=>$line)foreach(array('ordered_qty','delivered_qty','invoiced_qty') as $field)unset($before['lines'][$index][$field]);
        $current['effects']=array_values(array_filter($current['effects'],function($row)use($number){return (int)$row['order_no']!==$number;}));
        if($current!==$before)throw new RuntimeException('Sales agreement or previous order effects changed.');
    }
}

/** Verify immutable core publication after native effects or source-only terminal recovery. */
function verify_native_sales_order_completion($outcome,$draft,$actor,$comments='')
{
    if(!sales_order_source_context())return false;$service=new ApprovalWorkflow($actor['role_id'],$actor['user_id']);
    $data=$service->decodeStoredDraftData($draft['draft_data']);$current=get_approval_draft_for_update($draft['id']);
    if(!$current || $service->decodeStoredDraftData($current['draft_data'])!==$data)return false;
    if(!isset($data['native_sales_order']))return in_array($outcome,array('rejected','cancelled'),true);
    if(!native_sales_order_envelope_authorized($data))return false;$permit=native_sales_order_permit();
    if(!$permit || $permit['context']!==sales_order_source_context())return false;
    if($outcome!=='approved')return array_key_exists('action_source',$permit) && (empty($data['agreement_id'])?null:sales_agreement_lifecycle_source((int)$data['agreement_id']))===$permit['action_source'];
    if(!isset($permit['receipt']))return false;$r=$permit['receipt'];verify_agreement_sales_order_masters($r['snapshot']);
    return (int)$current['approved_trans_no']===$r['number'] && (int)$current['reserved_trans_no']===$r['number']
        && sales_order_dimension_rows('sales_orders',$r['number'],ST_SALESORDER)===$r['headers'] && sales_order_dimension_rows('sales_order_details',$r['number'],ST_SALESORDER)===$r['lines']
        && (empty($data['agreement_id'])?null:sales_agreement_lifecycle_source((int)$data['agreement_id']))===$r['source'];
}

/** Review the native user's prices and inherited labels without requiring agreement management. */
function render_native_sales_order_draft($data)
{
    $html=render_sales_draft($data).'<h3>'.htmlspecialchars(_('Order classifications')).'</h3>';
    foreach($data['native_sales_order']['dimensions'] as $dimension)$html.='<p>'.htmlspecialchars($dimension['name']).' ('.htmlspecialchars($dimension['id']).')</p>';
    return $html;
}

/**
 * Native APIs retain their existing authorization owner. A new agreement link
 * additionally requires its native order authority/current company and policy;
 * historical maintenance may retain closed source references without new approval.
 */
function sales_order_source_admission($number,$type,$operation,$order)
{
    if($type!==ST_SALESORDER)return array();
    $factory=$operation==='add' && function_exists('agreement_sales_order_native_metadata')?agreement_sales_order_native_metadata($order):array();
    if($factory===false)dimension_financial_reference_failure();if($factory)return array('factory'=>true);
    $existing=0;$source=null;$hint=null;
    if($operation!=='add'){
        $rows=db_query('SELECT agreement_id FROM '.TB_PREF.'sales_orders WHERE order_no='.$number.' AND trans_type='.$type);
        $hint=$rows?db_fetch_assoc($rows):false;if(!$hint || ($existing=dimension_writer_reference_id($hint['agreement_id']))===false)dimension_financial_reference_failure();
    }
    $requested=is_object($order)&&isset($order->agreement_id)?dimension_writer_reference_id($order->agreement_id):$existing;
    if($requested===false || ($operation!=='add' && $requested!==$existing))dimension_financial_reference_failure();
    $id=$operation==='add'?$requested:$existing;
    if($id){
        if(sales_agreement_lifecycle_rows('SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_SALESAGREEMENT.' ORDER BY id FOR UPDATE')===false)dimension_financial_reference_failure();
        $source=sales_agreement_lifecycle_source($id);if(!$source)dimension_financial_reference_failure();
    }
    $state=array('id'=>$id,'source'=>$source,'operation'=>$operation,'context'=>dimension_financial_reference_context());
    if($operation==='add'){
        $admitted=native_sales_order_metadata($order);if($admitted===false)dimension_financial_reference_failure();
        if($admitted){$state['inheritance']=$admitted;return $state;}
        if($id){
            if(!class_exists('ApprovalWorkflow'))dimension_financial_reference_failure();
            $snapshot=prepare_native_sales_order($order);$actor=$_SESSION['wa_current_user'];$service=new ApprovalWorkflow($actor->access,$actor->user);
            if(!ApprovalWorkflow::isRegistered(ST_SALESORDER) || $service->isApprovalRequired(ST_SALESORDER,$order->get_items_total(),true))dimension_financial_reference_failure();
            if($order->reference==='auto'){global $Refs;$order->reference=$Refs->get_next(ST_SALESORDER,null,array('date'=>$order->document_date,'customer'=>$order->customer_id,'branch'=>$order->Branch));}
            $fields=agreement_sales_order_header_intent($order);$fields['reference']=$order->reference;$fields['comments']=$order->Comments;
            $state['direct_snapshot']=$snapshot;$state['intent']=collect_sales_cart_data($order);$state['fields']=$fields;
            $state['inheritance']=array('header_payload'=>$snapshot['header_payload'],'line_payloads'=>$snapshot['line_payloads'],'reserved_number'=>0,'draft_id'=>0);
        }
    }elseif($source && $operation==='update'){
        $payloads=array();foreach($order->line_items as $index=>$line){
            if(!empty($line->id))continue;$raw='{}';foreach($source['lines'] as $source_line)if($source_line['stock_id']===$line->stock_id){$raw=$source_line['custom_data']===null || $source_line['custom_data']===''?'{}':$source_line['custom_data'];break;}$payloads[$index]=$raw;
        }
        if($payloads)$state['inheritance']=array('preserve_header'=>true,'line_payloads'=>$payloads,'reserved_number'=>0);
    }
    return $state;
}

/** Retain the exact source link and other agreement effects after any native writer/hook. */
function verify_sales_order_source_admission($state,$number,$order)
{
    if(!$state || !empty($state['factory']))return true;
    if(dimension_financial_reference_context()!==$state['context'])return false;
    if($state['operation']!=='delete'){
        $headers=sales_order_dimension_rows('sales_orders',$number,ST_SALESORDER);
        if(!$headers || (int)$headers[0]['agreement_id']!==$state['id'])return false;
    }
    if(isset($state['direct_snapshot'])){
        verify_native_sales_order_effect($state['direct_snapshot'],$state['intent'],$state['fields'],$number,true);
        $actor=$_SESSION['wa_current_user'];$service=new ApprovalWorkflow($actor->access,$actor->user);
        if($service->isApprovalRequired(ST_SALESORDER,$order->get_items_total(),true))return false;
    }
    if($state['source']){
        $before=$state['source'];$after=sales_agreement_lifecycle_source($state['id']);if(!$after)return false;
        foreach(array('total_ordered','total_delivered','total_invoiced') as $field)unset($before['header'][$field],$after['header'][$field]);
        foreach($before['lines'] as $index=>$line)foreach(array('ordered_qty','delivered_qty','invoiced_qty') as $field)unset($before['lines'][$index][$field]);
        foreach($after['lines'] as $index=>$line)foreach(array('ordered_qty','delivered_qty','invoiced_qty') as $field)unset($after['lines'][$index][$field]);
        $exclude=function($row)use($number){return (int)$row['order_no']!==$number;};$before['effects']=array_values(array_filter($before['effects'],$exclude));$after['effects']=array_values(array_filter($after['effects'],$exclude));
        if($before!==$after)return false;
    }
    return true;
}
