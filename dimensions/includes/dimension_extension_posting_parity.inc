<?php
/**
 * DIM-EXT-001: posting parity of inactive / extension-owned economic pages.
 *
 * A copied page (Trade Finance copies the core bank payment/deposit, journal and sales quotation pages)
 * is not "supported" because it looks like the core page it was copied from. This authority is read-only:
 * it characterizes, from source, what each extension page does with the Dimension pair and compares it
 * with the shared contract the core page now calls, and it gates activation:
 *
 *  - dimension_extension_parity_profiles()  the reviewed extension profiles (surfaces, stated activation).
 *  - dimension_extension_parity_characterize()  facts read from the package source (no execution).
 *  - dimension_extension_parity_findings()  the divergences from the shared contract.
 *  - dimension_extension_parity_activation_gate()  allowed only for a package that has no profile, or whose
 *    reviewed digest still matches AND whose profile activation is `accepted`. No profile is `accepted` in
 *    this release: activation of a profiled package is refused until a later slice adds the disposable
 *    full-lifecycle (source -> GL -> inverse) and own-report evidence and changes the profile in source.
 *  - dimension_extension_parity_registry_report()  an already-active profiled package (a hand-edited
 *    registry) is reported, never silently accepted.
 *
 * Nothing here writes, switches company, activates or deactivates anything. Core writers keep fencing every
 * Dimension coordinate that an extension hands them; this authority only refuses to call a copy "integrated".
 *
 * PHP 5.6 compatible syntax.
 */
require_once __DIR__.'/dimension_extension_authority.inc';

/** Machine codes of the gate. */
function dimension_extension_parity_codes()
{
    return array('not_profiled', 'profile_accepted', 'activation_blocked', 'package_changed', 'package_unreadable', 'package_missing');
}

/**
 * Reviewed posting profiles. `surfaces` lists, per package-relative file, the shared contract the core page it
 * copies calls. `activation` is `blocked` until the gates named in `required_gates` have real evidence.
 *
 * @return array package => profile
 */
function dimension_extension_parity_profiles()
{
    return array('trade_finance'=>array(
        'activation'=>'blocked',
        'required_gates'=>array(
            'disposable_full_lifecycle'=>'bank payment/deposit, FX journal, TFD/PDC journal and quotation each posted, read back and inversed with the Dimension pair intact',
            'source_to_gl_to_inverse'=>'every posted pair reaches gl_trans and bank_trans, and an inverse posts the exact same pair',
            'own_reports'=>'Trade Finance inquiries and reports expose or explicitly omit the Dimension pair consistently with the GL',
            'security_parity'=>'the copied pages call the same page authorization, approval and classification validation as the core page',
        ),
        'surfaces'=>array(
            'manage/gl_bank.php'=>array('copies'=>'gl/gl_bank.php', 'writer'=>'write_bank_transaction',
                'contract'=>array('dimension_validation_gl_cart', 'approval_check_before_save')),
            'manage/foreign_exchange.php'=>array('copies'=>'gl/gl_journal.php', 'writer'=>'write_journal_entries',
                'contract'=>array('dimension_validation_gl_cart')),
            'manage/incoming_tfd.php'=>array('copies'=>'gl/gl_journal.php', 'writer'=>'write_journal_entries',
                'contract'=>array('dimension_validation_gl_cart')),
            'manage/outgoing_tfd.php'=>array('copies'=>'gl/gl_journal.php', 'writer'=>'write_journal_entries',
                'contract'=>array('dimension_validation_gl_cart')),
            'manage/sales_quotation_entry.php'=>array('copies'=>'sales/sales_order_entry.php', 'writer'=>'items_cart->write',
                'contract'=>array('sales_classification_intents_from_values')),
        ),
        'journal_clearing_function'=>array('file'=>'includes/trade_finance_class.inc', 'function'=>'copy_to_tf_cart'),
        'own_report_files'=>array('reporting/rep101.php', 'reporting/rep201.php', 'reporting/rep_tfdocfx.php', 'reporting/rep_tfdocin.php',
            'reporting/rep_tfdocout.php', 'reporting/rep_voucherin.php', 'reporting/rep_voucherout.php', 'reporting/forms/tf_receipt.php',
            'inquiry/forex_inquiry.php', 'inquiry/incoming_doc_inquiry.php', 'inquiry/outgoing_doc_inquiry.php', 'inquiry/trans_inquiry.php',
            'view/view_incoming_doc.php', 'view/view_outgoing_doc.php'),
    ));
}

