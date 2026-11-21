<?php
/**********************************************************************
	Copyright (C) NotrinosERP.
	Released under the terms of the GNU General Public License, GPL,
	as published by the Free Software Foundation, either version 3
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

/*
	Minimal Office Open XML (.xlsx) workbook writer for the Excel report destination.

	It writes what reporting/includes/excel_report.inc needs: text, numbers and dates, merged cells, column widths,
	row heights, right-to-left sheets and cell formats (font size, bold, italic, horizontal and vertical alignment,
	top and bottom borders, number formats). Method names follow the earlier BIFF writer so FrontReport keeps its
	shape. It needs the PHP zip extension.
*/

define('XLSX_WRITER_MAX_ROWS', 1048576);
define('XLSX_WRITER_MAX_COLUMNS', 16384);
define('XLSX_WRITER_MAX_TEXT', 32767);
// Excel stores a column width as the character count plus cell padding; the earlier BIFF writer added the same.
define('XLSX_WRITER_COLUMN_PADDING', 0.72);
// Rows this far above the highest written row are kept as one string each, which keeps large reports small in memory.
define('XLSX_WRITER_OPEN_ROWS', 64);

/**
 * One cell format. Set its properties before the first cell is written with it: a cell keeps the format it had
 * when it was written.
 */
class XlsxCellFormat {
	/** @var float Font size in points. */
	public $size = 10;
	/** @var bool */
	public $bold = false;
	/** @var bool */
	public $italic = false;
	/** @var string '' or left, center, right, fill, justify, centerContinuous. */
	public $horizontal = '';
	/** @var string '' or top, center, bottom, justify. */
	public $vertical = '';
	/** @var string '' or a border style name (thin, medium, thick ...). */
	public $top = '';
	/** @var string '' or ARGB colour. */
	public $topColor = '';
	/** @var string */
	public $bottom = '';
	/** @var string */
	public $bottomColor = '';
	/** @var string|int Number format code, or a built-in format id. */
	public $numFormat = 0;

	/**
	 * Set the font size.
	 *
	 * @param float $size Points.
	 */
	function setSize($size) {
		$this->size = max(1, (float)$size);
	}

	/**
	 * Make the font bold.
	 *
	 * @param int $weight Non-zero for bold.
	 */
	function setBold($weight = 1) {
		$this->bold = (bool)$weight;
	}

	/** Make the font italic. */
	function setItalic() {
		$this->italic = true;
	}

	/**
	 * Set horizontal (left, right, center, fill, justify, merge) or vertical (top, vcenter, bottom, vjustify)
	 * alignment.
	 *
	 * @param string $location
	 */
	function setAlign($location) {
		$location = strtolower((string)$location);
		$horizontal = array('left' => 'left', 'right' => 'right', 'center' => 'center', 'centre' => 'center',
			'fill' => 'fill', 'justify' => 'justify', 'merge' => 'centerContinuous');
		$vertical = array('top' => 'top', 'vcentre' => 'center', 'vcenter' => 'center', 'bottom' => 'bottom',
			'vjustify' => 'justify');
		if (isset($horizontal[$location]))
			$this->horizontal = $horizontal[$location];
		elseif (isset($vertical[$location]))
			$this->vertical = $vertical[$location];
	}

	/**
	 * Set the top border: 0 none, 1 thin, 2 medium, 5 thick (the BIFF line style numbers).
	 *
	 * @param int $style
	 */
	function setTop($style) {
		$this->top = xlsx_writer_border_style($style);
	}

	/**
	 * Set the bottom border: 0 none, 1 thin, 2 medium, 5 thick.
	 *
	 * @param int $style
	 */
	function setBottom($style) {
		$this->bottom = xlsx_writer_border_style($style);
	}

	/**
	 * Set the top border colour by name (gray, black, red ...).
	 *
	 * @param string $color
	 */
	function setTopColor($color) {
		$this->topColor = xlsx_writer_color($color);
	}

	/**
	 * Set the bottom border colour by name.
	 *
	 * @param string $color
	 */
	function setBottomColor($color) {
		$this->bottomColor = xlsx_writer_color($color);
	}

	/**
	 * Set the number format: an Excel format code such as "#,##0.00", or a built-in format id.
	 *
	 * @param string|int $format
	 */
	function setNumFormat($format) {
		$this->numFormat = $format;
	}

