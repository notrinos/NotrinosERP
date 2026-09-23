<?php
/** Safe in-memory ZIP32 reader for PAY-CTRY-001. Never extracts untrusted bytes to disk. */
include_once(dirname(__FILE__).'/pay_ctry_001_contract.inc');

function pay_ctry_001_archive_fail(&$error, $code)
{
    $error = $code;
    return false;
}

function pay_ctry_001_u16le($bytes, $offset)
{
    $v = unpack('v', substr($bytes, $offset, 2));
    return $v ? (int)$v[1] : false;
}
function pay_ctry_001_u32le($bytes, $offset)
{
    $v = unpack('V', substr($bytes, $offset, 4));
    if (!$v) return false;
    return (int)sprintf('%u', $v[1]);
}

/** ASCII-only member paths eliminate Unicode/case normalization ambiguity at the archive boundary. */
function pay_ctry_001_normalize_member_path($path, &$error)
{
    if (!is_string($path) || $path === '' || strpos($path, "\0") !== false)
        return pay_ctry_001_archive_fail($error, 'archive_path_empty_or_nul');
    if (strpos($path, '\\') !== false)
        return pay_ctry_001_archive_fail($error, 'archive_backslash_path_denied');
    if ($path[0] === '/' || preg_match('/^[A-Za-z]:/', $path))
        return pay_ctry_001_archive_fail($error, 'archive_absolute_path_denied');
    if (preg_match('/^[A-Za-z0-9._\/-]+$/D', $path) !== 1)
        return pay_ctry_001_archive_fail($error, 'archive_path_charset_denied');
    $is_dir = substr($path, -1) === '/';
    $trimmed = $is_dir ? substr($path, 0, -1) : $path;
    if ($trimmed === '') return pay_ctry_001_archive_fail($error, 'archive_root_entry_denied');
    $parts = explode('/', $trimmed);
    foreach ($parts as $part) {
        if ($part === '' || $part === '.' || $part === '..')
            return pay_ctry_001_archive_fail($error, 'archive_path_traversal_denied');
    }
    return $trimmed.($is_dir ? '/' : '');
}

