<?php
/**
 * DIM-LFC-001 B09: authority over installed, inactive, unregistered and externally deployed extension code.
 *
 * An extension is arbitrary PHP that the application loads by path (hooks.php of EVERY installed extension is
 * included on every request, active or not) and that may write any table. A Dimension reference authority
 * therefore cannot call an extension "absent" because it is inactive or because nobody registered it. This
 * authority reports a Dimension's extension owner as `unused` only when the whole deployment is accounted for:
 *
 *  - Registry. Every entry of the global installed_extensions.php and of the active company's copy is read.
 *    An entry of type `extension` must name a package this file has REVIEWED (source digest below), live at
 *    exactly modules/<package>, and still match the reviewed digest byte for byte (line endings ignored,
 *    documentation/translation/image files ignored). A missing, unreviewed, edited, mis-pathed, duplicated or
 *    conflicting package is unknown. Data package types (chart, language, theme) execute no extension code.
 *  - Unregistered code. Every directory under modules/ must be a reviewed package with a matching digest or the
 *    deployment is unknown (a copied but never registered extension is still unregistered executable code).
 *    Stray files, and executable files inside the cache directory, are unknown too.
 *  - Live schema. A table of the active company that carries a Dimension-like column outside the tables that
 *    other authorities own (core, HRM, payroll, shared, session, approval) is unknown: an extension created
 *    its own Dimension storage, which only that extension's own authority could read.
 *  - External writers. A company may declare writers that live outside this source tree in
 *    company/<n>/dimension_external_writers.php; any declaration (or an unreadable declaration file) is unknown.
 *    Nothing here can prove the absence of a process that writes the database directly: that stays unknown only
 *    when it is declared; this authority never guesses an external absence and never records an owner signoff.
 *
 * Reviewed extensions own no Dimension storage and write Dimension coordinates only through the core writers
 * that already join the B02 fence (the unit suite re-proves that from source). They therefore cannot
 * contribute a `used` answer, and the owner reports `unused` exactly when everything above is clean.
 *
 * Fencing. The registry file is the activation point (it decides which hooks run), and it is not a database row.
 * dimension_extension_fence_write() makes every registry write take an exclusive InnoDB lock on the company's
 * sys_prefs `version_id` row, and the locked recheck takes a shared lock on the same row until the deleter's
 * transaction ends, so a registry write either completes before the recheck reads it or waits for the
 * deleter's transaction. The recheck also compares a signature of the whole deployment before and after its
 * locks. Files copied into modules/ by hand cannot be fenced by a database lock; the signature comparison
 * and the unregistered-code rule are the guard for that path. This is NOT a deletion certificate (C01).
 *
 * PHP 5.6 compatible syntax.
 */
require_once __DIR__.'/dimension_lifecycle_db.inc';

/**
 * Reviewed extension packages: source digest, Dimension storage and the core writers they use.
 * Digest = sha256 over the sorted lines "<relative path>\0<sha256 of the file with CR removed>\n" of every
 * file below the package directory except documentation, translation, image and csv files.
 *
 * @return array package => array(digest, storage, writers, tables, session_classes)
 */