	/**
	 * Font part of the format, used to share font records.
	 *
	 * @return string
	 */
	function fontKey() {
		return $this->size.'|'.(int)$this->bold.'|'.(int)$this->italic;
	}

	/**
	 * Border part of the format, used to share border records.
	 *
	 * @return string
	 */
	function borderKey() {
		return $this->top.'|'.$this->topColor.'|'.$this->bottom.'|'.$this->bottomColor;
	}
}

/**
 * Map a BIFF line style number to an Office Open XML border style.
 *
 * @param int $style
 * @return string '' for no border.
 */
function xlsx_writer_border_style($style) {
	$styles = array(1 => 'thin', 2 => 'medium', 3 => 'dashed', 4 => 'dotted', 5 => 'thick', 6 => 'double',
		7 => 'hair', 8 => 'mediumDashed', 9 => 'dashDot', 10 => 'mediumDashDot', 11 => 'dashDotDot',
		12 => 'mediumDashDotDot', 13 => 'slantDashDot');
	return isset($styles[(int)$style]) ? $styles[(int)$style] : '';
}

/**
 * Map a colour name to an ARGB value.
 *
 * @param string $color
 * @return string '' for the automatic colour.
 */
function xlsx_writer_color($color) {
	$colors = array('black' => 'FF000000', 'white' => 'FFFFFFFF', 'red' => 'FFFF0000', 'lime' => 'FF00FF00',
		'green' => 'FF008000', 'blue' => 'FF0000FF', 'yellow' => 'FFFFFF00', 'magenta' => 'FFFF00FF',
		'fuchsia' => 'FFFF00FF', 'cyan' => 'FF00FFFF', 'aqua' => 'FF00FFFF', 'brown' => 'FF800000',
		'navy' => 'FF000080', 'orange' => 'FFFF6600', 'purple' => 'FF800080', 'silver' => 'FFC0C0C0',
		'gray' => 'FF808080', 'grey' => 'FF808080');
	$color = strtolower((string)$color);
	return isset($colors[$color]) ? $colors[$color] : '';
}

/**
 * Prepare text for an XML text node: valid UTF-8, no characters XML 1.0 forbids, entities escaped.
 *
 * @param string $text UTF-8 text.
 * @return string
 */
function xlsx_writer_xml_text($text) {
	$text = (string)$text;
	if (!mb_check_encoding($text, 'UTF-8'))
		$text = mb_convert_encoding($text, 'UTF-8', 'UTF-8');
	$text = preg_replace('/[^\x{9}\x{A}\x{D}\x{20}-\x{D7FF}\x{E000}-\x{FFFD}\x{10000}-\x{10FFFF}]/u', '', $text);
	return htmlspecialchars((string)$text, ENT_QUOTES | ENT_XML1, 'UTF-8');
}

/**
 * Convert a zero-based column index to its letters (0 -> A, 26 -> AA).
 *
 * @param int $column
 * @return string
 */
function xlsx_writer_column_letters($column) {
	$letters = '';
	for ($column = (int)$column + 1; $column > 0; $column = intdiv($column - 1, 26))
		$letters = chr(65 + ($column - 1) % 26).$letters;
	return $letters;
}

/**
 * One worksheet. Cells are kept as rendered XML per row until the workbook is closed; rows well above the current
 * one are joined into a single string, and split again if a report writes to them later.
 */
class XlsxWorksheet {
	/** @var string */
	public $name;
	/** @var XlsxWorkbookWriter */
	private $workbook;
	/** @var array<int, array<int, string>|string> Row => column => cell XML, or the row's joined cell XML. */
	private $rows = array();
	/** @var int Highest row written so far. */
	private $highestRow = -1;
	/** @var int Rows below this number have been joined, or were empty. */
	private $joinedBelow = 0;
	/** @var array<int, float> Row => height in points. */
	private $rowHeights = array();
	/** @var array<int, float> Column => width in characters. */
	private $columnWidths = array();
	/** @var array<int, array<int, int>> Merged ranges as [first row, first column, last row, last column]. */
	private $merges = array();
	/** @var array<int, array<int, bool>> Row => merge numbers touching that row, for the overlap check. */
	private $mergesByRow = array();
	/** @var bool */
	private $rightToLeft = false;
	/** @var string Character set of the text passed in. */
	private $inputEncoding = 'UTF-8';

