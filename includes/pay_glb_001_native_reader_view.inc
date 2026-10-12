<?php
/** A6 formatting boundary. No authentication, source grant, route or live authority is conferred here. */
require_once __DIR__.'/pay_glb_001_contract.inc';
require_once __DIR__.'/pay_glb_001_s39_deployment_scope.inc';

/** Accept an exact civil date without PHP's date normalization or a timezone assumption. */
function pay_glb_001_native_reader_date($date)
{
    return is_string($date) && preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D',$date,$parts)===1
        && checkdate((int)$parts[2],(int)$parts[3],(int)$parts[1]);
}

/** Preserve the existing group's bounded UTF-8 code as text, without inventing an uppercase-only storage rule. */
function pay_glb_001_native_reader_group_code($code)
{
    return is_string($code) && preg_match('/^.{1,64}$/usD',$code)===1 && preg_match('/[\x00-\x1F\x7F]/',$code)===0;
}

/** Minimize a bounded, already verified server observation; never return source proofs, money or population counts. */
function pay_glb_001_native_reader_minimize(array $rows,$observed_at_utc,&$error=null)
{
    $error=null;
    if(!pay_glb_001_iso_utc($observed_at_utc) || count($rows)<1 || count($rows)>25
        || array_keys($rows)!==range(0,count($rows)-1)){$error='native_reader_view_invalid';return false;}
    $output=array();$seen=array();
    foreach($rows as $row) {
        if(!pay_glb_001_exact_keys($row,array('entity_code','country_code','payroll_group_code','period_start','period_end',
            'source_kind','payroll_state','financial_state','population','revision_sha256'))
            || !pay_glb_001_code($row['entity_code']) || !pay_glb_001_native_reader_group_code($row['payroll_group_code']) || !is_string($row['country_code'])
            || preg_match('/^[A-Z]{2}$/D',$row['country_code'])!==1
            || !pay_glb_001_native_reader_date($row['period_start']) || !pay_glb_001_native_reader_date($row['period_end'])
            || $row['period_start']>$row['period_end']
            || !pay_glb_001_s39_source_row_in_scope($row)
            || !in_array($row['payroll_state'],array('SELECTED_CURRENT_UNAPPROVED','APPROVED_CURRENT','UNAVAILABLE_SOURCE','UNAVAILABLE_NEGATIVE_SELECTION'),true)
            || !in_array($row['financial_state'],array('NO_CURRENT_APPROVAL','JOURNAL_UNPOSTED','POSTED_RECONCILED',
                'REPOSTED_RECONCILED','FULLY_REVERSED','UNAVAILABLE_CONFLICT','UNAVAILABLE_SOURCE'),true)
            || ($row['population']!==null && (!is_int($row['population']) || $row['population']<1 || $row['population']>4096))
            || ($row['revision_sha256']!==null && !pay_glb_001_sha256($row['revision_sha256']))) {
            $error='native_reader_row_invalid';return false;
        }
        $identity=$row['entity_code'].'|'.$row['payroll_group_code'].'|'.$row['period_start'].'|'.$row['period_end'];
        if(isset($seen[$identity])){$error='native_reader_row_ambiguous';return false;}$seen[$identity]=true;
        $suppressed=$row['population']===null || $row['population']<5;
        $output[]=array('entity_code'=>$row['entity_code'],'country_code'=>$row['country_code'],'payroll_group_code'=>$row['payroll_group_code'],
            'period_start'=>$row['period_start'],'period_end'=>$row['period_end'],'source_kind'=>$row['source_kind'],
            'payroll_state'=>$suppressed?'WITHHELD':$row['payroll_state'],
            'financial_state'=>$suppressed?'WITHHELD':$row['financial_state'],
            'population_disclosure'=>$suppressed?'WITHHELD':'AT_LEAST_FIVE',
            'revision_reference'=>$suppressed || $row['revision_sha256']===null?'':substr($row['revision_sha256'],0,12),
            'deadline_authority'=>'UNKNOWN');
    }
    return array('schema'=>'PAY_GLB_001_NATIVE_MINIMUM_VIEW_V1','observed_at_utc'=>$observed_at_utc,'rows'=>$output);
}