function dimension_extension_reviews()
{
    return array(
        'dummy_extension_store'=>array('digest'=>'b6b682429f04b55d4ba4ac9fbff9aa6d5c5eea129e4b622ae70b2b8a229f8270',
            'storage'=>'none', 'writers'=>'none', 'tables'=>array(), 'session_classes'=>array()),
        'import_export_customer'=>array('digest'=>'79dc48d9ef34401dcb4b4d0ced207264621dc538474d72c0fb64f129a15c5d83',
            'storage'=>'none', 'writers'=>'core: add_customer/update_customer (B02 fenced debtors_master writer); crm contact helpers; raw UPDATE of non-Dimension debtors_master columns only',
            'tables'=>array(), 'session_classes'=>array()),
        'import_export_item'=>array('digest'=>'5efa1cffaf55b4212b07d3cbfe05853172d29a6452777b4ff71a180280fb778f',
            'storage'=>'none', 'writers'=>'core: add_item/update_item, item_codes_entity, item price helpers; dimension_composite_metadata_write for purch_data and bom',
            'tables'=>array(), 'session_classes'=>array()),
        'reftest_a'=>array('digest'=>'dd16c8ef5f21927e01597b92e8e9d3dcd913ed9688fa4340d923a99869e13524',
            'storage'=>'none', 'writers'=>'reflines row with custom_data {} (no Dimension coordinate)', 'tables'=>array(), 'session_classes'=>array()),
        'reftest_b'=>array('digest'=>'195ab5b1612b6de895aefb12c823d6d23a167eef1437ac2604e26c577b31fbc2',
            'storage'=>'none', 'writers'=>'none', 'tables'=>array(), 'session_classes'=>array()),
        'reftest_c'=>array('digest'=>'7dde26136f4d7a1d07fffe28ac8ab8e63f9a928483329a907977c4e69bb77bcc',
            'storage'=>'none', 'writers'=>'none', 'tables'=>array(), 'session_classes'=>array()),
        'trade_finance'=>array('digest'=>'33a3a8dc481095faa092d726497595d44c6906702b44685165bdb51e1f656325',
            'storage'=>'none', 'writers'=>'core: bank payment/deposit and sales quotation writers carry the cart Dimension; own tables hold no Dimension column',
            'tables'=>array('in_tfd_pdc_parent', 'in_tfd_pdc', 'out_tfd_pdc_parent', 'out_tfd_pdc', 'forex', 'doc_trans', 'tf_user_discount_price'),
            'session_classes'=>array('tf_allocation')));
}

/** Digest of the unmodified modules/index.php guard stub. */
function dimension_extension_modules_index_digest()
{
    return '723c0af847523a750112361fd3c3215928d6933ccdef3a5cad86a64fc1a848e9';
}

/** File extensions excluded from the package digest (documentation, translation, images, data samples). */
function dimension_extension_digest_skipped()
{
    return array('md', 'po', 'mo', 'pot', 'txt', 'csv', 'png', 'jpg', 'jpeg', 'gif', 'svg', 'ico', 'pdf');
}

/** Data package types that install no executable extension code. */
function dimension_extension_data_types()
{
    return array('chart', 'language', 'theme');
}

/**
 * Tables of the active company whose Dimension-like columns other authorities own. Anything else with a
 * Dimension-like column is extension (or drift) storage and makes the deployment unknown.
 *
 * @return array table => true
 */
function dimension_extension_known_tables()
{
    require_once __DIR__.'/dimension_reference_recheck.inc';
    require_once dirname(dirname(__DIR__)).'/hrm/includes/db/pay_dimension_reference_authority_db.inc';
    $known = array('dimensions'=>true, 'approval_drafts'=>true, 'approval_actions'=>true, 'user_sessions'=>true,
        'session_draft_seals'=>true, 'session_draft_references'=>true, 'hrm_lifecycle_hire_commands'=>true,
        'hrm_lifecycle_hire_payloads'=>true, 'hrm_worker_profile_versions'=>true);
    foreach (array_keys(dimension_reference_recheck_owners()) as $table) $known[$table] = true;
    foreach (pay_dimension_reference_tables() as $table) $known[$table] = true;
    foreach (dimension_shared_record_tables() as $table) $known[$table] = true;
    return $known;
}

/** Root of the application tree, or an explicit tree (unit tests build a synthetic one). */
function dimension_extension_root($root = null)
{
    return $root === null ? dirname(dirname(__DIR__)) : rtrim(str_replace('\\', '/', $root), '/');
}

/**
 * Digest of one package directory.
 *
 * @param string $directory Absolute package directory
 * @return string|false Hex sha256, or false when a file cannot be read
 */
