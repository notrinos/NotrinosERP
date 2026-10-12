<?php
/** Internal financial-obligation checks; denial never rewrites payroll, payment, filing or GL custody. */
require_once dirname(dirname(dirname(dirname(__FILE__)))).'/includes/pay_gl_001_contract.inc';

/** Parse a signed two-place accounting value without floating-point arithmetic. */
function pay_glb_001_native_finance_minor($decimal,&$error=null)
{
    if (!is_string($decimal) || !preg_match('/^(-?)(0|[1-9][0-9]*)\.([0-9]{2})$/D',$decimal,$m)) {
        $error='native_finance_exact_two_place_amount_required';return false;
    }
    $digits=ltrim($m[2].$m[3],'0');if($digits==='')return 0;
    $limit=(string)PHP_INT_MAX;
    if(PHP_INT_SIZE<8 || strlen($digits)>strlen($limit) || (strlen($digits)===strlen($limit)&&strcmp($digits,$limit)>0)) {
        $error='native_finance_minor_overflow';return false;
    }
    return ($m[1]==='-'?-1:1)*(int)$digits;
}

/** Verify one actual ledger effect against every immutable signed posting-line link. */
function pay_glb_001_native_finance_ledger_proof(array $posting,&$error=null)
{
    $sql='SELECT pl.*,al.allocation_sha256,g.type ledger_type,g.type_no ledger_no,g.tran_date ledger_date,g.account ledger_account,'
        .'g.dimension_id ledger_dimension,g.dimension2_id ledger_dimension2,'
        .'CAST(ROUND(g.amount,2) AS DECIMAL(20,2)) ledger_amount,jl.account_code '
        .'FROM '.TB_PREF.'hrm_pay_gl_001_gl_posting_lines pl '
        .'JOIN '.TB_PREF.'gl_trans g ON g.counter=pl.gl_counter '
        .'JOIN '.TB_PREF.'hrm_pay_gl_001_journal_lines jl ON jl.journal_line_id=pl.journal_line_id '
        .'JOIN '.TB_PREF.'hrm_pay_gl_001_allocations al ON al.allocation_id=pl.allocation_id '
        .'WHERE pl.posting_id='.(int)$posting['posting_id'].' ORDER BY pl.posting_line_id LIMIT 16385 FOR UPDATE';
    $q=db_query($sql);if(!$q || db_num_rows($q)<2 || db_num_rows($q)>16384)return false;
    $sum=0;$counters=array();
    while($r=db_fetch_assoc($q)) {
        $minor=pay_glb_001_native_finance_minor($r['ledger_amount'],$error);
        $sha=pay_gl_001_hash(array('posting_sha256'=>$posting['posting_sha256'],'allocation_sha256'=>$r['allocation_sha256'],
            'dimension_id'=>(int)$r['core_dimension_id'],'dimension2_id'=>(int)$r['core_dimension2_id'],'gl_counter'=>(int)$r['gl_counter'],
            'source_amount_minor'=>(int)$r['source_amount_minor'],'home_amount_minor'=>(int)$r['home_amount_minor']));
        if($minor===false || $minor!==(int)$r['home_amount_minor'] || isset($counters[$r['gl_counter']])
            || (int)$r['ledger_type']!==(int)$posting['gl_type'] || (int)$r['ledger_no']!==(int)$posting['gl_trans_no']
            || $r['ledger_date']!==$posting['journal_date'] || $r['ledger_account']!==$r['account_code']
            || (int)$r['ledger_dimension']!==(int)$r['core_dimension_id'] || (int)$r['ledger_dimension2']!==(int)$r['core_dimension2_id']
            || $sha!==$r['projection_sha256'] || abs($minor)>10000000000000 || abs($sum)>PHP_INT_MAX-abs($minor))return false;
        $sum+=$minor;$counters[$r['gl_counter']]=true;
    }
    $all=db_query('SELECT counter FROM '.TB_PREF.'gl_trans WHERE type='.(int)$posting['gl_type'].' AND type_no='.(int)$posting['gl_trans_no'].' LIMIT 16385 FOR UPDATE');
    if(!$all || db_num_rows($all)!==count($counters) || $sum!==0)return false;
    while($r=db_fetch_assoc($all))if(!isset($counters[$r['counter']]))return false;
    return true;
}