/** Render only the typed minimum view with text escaping; no drilldown, export or hidden source payload. */
function pay_glb_001_native_reader_render(array $view)
{
    if(!pay_glb_001_exact_keys($view,array('schema','observed_at_utc','rows'))
        || $view['schema']!=='PAY_GLB_001_NATIVE_MINIMUM_VIEW_V1' || !pay_glb_001_iso_utc($view['observed_at_utc'])
        || !is_array($view['rows']) || count($view['rows'])<1 || count($view['rows'])>25)return false;
    $labels=array('GOVERNED_NATIVE_V1'=>'Native payroll','UNAVAILABLE_SOURCE'=>'Unavailable',
        'WITHHELD'=>'Withheld','SELECTED_CURRENT_UNAPPROVED'=>'Approval pending','APPROVED_CURRENT'=>'Approved',
        'UNAVAILABLE_NEGATIVE_SELECTION'=>'Suspended or revoked','NO_CURRENT_APPROVAL'=>'Approval pending',
        'JOURNAL_UNPOSTED'=>'Journal pending','POSTED_RECONCILED'=>'Posted and reconciled',
        'REPOSTED_RECONCILED'=>'Reposted and reconciled','FULLY_REVERSED'=>'Reversed',
        'UNAVAILABLE_CONFLICT'=>'Review required','AT_LEAST_FIVE'=>'Disclosure threshold met');
    $out='<table><caption>Payroll operations</caption><thead><tr><th>Entity</th><th>Country</th><th>Group</th><th>Period</th>'
        .'<th>Source</th><th>Payroll</th><th>Accounting</th><th>Disclosure</th></tr></thead><tbody>';
    foreach($view['rows'] as $row) {
        if(!pay_glb_001_exact_keys($row,array('entity_code','country_code','payroll_group_code','period_start','period_end','source_kind',
            'payroll_state','financial_state','population_disclosure','revision_reference','deadline_authority')))return false;
        foreach($row as $value)if(!is_string($value))return false;
        if(!pay_glb_001_code($row['entity_code']) || !pay_glb_001_native_reader_group_code($row['payroll_group_code'])
            || preg_match('/^[A-Z]{2}$/D',$row['country_code'])!==1
            || !pay_glb_001_native_reader_date($row['period_start']) || !pay_glb_001_native_reader_date($row['period_end'])
            || $row['period_start']>$row['period_end']
            || !pay_glb_001_s39_public_source_row_in_scope($row)
            || ($row['revision_reference']!=='' && preg_match('/^[a-f0-9]{12}$/D',$row['revision_reference'])!==1)
            || !in_array($row['payroll_state'],array('WITHHELD','SELECTED_CURRENT_UNAPPROVED','APPROVED_CURRENT','UNAVAILABLE_SOURCE','UNAVAILABLE_NEGATIVE_SELECTION'),true)
            || !in_array($row['financial_state'],array('WITHHELD','NO_CURRENT_APPROVAL','JOURNAL_UNPOSTED','POSTED_RECONCILED','REPOSTED_RECONCILED','FULLY_REVERSED','UNAVAILABLE_CONFLICT','UNAVAILABLE_SOURCE'),true)
            || !in_array($row['population_disclosure'],array('WITHHELD','AT_LEAST_FIVE'),true)
            || $row['deadline_authority']!=='UNKNOWN'
            || ($row['population_disclosure']==='WITHHELD' && ($row['payroll_state']!=='WITHHELD' || $row['financial_state']!=='WITHHELD' || $row['revision_reference']!=='')))return false;
        $cells=array($row['entity_code'],$row['country_code'],$row['payroll_group_code'],$row['period_start'].' to '.$row['period_end'],
            $labels[$row['source_kind']],$labels[$row['payroll_state']],$labels[$row['financial_state']],$labels[$row['population_disclosure']]);
        $out.='<tr>';foreach($cells as $cell){if(!is_string($cell))return false;
            $out.='<td>'.htmlspecialchars($cell,ENT_QUOTES,'UTF-8').'</td>';}$out.='</tr>';
    }
    return $out.'</tbody></table>';
}