function dimension_extension_tree_digest($directory)
{
    $skipped = array_flip(dimension_extension_digest_skipped());
    $rows = array();
    $stack = array(array($directory, ''));
    while (count($stack)) {
        $item = array_pop($stack);
        $names = @scandir($item[0]);
        if ($names === false) return false;
        foreach ($names as $name) {
            if ($name === '.' || $name === '..') continue;
            $path = $item[0].'/'.$name;
            $relative = $item[1].$name;
            if (is_link($path)) return false;
            if (is_dir($path)) { $stack[] = array($path, $relative.'/'); continue; }
            $extension = strtolower(pathinfo($name, PATHINFO_EXTENSION));
            if (isset($skipped[$extension])) continue;
            $bytes = @file_get_contents($path);
            if ($bytes === false) return false;
            $rows[] = $relative."\0".hash('sha256', str_replace("\r", '', $bytes));
        }
    }
    sort($rows);
    return hash('sha256', implode("\n", $rows)."\n");
}

/**
 * Load one installed_extensions.php in an isolated scope, exactly as get_company_extensions() does.
 *
 * @return array|null|false entries, null when the file is absent, false when unreadable or malformed
 */
function dimension_extension_load_list($file)
{
    if (!file_exists($file)) return null;
    if (!is_file($file) || !is_readable($file)) return false;
    $installed_extensions = array();
    $loader = function () use ($file) { $installed_extensions = array(); include $file; return $installed_extensions; };
    try { $installed_extensions = $loader(); }
    catch (Exception $exception) { return false; }
    catch (Throwable $error) { return false; }
    return is_array($installed_extensions) ? $installed_extensions : false;
}

/**
 * Read the whole extension deployment of one company and classify it.
 *
 * @param int $company Company number whose installed_extensions.php is read
 * @param string|null $root Application tree (null = this installation)
 * @param array|null $reviews Reviewed packages (null = dimension_extension_reviews(); a synthetic tree supplies its own)
 * @return array issues list of "kind:name", entries list of package names, signature hex sha256
 */