/** Verify the entire bounded linear post/reverse/repost chain and return its actual immutable leaf. */
function pay_glb_001_native_finance_posting_leaf(array $journal,&$error=null)
{
    global $path_to_root;
    if(pay_gl_001_journal_sha256($journal)!==$journal['journal_sha256'])return false;
    $authority=db_query('SELECT a.approval_id,i.projection_contract_version FROM '.TB_PREF.'hrm_pay_core_010_approvals a '
        .'JOIN '.TB_PREF.'hrm_pay_core_010_approval_intents i ON i.approval_intent_id=a.approval_intent_id '
        .'WHERE a.approval_sha256='.db_escape($journal['approval_sha256']).' AND a.result_set_sha256='.db_escape($journal['result_set_sha256']).' LIMIT 2 FOR UPDATE');
    if(!$authority || db_num_rows($authority)!==1)return false;$approved=db_fetch_assoc($authority);$native_binding=null;
    if($approved['projection_contract_version']==='native-result-v2') {
        require_once dirname(__FILE__).'/pay_glb_001_native_finance_db.inc';
        $native_binding=pay_glb_001_native_finance_approval_binding((string)$approved['approval_id'],false,$error);
        if($native_binding===false)return false;
    } elseif($approved['projection_contract_version']!==HRM_PAY_CORE_010_PROJECTION_CONTRACT_VERSION)return false;
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_gl_postings WHERE journal_intent_id='.(int)$journal['journal_intent_id'].' ORDER BY posting_id LIMIT 4097 FOR UPDATE');
    if(!$q || db_num_rows($q)>4096)return false;$leaf=null;
    while($p=db_fetch_assoc($q)) {
        $a=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_posting_authorities WHERE authority_id='.(int)$p['authority_id'].' LIMIT 2 FOR UPDATE');
        $pred=$leaf===null?null:$leaf['effect_key'];
        if(!$a || (string)$a['journal_intent_id']!==(string)$journal['journal_intent_id']
            || ($leaf===null && ($p['posting_kind']!=='post' || $p['predecessor_posting_id']!==null))
            || ($leaf!==null && ((string)$p['predecessor_posting_id']!==(string)$leaf['posting_id']
                || ($p['posting_kind']==='reverse' && !in_array($leaf['posting_kind'],array('post','repost'),true))
                || ($p['posting_kind']==='repost' && $leaf['posting_kind']!=='reverse') || $p['posting_kind']==='post'))
            || pay_gl_001_effect_key($p['posting_kind'],$journal['journal_sha256'],$pred)!==$p['effect_key']
            || pay_gl_001_hash(array('effect_key'=>$p['effect_key'],'journal_sha256'=>$journal['journal_sha256'],
                'authority_sha256'=>$a['authority_sha256'],'gl_type'=>(int)$p['gl_type'],'gl_trans_no'=>(int)$p['gl_trans_no'],
                'journal_date'=>$p['journal_date'],'posted_by'=>(int)$p['posted_by']))!==$p['posting_sha256']
            || !pay_glb_001_native_finance_ledger_proof($p,$error)
            || ($native_binding!==null && pay_glb_001_native_finance_gl_binding_proof($p,$native_binding,$error)===false))return false;
        if($leaf!==null) {
            $link=hrm_pay_core_004_read_one('SELECT rr.*,rc.command_sha256,rc.reason_code,rc.command_kind,rc.target_posting_id,rc.journal_intent_id '
                .'FROM '.TB_PREF.'hrm_pay_gl_001_reversal_repost_links rr JOIN '.TB_PREF.'hrm_pay_gl_001_recovery_commands rc '
                .'ON rc.recovery_command_id=rr.recovery_command_id WHERE rr.successor_posting_id='.(int)$p['posting_id'].' LIMIT 2 FOR UPDATE');
            if(!$link || (string)$link['target_posting_id']!==(string)$leaf['posting_id']
                || (string)$link['predecessor_posting_id']!==(string)$leaf['posting_id'] || $link['command_kind']!==$p['posting_kind']
                || $link['link_kind']!==$p['posting_kind'] || $link['effect_key']!==$p['effect_key']
                || (string)$link['journal_intent_id']!==(string)$journal['journal_intent_id']
                || pay_gl_001_hash(array('command_sha256'=>$link['command_sha256'],'predecessor_effect'=>$leaf['effect_key'],
                    'successor_posting_sha256'=>$p['posting_sha256'],'reason_code'=>$link['reason_code']))!==$link['evidence_sha256'])return false;
        }
        $leaf=$p;
    }
    return $leaf;
}

