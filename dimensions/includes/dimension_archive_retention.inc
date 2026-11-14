<?php
/**
 * DIM-ARC-001: fiscal close, retention and historical retrieval safety.
 *
 * The existing fiscal-year purge deletes a closed year's budgets and ledger rows and re-summarizes
 * balance-sheet ledger rows by account and person only, so a Dimension classification cannot survive it.
 * B07 already refuses a purge when a destroyed row carries a native or retained Dimension reference. This
 * authority adds what that barrier does not cover and never executes a purge itself:
 *
 * - a reviewed retention policy for the protected lineage and payroll stores that reference the ledger rows
 *   a purge would destroy (posting links, source revisions, native payroll postings, payroll periods and
 *   payslips). A purge is refused while any of them references the purged range; an absent optional store is
 *   clear and an unreadable or schema-drifted one is unknown and blocks;
 * - a read-only impact preview (counts and states only, never a payload) that shows what the purge would
 *   destroy and which classifications the summary would lose;
 * - a verified archive barrier: the purge needs a backup that was really written and whose ledger and budget
 *   history can be retrieved WITHOUT restoring it, with the same original identifiers, row digest and
 *   per-classification totals as the live rows about to be destroyed;
 * - archive retrieval: a streaming reader for the application's own SQL backup that returns original ledger
 *   and budget identifiers, digests and classification totals, confined to the company backup directory and
 *   denied without the backup and fiscal-year authorities.
 *
 * Nothing here deletes, rewrites, backfills or restores history. PHP 5.6 compatible syntax.
 */
require_once __DIR__.'/dimension_retention_authority.inc';

/** Closing-date format accepted everywhere in this file. */
function dimension_archive_valid_date($date)
{
    return is_string($date) && preg_match('/^[0-9]{4}-(?:0[1-9]|1[0-2])-(?:0[1-9]|[12][0-9]|3[01])$/D', $date) === 1;
}

/**
 * Reviewed protected stores that reference ledger history. Each entry names the store tables it needs,
 * the FROM clause (alias t) and the predicate selecting the rows that reference the purged range.
 * Placeholder {to} is the validated closing date.
 *
 * @return array store => array(tables => table => columns, from, where, why)
 */
function dimension_archive_protected_stores()
{
    $p = TB_PREF;
    return array(
        'dimension_posting_links'=>array(
            'tables'=>array('dimension_posting_links'=>array('gl_counter'), 'gl_trans'=>array('counter', 'tran_date')),
            'from'=>$p.'dimension_posting_links t INNER JOIN '.$p.'gl_trans g ON g.counter=t.gl_counter',
            'where'=>"g.tran_date <= '{to}'", 'why'=>'ledger-to-classification lineage'),
        'dimension_source_revisions'=>array(
            'tables'=>array('dimension_source_revisions'=>array('effective_date')),
            'from'=>$p.'dimension_source_revisions t', 'where'=>"t.effective_date <= '{to}'", 'why'=>'resolved source classification revisions'),
        'hrm_pay_gl_001_gl_postings'=>array(
            'tables'=>array('hrm_pay_gl_001_gl_postings'=>array('journal_date')),
            'from'=>$p.'hrm_pay_gl_001_gl_postings t', 'where'=>"t.journal_date <= '{to}'", 'why'=>'native payroll journal postings'),
        'hrm_pay_gl_001_gl_posting_lines'=>array(
            'tables'=>array('hrm_pay_gl_001_gl_posting_lines'=>array('gl_counter'), 'gl_trans'=>array('counter', 'tran_date')),
            'from'=>$p.'hrm_pay_gl_001_gl_posting_lines t INNER JOIN '.$p.'gl_trans g ON g.counter=t.gl_counter',
            'where'=>"g.tran_date <= '{to}'", 'why'=>'native payroll posting ledger lines'),
        'payroll_periods'=>array(
            'tables'=>array('payroll_periods'=>array('to_date', 'gl_trans_no')),
            'from'=>$p.'payroll_periods t', 'where'=>"t.to_date <= '{to}' AND IFNULL(t.gl_trans_no,0)<>0", 'why'=>'legacy payroll period journal'),
        'payslips'=>array(
            'tables'=>array('payslips'=>array('tran_date', 'trans_no')),
            'from'=>$p.'payslips t', 'where'=>"t.tran_date <= '{to}' AND IFNULL(t.trans_no,0)<>0", 'why'=>'historical payslip journal'));
}