	/**
	 * @param XlsxWorkbookWriter $workbook
	 * @param string $name Unique, valid sheet name.
	 */
	function __construct($workbook, $name) {
		$this->workbook = $workbook;
		$this->name = $name;
	}

	/**
	 * Set the character set of the text that will be written (the report language encoding).
	 *
	 * @param string $encoding
	 */
	function setInputEncoding($encoding) {
		$encoding = trim((string)$encoding);
		$this->inputEncoding = $encoding === '' ? 'UTF-8' : $encoding;
	}

	/** Show the sheet right to left. */
	function setRTL() {
		$this->rightToLeft = true;
	}

	/**
	 * Set the width of a range of columns. The first width set for a column is kept, as the earlier BIFF writer did,
	 * so a report that calls Info() again for a second table keeps the widths of its first table.
	 *
	 * @param int $first_column Zero-based.
	 * @param int $last_column Zero-based.
	 * @param float $width Width in characters.
	 */
	function setColumn($first_column, $last_column, $width) {
		$width = is_numeric($width) ? min(255, max(0, (float)$width)) : 0;
		for ($column = max(0, (int)$first_column); $column <= min((int)$last_column, XLSX_WRITER_MAX_COLUMNS - 1); $column++)
			if (!isset($this->columnWidths[$column]))
				$this->columnWidths[$column] = $width;
	}

	/**
	 * Set the height of a row.
	 *
	 * @param int $row Zero-based.
	 * @param float $height Points.
	 */
	function setRow($row, $height) {
		if ($this->inRange($row, 0) && is_numeric($height))
			$this->rowHeights[(int)$row] = min(409, max(0, (float)$height));
	}

	/**
	 * Write a text cell.
	 *
	 * @param int $row Zero-based.
	 * @param int $column Zero-based.
	 * @param string $text Text in the input encoding.
	 * @param XlsxCellFormat|null $format
	 */
	function writeString($row, $column, $text, $format = null) {
		if (!$this->inRange($row, $column))
			return;
		$text = (string)$text;
		if (strcasecmp($this->inputEncoding, 'UTF-8') !== 0 && strcasecmp($this->inputEncoding, 'UTF8') !== 0) {
			$converted = @mb_convert_encoding($text, 'UTF-8', $this->inputEncoding);
			if (is_string($converted))
				$text = $converted;
		}
		if (mb_strlen($text, 'UTF-8') > XLSX_WRITER_MAX_TEXT)
			$text = mb_substr($text, 0, XLSX_WRITER_MAX_TEXT, 'UTF-8');
		$this->putCell($row, $column, '<c r="'.xlsx_writer_column_letters($column).((int)$row + 1).'"'
			.$this->styleAttribute($format).' t="inlineStr"><is><t xml:space="preserve">'
			.xlsx_writer_xml_text($text).'</t></is></c>');
	}

	/**
	 * Write a number cell. Dates are numbers (days since 1899-12-30) with a date format.
	 *
	 * @param int $row Zero-based.
	 * @param int $column Zero-based.
	 * @param int|float|string $number
	 * @param XlsxCellFormat|null $format
	 */
	function writeNumber($row, $column, $number, $format = null) {
		if (!$this->inRange($row, $column))
			return;
		$number = is_numeric($number) ? $number + 0 : 0;
		if (is_float($number) && !is_finite($number))
			$number = 0;
		$value = is_int($number) ? (string)$number : json_encode($number);
		$this->putCell($row, $column, '<c r="'.xlsx_writer_column_letters($column).((int)$row + 1).'"'
			.$this->styleAttribute($format).'><v>'.$value.'</v></c>');
	}

	/**
	 * Write an empty cell that only carries a format (used for borders across a row).
	 *
	 * @param int $row Zero-based.
	 * @param int $column Zero-based.
	 * @param XlsxCellFormat|null $format
	 */
	function writeBlank($row, $column, $format = null) {
		if (!$this->inRange($row, $column) || $format === null)
			return;
		$this->putCell($row, $column, '<c r="'.xlsx_writer_column_letters($column).((int)$row + 1).'"'
			.$this->styleAttribute($format).'/>');
	}