/** Read a package file, or false. */
function dimension_extension_parity_read($package, $relative, $root = null)
{
    $path = dimension_extension_root($root).'/modules/'.$package.'/'.$relative;
    if (!is_file($path)) return false;
    $text = @file_get_contents($path);
    return $text === false ? false : str_replace("\r\n", "\n", $text);
}

/** Body of a top-level function, or false. */
function dimension_extension_parity_function_body($source, $function)
{
    if (!preg_match('/^function\s+'.preg_quote($function, '/').'\s*\([^)]*\)\s*\{/m', $source, $match, PREG_OFFSET_CAPTURE)) return false;
    $start = $match[0][1] + strlen($match[0][0]);
    $depth = 1;
    $length = strlen($source);
    for ($i = $start; $i < $length; $i++) {
        $char = $source[$i];
        if ($char === '{') $depth++;
        elseif ($char === '}' && --$depth === 0) return substr($source, $start, $i - $start);
    }
    return false;
}

/**
 * Literal Dimension arguments of every `->add_gl_item(` call in a source: each call whose 2nd and 3rd
 * arguments are the integer literals 0 and 0 is `literal_zero`; any other call is `dynamic`.
 *
 * @return array(literal_zero=>int, dynamic=>int)
 */
function dimension_extension_parity_gl_item_calls($source)
{
    $zero = 0;
    $dynamic = 0;
    if (preg_match_all('/->add_gl_item\s*\(([^;]*?)\)\s*;/s', $source, $matches)) {
        foreach ($matches[1] as $arguments) {
            $parts = array_map('trim', explode(',', $arguments));
            if (isset($parts[2]) && $parts[1] === '0' && $parts[2] === '0') $zero++;
            else $dynamic++;
        }
    }
    return array('literal_zero'=>$zero, 'dynamic'=>$dynamic);
}

/**
 * Characterize one profiled package from its source. Pure reading.
 *
 * @param string $package
 * @param string|null $root Application root (tests build a synthetic tree)
 * @return array|false
 */
function dimension_extension_parity_characterize($package, $root = null)
{
    $profiles = dimension_extension_parity_profiles();
    if (!isset($profiles[$package])) return false;
    $profile = $profiles[$package];
    $facts = array('package'=>$package, 'surfaces'=>array(), 'unreadable'=>array());
    foreach ($profile['surfaces'] as $relative=>$surface) {
        $source = dimension_extension_parity_read($package, $relative, $root);
        if ($source === false) {
            $facts['unreadable'][] = $relative;
            continue;
        }
        $row = array('copies'=>$surface['copies'], 'writer'=>$surface['writer'], 'calls_writer'=>false, 'contract'=>array(),
            'gl_items'=>dimension_extension_parity_gl_item_calls($source),
            'reads_dimension_post'=>(bool)preg_match('/\$_POST\s*\[\s*[\'"]dimension2?_id[\'"]\s*\]/', $source),
            'sets_cart_dimension'=>(bool)preg_match('/->dimension2?_id\s*=/', $source));
        $writer = $surface['writer'] === 'items_cart->write' ? '->write(' : $surface['writer'].'(';
        $row['calls_writer'] = strpos($source, $writer) !== false;
        foreach ($surface['contract'] as $function) $row['contract'][$function] = strpos($source, $function.'(') !== false;
        $facts['surfaces'][$relative] = $row;
    }
    $clearing = $profile['journal_clearing_function'];
    $clearing_source = dimension_extension_parity_read($package, $clearing['file'], $root);
    $body = $clearing_source === false ? false : dimension_extension_parity_function_body($clearing_source, $clearing['function']);
    $facts['clearing'] = $body === false ? false : dimension_extension_parity_gl_item_calls($body);
    $facts['reports'] = array();
    foreach ($profile['own_report_files'] as $relative) {
        $source = dimension_extension_parity_read($package, $relative, $root);
        if ($source === false) {
            $facts['unreadable'][] = $relative;
            continue;
        }
        $facts['reports'][$relative] = (bool)preg_match('/dimension/i', $source);
    }
    return $facts;
}