/** Return associative member map path=>metadata/content, or false. */
function pay_ctry_001_read_zip_members($raw, &$error)
{
    $error = null;
    if (!is_string($raw) || strlen($raw) < 22 || strlen($raw) > PAY_CTRY_001_MAX_ARCHIVE_BYTES)
        return pay_ctry_001_archive_fail($error, 'archive_size_invalid');

    $min = max(0, strlen($raw) - 65557);
    $eocd = strrpos(substr($raw, $min), "PK\x05\x06");
    if ($eocd === false) return pay_ctry_001_archive_fail($error, 'zip_eocd_missing');
    $eocd += $min;
    if ($eocd + 22 > strlen($raw)) return pay_ctry_001_archive_fail($error, 'zip_eocd_truncated');

    $disk = pay_ctry_001_u16le($raw, $eocd + 4);
    $cd_disk = pay_ctry_001_u16le($raw, $eocd + 6);
    $disk_entries = pay_ctry_001_u16le($raw, $eocd + 8);
    $entries = pay_ctry_001_u16le($raw, $eocd + 10);
    $cd_size = pay_ctry_001_u32le($raw, $eocd + 12);
    $cd_offset = pay_ctry_001_u32le($raw, $eocd + 16);
    $comment_len = pay_ctry_001_u16le($raw, $eocd + 20);
    if ($disk !== 0 || $cd_disk !== 0 || $disk_entries !== $entries)
        return pay_ctry_001_archive_fail($error, 'zip_multidisk_denied');
    if ($entries === 0 || $entries > PAY_CTRY_001_MAX_MEMBERS + 32 || $entries === 0xffff || $cd_size === 0xffffffff || $cd_offset === 0xffffffff)
        return pay_ctry_001_archive_fail($error, 'zip_entry_count_or_zip64_denied');
    if ($eocd + 22 + $comment_len !== strlen($raw))
        return pay_ctry_001_archive_fail($error, 'zip_trailing_data_denied');
    if ($cd_offset + $cd_size > $eocd)
        return pay_ctry_001_archive_fail($error, 'zip_central_directory_bounds_invalid');

    $members = array();
    $case_seen = array();
    $total = 0;
    $p = $cd_offset;
    for ($i = 0; $i < $entries; $i++) {
        if ($p + 46 > strlen($raw) || substr($raw, $p, 4) !== "PK\x01\x02")
            return pay_ctry_001_archive_fail($error, 'zip_central_entry_invalid');
        $made_by = pay_ctry_001_u16le($raw, $p + 4);
        $flags = pay_ctry_001_u16le($raw, $p + 8);
        $method = pay_ctry_001_u16le($raw, $p + 10);
        $crc = pay_ctry_001_u32le($raw, $p + 16);
        $compressed = pay_ctry_001_u32le($raw, $p + 20);
        $size = pay_ctry_001_u32le($raw, $p + 24);
        $name_len = pay_ctry_001_u16le($raw, $p + 28);
        $extra_len = pay_ctry_001_u16le($raw, $p + 30);
        $comment = pay_ctry_001_u16le($raw, $p + 32);
        $ext_attr = pay_ctry_001_u32le($raw, $p + 38);
        $local_offset = pay_ctry_001_u32le($raw, $p + 42);
        if ($compressed === 0xffffffff || $size === 0xffffffff || $local_offset === 0xffffffff)
            return pay_ctry_001_archive_fail($error, 'zip64_denied');
        if (($flags & 0x0001) !== 0 || ($flags & 0x0040) !== 0)
            return pay_ctry_001_archive_fail($error, 'zip_encryption_denied');
        if (($flags & 0x0008) !== 0)
            return pay_ctry_001_archive_fail($error, 'zip_data_descriptor_denied');
        if ($method !== 0 && $method !== 8)
            return pay_ctry_001_archive_fail($error, 'zip_compression_method_denied');
        if ($name_len < 1 || $name_len > 255)
            return pay_ctry_001_archive_fail($error, 'zip_member_name_length_invalid');
        $name = substr($raw, $p + 46, $name_len);
        $normalized = pay_ctry_001_normalize_member_path($name, $error);
        if ($normalized === false) return false;
        $is_dir = substr($normalized, -1) === '/';

        $host_os = ($made_by >> 8) & 0xff;
        $unix_mode = ($ext_attr >> 16) & 0xffff;
        if ($host_os === 3 && (($unix_mode & 0170000) === 0120000))
            return pay_ctry_001_archive_fail($error, 'zip_symlink_denied');

        $fold = strtolower($normalized);
        if (isset($case_seen[$fold]))
            return pay_ctry_001_archive_fail($error, 'zip_duplicate_normalized_path_denied');
        $case_seen[$fold] = true;

        if (!$is_dir) {
            if ($size > PAY_CTRY_001_MAX_MEMBER_BYTES)
                return pay_ctry_001_archive_fail($error, 'zip_member_size_exceeded');
            $total += $size;
            if ($total > PAY_CTRY_001_MAX_TOTAL_MEMBER_BYTES)
                return pay_ctry_001_archive_fail($error, 'zip_total_uncompressed_size_exceeded');
            if ($compressed === 0 && $size > 0)
                return pay_ctry_001_archive_fail($error, 'zip_zero_compressed_size_invalid');
            if ($compressed > 0 && $size > $compressed * PAY_CTRY_001_MAX_COMPRESSION_RATIO)
                return pay_ctry_001_archive_fail($error, 'zip_compression_ratio_exceeded');

            if ($local_offset + 30 > strlen($raw) || substr($raw, $local_offset, 4) !== "PK\x03\x04")
                return pay_ctry_001_archive_fail($error, 'zip_local_header_invalid');
            $local_flags = pay_ctry_001_u16le($raw, $local_offset + 6);
            $local_method = pay_ctry_001_u16le($raw, $local_offset + 8);
            $local_crc = pay_ctry_001_u32le($raw, $local_offset + 14);
            $local_compressed = pay_ctry_001_u32le($raw, $local_offset + 18);
            $local_size = pay_ctry_001_u32le($raw, $local_offset + 22);
            $local_name_len = pay_ctry_001_u16le($raw, $local_offset + 26);
            $local_extra_len = pay_ctry_001_u16le($raw, $local_offset + 28);
            if ($local_method !== $method || $local_flags !== $flags || $local_crc !== $crc || $local_compressed !== $compressed || $local_size !== $size)
                return pay_ctry_001_archive_fail($error, 'zip_local_header_mismatch');
            $local_name = substr($raw, $local_offset + 30, $local_name_len);
            if ($local_name !== $name)
                return pay_ctry_001_archive_fail($error, 'zip_local_name_mismatch');
            $data_offset = $local_offset + 30 + $local_name_len + $local_extra_len;
            if ($data_offset + $compressed > $cd_offset)
                return pay_ctry_001_archive_fail($error, 'zip_member_data_truncated_or_overlapping');
            $compressed_data = substr($raw, $data_offset, $compressed);
            if ($method === 0) $content = $compressed_data;
            else {
                // The declared size was bounded above. Ask zlib for at most one byte
                // beyond it so a forged central-directory size cannot inflate an
                // unbounded member before the size mismatch is detected.
                $content = @gzinflate($compressed_data, $size + 1);
                if (!is_string($content)) return pay_ctry_001_archive_fail($error, 'zip_deflate_invalid');
            }
            if (strlen($content) !== $size)
                return pay_ctry_001_archive_fail($error, 'zip_uncompressed_size_mismatch');
            $actual_crc_hex = hash('crc32b', $content);
            $actual_crc = (int)hexdec($actual_crc_hex);
            if (sprintf('%u', $actual_crc) !== sprintf('%u', $crc))
                return pay_ctry_001_archive_fail($error, 'zip_crc_mismatch');
            $members[$normalized] = array('path'=>$normalized,'bytes'=>$size,'sha256'=>hash('sha256',$content),'content'=>$content,'method'=>$method);
        }
        $p += 46 + $name_len + $extra_len + $comment;
    }
    if ($p !== $cd_offset + $cd_size)
        return pay_ctry_001_archive_fail($error, 'zip_central_directory_length_mismatch');
    if (count($members) < 1 || count($members) > PAY_CTRY_001_MAX_MEMBERS)
        return pay_ctry_001_archive_fail($error, 'zip_file_member_count_invalid');
    return $members;
}
?>