	/**
	 * Merge a range of cells. Invalid ranges and single cells are ignored. A merge replaces any earlier merge it
	 * overlaps, as Excel does when cells are merged again, because spreadsheet programs reject a file with
	 * overlapping merges.
	 *
	 * @param int $first_row
	 * @param int $first_column
	 * @param int $last_row
	 * @param int $last_column
	 */
	function mergeCells($first_row, $first_column, $last_row, $last_column) {
		$range = array((int)$first_row, (int)$first_column, (int)$last_row, (int)$last_column);
		if ($range[0] > $range[2] || $range[1] > $range[3] || !$this->inRange($range[0], $range[1])
			|| !$this->inRange($range[2], $range[3]) || ($range[0] == $range[2] && $range[1] == $range[3]))
			return;
		for ($row = $range[0]; $row <= $range[2]; $row++) {
			if (!isset($this->mergesByRow[$row]))
				continue;
			foreach ($this->mergesByRow[$row] as $index => $unused) {
				$other = $this->merges[$index];
				if ($range[1] <= $other[3] && $other[1] <= $range[3]) {
					for ($other_row = $other[0]; $other_row <= $other[2]; $other_row++)
						unset($this->mergesByRow[$other_row][$index]);
					unset($this->merges[$index]);
				}
			}
		}
		$this->merges[] = $range;
		end($this->merges);
		$index = key($this->merges);
		for ($row = $range[0]; $row <= $range[2]; $row++)
			$this->mergesByRow[$row][$index] = true;
	}

	/**
	 * Write the worksheet XML to an open file.
	 *
	 * @param resource $handle
	 * @param bool $selected Whether this is the active sheet.
	 * @return bool False when writing failed.
	 */
	function writeXml($handle, $selected) {
		$xml = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'."\n"
			.'<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"'
			.' xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">'
			.'<sheetViews><sheetView'.($selected ? ' tabSelected="1"' : '').($this->rightToLeft ? ' rightToLeft="1"' : '')
			.' workbookViewId="0"/></sheetViews><sheetFormatPr defaultRowHeight="12.75"/>';
		if ($this->columnWidths) {
			ksort($this->columnWidths);
			$xml .= '<cols>';
			foreach ($this->columnWidths as $column => $width)
				$xml .= '<col min="'.($column + 1).'" max="'.($column + 1).'" width="'
					.round($width > 0 ? $width + XLSX_WRITER_COLUMN_PADDING : 0, 4).'" customWidth="1"/>';
			$xml .= '</cols>';
		}
		$xml .= '<sheetData>';
		$rows = array_keys($this->rows + $this->rowHeights);
		sort($rows);
		foreach ($rows as $row) {
			$xml .= '<row r="'.($row + 1).'"'
				.(isset($this->rowHeights[$row]) ? ' ht="'.round($this->rowHeights[$row], 2).'" customHeight="1"' : '').'>';
			if (isset($this->rows[$row]))
				$xml .= is_array($this->rows[$row]) ? $this->joinRow($this->rows[$row]) : $this->rows[$row];
			$xml .= '</row>';
			if (strlen($xml) > 1048576) {
				if (fwrite($handle, $xml) !== strlen($xml))
					return false;
				$xml = '';
			}
		}
		$xml .= '</sheetData>';
		if ($this->merges) {
			$xml .= '<mergeCells count="'.count($this->merges).'">';
			foreach ($this->merges as $range)
				$xml .= '<mergeCell ref="'.xlsx_writer_column_letters($range[1]).($range[0] + 1).':'
					.xlsx_writer_column_letters($range[3]).($range[2] + 1).'"/>';
			$xml .= '</mergeCells>';
		}
		$xml .= '<pageMargins left="0.75" right="0.75" top="1" bottom="1" header="0.5" footer="0.5"/></worksheet>';
		return fwrite($handle, $xml) === strlen($xml);
	}

	/**
	 * Store one cell's XML, splitting its row again if it was joined, and join rows that are well behind.
	 *
	 * @param int $row
	 * @param int $column
	 * @param string $xml
	 */
	private function putCell($row, $column, $xml) {
		$row = (int)$row;
		if (isset($this->rows[$row]) && is_string($this->rows[$row]))
			$this->rows[$row] = $this->splitRow($this->rows[$row]);
		$this->rows[$row][(int)$column] = $xml;
		if ($row > $this->highestRow) {
			$this->highestRow = $row;
			for (; $this->joinedBelow < $row - XLSX_WRITER_OPEN_ROWS; $this->joinedBelow++)
				if (isset($this->rows[$this->joinedBelow]) && is_array($this->rows[$this->joinedBelow]))
					$this->rows[$this->joinedBelow] = $this->joinRow($this->rows[$this->joinedBelow]);
		}
	}