function dimension_extension_snapshot($company, $root = null, $reviews = null)
{
    $root = dimension_extension_root($root);
    $company = (int)$company;
    if ($reviews === null) $reviews = dimension_extension_reviews();
    $issues = array();
    $names = array();
    $sources = array('global'=>$root.'/installed_extensions.php', 'company'=>$root.'/company/'.$company.'/installed_extensions.php');
    $lists = array();
    foreach ($sources as $scope=>$file) {
        $list = dimension_extension_load_list($file);
        if ($list === false) { $issues[] = 'registry-unreadable:'.$scope; continue; }
        if ($list === null) { $lists[$scope] = array(); continue; }
        $lists[$scope] = $list;
    }
    $seen = array();
    foreach ($lists as $scope=>$list) {
        foreach ($list as $key=>$entry) {
            if (!is_array($entry) || !isset($entry['package']) || !is_string($entry['package']) || !isset($entry['type']) || !is_string($entry['type'])
                || !isset($entry['path']) || !is_string($entry['path'])) { $issues[] = 'entry-malformed:'.$scope.'#'.$key; continue; }
            if (in_array($entry['type'], dimension_extension_data_types(), true)) continue;
            $package = $entry['package'];
            if ($entry['type'] !== 'extension') { $issues[] = 'type-unknown:'.$package; continue; }
            if (!preg_match('/^[A-Za-z0-9_]{1,64}$/D', $package)) { $issues[] = 'package-name:'.$scope.'#'.$key; continue; }
            if (isset($seen[$package]) && $seen[$package] !== $entry['path']) $issues[] = 'path-conflict:'.$package;
            $seen[$package] = $entry['path'];
            $names[$package] = true;
            if ($entry['path'] !== 'modules/'.$package) $issues[] = 'path:'.$package;
        }
    }
    $present = array();
    $modules = $root.'/modules';
    $listing = is_dir($modules) ? @scandir($modules) : array();
    if ($listing === false) { $issues[] = 'modules-unreadable'; $listing = array(); }
    foreach ($listing as $name) {
        if ($name === '.' || $name === '..') continue;
        $path = $modules.'/'.$name;
        if (is_link($path)) { $issues[] = 'link:'.$name; continue; }
        if (!is_dir($path)) {
            if (!($name === 'index.php' && @hash_file('sha256', $path) === dimension_extension_modules_index_digest())) $issues[] = 'stray:'.$name;
            continue;
        }
        if ($name === '_cache') {
            $cache = @scandir($path);
            if ($cache === false) { $issues[] = 'cache-unreadable'; continue; }
            foreach ($cache as $cached) if ($cached !== '.' && $cached !== '..' && preg_match('/\.(php|inc|phtml|phar)$/i', $cached)) $issues[] = 'cache-code:'.$cached;
            continue;
        }
        $present[$name] = true;
        if (!isset($reviews[$name])) { $issues[] = (isset($names[$name]) ? 'unreviewed:' : 'unregistered:').$name; continue; }
        $digest = dimension_extension_tree_digest($path);
        if ($digest === false || $digest !== $reviews[$name]['digest']) $issues[] = 'digest:'.$name;
    }
    foreach (array_keys($names) as $package) if (!isset($present[$package])) $issues[] = 'missing:'.$package;
    $external = $root.'/company/'.$company.'/dimension_external_writers.php';
    if (file_exists($external)) {
        $dimension_external_writers = null;
        $loader = function () use ($external) { $dimension_external_writers = null; include $external; return $dimension_external_writers; };
        try { $declared = $loader(); } catch (Exception $exception) { $declared = false; } catch (Throwable $error) { $declared = false; }
        if (!is_array($declared) || count($declared)) $issues[] = 'external-writers';
    }
    $issues = array_values(array_unique($issues));
    sort($issues);
    $entries = array_keys($names);
    sort($entries);
    $present = array_keys($present);
    sort($present);
    $digests = array();
    foreach ($present as $name) $digests[$name] = isset($reviews[$name]) ? (string)dimension_extension_tree_digest($modules.'/'.$name) : 'unreviewed';
    return array('issues'=>$issues, 'entries'=>$entries, 'signature'=>hash('sha256', json_encode(array($company, $entries, $present, $digests, $issues,
        $lists, file_exists($external) ? (string)@hash_file('sha256', $external) : ''))));
}

/**
 * Live Dimension-like columns of the active company outside the tables other authorities own.
 *
 * @return array|false list of "table.column", false when the schema cannot be read
 */
function dimension_extension_schema_issues()
{
    $company = isset($_SESSION['wa_current_user']->cur_con) ? $_SESSION['wa_current_user']->cur_con : 0;
    $prefix = isset($GLOBALS['db_connections'][$company]['tbpref']) ? (string)$GLOBALS['db_connections'][$company]['tbpref'] : '';
    $rows = db_query('SELECT TABLE_NAME, COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME LIKE '
        .db_escape(str_replace(array('\\', '_', '%'), array('\\\\', '\\_', '\\%'), $prefix).'%')
        ." AND (COLUMN_NAME REGEXP '(^|_)(dimension[0-9]*|dim[0-9]+)(_id)?$' OR COLUMN_NAME LIKE '%cost\\_cent%')");
    if (!$rows) return false;
    $known = dimension_extension_known_tables();
    $issues = array();
    while ($row = db_fetch_row($rows)) {
        $table = (string)$row[0];
        if ($prefix !== '' && strncmp($table, $prefix, strlen($prefix)) !== 0) continue;
        $bare = substr($table, strlen($prefix));
        if (!isset($known[$bare])) $issues[] = $bare.'.'.$row[1];
    }
    sort($issues);
    return $issues;
}

/** Active company number of the authenticated request. */
function dimension_extension_company()
{
    return isset($_SESSION['wa_current_user']->cur_con) ? (int)$_SESSION['wa_current_user']->cur_con : 0;
}

