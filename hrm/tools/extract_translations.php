<?php

/**
 * Extract HRM translation strings into a POT-style template.
 *
 * Usage:
 *   php hrm/tools/extract_translations.php
 */

$root = dirname(__DIR__);
$output_dir = $root.'/lang';
$output_file = $output_dir.'/hrm.pot';
$extensions = array('php', 'inc', 'js');
$messages = array();

/**
 * Normalize a discovered translation string.
 *
 * @param string $text
 * @return string
 */
function hrm_i18n_normalize($text) {
	$text = stripcslashes($text);
	$text = str_replace("\r", '', $text);
	return trim($text);
}

/**
 * Collect translation strings from one file.
 *
 * @param string $file_path
 * @param array $extensions
 * @param array $messages
 * @return void
 */
function hrm_i18n_collect_file($file_path, $extensions, &$messages) {
	$extension = strtolower(pathinfo($file_path, PATHINFO_EXTENSION));
	if (!in_array($extension, $extensions, true))
		return;

	$relative_path = str_replace('\\', '/', substr($file_path, strlen(dirname(__DIR__)) + 1));
	$content = file_get_contents($file_path);
	if ($content === false)
		return;

	if (!preg_match_all('/_\(\s*([\"\'])(.*?)\1\s*\)/s', $content, $matches, PREG_OFFSET_CAPTURE))
		return;

	foreach ($matches[2] as $index => $match) {
		$message = hrm_i18n_normalize($match[0]);
		if ($message === '')
			continue;

		$line = substr_count(substr($content, 0, $matches[0][$index][1]), "\n") + 1;
		if (!isset($messages[$message]))
			$messages[$message] = array();
		$messages[$message][] = $relative_path.':'.$line;
	}
}

/**
 * Write a POT-style template file.
 *
 * @param string $output_file
 * @param array $messages
 * @return void
 */
function hrm_i18n_write_pot($output_file, $messages) {
	ksort($messages);
	$lines = array(
		'msgid ""',
		'msgstr ""',
		'"Project-Id-Version: NotrinosERP HRM\\n"',
		'"POT-Creation-Date: '.gmdate('Y-m-d H:i').'+0000\\n"',
		'"Content-Type: text/plain; charset=UTF-8\\n"',
		'"Content-Transfer-Encoding: 8bit\\n"',
		''
	);

	foreach ($messages as $message => $references) {
		$lines[] = '#: '.implode(' ', array_unique($references));
		$lines[] = 'msgid "'.addcslashes($message, "\\\"").'"';
		$lines[] = 'msgstr ""';
		$lines[] = '';
	}

	file_put_contents($output_file, implode(PHP_EOL, $lines));
}

if (!is_dir($output_dir))
	mkdir($output_dir, 0777, true);

$iterator = new RecursiveIteratorIterator(
	new RecursiveDirectoryIterator($root, FilesystemIterator::SKIP_DOTS)
);

foreach ($iterator as $file_info) {
	if (!$file_info->isFile())
		continue;

	$path_name = $file_info->getPathname();
	if (strpos(str_replace('\\', '/', $path_name), '/lang/') !== false)
		continue;

	hrm_i18n_collect_file($path_name, $extensions, $messages);
}

hrm_i18n_write_pot($output_file, $messages);
echo 'Wrote '.count($messages).' message(s) to '.$output_file.PHP_EOL;
