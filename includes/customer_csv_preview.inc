<?php
/** Server-owned preview custody for the installed customer CSV importer. */

/** Bind a preview to the authenticated actor and actual selected connection. */
function customer_csv_preview_scope()
{
    global $db_connections;
    $user = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
    if (!$user || empty($user->user) || !isset($user->cur_con)
        || !isset($db_connections[$user->cur_con]))
        throw new RuntimeException('Customer CSV preview requires an authenticated company.');
    $connection = $db_connections[$user->cur_con];
    $scope = array((string)$user->user, (string)$user->loginname,
        (string)$user->company, (string)$user->cur_con);
    foreach (array('host', 'port', 'dbname', 'tbpref') as $key)
        $scope[] = isset($connection[$key]) ? (string)$connection[$key] : '';
    return hash('sha256', serialize($scope));
}

/** Retain bounded exact CSV bytes and validated settings inside the private session. */
function customer_csv_preview_save($bytes, $separator, $type)
{
    if (!is_string($bytes) || strlen($bytes) > 10 * 1024 * 1024
        || !is_string($separator) || strlen($separator) !== 1
        || !in_array((int)$type, array(1, 2, 3), true))
        throw new RuntimeException('Invalid customer CSV preview.');
    $random = openssl_random_pseudo_bytes(24, $strong);
    if ($random === false || !$strong)
        throw new RuntimeException('Could not retain the customer CSV preview.');
    $preview = array('version'=>1, 'scope'=>customer_csv_preview_scope(),
        'nonce'=>bin2hex($random), 'created'=>time(), 'bytes'=>$bytes,
        'separator'=>$separator, 'type'=>(int)$type);
    $_SESSION['customer_csv_preview'] = $preview;
    return $preview['nonce'];
}

/** Reject legacy, stale, cross-actor/company or superseded confirmation before parsing. */
function customer_csv_preview_load($nonce)
{
    $preview = isset($_SESSION['customer_csv_preview']) ? $_SESSION['customer_csv_preview'] : null;
    if (!is_array($preview) || !isset($preview['version'], $preview['scope'],
            $preview['nonce'], $preview['created'], $preview['bytes'], $preview['separator'], $preview['type'])
        || $preview['version'] !== 1 || !is_string($nonce)
        || !hash_equals($preview['nonce'], $nonce)
        || !hash_equals($preview['scope'], customer_csv_preview_scope())
        || $preview['created'] > time() || time() - $preview['created'] > 1800
        || !is_string($preview['bytes']) || strlen($preview['bytes']) > 10 * 1024 * 1024
        || !is_string($preview['separator']) || strlen($preview['separator']) !== 1
        || !in_array($preview['type'], array(1, 2, 3), true))
        throw new RuntimeException('Your import preview has expired or changed. Please upload the file again.');
    return $preview;
}