/**
 * Signature of the whole extension deployment of the active company, for the recheck's before/after comparison.
 *
 * @return string Hex sha256 ('' when it cannot be computed)
 */
function dimension_extension_signature($root = null, $reviews = null)
{
    try { $snapshot = dimension_extension_snapshot(dimension_extension_company(), $root, $reviews); }
    catch (Exception $exception) { return ''; }
    catch (Throwable $error) { return ''; }
    return $snapshot['signature'];
}

/**
 * Shared lock on the company's registry fence row (sys_prefs version_id) inside the caller's transaction.
 *
 * @return bool True when the shared lock is held until the transaction ends
 */
function dimension_extension_fence_read()
{
    $row = db_query("SELECT value FROM ".TB_PREF."sys_prefs WHERE name='version_id' LOCK IN SHARE MODE");
    return $row && db_num_rows($row) === 1;
}

/**
 * Locked current read of the extension owner for one Dimension (runs inside the recheck transaction).
 *
 * @param int $id Exact positive native Dimension identity
 * @param string|null $root Application tree (null = this installation)
 * @param array|null $reviews Reviewed packages (null = dimension_extension_reviews())
 * @return array extensions => unused|unknown (never used: reviewed extensions own no storage)
 */
function dimension_extension_observe($id, $root = null, $reviews = null)
{
    $unknown = array('extensions'=>'unknown');
    if (!dimension_lifecycle_valid_id($id) || empty($GLOBALS['transaction_level'])) return $unknown;
    try {
        if (!dimension_extension_fence_read()) return $unknown;
        $snapshot = dimension_extension_snapshot(dimension_extension_company(), $root, $reviews);
        if (count($snapshot['issues'])) return $unknown;
        $schema = dimension_extension_schema_issues();
        if ($schema === false || count($schema)) return $unknown;
        return array('extensions'=>'unused');
    } catch (Exception $exception) {
        return $unknown;
    } catch (Throwable $error) {
        return $unknown;
    }
}

/**
 * Run a registry write (installed_extensions.php) while holding the exclusive fence row of the company, so it
 * cannot interleave with a Dimension recheck that holds the shared lock. Nothing is written when the lock cannot
 * be taken (a recheck is in progress, or the company database has no fence row): the caller must retry later.
 *
 * @param int $company Company number whose registry is written
 * @param callable $callback Performs the write; returns true on success
 * @param callable|null $switch Connection switch for another company (null = the application's set_global_connection)
 * @return bool True only when the lock was held and the callback succeeded
 */
function dimension_extension_fence_write($company, $callback, $switch = null)
{
    if ($switch === null) $switch = 'set_global_connection';
    $company = (int)$company;
    $current = dimension_extension_company();
    // A registry write must own its transaction: an outer one would be cancelled by a refused write.
    if (!empty($GLOBALS['transaction_level'])) return false;
    $switched = false;
    if ($company !== $current) {
        if (!is_callable($switch) || !call_user_func($switch, $company)) return false;
        $switched = true;
    }
    $result = false;
    try {
        begin_transaction();
        $row = db_query("SELECT value FROM ".TB_PREF."sys_prefs WHERE name='version_id' FOR UPDATE");
        // A company database that has no sys_prefs table or no version row is not installed yet: it holds no Dimension
        // data, so there is nothing to fence. Any other failure (a lock wait timeout while a recheck holds the shared lock,
        // a deadlock) refuses the write.
        $proceed = $row ? true : in_array(db_error_no(), array(1146, 1049), true);
        if ($proceed && call_user_func($callback)) {
            commit_transaction();
            $result = true;
        } else {
            cancel_transaction();
        }
    } catch (Exception $exception) {
        cancel_transaction();
        $result = false;
    } catch (Throwable $error) {
        cancel_transaction();
        $result = false;
    }
    if ($switched) call_user_func($switch, $current);
    return $result;
}