/** Deny legacy co-production and any retained payment/filing obligation until a specific governed protocol exists. */
function pay_glb_001_native_finance_external_obligations_clear(array $scope,&$error=null)
{
    if(empty($GLOBALS['transaction_level']))return false;
    foreach(array('payslips','hrm_pay_pmt_001_payment_intents','hrm_pay_fil_001_obligations','hrm_pay_fil_001_source_sets','hrm_pay_fil_001_source_links') as $table)
        if(!hrm_migration_table_exists($table)){$error='native_financial_obligation_schema_required';return false;}
    $period=(int)$scope['payroll_period_id'];$company=(int)$scope['company_id'];$legal=(int)$scope['legal_entity_id'];$group=(int)$scope['payroll_group_id'];
    $checks=array(
        'SELECT payslip_id FROM '.TB_PREF.'payslips WHERE payroll_period_id='.$period.' LIMIT 1 FOR UPDATE',
        'SELECT payment_intent_id FROM '.TB_PREF.'hrm_pay_pmt_001_payment_intents WHERE company_id='.$company.' AND legal_entity_id='.$legal
            .' AND payroll_group_id='.$group.' AND payroll_period_id='.$period.' LIMIT 1 FOR UPDATE',
        'SELECT l.filing_source_link_id FROM '.TB_PREF.'hrm_pay_fil_001_source_links l JOIN '.TB_PREF.'hrm_pay_fil_001_source_sets s '
            .'ON s.filing_source_set_id=l.filing_source_set_id JOIN '.TB_PREF.'hrm_pay_fil_001_obligations o ON o.filing_obligation_id=s.filing_obligation_id '
            .'WHERE o.company_id='.$company.' AND o.legal_entity_id='.$legal.' AND l.source_kind=\'run\' AND l.source_key='.db_escape('period:'.$period).' LIMIT 1 FOR UPDATE');
    foreach($checks as $sql){$q=db_query($sql);if(!$q || db_num_rows($q)!==0){$error='native_legacy_payment_or_filing_recovery_protocol_required';return false;}}
    return true;
}

/** Require all other historical journals reversed before a new source can replace or incur financial liability. */
function pay_glb_001_native_finance_obligations_clear(array $scope,$excluded_journal_id=null,&$error=null)
{
    if(!pay_glb_001_native_finance_external_obligations_clear($scope,$error))return false;
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_journal_intents WHERE company_id='.(int)$scope['company_id']
        .' AND legal_entity_id='.(int)$scope['legal_entity_id'].' AND payroll_group_id='.(int)$scope['payroll_group_id']
        .' AND payroll_period_id='.(int)$scope['payroll_period_id'].' ORDER BY journal_intent_id LIMIT 4097 FOR UPDATE');
    if(!$q || db_num_rows($q)>4096)return false;
    while($j=db_fetch_assoc($q)) {
        if($excluded_journal_id!==null && (string)$j['journal_intent_id']===$excluded_journal_id)continue;
        $leaf=pay_glb_001_native_finance_posting_leaf($j,$error);
        if($leaf===false || ($leaf!==null && $leaf['posting_kind']!=='reverse')){$error='native_unresolved_posted_payroll_requires_governed_reversal';return false;}
    }
    return true;
}