/**
 * Reviewed statement of what a fiscal-year purge may never do and which stores hold the reason.
 *
 * @return array names of the protected stores, the destroyed history stores and the refusal rules
 */
function dimension_archive_retention_policy()
{
    return array(
        'protected_stores'=>array_keys(dimension_archive_protected_stores()),
        'history_stores'=>array('gl_trans', 'budget_trans'),
        'rules'=>array(
            'A purge is refused while a destroyed row retains a Dimension reference (B07).',
            'A purge is refused while lineage or payroll history references the purged range.',
            'A purge needs a backup that is retrievable without restore and matches the live history.',
            'Nothing is purged, summarized with new axes, backfilled or restored automatically.'));
}

/**
 * Assess the protected stores against the purge range.
 *
 * @param string $to Closing date, Y-m-d
 * @param bool $lock Take shared locks until the caller's transaction ends (caller must own one)
 * @param bool $count Count the referencing rows instead of stopping at the first (read-only preview)
 * @return array state clear|blocked, stores name => clear|absent|referenced|unknown, counts name => int
 */
function dimension_archive_protected_assess($to, $lock = false, $count = false)
{
    $stores = dimension_archive_protected_stores();
    $result = array('state'=>'blocked', 'stores'=>array(), 'counts'=>array());
    foreach ($stores as $name=>$spec) {
        $result['stores'][$name] = 'unknown';
        $result['counts'][$name] = 0;
    }
    if (!dimension_archive_valid_date($to)) return $result;
    if ($lock && empty($GLOBALS['transaction_level'])) return $result;
    try {
        $tables = array();
        foreach ($stores as $spec) foreach (array_keys($spec['tables']) as $table) $tables[$table] = true;
        $columns = dimension_reference_recheck_columns(array_keys($tables));
        if ($columns === false) return $result;
        foreach ($stores as $name=>$spec) {
            $own = key($spec['tables']);
            if (empty($columns[$own])) {
                $result['stores'][$name] = 'absent';
                continue;
            }
            $drift = false;
            foreach ($spec['tables'] as $table=>$needed) {
                if (empty($columns[$table])) { $drift = true; continue; }
                foreach ($needed as $column) if (!isset($columns[$table][$column])) $drift = true;
            }
            if ($drift) continue;
            $from = str_replace('{to}', $to, $spec['from']);
            $where = str_replace('{to}', $to, $spec['where']);
            if ($count) {
                $rows = db_query('SELECT COUNT(*) FROM '.$from.' WHERE '.$where);
                if (!$rows) continue;
                $row = db_fetch_row($rows);
                $result['counts'][$name] = (int)$row[0];
                $result['stores'][$name] = $result['counts'][$name] > 0 ? 'referenced' : 'clear';
            } else {
                $rows = db_query('SELECT 1 FROM '.$from.' WHERE '.$where.' LIMIT 1'.($lock ? ' LOCK IN SHARE MODE' : ''));
                if (!$rows) continue;
                $result['stores'][$name] = db_num_rows($rows) ? 'referenced' : 'clear';
            }
        }
    } catch (Exception $exception) {
        return $result;
    }
    $result['state'] = count(array_diff($result['stores'], array('clear', 'absent'))) ? 'blocked' : 'clear';
    return $result;
}

/**
 * Names of the protected stores that block, for an operator message (no values).
 *
 * @param array $assessment Result of dimension_archive_protected_assess()
 * @return string[]
 */
function dimension_archive_protected_blocking($assessment)
{
    $names = array();
    foreach ($assessment['stores'] as $name=>$state) if ($state !== 'clear' && $state !== 'absent') $names[] = $name;
    return $names;
}

/**
 * Admit a purge against the protected lineage and payroll stores; early lock-free, then under shared locks.
 *
 * @param string $to Closing date, Y-m-d
 * @param bool $lock True inside the purge transaction
 * @return bool True only when no protected store references the purged range
 */
function dimension_archive_protected_admit($to, $lock)
{
    $assessment = dimension_archive_protected_assess($to, $lock);
    if ($assessment['state'] === 'clear') return true;
    display_error(_('This fiscal year cannot be deleted because retained classification lineage or payroll history refers to it. Blocking records: ')
        .implode(', ', dimension_archive_protected_blocking($assessment)));
    return false;
}

/**
 * Read-only impact preview of a fiscal-year purge up to one closing date. Counts and states only.
 *
 * @param string $to Closing date, Y-m-d
 * @return array valid, to, state, ledger, budget, stores, protected, blocking, policy
 */