	/**
	 * Join a row's cells in column order.
	 *
	 * @param array<int, string> $cells
	 * @return string
	 */
	private function joinRow($cells) {
		ksort($cells);
		return implode('', $cells);
	}

	/**
	 * Split a joined row back into column => cell XML. Cell text is escaped, so a cell never contains "</c>".
	 *
	 * @param string $joined
	 * @return array<int, string>
	 */
	private function splitRow($joined) {
		$cells = array();
		preg_match_all('~<c r="([A-Z]+)[0-9]+"[^>]*?(?:/>|>.*?</c>)~s', $joined, $matches, PREG_SET_ORDER);
		foreach ($matches as $match) {
			$column = 0;
			foreach (str_split($match[1]) as $letter)
				$column = $column * 26 + ord($letter) - 64;
			$cells[$column - 1] = $match[0];
		}
		return $cells;
	}

	/**
	 * Whether a cell address is inside the sheet limits.
	 *
	 * @param int $row
	 * @param int $column
	 * @return bool
	 */
	private function inRange($row, $column) {
		return is_numeric($row) && is_numeric($column) && $row >= 0 && $column >= 0
			&& $row < XLSX_WRITER_MAX_ROWS && $column < XLSX_WRITER_MAX_COLUMNS;
	}

	/**
	 * Style attribute for a cell written with the given format.
	 *
	 * @param XlsxCellFormat|null $format
	 * @return string
	 */
	private function styleAttribute($format) {
		$style = $format instanceof XlsxCellFormat ? $this->workbook->styleIndex($format) : 0;
		return $style > 0 ? ' s="'.$style.'"' : '';
	}
}

/**
 * The workbook: worksheets, shared styles and the .xlsx package.
 */
class XlsxWorkbookWriter {
	/** @var string Target file path. */
	private $path;
	/** @var string Document title stored in the file properties. */
	private $title = '';
	/** @var XlsxWorksheet[] */
	private $sheets = array();
	/** @var array<string, int> Cell format signature => cellXfs index. */
	private $styles = array('' => 0);
	/** @var array<int, array> cellXfs index => [font id, border id, number format id, horizontal, vertical]. */
	private $styleRecords = array(array(0, 0, 0, '', ''));
	/** @var array<string, int> Font key => font id. */
	private $fonts = array('10|0|0' => 0);
	/** @var array<string, int> Border key => border id. */
	private $borders = array('|||' => 0);
	/** @var array<string, int> Format code => custom number format id. */
	private $numberFormats = array();

	/**
	 * @param string $path File to write when the workbook is closed.
	 */
	function __construct($path) {
		$this->path = $path;
	}

	/**
	 * Set the document title stored in the file properties.
	 *
	 * @param string $title UTF-8 text.
	 */
	function setTitle($title) {
		$this->title = (string)$title;
	}

	/**
	 * Create a cell format.
	 *
	 * @return XlsxCellFormat
	 */
	function addFormat() {
		return new XlsxCellFormat();
	}

	/**
	 * Add a worksheet. The name is cleaned to Excel's rules (no : \ / ? * [ ], no leading or trailing apostrophe,
	 * at most 31 characters, not the reserved name History, unique).
	 *
	 * @param string $name
	 * @return XlsxWorksheet
	 */
	function addWorksheet($name = '') {
		$name = trim(str_replace(array(':', '\\', '/', '?', '*', '[', ']'), '', (string)$name), "' ");
		if (!mb_check_encoding($name, 'UTF-8'))
			$name = mb_convert_encoding($name, 'UTF-8', 'UTF-8');
		$name = trim(mb_substr($name, 0, 31, 'UTF-8'), "' ");
		if ($name === '' || strcasecmp($name, 'History') === 0)
			$name = 'Sheet'.(count($this->sheets) + 1);
		$used = array();
		foreach ($this->sheets as $sheet)
			$used[mb_strtolower($sheet->name, 'UTF-8')] = true;
		for ($base = $name, $suffix = 2; isset($used[mb_strtolower($name, 'UTF-8')]); $suffix++)
			$name = mb_substr($base, 0, 31 - strlen(' ('.$suffix.')'), 'UTF-8').' ('.$suffix.')';
		$sheet = new XlsxWorksheet($this, $name);
		$this->sheets[] = $sheet;
		return $sheet;
	}

