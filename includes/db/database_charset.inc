<?php
/**********************************************************************
	Database character set: every company table stores text as utf8mb4
	with one collation, utf8mb4_general_ci, so any two text columns can be
	compared. Binary collations stay binary (utf8mb4_bin) and ascii columns
	stay as they are.

	A fresh install (sql/en_US-new.sql, sql/en_US-demo.sql, the chart files)
	names utf8mb4_general_ci for every table. A database installed before
	that keeps utf8 (3-byte) tables, and tables of other collations, until
	db_convert_tables_to_utf8mb4() converts them; the command
	admin/convert_database_charset.php runs it for one company.
***********************************************************************/

/**
 * The collation of every company table and text column that is not binary.
 *
 * @return string utf8mb4_general_ci.
 */
function db_company_text_collation()
{
	return 'utf8mb4_general_ci';
}

/**
 * The collation a utf8 or utf8mb4 table or column takes in a company database.
 *
 * @param string $collation Collation name, for example utf8_general_ci, utf8mb3_unicode_ci or utf8mb4_bin.
 * @return string|null utf8mb4_bin for a binary collation, otherwise db_company_text_collation(); null when the
 *     collation is not a utf8 or utf8mb4 one (ascii, latin1, binary or empty).
 */
function db_utf8mb4_collation($collation)
{
	if (!preg_match('/^utf8(?:mb3|mb4)?_([a-z0-9_]+)$/D', (string)$collation, $matches))
		return null;
	return preg_match('/(^|_)bin$/D', $matches[1]) ? 'utf8mb4_bin' : db_company_text_collation();
}

/**
 * Tables of one company prefix whose table default or a text column is utf8 (3-byte) or another utf8mb4 collation
 * than db_utf8mb4_collation() gives.
 *
 * @param string $prefix Company table prefix, for example 0_.
 * @return array Table name => array('collation' => table collation, 'columns' => array(column name => collation)).
 */