function dimension_archive_impact_preview($to)
{
    $preview = array('valid'=>false, 'to'=>$to, 'state'=>'blocked', 'ledger'=>array(), 'budget'=>array(),
        'stores'=>array(), 'protected'=>array(), 'blocking'=>array(), 'policy'=>dimension_archive_retention_policy());
    if (!dimension_archive_valid_date($to)) return $preview;
    $p = TB_PREF;
    $one = function ($sql) {
        $rows = db_query($sql);
        if (!$rows) return null;
        $row = db_fetch_row($rows);
        return $row ? (int)$row[0] : 0;
    };
    $balance = 'g.account IN (SELECT m.account_code FROM '.$p.'chart_master m INNER JOIN '.$p.'chart_types y ON y.id=m.account_type'
        .' INNER JOIN '.$p.'chart_class c ON c.cid=y.class_id WHERE c.ctype>0 AND c.ctype<'.(int)CL_INCOME.')';
    $tagged = '(g.dimension_id<>0 OR g.dimension2_id<>0)';
    $preview['ledger'] = array(
        'rows'=>$one('SELECT COUNT(*) FROM '.$p.'gl_trans g WHERE g.tran_date <= \''.$to.'\''),
        'tagged_rows'=>$one('SELECT COUNT(*) FROM '.$p.'gl_trans g WHERE g.tran_date <= \''.$to.'\' AND '.$tagged),
        'balance_sheet_rows'=>$one('SELECT COUNT(*) FROM '.$p.'gl_trans g WHERE g.tran_date <= \''.$to.'\' AND '.$balance),
        'summary_rows'=>$one('SELECT COUNT(*) FROM (SELECT 1 FROM '.$p.'gl_trans g WHERE g.tran_date <= \''.$to.'\' AND '.$balance
            .' GROUP BY g.account, g.person_type_id, g.person_id) s'),
        'summarized_tagged_rows'=>$one('SELECT COUNT(*) FROM '.$p.'gl_trans g WHERE g.tran_date <= \''.$to.'\' AND '.$balance.' AND '.$tagged));
    $preview['budget'] = array(
        'rows'=>$one('SELECT COUNT(*) FROM '.$p.'budget_trans WHERE tran_date <= \''.$to.'\''),
        'tagged_rows'=>$one('SELECT COUNT(*) FROM '.$p.'budget_trans WHERE tran_date <= \''.$to.'\' AND (dimension_id<>0 OR dimension2_id<>0)'));
    $assessment = dimension_retention_purge_assess($to, false);
    $protected = dimension_archive_protected_assess($to, false, true);
    $preview['stores'] = $assessment['stores'];
    $preview['protected'] = $protected['stores'];
    $preview['protected_counts'] = $protected['counts'];
    $preview['blocking'] = array_merge(dimension_retention_blocking_stores($assessment), dimension_archive_protected_blocking($protected));
    $preview['state'] = ($assessment['state'] === 'clear' && $protected['state'] === 'clear') ? 'clear' : 'blocked';
    $preview['valid'] = !in_array(null, $preview['ledger'], true) && !in_array(null, $preview['budget'], true);
    if (!$preview['valid']) $preview['state'] = 'blocked';
    return $preview;
}

//---------------------------------------------------------------------------------------------
// Archive reading and retrieval
//---------------------------------------------------------------------------------------------

/**
 * Company backup directory, only when it exists. The caller must refuse the purge on false.
 *
 * @return string|false Directory with trailing separator, or false when unavailable
 */
function dimension_archive_backup_directory()
{
    global $SysPrefs;
    if (!is_object($SysPrefs) || !method_exists($SysPrefs, 'backup_dir')) return false;
    $directory = $SysPrefs->backup_dir();
    if (!is_string($directory) || $directory === '' || !is_dir($directory) || !is_writable($directory)) return false;
    return rtrim($directory, '/\\').'/';
}

/** Decode one MySQL string literal body (the text between the quotes) written by the application's exporter. */
function dimension_archive_unescape($body)
{
    static $map = array('0'=>"\0", 'n'=>"\n", 'r'=>"\r", 't'=>"\t", 'b'=>"\x08", 'Z'=>"\x1a");
    if (strpos($body, '\\') === false && strpos($body, "''") === false) return $body;
    return preg_replace_callback('/\\\\(.)|\'\'/s', function ($match) use ($map) {
        if (!isset($match[1])) return "'";
        return isset($map[$match[1]]) ? $map[$match[1]] : $match[1];
    }, $body);
}