	/**
	 * Return the cellXfs index for a format, registering its font, border and number format on first use.
	 *
	 * @param XlsxCellFormat $format
	 * @return int
	 */
	function styleIndex($format) {
		$font_key = $format->fontKey();
		if (!isset($this->fonts[$font_key]))
			$this->fonts[$font_key] = count($this->fonts);
		$border_key = $format->borderKey();
		if (!isset($this->borders[$border_key]))
			$this->borders[$border_key] = count($this->borders);
		$number_format = $this->numberFormatId($format->numFormat);
		$record = array($this->fonts[$font_key], $this->borders[$border_key], $number_format, $format->horizontal,
			$format->vertical);
		$signature = implode('|', $record);
		if ($signature === '0|0|0||')
			return 0;
		if (!isset($this->styles[$signature])) {
			$this->styles[$signature] = count($this->styleRecords);
			$this->styleRecords[] = $record;
		}
		return $this->styles[$signature];
	}

	/**
	 * Write the .xlsx file.
	 *
	 * @return bool True when the file was written.
	 */
	function close() {
		if (!class_exists('ZipArchive'))
			return false;
		if (!$this->sheets)
			$this->addWorksheet();
		$zip = new ZipArchive();
		if ($zip->open($this->path, ZipArchive::CREATE | ZipArchive::OVERWRITE) !== true)
			return false;
		$main = 'http://schemas.openxmlformats.org/spreadsheetml/2006/main';
		$relationships = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships';
		$package_relationships = 'http://schemas.openxmlformats.org/package/2006/relationships';
		$header = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'."\n";

		$types = $header.'<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
			.'<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
			.'<Default Extension="xml" ContentType="application/xml"/>'
			.'<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
			.'<Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>'
			.'<Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>';
		$workbook_sheets = '';
		$workbook_relationships = '';
		$temporary_files = array();
		foreach ($this->sheets as $index => $sheet) {
			$number = $index + 1;
			$types .= '<Override PartName="/xl/worksheets/sheet'.$number.'.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>';
			$workbook_sheets .= '<sheet name="'.xlsx_writer_xml_text($sheet->name).'" sheetId="'.$number.'" r:id="rId'.$number.'"/>';
			$workbook_relationships .= '<Relationship Id="rId'.$number.'" Type="'.$relationships.'/worksheet" Target="worksheets/sheet'.$number.'.xml"/>';
			// Sheets go through a temporary file next to the target so a large report is never one string in memory.
			$temporary = $this->path.'.sheet'.$number.'.tmp';
			$handle = @fopen($temporary, 'wb');
			$written = $handle !== false && $sheet->writeXml($handle, $index == 0);
			if ($handle !== false)
				fclose($handle);
			$temporary_files[] = $temporary;
			if (!$written || !$zip->addFile($temporary, 'xl/worksheets/sheet'.$number.'.xml')) {
				$zip->close();
				$this->removeFiles(array_merge($temporary_files, array($this->path)));
				return false;
			}
		}
		$styles_id = count($this->sheets) + 1;
		$zip->addFromString('[Content_Types].xml', $types.'</Types>');
		$zip->addFromString('_rels/.rels', $header.'<Relationships xmlns="'.$package_relationships.'">'
			.'<Relationship Id="rId1" Type="'.$relationships.'/officeDocument" Target="xl/workbook.xml"/>'
			.'<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" Target="docProps/core.xml"/>'
			.'</Relationships>');
		$zip->addFromString('docProps/core.xml', $header.'<cp:coreProperties'
			.' xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties"'
			.' xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dcterms="http://purl.org/dc/terms/"'
			.' xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">'
			.'<dc:title>'.xlsx_writer_xml_text($this->title).'</dc:title><dc:creator>NotrinosERP</dc:creator>'
			.'<dcterms:created xsi:type="dcterms:W3CDTF">'.gmdate('Y-m-d\TH:i:s\Z').'</dcterms:created>'
			.'</cp:coreProperties>');
		$zip->addFromString('xl/workbook.xml', $header.'<workbook xmlns="'.$main.'" xmlns:r="'.$relationships.'">'
			.'<bookViews><workbookView/></bookViews><sheets>'.$workbook_sheets.'</sheets></workbook>');
		$zip->addFromString('xl/_rels/workbook.xml.rels', $header.'<Relationships xmlns="'.$package_relationships.'">'
			.$workbook_relationships.'<Relationship Id="rId'.$styles_id.'" Type="'.$relationships.'/styles" Target="styles.xml"/>'
			.'</Relationships>');
		$zip->addFromString('xl/styles.xml', $this->stylesXml());
		$closed = $zip->close();
		$this->removeFiles($closed ? $temporary_files : array_merge($temporary_files, array($this->path)));
		return $closed;
	}