function db_tables_needing_utf8mb4($prefix)
{
	$like = str_replace(array('\\', '_', '%'), array('\\\\', '\\_', '\\%'), $prefix).'%';
	$tables = array();
	$result = db_query_params("SELECT TABLE_NAME, TABLE_COLLATION FROM information_schema.TABLES
		WHERE TABLE_SCHEMA = DATABASE() AND TABLE_TYPE = 'BASE TABLE' AND TABLE_NAME LIKE ?", array($like),
		'could not read the table list');
	while ($row = db_fetch_assoc($result))
		if (db_utf8mb4_needs_change($row['TABLE_COLLATION']))
			$tables[$row['TABLE_NAME']] = array('collation' => $row['TABLE_COLLATION'], 'columns' => array());
	$result = db_query_params("SELECT c.TABLE_NAME, c.COLUMN_NAME, c.COLLATION_NAME FROM information_schema.COLUMNS c
		JOIN information_schema.TABLES t ON t.TABLE_SCHEMA = c.TABLE_SCHEMA AND t.TABLE_NAME = c.TABLE_NAME
		WHERE c.TABLE_SCHEMA = DATABASE() AND t.TABLE_TYPE = 'BASE TABLE' AND c.TABLE_NAME LIKE ?
		ORDER BY c.TABLE_NAME, c.ORDINAL_POSITION", array($like), 'could not read the column list');
	while ($row = db_fetch_assoc($result)) {
		if (!db_utf8mb4_needs_change($row['COLLATION_NAME']))
			continue;
		if (!isset($tables[$row['TABLE_NAME']]))
			$tables[$row['TABLE_NAME']] = array('collation' => null, 'columns' => array());
		$tables[$row['TABLE_NAME']]['columns'][$row['COLUMN_NAME']] = $row['COLLATION_NAME'];
	}
	ksort($tables);
	return $tables;
}

/**
 * Whether a table or column collation differs from the one it takes in a company database.
 *
 * @param string|null $collation Collation name.
 * @return bool True for a utf8 or utf8mb4 collation that is not already the target one.
 */
function db_utf8mb4_needs_change($collation)
{
	$target = db_utf8mb4_collation($collation);
	return $target !== null && $target !== $collation;
}

/**
 * The ALTER TABLE statement that moves one table to utf8mb4.
 *
 * Column definitions come from SHOW CREATE TABLE, so type, length, null, default, comment and generated parts stay
 * as they are; only the character set and collation of the listed columns change.
 *
 * @param string $table Table name.
 * @param array $entry One entry of db_tables_needing_utf8mb4().
 * @param string $create_table Text of SHOW CREATE TABLE for the table.
 * @return string|false Statement, or false when a listed column cannot be found in $create_table.
 */
function db_utf8mb4_alter_statement($table, $entry, $create_table)
{
	$parts = array();
	if ($entry['collation'] !== null)
		$parts[] = 'DEFAULT CHARACTER SET utf8mb4 COLLATE '.db_utf8mb4_collation($entry['collation']);
	foreach ($entry['columns'] as $column => $collation) {
		$quoted = '`'.str_replace('`', '``', $column).'`';
		if (!preg_match('/^\s*'.preg_quote($quoted, '/').' (.+?),?$/m', $create_table, $matches))
			return false;
		$definition = preg_replace('/ (CHARACTER SET|COLLATE) [A-Za-z0-9_]+/', '', $matches[1]);
		if (!preg_match('/^((?:enum|set)\((?:[^()\']|\'(?:[^\']|\'\')*\')*\)|[a-z]+(?:\([0-9, ]+\))?)(.*)$/Dis', $definition, $type))
			return false;
		$parts[] = 'MODIFY '.$quoted.' '.$type[1].' CHARACTER SET utf8mb4 COLLATE '.db_utf8mb4_collation($collation).$type[2];
	}
	return 'ALTER TABLE `'.str_replace('`', '``', $table).'` '.implode(', ', $parts);
}

/**
 * Convert every utf8 (3-byte) table and text column of one company prefix to utf8mb4, and give every one that is not
 * binary the company collation (db_company_text_collation()).
 *
 * The database default for new tables moves to the company collation too. Foreign key checks are off while the tables change,
 * because both sides of a foreign key change in the same run. Running it again on a converted database changes nothing.
 *
 * @param string $prefix Company table prefix, for example 0_.
 * @return array array(converted table count, list of tables that could not be converted with the reason).
 */
function db_convert_tables_to_utf8mb4($prefix)
{
	$converted = 0;
	$failed = array();
	db_query('SET FOREIGN_KEY_CHECKS = 0', 'could not pause foreign key checks');
	try {
		$result = db_query('SELECT DEFAULT_COLLATION_NAME FROM information_schema.SCHEMATA WHERE SCHEMA_NAME = DATABASE()',
			'could not read the database collation');
		$row = $result ? db_fetch_row($result) : false;
		if ($row && $row[0] !== db_company_text_collation()
			&& !db_query('ALTER DATABASE CHARACTER SET utf8mb4 COLLATE '.db_company_text_collation()))
			$failed['(database default)'] = 'server error '.db_error_no();
		foreach (db_tables_needing_utf8mb4($prefix) as $table => $entry) {
			$result = db_query('SHOW CREATE TABLE `'.str_replace('`', '``', $table).'`', 'could not read the table '.$table);
			$row = $result ? db_fetch_row($result) : false;
			$statement = $row ? db_utf8mb4_alter_statement($table, $entry, $row[1]) : false;
			if ($statement === false) {
				$failed[$table] = 'definition not understood';
				continue;
			}
			if (db_query($statement))
				$converted++;
			else
				$failed[$table] = 'server error '.db_error_no();
		}
	} finally {
		db_query('SET FOREIGN_KEY_CHECKS = 1', 'could not restore foreign key checks');
	}
	return array($converted, $failed);
}