/**
 * Split one exported tuple body into values: a string, or null for SQL NULL.
 *
 * @return array|false Values, or false when the tuple is malformed
 */
function dimension_archive_split_tuple($body)
{
    $values = array();
    $offset = 0;
    $length = strlen($body);
    $pattern = '~\G\s*(?:\'((?:[^\'\\\\]++|\\\\.|\'\')*+)\'|([^,\']*))\s*(?:,|$)~s';
    while (true) {
        if (!preg_match($pattern, $body, $match, PREG_OFFSET_CAPTURE, $offset)) return false;
        if ($match[1][1] !== -1) {
            $values[] = dimension_archive_unescape($match[1][0]);
        } else {
            $token = trim($match[2][0]);
            $values[] = strtoupper($token) === 'NULL' ? null : $token;
        }
        $offset = $match[0][1] + strlen($match[0][0]);
        if ($offset >= $length) return $values;
    }
    return false;
}

/**
 * Stream every row of one table from a backup written by the application's exporter, without restoring it.
 * Plain `.sql` and gzip files are read; the structure block provides the column names.
 *
 * @param string $path Backup file
 * @param string $table Prefixed table name as written in the backup
 * @param callable $consumer Called with column => value|null per row
 * @return array|false array(columns, rows) or false when unreadable, structureless or malformed
 */
function dimension_archive_read_table($path, $table, $consumer)
{
    if (!is_string($path) || !is_file($path) || !is_readable($path) || !preg_match('/^[A-Za-z0-9_]+$/D', $table)) return false;
    $handle = @gzopen($path, 'rb');
    if (!$handle) return false;
    $columns = array();
    $rows = 0;
    $structure = false;
    $in_create = false;
    $in_insert = false;
    $buffer = '';
    $tuple = '~\G\s*\(((?:\'(?:[^\'\\\\]|\\\\.|\'\')*+\'|[^\'()]++)*+)\)\s*([,;])~s';
    $ok = true;
    while (($line = gzgets($handle, 8388608)) !== false) {
        if ($in_insert) {
            $buffer .= $line;
            $offset = 0;
            while (preg_match($tuple, $buffer, $match, 0, $offset)) {
                $values = dimension_archive_split_tuple($match[1]);
                if ($values === false || count($values) !== count($columns)) { $ok = false; break 2; }
                call_user_func($consumer, array_combine($columns, $values));
                $rows++;
                $offset += strlen($match[0]);
                if ($match[2] === ';') { $in_insert = false; $buffer = ''; continue 2; }
            }
            $buffer = substr($buffer, $offset);
            continue;
        }
        if ($in_create) {
            if (preg_match('/^\s*`([^`]+)`\s/', $line, $match)) $columns[] = $match[1];
            if (strncmp(ltrim($line), ')', 1) === 0) { $in_create = false; $structure = true; }
            continue;
        }
        if (!$structure && strpos($line, 'CREATE TABLE `'.$table.'`') === 0) { $in_create = true; $columns = array(); continue; }
        if ($structure && strpos($line, 'INSERT INTO `'.$table.'` VALUES') === 0) { $in_insert = true; $buffer = substr($line, strlen('INSERT INTO `'.$table.'` VALUES')); }
    }
    $ended_clean = !$in_insert && !$in_create;
    gzclose($handle);
    if (!$ok || !$structure || !$ended_clean || !count($columns)) return false;
    return array('columns'=>$columns, 'rows'=>$rows);
}

/**
 * Whether a backup ends with the exporter's final statement, i.e. it was written completely. A backup cut
 * short anywhere (including after the history tables) is not a usable archive.
 *
 * @param string $path Backup file
 * @return bool
 */
function dimension_archive_is_complete($path)
{
    if (!is_string($path) || !is_file($path) || !is_readable($path)) return false;
    $handle = @gzopen($path, 'rb');
    if (!$handle) return false;
    $last = '';
    while (($line = gzgets($handle, 8388608)) !== false) {
        $trimmed = trim($line);
        if ($trimmed !== '') $last = $trimmed;
    }
    gzclose($handle);
    return strpos($last, 'ALTER DATABASE CHARACTER SET ') === 0;
}

/**
 * Fresh order-independent accumulator of a history store (ledger or budget).
 *
 * @param string $date_column Date column used to select the purged range
 * @param string $to Closing date, Y-m-d
 */
function dimension_archive_accumulator($date_column, $to)
{
    return array('date'=>$date_column, 'to'=>$to, 'rows'=>0, 'crc_a'=>0, 'crc_b'=>0, 'ids'=>array(), 'totals'=>array(), 'accounts'=>array());
}