/**
 * Divergences from the shared contract, as stable codes. Empty only when every copied surface calls the shared
 * contract, so an empty list is necessary (never sufficient) for activation.
 *
 * @return array|false list of array(code, file, detail)
 */
function dimension_extension_parity_findings($package, $root = null)
{
    $facts = dimension_extension_parity_characterize($package, $root);
    if ($facts === false) return false;
    $findings = array();
    foreach ($facts['unreadable'] as $relative) $findings[] = array('source_unreadable', $relative, 'file missing or unreadable');
    foreach ($facts['surfaces'] as $relative=>$row) {
        if (!$row['calls_writer']) $findings[] = array('writer_not_called', $relative, $row['writer']);
        foreach ($row['contract'] as $function=>$present) {
            if (!$present) $findings[] = array('shared_contract_not_called', $relative, $function);
        }
        if ($row['gl_items']['literal_zero'] > 0) $findings[] = array('literal_zero_classification', $relative, $row['gl_items']['literal_zero'].' line(s)');
    }
    if ($facts['clearing'] !== false && $facts['clearing']['literal_zero'] > 0)
        $findings[] = array('literal_zero_classification', 'includes/trade_finance_class.inc', 'copy_to_tf_cart '.$facts['clearing']['literal_zero'].' clearing/partner line(s)');
    if ($facts['clearing'] === false) $findings[] = array('source_unreadable', 'includes/trade_finance_class.inc', 'copy_to_tf_cart');
    foreach ($facts['reports'] as $relative=>$mentions) {
        if (!$mentions) $findings[] = array('own_report_omits_dimension', $relative, 'no Dimension reference');
    }
    return $findings;
}

/**
 * Activation gate for one package.
 *
 * @param string $package
 * @param string|null $root
 * @param array|null $reviews Reviewed package digests (tests override)
 * @param array|null $profiles Profiles (tests override)
 * @return array allowed, code, reasons
 */
function dimension_extension_parity_activation_gate($package, $root = null, $reviews = null, $profiles = null)
{
    if ($profiles === null) $profiles = dimension_extension_parity_profiles();
    if (!is_string($package) || !isset($profiles[$package]))
        return array('allowed'=>true, 'code'=>'not_profiled', 'reasons'=>array());
    if ($reviews === null) $reviews = dimension_extension_reviews();
    $directory = dimension_extension_root($root).'/modules/'.$package;
    if (!is_dir($directory))
        return array('allowed'=>false, 'code'=>'package_missing', 'reasons'=>array('Package directory is not deployed.'));
    $digest = dimension_extension_tree_digest($directory);
    if ($digest === false)
        return array('allowed'=>false, 'code'=>'package_unreadable', 'reasons'=>array('Package source cannot be read completely.'));
    if (!isset($reviews[$package]) || $reviews[$package]['digest'] !== $digest)
        return array('allowed'=>false, 'code'=>'package_changed', 'reasons'=>array('Package source differs from the reviewed source; its Dimension behavior is unreviewed.'));
    $profile = $profiles[$package];
    if ($profile['activation'] === 'accepted')
        return array('allowed'=>true, 'code'=>'profile_accepted', 'reasons'=>array());
    $reasons = array();
    foreach ($profile['required_gates'] as $gate=>$text) $reasons[] = $gate.': '.$text;
    return array('allowed'=>false, 'code'=>'activation_blocked', 'reasons'=>$reasons);
}

/**
 * Already-active profiled packages in a registry list (a hand-edited installed_extensions.php). Reports only.
 *
 * @param array $installed Registry entries id => array(package, active, ...)
 * @return array list of array(id, package, gate_code)
 */
function dimension_extension_parity_registry_report($installed, $root = null, $reviews = null, $profiles = null)
{
    $report = array();
    foreach ((array)$installed as $id=>$entry) {
        if (!is_array($entry) || empty($entry['active']) || !isset($entry['package'])) continue;
        $gate = dimension_extension_parity_activation_gate($entry['package'], $root, $reviews, $profiles);
        if (!$gate['allowed']) $report[] = array($id, $entry['package'], $gate['code']);
    }
    return $report;
}
