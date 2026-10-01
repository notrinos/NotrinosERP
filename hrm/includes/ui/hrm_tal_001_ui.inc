<?php
/** Accessible server-rendered presentation helpers for HRM-TAL-001. */

/**
 * Render a minimized TAL result table with a caption and scoped column headers.
 *
 * @param string $caption
 * @param array $headers
 * @param array $rows
 * @param array $keys
 * @return void
 */
function hrm_tal_001_ui_table($caption, array $headers, array $rows, array $keys)
{
    static $table_number = 0;
    ++$table_number;
    $caption_id = 'hrm-tal-001-table-caption-'.$table_number;
    echo '<table class="tablestyle">';
    echo '<caption id="'.htmlspecialchars($caption_id, ENT_QUOTES, 'UTF-8').'">'.htmlspecialchars((string)$caption, ENT_QUOTES, 'UTF-8').'</caption>';
    echo '<thead><tr>';
    foreach ($headers as $header)
        echo '<th scope="col">'.htmlspecialchars((string)$header, ENT_QUOTES, 'UTF-8').'</th>';
    echo '</tr></thead><tbody>';
    foreach ($rows as $row) {
        echo '<tr>';
        foreach ($keys as $key) {
            $value = is_array($row) && isset($row[$key]) && is_scalar($row[$key]) ? (string)$row[$key] : '';
            echo '<td>'.htmlspecialchars($value, ENT_QUOTES, 'UTF-8').'</td>';
        }
        echo '</tr>';
    }
    echo '</tbody></table>';
}
?>