/**
 * Add one row to an accumulator when it belongs to the purged range. The identifier is the first column.
 *
 * @param array $acc Accumulator by reference
 * @param array $row column => value|null
 */
function dimension_archive_accumulate(&$acc, $row)
{
    if (!isset($row[$acc['date']]) || (string)$row[$acc['date']] > $acc['to']) return;
    $canon = array();
    foreach ($row as $value) $canon[] = $value === null ? "\x00N" : (string)$value;
    $text = implode("\x1f", $canon);
    $acc['rows']++;
    $acc['crc_a'] = ($acc['crc_a'] + crc32($text)) % 1099511627776;
    $acc['crc_b'] = ($acc['crc_b'] + crc32("\x01".$text.strrev($text))) % 1099511627776;
    $identifier = (string)reset($row);
    if (!isset($acc['ids']['min']) || (int)$identifier < $acc['ids']['min']) $acc['ids']['min'] = (int)$identifier;
    if (!isset($acc['ids']['max']) || (int)$identifier > $acc['ids']['max']) $acc['ids']['max'] = (int)$identifier;
    $pair = (int)(isset($row['dimension_id']) ? $row['dimension_id'] : 0).'|'.(int)(isset($row['dimension2_id']) ? $row['dimension2_id'] : 0);
    $minor = (int)round((float)(isset($row['amount']) ? $row['amount'] : 0) * 10000);
    $acc['totals'][$pair] = (isset($acc['totals'][$pair]) ? $acc['totals'][$pair] : 0) + $minor;
    $account = isset($row['account']) ? (string)$row['account'] : '';
    $acc['accounts'][$account] = (isset($acc['accounts'][$account]) ? $acc['accounts'][$account] : 0) + $minor;
}

/**
 * Comparable summary of an accumulator (sorted, no row content).
 *
 * @return array rows, digest, id range, totals by classification pair and by account
 */
function dimension_archive_accumulator_summary($acc)
{
    $totals = $acc['totals'];
    $accounts = $acc['accounts'];
    ksort($totals);
    ksort($accounts);
    return array('rows'=>$acc['rows'], 'digest'=>sprintf('%d:%d', $acc['crc_a'], $acc['crc_b']),
        'first_id'=>isset($acc['ids']['min']) ? $acc['ids']['min'] : null, 'last_id'=>isset($acc['ids']['max']) ? $acc['ids']['max'] : null,
        'totals'=>$totals, 'accounts'=>$accounts);
}

/** History stores an archive must retrieve: store => date column. */
function dimension_archive_history_stores()
{
    return array('gl_trans'=>'tran_date', 'budget_trans'=>'tran_date');
}

/**
 * Retrieve the original ledger and budget history up to a closing date from a backup file, without restore.
 *
 * @param string $path Backup file
 * @param string $tbpref Company table prefix used by the backup
 * @param string $to Closing date, Y-m-d
 * @return array|false store => summary, or false when the archive is unreadable or incomplete
 */
function dimension_archive_retrieve_history($path, $tbpref, $to)
{
    if (!dimension_archive_valid_date($to) || !is_string($tbpref) || !preg_match('/^[A-Za-z0-9_]*$/D', $tbpref)) return false;
    $result = array();
    foreach (dimension_archive_history_stores() as $store=>$date_column) {
        $acc = dimension_archive_accumulator($date_column, $to);
        $read = dimension_archive_read_table($path, $tbpref.$store, function ($row) use (&$acc) { dimension_archive_accumulate($acc, $row); });
        if ($read === false) return false;
        $result[$store] = dimension_archive_accumulator_summary($acc);
    }
    return $result;
}

/**
 * Summarize the live history up to a closing date exactly like the archive reader does.
 *
 * @param string $to Closing date, Y-m-d
 * @return array|false store => summary, or false on a read failure
 */
function dimension_archive_live_history($to)
{
    if (!dimension_archive_valid_date($to)) return false;
    $result = array();
    foreach (dimension_archive_history_stores() as $store=>$date_column) {
        $rows = db_query('SELECT * FROM '.TB_PREF.$store.' WHERE '.$date_column.' <= \''.$to.'\'');
        if (!$rows) return false;
        $acc = dimension_archive_accumulator($date_column, $to);
        while ($row = db_fetch_assoc($rows)) dimension_archive_accumulate($acc, $row);
        $result[$store] = dimension_archive_accumulator_summary($acc);
    }
    return $result;
}