	/**
	 * Delete the files that exist.
	 *
	 * @param string[] $paths
	 */
	private function removeFiles($paths) {
		foreach ($paths as $path)
			if (is_file($path))
				@unlink($path);
	}

	/**
	 * Return the number format id for a format code or built-in id.
	 *
	 * @param string|int $format
	 * @return int
	 */
	private function numberFormatId($format) {
		if (is_int($format) || (is_string($format) && ctype_digit($format)))
			return (int)$format;
		$format = (string)$format;
		if ($format === '' || strcasecmp($format, 'General') === 0)
			return 0;
		if (!isset($this->numberFormats[$format]))
			$this->numberFormats[$format] = 164 + count($this->numberFormats);
		return $this->numberFormats[$format];
	}

	/**
	 * Build styles.xml from the registered fonts, borders, number formats and cell formats.
	 *
	 * @return string
	 */
	private function stylesXml() {
		$xml = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'."\n"
			.'<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">';
		if ($this->numberFormats) {
			$xml .= '<numFmts count="'.count($this->numberFormats).'">';
			foreach ($this->numberFormats as $code => $id)
				$xml .= '<numFmt numFmtId="'.$id.'" formatCode="'.xlsx_writer_xml_text($code).'"/>';
			$xml .= '</numFmts>';
		}
		$xml .= '<fonts count="'.count($this->fonts).'">';
		foreach (array_keys($this->fonts) as $key) {
			list($size, $bold, $italic) = explode('|', $key);
			$xml .= '<font>'.($bold ? '<b/>' : '').($italic ? '<i/>' : '').'<sz val="'.$size.'"/><name val="Arial"/><family val="2"/></font>';
		}
		$xml .= '</fonts><fills count="2"><fill><patternFill patternType="none"/></fill>'
			.'<fill><patternFill patternType="gray125"/></fill></fills>';
		$xml .= '<borders count="'.count($this->borders).'">';
		foreach (array_keys($this->borders) as $key) {
			list($top, $top_color, $bottom, $bottom_color) = explode('|', $key);
			$xml .= '<border><left/><right/>'.$this->borderEdge('top', $top, $top_color)
				.$this->borderEdge('bottom', $bottom, $bottom_color).'<diagonal/></border>';
		}
		$xml .= '</borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>';
		$xml .= '<cellXfs count="'.count($this->styleRecords).'">';
		foreach ($this->styleRecords as $record) {
			list($font, $border, $number_format, $horizontal, $vertical) = $record;
			$xml .= '<xf numFmtId="'.$number_format.'" fontId="'.$font.'" fillId="0" borderId="'.$border.'" xfId="0"'
				.($number_format ? ' applyNumberFormat="1"' : '').($font ? ' applyFont="1"' : '')
				.($border ? ' applyBorder="1"' : '');
			if ($horizontal !== '' || $vertical !== '')
				$xml .= ' applyAlignment="1"><alignment'.($horizontal !== '' ? ' horizontal="'.$horizontal.'"' : '')
					.($vertical !== '' ? ' vertical="'.$vertical.'"' : '').'/></xf>';
			else
				$xml .= '/>';
		}
		return $xml.'</cellXfs><cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles></styleSheet>';
	}

	/**
	 * One border edge element.
	 *
	 * @param string $edge top or bottom.
	 * @param string $style '' for none.
	 * @param string $color ARGB or '' for automatic.
	 * @return string
	 */
	private function borderEdge($edge, $style, $color) {
		if ($style === '')
			return '<'.$edge.'/>';
		return '<'.$edge.' style="'.$style.'">'.($color !== '' ? '<color rgb="'.$color.'"/>' : '<color auto="1"/>').'</'.$edge.'>';
	}
}