/**
 * Verify that a backup retrieves the same original history as the live rows a purge would destroy.
 *
 * @param string|false $path Backup file returned by the exporter
 * @param string $tbpref Company table prefix
 * @param string $to Closing date, Y-m-d
 * @return array ok, reason none|missing_file|incomplete|unreadable|mismatch, stores store => match|mismatch
 */
function dimension_archive_verify($path, $tbpref, $to)
{
    $result = array('ok'=>false, 'reason'=>'missing_file', 'stores'=>array());
    if (!is_string($path) || $path === '' || !is_file($path) || filesize($path) === 0) return $result;
    if (!dimension_archive_is_complete($path)) { $result['reason'] = 'incomplete'; return $result; }
    $archive = dimension_archive_retrieve_history($path, $tbpref, $to);
    $live = dimension_archive_live_history($to);
    if ($archive === false || $live === false) { $result['reason'] = 'unreadable'; return $result; }
    $result['ok'] = true;
    $result['reason'] = 'none';
    foreach ($live as $store=>$summary) {
        $result['stores'][$store] = $summary === $archive[$store] ? 'match' : 'mismatch';
        if ($summary !== $archive[$store]) { $result['ok'] = false; $result['reason'] = 'mismatch'; }
    }
    return $result;
}

/**
 * Admit the purge only after its backup is proven retrievable and equal to the live history.
 * Displays the refusal; names the reason only.
 *
 * @param string|false $path Value returned by the exporter
 * @param string $to Closing date, Y-m-d
 * @param string $tbpref Company table prefix
 * @return bool
 */
function dimension_archive_admit_backup($path, $to, $tbpref)
{
    $verified = dimension_archive_verify($path, $tbpref, $to);
    if ($verified['ok']) return true;
    display_error(_('This fiscal year cannot be deleted because its security backup could not be verified as retrievable (')
        .$verified['reason'].'). '._('Nothing was changed.'));
    return false;
}

//---------------------------------------------------------------------------------------------
// Authorized retrieval
//---------------------------------------------------------------------------------------------

/**
 * Whether the signed-in user may retrieve archived history: both the backup and the fiscal-year authority.
 *
 * @return bool
 */
function dimension_archive_retrieval_authorized()
{
    if (empty($_SESSION['wa_current_user']) || !is_object($_SESSION['wa_current_user'])
        || !method_exists($_SESSION['wa_current_user'], 'can_access')) return false;
    $user = $_SESSION['wa_current_user'];
    return $user->can_access('SA_BACKUP') && $user->can_access('SA_FISCALYEARS');
}

/**
 * Resolve an archive name inside the company backup directory. A path, traversal or foreign extension is refused.
 *
 * @param string $name Base name of the backup file
 * @return string|false Real path, or false
 */
function dimension_archive_resolve_file($name)
{
    if (!is_string($name) || !preg_match('/^[A-Za-z0-9][A-Za-z0-9_.\-]{0,200}\.sql(?:\.gz)?$/D', $name) || strpos($name, '..') !== false) return false;
    $directory = dimension_archive_backup_directory();
    if ($directory === false) return false;
    $base = realpath($directory);
    $file = realpath($directory.$name);
    if ($base === false || $file === false || strpos($file, $base.DIRECTORY_SEPARATOR) !== 0 || !is_file($file)) return false;
    return $file;
}

/**
 * Retrieve archived history for an authorized user: original identifiers, digest and classification totals.
 *
 * @param string $name Backup base name
 * @param string $to Closing date, Y-m-d
 * @param string $tbpref Company table prefix
 * @return array state ok|denied|unavailable, history when ok
 */
function dimension_archive_retrieve_authorized($name, $to, $tbpref)
{
    if (!dimension_archive_retrieval_authorized()) return array('state'=>'denied');
    $file = dimension_archive_resolve_file($name);
    if ($file === false || !dimension_archive_is_complete($file)) return array('state'=>'unavailable');
    $history = dimension_archive_retrieve_history($file, $tbpref, $to);
    return $history === false ? array('state'=>'unavailable') : array('state'=>'ok', 'history'=>$history);
}

/**
 * Backup file names available for retrieval (names only).
 *
 * @return string[]
 */
function dimension_archive_list_files()
{
    $directory = dimension_archive_backup_directory();
    if ($directory === false) return array();
    $names = array();
    foreach (scandir($directory) as $entry)
        if (dimension_archive_resolve_file($entry) !== false) $names[] = $entry;
    rsort($names);
    return $names;
}
