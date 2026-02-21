<?php
/**
* NotrinosERP Chart Library  �  class.graphic.inc
*
* Modern PHP/GD chart renderer visual design.
* Author: Tran Anh Phuong
* E-mail: anhphuong@aodieu.com
*
* Supported chart types:
*   1  Vertical bars
*   2  Horizontal bars
*   3  Scatter / dots
*   4  Line
*   5  Pie
*   6  Doughnut
*
* Two output modes (controlled by the $save argument of display()):
*   display($filename, true)  ?  save PNG to file  (PDF / report mode)
*   display()                 ?  echo inline base64 <img> tag (dashboard / HTML mode)
*
* The public API ($pg->x[], $pg->y[], $pg->z[], $pg->type, $pg->skin,
* $pg->graphic_1/2, $pg->axis_x/y, $pg->latin_notation, $pg->built_in,
* $pg->display($file, $border)) is kept 100 % compatible with the old class
* so existing report files (e.g. rep706.php) need zero changes.
*
* After display() returns, $pg->width and $pg->height contain the pixel
* dimensions of the rendered image (required by PDF embed code).
*/
/* =====================================================================
*  Standalone RTL / Arabic / Hebrew helpers  (i18n compat)
* ===================================================================== */

function is_arabic($text) {
	return preg_match('/\p{Arabic}/u', $text);
}
function is_hebrew($text) {
	return preg_match('/\p{Hebrew}/u', $text);
}
function utf8_strlen($str) {
	return preg_match_all('/[\x00-\x7F\xC0-\xFD]/', $str, $dummy);
}
function utf8_chr($uni) {
	if ($uni >= 0 && $uni <= 0x007f) return chr($uni);
	if ($uni <= 0x07ff) return chr(0xc0 | ($uni >> 6)) . chr(0x80 | ($uni & 0x003f));
	if ($uni == 0xFEFF) return chr(0);
	if ($uni >= 0xD800 && $uni <= 0xDFFF) return chr(0);
	if ($uni <= 0xffff)
	return chr(0xe0 | ($uni >> 12)) . chr(0x80 | (($uni >> 6) & 0x003f)) . chr(0x80 | ($uni & 0x003f));
	if ($uni <= 0x10ffff)
	return chr(0xf0 | ($uni >> 18)) . chr(0x80 | (($uni >> 12) & 0x3f))
	. chr(0x80 | (($uni >> 6) & 0x3f)) . chr(0x80 | ($uni & 0x3f));
	return chr(0);
}
function arabic($str, $z = '', $method = 'normal') {
	$p_chars = array(
	utf8_chr(0x0622)=>array(utf8_chr(0xfe82),utf8_chr(0xfe82),utf8_chr(0x0622)),
	utf8_chr(0x0627)=>array(utf8_chr(0xfe8e),utf8_chr(0xfe8e),utf8_chr(0x0627)),
	utf8_chr(0x0628)=>array(utf8_chr(0xfe90),utf8_chr(0xfe92),utf8_chr(0xfe91)),
	utf8_chr(0x067e)=>array(utf8_chr(0xfb57),utf8_chr(0xfb59),utf8_chr(0xfb58)),
	utf8_chr(0x062a)=>array(utf8_chr(0xfe96),utf8_chr(0xfe98),utf8_chr(0xfe97)),
	utf8_chr(0x062b)=>array(utf8_chr(0xfe9a),utf8_chr(0xfe9c),utf8_chr(0xfe9b)),
	utf8_chr(0x062c)=>array(utf8_chr(0xfe9e),utf8_chr(0xfea0),utf8_chr(0xfe9f)),
	utf8_chr(0x0686)=>array(utf8_chr(0xfb7b),utf8_chr(0xfb7d),utf8_chr(0xfb7c)),
	utf8_chr(0x062d)=>array(utf8_chr(0xfea2),utf8_chr(0xfea4),utf8_chr(0xfea3)),
	utf8_chr(0x062e)=>array(utf8_chr(0xfea6),utf8_chr(0xfea8),utf8_chr(0xfea7)),
	utf8_chr(0x062f)=>array(utf8_chr(0xfeaa),utf8_chr(0xfeaa),utf8_chr(0xfea9)),
	utf8_chr(0x0630)=>array(utf8_chr(0xfeac),utf8_chr(0xfeac),utf8_chr(0xfeab)),
	utf8_chr(0x0631)=>array(utf8_chr(0xfeae),utf8_chr(0xfeae),utf8_chr(0xfead)),
	utf8_chr(0x0632)=>array(utf8_chr(0xfeb0),utf8_chr(0xfeb0),utf8_chr(0xfeaf)),
	utf8_chr(0x0698)=>array(utf8_chr(0xfb8b),utf8_chr(0xfb8b),utf8_chr(0xfb8a)),
	utf8_chr(0x0633)=>array(utf8_chr(0xfeb2),utf8_chr(0xfeb4),utf8_chr(0xfeb3)),
	utf8_chr(0x0634)=>array(utf8_chr(0xfeb6),utf8_chr(0xfeb8),utf8_chr(0xfeb7)),
	utf8_chr(0x0635)=>array(utf8_chr(0xfeba),utf8_chr(0xfebc),utf8_chr(0xfebb)),
	utf8_chr(0x0636)=>array(utf8_chr(0xfebe),utf8_chr(0xfec0),utf8_chr(0xfebf)),
	utf8_chr(0x0637)=>array(utf8_chr(0xfec2),utf8_chr(0xfec4),utf8_chr(0xfec3)),
	utf8_chr(0x0638)=>array(utf8_chr(0xfec6),utf8_chr(0xfec8),utf8_chr(0xfec7)),
	utf8_chr(0x0639)=>array(utf8_chr(0xfeca),utf8_chr(0xfecc),utf8_chr(0xfecb)),
	utf8_chr(0x063a)=>array(utf8_chr(0xfece),utf8_chr(0xfed0),utf8_chr(0xfecf)),
	utf8_chr(0x0641)=>array(utf8_chr(0xfed2),utf8_chr(0xfed4),utf8_chr(0xfed3)),
	utf8_chr(0x0642)=>array(utf8_chr(0xfed6),utf8_chr(0xfed8),utf8_chr(0xfed7)),
	utf8_chr(0x06a9)=>array(utf8_chr(0xfeda),utf8_chr(0xfedc),utf8_chr(0xfedb)),
	utf8_chr(0x06af)=>array(utf8_chr(0xfb93),utf8_chr(0xfb95),utf8_chr(0xfb94)),
	utf8_chr(0x0644)=>array(utf8_chr(0xfede),utf8_chr(0xfee0),utf8_chr(0xfedf)),
	utf8_chr(0x0645)=>array(utf8_chr(0xfee2),utf8_chr(0xfee4),utf8_chr(0xfee3)),
	utf8_chr(0x0646)=>array(utf8_chr(0xfee6),utf8_chr(0xfee8),utf8_chr(0xfee7)),
	utf8_chr(0x0648)=>array(utf8_chr(0xfeee),utf8_chr(0xfeee),utf8_chr(0xfeed)),
	utf8_chr(0x06cc)=>array(utf8_chr(0xfbfd),utf8_chr(0xfbff),utf8_chr(0xfbfe)),
	utf8_chr(0x0643)=>array(utf8_chr(0xfeda),utf8_chr(0xfedc),utf8_chr(0xfedb)),
	utf8_chr(0x064a)=>array(utf8_chr(0xfef2),utf8_chr(0xfef4),utf8_chr(0xfef3)),
	utf8_chr(0x0623)=>array(utf8_chr(0xfe84),utf8_chr(0xfe84),utf8_chr(0xfe83)),
	utf8_chr(0x0624)=>array(utf8_chr(0xfe86),utf8_chr(0xfe86),utf8_chr(0xfe85)),
	utf8_chr(0x0625)=>array(utf8_chr(0xfe88),utf8_chr(0xfe88),utf8_chr(0xfe87)),
	utf8_chr(0x0626)=>array(utf8_chr(0xfe8a),utf8_chr(0xfe8c),utf8_chr(0xfe8b)),
	utf8_chr(0x0629)=>array(utf8_chr(0xfe94),utf8_chr(0xfe98),utf8_chr(0xfe97))
	);
	$nastaligh = array(utf8_chr(0x0647)=>array(utf8_chr(0xfbab),utf8_chr(0xfbad),utf8_chr(0xfbac)));
	$normal    = array(utf8_chr(0x0647)=>array(utf8_chr(0xfeea),utf8_chr(0xfeec),utf8_chr(0xfeeb)));
	$mp_chars  = array(utf8_chr(0x0622),utf8_chr(0x0627),utf8_chr(0x062f),utf8_chr(0x0630),
	utf8_chr(0x0631),utf8_chr(0x0632),utf8_chr(0x0698),utf8_chr(0x0648),
	utf8_chr(0x0623),utf8_chr(0x0625),utf8_chr(0x0624));
	$ignorelist = array(utf8_chr(0x0000),utf8_chr(0x064c),utf8_chr(0x064d),utf8_chr(0x064b),
	utf8_chr(0x064f),utf8_chr(0x0650),utf8_chr(0x064e),utf8_chr(0x0651),
	utf8_chr(0x0653),utf8_chr(0x0670),utf8_chr(0x0654),utf8_chr(0xfe76),
	utf8_chr(0xfe7a),utf8_chr(0xfe78),utf8_chr(0xfe7c),utf8_chr(0xfe7e),
	utf8_chr(0xfe74),utf8_chr(0xfe70),utf8_chr(0xfc5e),utf8_chr(0xfc5f),
	utf8_chr(0xfc60),utf8_chr(0xfc61),utf8_chr(0xfc62),utf8_chr(0xfc63));
	$output = ''; $e_output = ''; $num = '';
	if ($method == 'nastaligh') $p_chars = array_merge($p_chars, $nastaligh);
	else                        $p_chars = array_merge($p_chars, $normal);
	$str_len = utf8_strlen($str);
	preg_match_all("/./u", $str, $ar);
	for ($i = 0; $i < $str_len; $i++) {
		$str1     = isset($ar[0][$i])   ? $ar[0][$i]   : '';
		$str_next = isset($ar[0][$i+1]) ? $ar[0][$i+1] : null;
		$str_back = ($i > 0 && isset($ar[0][$i-1])) ? $ar[0][$i-1] : null;
		if (isset($ar[0][$i+1]) && in_array($ar[0][$i+1], $ignorelist)) {
			$str_next = isset($ar[0][$i+2]) ? $ar[0][$i+2] : null;
			$str_back = ($i >= 1) ? (isset($ar[0][$i-1]) ? $ar[0][$i-1] : null) : null;
		}
		if (!in_array($str1, $ignorelist)) {
			if (array_key_exists($str1, $p_chars)) {
				if (!$str_back || $str_back == " " || !array_key_exists($str_back, $p_chars)) {
					$output = (!array_key_exists($str_back, $p_chars) && !array_key_exists($str_next, $p_chars))
					? $str1 . $output : $p_chars[$str1][2] . $output;
				} elseif (array_key_exists($str_next, $p_chars) && array_key_exists($str_back, $p_chars)) {
					$output = (in_array($str_back, $mp_chars) && array_key_exists($str_next, $p_chars))
					? $p_chars[$str1][2] . $output : $p_chars[$str1][1] . $output;
				} else {
					$output = in_array($str_back, $mp_chars) ? $str1 . $output : $p_chars[$str1][0] . $output;
				}
				continue;
			} elseif ($z == "we") {
				$number = array('0','1','2','3','4','5','6','7','8','9',
				"\xD9\xA0","\xD9\xA1","\xD9\xA2","\xD9\xA3","\xD9\xA4","\xD9\xA5",
				"\xD9\xA6","\xD9\xA7","\xD9\xA8","\xD9\xA9","\xDB\xB0","\xDB\xB1",
				"\xDB\xB2","\xDB\xB3","\xDB\xB4","\xDB\xB5","\xDB\xB6","\xDB\xB7",
				"\xDB\xB8","\xDB\xB9");
				if (in_array($str1, $number)) { $num .= $str1; $str1 = ''; }
				if (!in_array($str_next, $number)) { $str1 .= $num; $num = ''; }
				$output = $output . $str1;
			} else {
				if (($str1 == utf8_chr(0x060c)) || ($str1 == utf8_chr(0x061f)) || ($str1 == utf8_chr(0x0621)) ||
				(array_key_exists($str_next, $p_chars) && array_key_exists($str_back, $p_chars)) ||
				($str1 == " " && array_key_exists($str_back, $p_chars)) ||
				($str1 == " " && array_key_exists($str_next, $p_chars))) {
					if ($e_output) { $output = $e_output . $output; $e_output = ''; }
					$output = $str1 . $output;
				} else {
					$e_output .= $str1;
					if (array_key_exists($str_next, $p_chars) || $str_next == '') {
						$output = $e_output . $output; $e_output = '';
					}
				}
			}
		} else {
			$output = $str1 . $output;
		}
	}
	return $output;
}
function hebrew($str) {
	preg_match_all("/./u", $str, $chars);
	return implode('', array_reverse($chars[0]));
}
/* =====================================================================
*  graph  �  The chart class
* ===================================================================== */

class graph {
	/* --- Public data interface (backward-compatible) -------------- */
	public $x = array();   // category labels
	public $y = array();   // dataset 1 values
	public $z = array();   // dataset 2 values
	/**
	* Optional Chart.js-like config array.
	*
	* Supported subset:
	*   type: bar|line|pie|doughnut|scatter|horizontalBar
	*   data.labels
	*   data.datasets[0..1].label
	*   data.datasets[0..1].data
	*   data.datasets[0..1].backgroundColor (hex)
	*   data.datasets[0..1].borderColor (hex)
	*   options.plugins.title.text
	*   options.scales.x.title.text
	*   options.scales.y.title.text
	*/
	public $chartjs_config = array();
	/* --- Chart metadata ------------------------------------------- */
	public $title     = '';
	public $axis_x    = '';
	public $axis_y    = '';
	public $graphic_1 = '';
	public $graphic_2 = '';
	/** Chart type: 1=vert-bar  2=horiz-bar  3=dots  4=line  5=pie  6=donut */
	public $type  = 1;
	/**
	* Skin � accepted for API compatibility but ignored visually.
	* Style is now fixed to the Chart.js-inspired palette.
	*/
	public $skin    = 1;
	public $credits = 0;
	/* --- Output dimensions (set after display()) ------------------ */
	public $width  = 700;
	public $height = 380;
	/* --- Misc compat properties ----------------------------------- */
	public $latin_notation   = false;
	public $built_in         = false;
	public $dec1             = 0;
	public $dec2             = 0;
	public $size             = 2;
	public $tsize            = 5;
	public $h3d              = 0;
	public $encoding         = 'UTF-8';
	public $fontfile         = '';
	public $alternate_x      = false;
	public $biggest_x        = '';
	public $biggest_y        = 0;
	public $graphic_2_exists = false;
	public $legend_exists    = false;
	public $total_parameters = 0;
	public $sum_total        = 1;
	public $biggest_value    = 0;
	public $higher_value     = 0;
	public $higher_value_str = '';
	public $highest_value    = 0;
	/* --- Private rendering state ---------------------------------- */
	private $img;           // GD image resource
	private $c = array();   // allocated GD colors
	private $pad;           // array: top, right, bottom, left (pixel padding)
	private $fontOk = false;// whether TTF font is usable
	private $dataset_styles = array();
	private $report_mode = false;
	
	/**
	* Chart.js default color palette.
	* Each entry: [fill_R, fill_G, fill_B, border_R, border_G, border_B]
	*/
	private static $PALETTE = array(
	1 => array(54,  162, 235,   10, 118, 191),  // Blue
	2 => array(255,  99, 132,  200,  55,  88),  // Red / Pink
	3 => array(75,  192, 192,   31, 148, 148),  // Teal
	4 => array(255, 205,  86,  200, 161,  42),  // Yellow
	5 => array(153, 102, 255,  109,  58, 211),  // Purple
	6 => array(255, 159,  64,  200, 115,  20),  // Orange
	7 => array(100, 181, 103,   56, 137,  59),  // Green
	);
	/* ================================================================
	*  Constructor
	* ================================================================ */
	public function __construct() {
		global $SysPrefs;
		$enc = (isset($_SESSION['language']) && $_SESSION['language']) ?
		strtoupper($_SESSION['language']->encoding) : 'UTF-8';
		$this->encoding = $enc;
		$path = dirname(__FILE__) . '/../fonts/';
		if ($enc === 'UTF-8') {
			$this->fontfile = (isset($SysPrefs->UTF8_fontfile) && $SysPrefs->UTF8_fontfile !== '')
			? $path . $SysPrefs->UTF8_fontfile
			: $path . 'FreeSans.ttf';
		} else {
			$this->fontfile = $path . 'LiberationSans-Regular.ttf';
		}
		$this->x = $this->y = $this->z = array();
	}

	/**
	* Set chart configuration using Chart.js-like syntax.
	* Returns $this for fluent style.
	*/
	public function setChartJsConfig($config) {
		if (!is_array($config)) {
			return $this;
		}
		$this->chartjs_config = $config;
		return $this;
	}

	/** Alias for setChartJsConfig() */
	public function chart($config) {
		return $this->setChartJsConfig($config);
	}
	/* ================================================================
	*  Public API
	* ================================================================ */
	/**
	* Render the chart.
	*
	* @param string $save   File path to save PNG (PDF/report mode).
	*                       Empty string -> echo inline base64 <img> (HTML/dashboard mode).
	* @param bool   $border Legacy parameter � accepted but unused.
	*/
	public function display($save = '', $border = false) {
		$this->report_mode = ($save !== '');
		$this->_applyChartJsConfig();

		if (!$this->report_mode) {
			$this->_prepare();
			$this->_buildPadding();
			$this->_calcDimensions();
			$this->_renderDashboardChartJs();
			return;
		}

		$this->_prepare();
		$this->_buildPadding();
		$this->_calcDimensions();
		$this->_createCanvas();
		$this->_allocColors();
		$this->_drawBackground();
		$this->_drawTitle();
		if (in_array($this->type, array(5, 6))) {
			$this->_drawPie($this->type == 6);
			$this->_drawLegendPie();
		} elseif ($this->type == 2) {
			$this->_drawGridHoriz();
			$this->_drawHorizontalBars();
			$this->_drawAxes();
			$this->_drawLegendDatasets();
		} elseif ($this->type == 3) {
			$this->_drawGridVert();
			$this->_drawAxes();
			$this->_drawDots();
			$this->_drawLegendDatasets();
		} elseif ($this->type == 4) {
			$this->_drawGridVert();
			$this->_drawLines();
			$this->_drawAxes();
			$this->_drawLegendDatasets();
		} else {
			// type 1: vertical bars (default)
			$this->_drawGridVert();
			$this->_drawVerticalBars();
			$this->_drawAxes();
			$this->_drawLegendDatasets();
		}
		if ($save !== '') {
			imagepng($this->img, $save);
		}
		imagedestroy($this->img);
		$this->img = null;
	}

	private function _renderDashboardChartJs() {
		$chartId = 'fa_chart_' . substr(md5(uniqid('', true)), 0, 10);
		$config = (!empty($this->chartjs_config) && is_array($this->chartjs_config))
		? $this->chartjs_config
		: $this->_buildChartJsConfigFromLegacy();

		if (!isset($config['options']) || !is_array($config['options'])) {
			$config['options'] = array();
		}
		if (!isset($config['options']['responsive'])) {
			$config['options']['responsive'] = true;
		}
		if (!isset($config['options']['maintainAspectRatio'])) {
			$config['options']['maintainAspectRatio'] = false;
		}
		if (!isset($config['options']['animation'])) {
			$config['options']['animation'] = true;
		}
		if (!isset($config['options']['interaction'])) {
			$config['options']['interaction'] = array('mode' => 'nearest', 'intersect' => false);
		}

		$flags = 0;
		if (defined('JSON_UNESCAPED_UNICODE')) $flags |= JSON_UNESCAPED_UNICODE;
		if (defined('JSON_UNESCAPED_SLASHES')) $flags |= JSON_UNESCAPED_SLASHES;
		if (defined('JSON_HEX_TAG')) $flags |= JSON_HEX_TAG;
		if (defined('JSON_HEX_AMP')) $flags |= JSON_HEX_AMP;
		if (defined('JSON_HEX_APOS')) $flags |= JSON_HEX_APOS;
		if (defined('JSON_HEX_QUOT')) $flags |= JSON_HEX_QUOT;
		$configJson = json_encode($config, $flags);

		echo '<div style="position:relative;width:100%;height:20em;">';
		echo '<canvas id="' . $chartId . '"></canvas>';
		echo '</div>';
		echo '<script>(function(){var c=document.getElementById("' . $chartId . '");if(!c||typeof Chart==="undefined")return;new Chart(c,' . $configJson . ');})();</script>';
	}

	private function _buildChartJsConfigFromLegacy() {
		$labels = array();
		for ($i = 0; $i < count($this->x); $i++) {
			$labels[] = (string)$this->x[$i];
		}

		$typeMap = array(1 => 'bar', 2 => 'bar', 3 => 'line', 4 => 'line', 5 => 'pie', 6 => 'doughnut');
		$chartType = isset($typeMap[(int)$this->type]) ? $typeMap[(int)$this->type] : 'bar';

		$config = array(
		'type' => $chartType,
		'data' => array(
		'labels' => $labels,
		'datasets' => array(),
		),
		'options' => array(
		'plugins' => array(
		'legend' => array('display' => true),
		'title' => array('display' => !empty($this->title), 'text' => $this->title),
		),
		),
		);

		if ($this->type == 2) {
			$config['options']['indexAxis'] = 'y';
		}

		if (in_array((int)$this->type, array(1, 2, 3, 4))) {
			$ds1 = array(
			'label' => ($this->graphic_1 !== '' ? $this->graphic_1 : _('Amount')),
			'data' => array_values($this->y),
			'backgroundColor' => $this->_paletteCssColor(1, false, 0.50),
			'borderColor' => $this->_paletteCssColor(1, true, 1),
			'borderWidth' => 2,
			);
			if ($this->type == 3) {
				$ds1['showLine'] = false;
				$ds1['pointRadius'] = 4;
			}
			if ($this->type == 4) {
				$ds1['fill'] = true;
				$ds1['tension'] = 0.25;
				$ds1['pointRadius'] = 3;
			}
			$config['data']['datasets'][] = $ds1;

			if (!empty($this->z)) {
				$ds2 = array(
				'label' => ($this->graphic_2 !== '' ? $this->graphic_2 : _('Amount 2')),
				'data' => array_values($this->z),
				'backgroundColor' => $this->_paletteCssColor(2, false, 0.50),
				'borderColor' => $this->_paletteCssColor(2, true, 1),
				'borderWidth' => 2,
				);
				if ($this->type == 3) {
					$ds2['showLine'] = false;
					$ds2['pointRadius'] = 4;
				}
				if ($this->type == 4) {
					$ds2['fill'] = true;
					$ds2['tension'] = 0.25;
					$ds2['pointRadius'] = 3;
				}
				$config['data']['datasets'][] = $ds2;
			}

			$config['options']['scales'] = array(
			'x' => array('title' => array('display' => !empty($this->axis_x), 'text' => $this->axis_x)),
			'y' => array('title' => array('display' => !empty($this->axis_y), 'text' => $this->axis_y), 'beginAtZero' => true),
			);
		} else {
			$pieColors = array();
			for ($i = 0; $i < max(1, count($this->y)); $i++) {
				$paletteIndex = ($i % 7) + 1;
				$pieColors[] = $this->_paletteCssColor($paletteIndex, false, 0.70);
			}
			$config['data']['datasets'][] = array(
			'label' => ($this->graphic_1 !== '' ? $this->graphic_1 : _('Amount')),
			'data' => array_values($this->y),
			'backgroundColor' => $pieColors,
			'borderColor' => '#ffffff',
			'borderWidth' => 1,
			);
			if (!isset($config['options']['plugins'])) {
				$config['options']['plugins'] = array();
			}
			if (!isset($config['options']['plugins']['legend'])) {
				$config['options']['plugins']['legend'] = array();
			}
			$config['options']['plugins']['legend']['display'] = true;
			$config['options']['plugins']['legend']['position'] = 'right';
			$config['options']['plugins']['legend']['align'] = 'start';
		}

		return $config;
	}

	private function _paletteCssColor($index, $border = false, $alpha = 1) {
		$idx = (int)$index;
		if (!isset(self::$PALETTE[$idx])) {
			$idx = 1;
		}
		$rgb = self::$PALETTE[$idx];
		if ($border) {
			return 'rgba(' . $rgb[3] . ',' . $rgb[4] . ',' . $rgb[5] . ',' . $alpha . ')';
		}
		return 'rgba(' . $rgb[0] . ',' . $rgb[1] . ',' . $rgb[2] . ',' . $alpha . ')';
	}

	private function _applyChartJsConfig() {
		if (empty($this->chartjs_config) || !is_array($this->chartjs_config)) {
			return;
		}

		$cfg = $this->chartjs_config;
		$data = isset($cfg['data']) && is_array($cfg['data']) ? $cfg['data'] : array();
		$datasets = isset($data['datasets']) && is_array($data['datasets']) ? $data['datasets'] : array();

		if (isset($data['labels']) && is_array($data['labels'])) {
			$this->x = array_values($data['labels']);
		}

		$type = isset($cfg['type']) ? strtolower(trim((string)$cfg['type'])) : '';
		$indexAxis = '';
		if (isset($cfg['options']['indexAxis'])) {
			$indexAxis = strtolower(trim((string)$cfg['options']['indexAxis']));
		}

		if ($type === 'line') {
			$this->type = 4;
		} elseif ($type === 'pie') {
			$this->type = 5;
		} elseif ($type === 'doughnut') {
			$this->type = 6;
		} elseif ($type === 'scatter') {
			$this->type = 3;
		} elseif ($type === 'horizontalbar' || ($type === 'bar' && $indexAxis === 'y')) {
			$this->type = 2;
		} else {
			$this->type = 1;
		}

		$this->dataset_styles = array();
		$this->y = array();
		$this->z = array();

		if (isset($datasets[0]) && is_array($datasets[0])) {
			$ds1 = $datasets[0];
			$this->graphic_1 = isset($ds1['label']) ? (string)$ds1['label'] : $this->graphic_1;
			$this->y = $this->_extractDatasetValues($ds1);
			$this->dataset_styles[1] = array(
			'bg' => isset($ds1['backgroundColor']) ? $ds1['backgroundColor'] : null,
			'border' => isset($ds1['borderColor']) ? $ds1['borderColor'] : null,
			);
		}
		if (isset($datasets[1]) && is_array($datasets[1])) {
			$ds2 = $datasets[1];
			$this->graphic_2 = isset($ds2['label']) ? (string)$ds2['label'] : $this->graphic_2;
			$this->z = $this->_extractDatasetValues($ds2);
			$this->dataset_styles[2] = array(
			'bg' => isset($ds2['backgroundColor']) ? $ds2['backgroundColor'] : null,
			'border' => isset($ds2['borderColor']) ? $ds2['borderColor'] : null,
			);
		}

		if (isset($cfg['options']['plugins']['title']['text'])) {
			$title = $cfg['options']['plugins']['title']['text'];
			$this->title = is_array($title) ? implode(' ', $title) : (string)$title;
		}
		if (isset($cfg['options']['scales']['x']['title']['text'])) {
			$axisX = $cfg['options']['scales']['x']['title']['text'];
			$this->axis_x = is_array($axisX) ? implode(' ', $axisX) : (string)$axisX;
		}
		if (isset($cfg['options']['scales']['y']['title']['text'])) {
			$axisY = $cfg['options']['scales']['y']['title']['text'];
			$this->axis_y = is_array($axisY) ? implode(' ', $axisY) : (string)$axisY;
		}

		if (empty($this->x) && !empty($this->y)) {
			$this->x = array();
			for ($i = 0; $i < count($this->y); $i++) {
				$this->x[] = (string)($i + 1);
			}
		}
	}

	private function _extractDatasetValues($dataset) {
		$out = array();
		if (!isset($dataset['data']) || !is_array($dataset['data'])) {
			return $out;
		}
		foreach ($dataset['data'] as $idx => $point) {
			if (is_array($point)) {
				if (isset($point['y'])) {
					$out[$idx] = (float)$point['y'];
					if (!isset($this->x[$idx])) {
						$this->x[$idx] = isset($point['x']) ? (string)$point['x'] : (string)($idx + 1);
					}
				} else {
					$out[$idx] = 0.0;
				}
			} else {
				$out[$idx] = (float)$point;
			}
		}
		ksort($out);
		return array_values($out);
	}
	/* ================================================================
	*  Preparation helpers
	* ================================================================ */
	private function _prepare() {
		$this->total_parameters  = count($this->x);
		$this->graphic_2_exists  = (count($this->z) > 0) && in_array($this->type, array(1,2,3,4));
		$this->legend_exists     = $this->graphic_2_exists
		|| !empty($this->graphic_1) || !empty($this->graphic_2)
		|| in_array($this->type, array(5, 6));
		$this->sum_total = array_sum($this->y);
		if ($this->sum_total == 0) $this->sum_total = 1;
		$this->biggest_y = 0;
		for ($i = 0; $i < $this->total_parameters; $i++) {
			$v = abs((float)(isset($this->y[$i]) ? $this->y[$i] : 0));
			if ($v > $this->biggest_y) $this->biggest_y = $v;
			if ($this->graphic_2_exists && isset($this->z[$i])) {
				$v2 = abs((float)$this->z[$i]);
				if ($v2 > $this->biggest_y) $this->biggest_y = $v2;
			}
		}
		$this->highest_value    = $this->_niceMax($this->biggest_y);
		$this->higher_value     = $this->highest_value;
		$this->biggest_value    = $this->highest_value;
		$this->higher_value_str = $this->_fmt($this->highest_value);
		$this->fontOk = function_exists('imagettftext') && file_exists($this->fontfile);
	}
	/** Round up to a "nice" chart maximum (mirrors Chart.js auto-scaling). */
	private function _niceMax($v) {
		if ($v <= 0) return 10;
		$exp  = (int)floor(log10($v));
		$frac = $v / pow(10, $exp);
		if ($frac <= 1)     $nice = 1;
		elseif ($frac <= 2) $nice = 2;
		elseif ($frac <= 5) $nice = 5;
		else                $nice = 10;
		return $nice * pow(10, $exp);
	}
	private function _buildPadding() {
		$titleH = !empty($this->title) ? 48 : 20;
		$hasLeg = $this->legend_exists && ($this->graphic_2_exists || !empty($this->graphic_1));
		if (in_array($this->type, array(5, 6))) {
			$this->pad = array('top'=>$titleH,'right'=>230,'bottom'=>30,'left'=>30);
		} elseif ($this->type == 2) {
			$this->pad = array('top'=>$titleH,'right'=>($hasLeg?180:40),'bottom'=>55,'left'=>130);
		} else {
			$this->pad = array('top'=>$titleH,'right'=>($hasLeg?180:40),'bottom'=>72,'left'=>72);
		}
	}
	private function _calcDimensions() {
		$n = max(1, $this->total_parameters);
		switch ((int)$this->type) {
			case 2:
				$this->width  = 680;
				$this->height = max(300, $this->pad['top'] + $this->pad['bottom'] + $n * 38 + 20);
				break;
			case 5:
			case 6:
				$this->width  = 660;
				$this->height = max(360, $this->pad['top'] + 30 + $n * 22 + 40);
				break;
			default:
				$minW = $this->pad['left'] + $this->pad['right'] + $n * 62 + 30;
				$this->width  = max(520, $minW);
				$this->height = 400;
				break;
		}
	}
	/* ================================================================
	*  Canvas / colour setup
	* ================================================================ */
	private function _createCanvas() {
		$this->img = imagecreatetruecolor($this->width, $this->height);
		if (function_exists('imageantialias')) {
			imageantialias($this->img, true);
		}
	}
	private function _allocColors() {
		$img = $this->img;
		$c   = &$this->c;
		$c['bg']        = imagecolorallocate($img, 255, 255, 255);
		$c['border']    = imagecolorallocate($img, 210, 214, 222);
		$c['title']     = imagecolorallocate($img,  50,  50,  50);
		$c['axis']      = imagecolorallocate($img, 180, 180, 180);
		$c['grid']      = imagecolorallocate($img, 234, 236, 240);
		$c['text']      = imagecolorallocate($img,  90,  90,  90);
		foreach (self::$PALETTE as $n => $rgb) {
			list($fr,$fg,$fb,$br,$bg,$bb) = $rgb;
			$c["fill_$n"]   = imagecolorallocate($img, $fr, $fg, $fb);
			$c["border_$n"] = imagecolorallocate($img, $br, $bg, $bb);
			// Lighter tint for line-chart fill area (~60% white blend)
			$lr = min(255, $fr + (int)(($fr < 220 ? 60 : 15)));
			$lg = min(255, $fg + (int)(($fg < 220 ? 60 : 15)));
			$lb = min(255, $fb + (int)(($fb < 220 ? 60 : 15)));
			$c["light_$n"] = imagecolorallocate($img, $lr, $lg, $lb);
		}

		if (!empty($this->dataset_styles)) {
			for ($i = 1; $i <= 2; $i++) {
				if (!isset($this->dataset_styles[$i])) {
					continue;
				}
				$style = $this->dataset_styles[$i];
				if (!empty($style['bg'])) {
					$rgb = $this->_toRgb($style['bg']);
					if ($rgb) {
						$c["fill_$i"] = imagecolorallocate($img, $rgb[0], $rgb[1], $rgb[2]);
						$c["light_$i"] = imagecolorallocate($img,
							min(255, $rgb[0] + 60),
							min(255, $rgb[1] + 60),
							min(255, $rgb[2] + 60)
						);
					}
				}
				if (!empty($style['border'])) {
					$rgb = $this->_toRgb($style['border']);
					if ($rgb) {
						$c["border_$i"] = imagecolorallocate($img, $rgb[0], $rgb[1], $rgb[2]);
					}
				}
			}
		}
	}

	private function _toRgb($value) {
		if (is_array($value)) {
			if (isset($value[0], $value[1], $value[2])) {
				return array((int)$value[0], (int)$value[1], (int)$value[2]);
			}
			return null;
		}

		$raw = trim((string)$value);
		if ($raw === '') {
			return null;
		}

		if (preg_match('/^#([0-9a-fA-F]{6})$/', $raw, $m)) {
			$hex = $m[1];
			return array(
				hexdec(substr($hex, 0, 2)),
				hexdec(substr($hex, 2, 2)),
				hexdec(substr($hex, 4, 2))
			);
		}

		if (preg_match('/^#([0-9a-fA-F]{3})$/', $raw, $m)) {
			$hex = $m[1];
			return array(
				hexdec(str_repeat(substr($hex, 0, 1), 2)),
				hexdec(str_repeat(substr($hex, 1, 1), 2)),
				hexdec(str_repeat(substr($hex, 2, 1), 2))
			);
		}

		if (preg_match('/rgba?\(([^\)]*)\)/i', $raw, $m)) {
			$parts = explode(',', $m[1]);
			if (count($parts) >= 3) {
				return array((int)$parts[0], (int)$parts[1], (int)$parts[2]);
			}
		}

		return null;
	}
	/* ================================================================
	*  Background & title
	* ================================================================ */
	private function _drawBackground() {
		imagefill($this->img, 0, 0, $this->c['bg']);
		if ($this->report_mode) {
			imagerectangle($this->img, 0, 0, $this->width-1, $this->height-1, $this->c['border']);
		}
	}
	private function _drawTitle() {
		if (empty($this->title)) return;
		$txt = $this->_encodeText($this->title);
		if ($this->fontOk) {
			$box = imagettfbbox(13, 0, $this->fontfile, $txt);
			$tw  = abs($box[2] - $box[0]);
			$cx  = max(10, (int)(($this->width - $tw) / 2));
			imagettftext($this->img, 13, 0, $cx, 32, $this->c['title'], $this->fontfile, $txt);
		} else {
			$tw = imagefontwidth(5) * strlen($txt);
			$cx = max(0, (int)(($this->width - $tw) / 2));
			imagestring($this->img, 5, $cx, 12, $txt, $this->c['title']);
		}
	}
	/* ================================================================
	*  Helper: chart plot area [x1, y1, x2, y2]
	* ================================================================ */
	private function _area() {
		return array(
			$this->pad['left'],
			$this->pad['top'],
			$this->width  - $this->pad['right'],
			$this->height - $this->pad['bottom'],
		);
	}
	/* ================================================================
	*  Grid lines & axes
	* ================================================================ */
	/** Horizontal grid lines for vertical bar, dots, line charts */
	private function _drawGridVert() {
		list($x1, $y1, $x2, $y2) = $this->_area();
		$aH   = $y2 - $y1;
		$max  = $this->highest_value ?: 1;
		$steps = 5;
		for ($s = 0; $s <= $steps; $s++) {
			$v  = $max * $s / $steps;
			$py = $y2 - (int)($aH * $s / $steps);
			imageline($this->img, $x1, $py, $x2, $py, $this->c['grid']);
			$lbl = $this->_fmt($v);
			$lw  = $this->_textWidth(8, $lbl);
			$this->_text(8, $lbl, $x1 - $lw - 6, $py + 4, 'text');
		}
		// Vertical axis label (rotated 90 degrees)
		if (!empty($this->axis_y) && $this->fontOk) {
			$txt = $this->_encodeText($this->axis_y);
			$box = imagettfbbox(9, 90, $this->fontfile, $txt);
			$th  = abs($box[1] - $box[7]);
			$ty  = (int)(($y1 + $y2 + $th) / 2);
			imagettftext($this->img, 9, 90, 12, $ty, $this->c['title'], $this->fontfile, $txt);
		}
	}
	/** Vertical grid lines for horizontal bar chart */
	private function _drawGridHoriz() {
		list($x1, $y1, $x2, $y2) = $this->_area();
		$aW   = $x2 - $x1;
		$max  = $this->highest_value ?: 1;
		$steps = 5;
		for ($s = 0; $s <= $steps; $s++) {
			$v  = $max * $s / $steps;
			$px = $x1 + (int)($aW * $s / $steps);
			imageline($this->img, $px, $y1, $px, $y2, $this->c['grid']);
			$lbl = $this->_fmt($v);
			$lw  = $this->_textWidth(8, $lbl);
			$this->_text(8, $lbl, (int)($px - $lw/2), $y2 + 15, 'text');
		}
		// Horizontal axis label
		if (!empty($this->axis_x)) {
			$lw = $this->_textWidth(9, $this->axis_x);
			$this->_text(9, $this->axis_x, (int)(($x1+$x2-$lw)/2), $this->height - 8, 'title');
		}
	}
	private function _drawAxes() {
		list($x1, $y1, $x2, $y2) = $this->_area();
		imageline($this->img, $x1, $y2, $x2, $y2, $this->c['axis']); // X
		imageline($this->img, $x1, $y1, $x1, $y2, $this->c['axis']); // Y
		if ($this->type != 2 && !empty($this->axis_x)) {
			$lw = $this->_textWidth(9, $this->axis_x);
			$this->_text(9, $this->axis_x, (int)(($x1+$x2-$lw)/2), $this->height - 8, 'title');
		}
	}
	/* ================================================================
	*  Chart type: 1  Vertical Bars
	* ================================================================ */
	private function _drawVerticalBars() {
		list($x1, $y1, $x2, $y2) = $this->_area();
		$n   = $this->total_parameters;
		if ($n === 0) return;
		$aW  = $x2 - $x1;
		$aH  = $y2 - $y1;
		$max = $this->highest_value ?: 1;
		$has2 = $this->graphic_2_exists;
		$groupW = (int)($aW / $n);
		$barW   = $has2 ? max(10, (int)($groupW * 0.34)) : max(10, (int)($groupW * 0.55));
		$gap    = $has2 ? 2 : 0;
		for ($i = 0; $i < $n; $i++) {
			$cx = $x1 + (int)(($i + 0.5) * $groupW);
			$v1 = abs((float)(isset($this->y[$i]) ? $this->y[$i] : 0));
			$h1 = (int)($aH * $v1 / $max);
			// Dataset 2 bar (right of centre)
			if ($has2 && isset($this->z[$i])) {
				$v2  = abs((float)$this->z[$i]);
				$h2  = (int)($aH * $v2 / $max);
				$bx1 = $cx + $gap;
				$bx2 = $bx1 + $barW;
				$by1 = $y2 - $h2;
				imagefilledrectangle($this->img, $bx1, $by1, $bx2, $y2, $this->c['fill_2']);
				imagerectangle($this->img, $bx1, $by1, $bx2, $y2, $this->c['border_2']);
			}
			// Dataset 1 bar
			$bx1 = $has2 ? ($cx - $gap - $barW) : ($cx - (int)($barW/2));
			$bx2 = $bx1 + $barW;
			$by1 = $y2 - $h1;
			imagefilledrectangle($this->img, $bx1, $by1, $bx2, $y2, $this->c['fill_1']);
			imagerectangle($this->img, $bx1, $by1, $bx2, $y2, $this->c['border_1']);
			// Value label above bar
			if ($h1 > 0 && $h1 > 14) {
				$vlbl = $this->_fmt($v1);
				$vlw  = $this->_textWidth(7, $vlbl);
				$this->_text(7, $vlbl, (int)($bx1 + ($barW - $vlw)/2), $by1 - 4, 'text');
			}
			// X-axis category label
			$lbl = $this->_truncate((string)(isset($this->x[$i]) ? $this->x[$i] : ''), 12);
			$lw  = $this->_textWidth(8, $lbl);
			$this->_text(8, $lbl, (int)($cx - $lw/2), $y2 + 16, 'text');
		}
	}
	/* ================================================================
	*  Chart type: 2  Horizontal Bars
	* ================================================================ */
	private function _drawHorizontalBars() {
		list($x1, $y1, $x2, $y2) = $this->_area();
		$n   = $this->total_parameters;
		if ($n === 0) return;
		$aW  = $x2 - $x1;
		$aH  = $y2 - $y1;
		$max = $this->highest_value ?: 1;
		$has2 = $this->graphic_2_exists;
		$groupH = (int)($aH / $n);
		$barH   = $has2 ? max(8, (int)($groupH * 0.34)) : max(8, (int)($groupH * 0.55));
		$gap    = $has2 ? 2 : 0;
		for ($i = 0; $i < $n; $i++) {
			$cy  = $y1 + (int)(($i + 0.5) * $groupH);
			$v1  = abs((float)(isset($this->y[$i]) ? $this->y[$i] : 0));
			$w1  = (int)($aW * $v1 / $max);
			// Dataset 2
			if ($has2 && isset($this->z[$i])) {
				$v2  = abs((float)$this->z[$i]);
				$w2  = (int)($aW * $v2 / $max);
				$by1 = $cy + $gap;
				$by2 = $by1 + $barH;
				imagefilledrectangle($this->img, $x1, $by1, $x1+$w2, $by2, $this->c['fill_2']);
				imagerectangle($this->img, $x1, $by1, $x1+$w2, $by2, $this->c['border_2']);
			}
			// Dataset 1
			$by1 = $has2 ? ($cy - $gap - $barH) : ($cy - (int)($barH/2));
			$by2 = $by1 + $barH;
			imagefilledrectangle($this->img, $x1, $by1, $x1+$w1, $by2, $this->c['fill_1']);
			imagerectangle($this->img, $x1, $by1, $x1+$w1, $by2, $this->c['border_1']);
			// Category label on left
			$lbl = $this->_truncate((string)(isset($this->x[$i]) ? $this->x[$i] : ''), 18);
			$lh  = $this->_textHeight(8);
			$lw  = $this->_textWidth(8, $lbl);
			$this->_text(8, $lbl, $x1 - $lw - 8, (int)($cy + $lh * 0.5), 'text');
		}
	}
	/* ================================================================
	*  Chart type: 3  Scatter / Dots
	* ================================================================ */
	private function _drawDots() {
		list($x1, $y1, $x2, $y2) = $this->_area();
		$n   = $this->total_parameters;
		if ($n === 0) return;
		$aW  = $x2 - $x1;
		$aH  = $y2 - $y1;
		$max = $this->highest_value ?: 1;
		$has2 = $this->graphic_2_exists;
		$step = $n > 1 ? (int)($aW / ($n-1)) : (int)($aW / 2);
		for ($i = 0; $i < $n; $i++) {
			$px  = $x1 + $i * $step;
			$v1  = abs((float)(isset($this->y[$i]) ? $this->y[$i] : 0));
			$py1 = $y2 - (int)($aH * $v1 / $max);
			imagefilledellipse($this->img, $px, $py1, 10, 10, $this->c['fill_1']);
			imageellipse($this->img, $px, $py1, 10, 10, $this->c['border_1']);
			if ($has2 && isset($this->z[$i])) {
				$v2  = abs((float)$this->z[$i]);
				$py2 = $y2 - (int)($aH * $v2 / $max);
				imagefilledellipse($this->img, $px, $py2, 10, 10, $this->c['fill_2']);
				imageellipse($this->img, $px, $py2, 10, 10, $this->c['border_2']);
			}
			imageline($this->img, $px, $y2, $px, $y2+5, $this->c['axis']);
			$lbl = $this->_truncate((string)(isset($this->x[$i]) ? $this->x[$i] : ''), 10);
			$lw  = $this->_textWidth(8, $lbl);
			$this->_text(8, $lbl, (int)($px - $lw/2), $y2 + 16, 'text');
		}
	}
	/* ================================================================
	*  Chart type: 4  Line
	* ================================================================ */
	private function _drawLines() {
		list($x1, $y1, $x2, $y2) = $this->_area();
		$n   = $this->total_parameters;
		if ($n === 0) return;
		$aW  = $x2 - $x1;
		$aH  = $y2 - $y1;
		$max = $this->highest_value ?: 1;
		$has2 = $this->graphic_2_exists;
		$step = $n > 1 ? (int)($aW / ($n-1)) : (int)($aW / 2);
		$pts1 = array(); $pts2 = array();
		for ($i = 0; $i < $n; $i++) {
			$px = $x1 + $i * $step;
			$v1 = abs((float)(isset($this->y[$i]) ? $this->y[$i] : 0));
			$pts1[] = array($px, $y2 - (int)($aH * $v1 / $max));
			if ($has2 && isset($this->z[$i])) {
				$v2 = abs((float)$this->z[$i]);
				$pts2[] = array($px, $y2 - (int)($aH * $v2 / $max));
			}
			imageline($this->img, $px, $y2, $px, $y2+5, $this->c['axis']);
			$lbl = $this->_truncate((string)(isset($this->x[$i]) ? $this->x[$i] : ''), 10);
			$lw  = $this->_textWidth(8, $lbl);
			$this->_text(8, $lbl, (int)($px - $lw/2), $y2 + 16, 'text');
		}
		// Shaded fill under line 1
		if (count($pts1) >= 2) {
			$last = end($pts1); $first = reset($pts1);
			$poly = array();
			foreach ($pts1 as $pt) { $poly[] = $pt[0]; $poly[] = $pt[1]; }
			$poly[] = $last[0];  $poly[] = $y2;
			$poly[] = $first[0]; $poly[] = $y2;
			imagefilledpolygon($this->img, $poly, (int)(count($poly)/2), $this->c['light_1']);
		}
		if ($has2 && count($pts2) >= 2) {
			$last = end($pts2); $first = reset($pts2);
			$poly = array();
			foreach ($pts2 as $pt) { $poly[] = $pt[0]; $poly[] = $pt[1]; }
			$poly[] = $last[0];  $poly[] = $y2;
			$poly[] = $first[0]; $poly[] = $y2;
			imagefilledpolygon($this->img, $poly, (int)(count($poly)/2), $this->c['light_2']);
		}
		// Lines (2 px thick) then dots
		for ($i = 0; $i < count($pts1)-1; $i++) {
			imageline($this->img, $pts1[$i][0], $pts1[$i][1],   $pts1[$i+1][0], $pts1[$i+1][1],   $this->c['fill_1']);
			imageline($this->img, $pts1[$i][0], $pts1[$i][1]+1, $pts1[$i+1][0], $pts1[$i+1][1]+1, $this->c['fill_1']);
		}
		foreach ($pts1 as $pt) {
			imagefilledellipse($this->img, $pt[0], $pt[1], 8, 8, $this->c['fill_1']);
			imageellipse($this->img, $pt[0], $pt[1], 8, 8, $this->c['border_1']);
		}
		if ($has2 && count($pts2) >= 2) {
			for ($i = 0; $i < count($pts2)-1; $i++) {
				imageline($this->img, $pts2[$i][0], $pts2[$i][1],   $pts2[$i+1][0], $pts2[$i+1][1],   $this->c['fill_2']);
				imageline($this->img, $pts2[$i][0], $pts2[$i][1]+1, $pts2[$i+1][0], $pts2[$i+1][1]+1, $this->c['fill_2']);
			}
			foreach ($pts2 as $pt) {
				imagefilledellipse($this->img, $pt[0], $pt[1], 8, 8, $this->c['fill_2']);
				imageellipse($this->img, $pt[0], $pt[1], 8, 8, $this->c['border_2']);
			}
		}
	}
	/* ================================================================
	*  Chart type: 5/6  Pie & Doughnut
	* ================================================================ */
	private function _drawPie($donut) {
		list($x1, $y1, $x2, $y2) = $this->_area();
		$n = $this->total_parameters;
		if ($n === 0) return;
		$aW   = $x2 - $x1;
		$aH   = $y2 - $y1;
		$r    = (int)(min($aW, $aH) * 0.42);
		$diam = $r * 2;
		// Centre the circle on left portion of chart area
		$cx   = $x1 + $r + 10;
		$cy   = (int)(($y1 + $y2) / 2);
		$sum  = $this->sum_total ?: 1;
		$start = -90; // 12 o'clock (Chart.js default)
		for ($i = 0; $i < $n; $i++) {
			$pct   = (float)(isset($this->y[$i]) ? $this->y[$i] : 0) / $sum;
			$deg   = (int)round($pct * 360);
			$end   = $start + $deg;
			$num   = ($i % 7) + 1;
			imagefilledarc($this->img, $cx, $cy, $diam, $diam, $start, $end,
				$this->c["fill_$num"], IMG_ARC_PIE);
			// White separator between segments (1-pixel)
			imagearc($this->img, $cx, $cy, $diam+2, $diam+2, $start, $start+1, $this->c['bg']);
			$start = $end;
		}
		// Outer ring border
		imageellipse($this->img, $cx, $cy, $diam+1, $diam+1, $this->c['border']);
		if ($donut) {
			$hd = (int)($diam * 0.55);
			imagefilledellipse($this->img, $cx, $cy, $hd, $hd, $this->c['bg']);
			imageellipse($this->img, $cx, $cy, $hd, $hd, $this->c['border']);
		}
	}
	/* ================================================================
	*  Legends
	* ================================================================ */
	/** Legend for pie/donut: each slice label + percentage */
	private function _drawLegendPie() {
		list(,, $x2, $y1) = $this->_area();
		$lx = $x2 + 18;
		$ly = (int)$this->pad['top'] + 10;
		$bx = 14;
		if (!empty($this->axis_x)) {
			$this->_text(10, $this->axis_x, $lx, $ly + 10, 'title');
			$ly += 26;
		}
		$sum = $this->sum_total ?: 1;
		for ($i = 0; $i < $this->total_parameters; $i++) {
			$num = ($i % 7) + 1;
			imagefilledrectangle($this->img, $lx, $ly, $lx+$bx, $ly+$bx, $this->c["fill_$num"]);
			imagerectangle($this->img, $lx, $ly, $lx+$bx, $ly+$bx, $this->c["border_$num"]);
			$pct  = number_format((float)(isset($this->y[$i]) ? $this->y[$i] : 0) * 100 / $sum, 1);
			$lbl  = $this->_truncate((string)(isset($this->x[$i]) ? $this->x[$i] : ''), 24);
			$this->_text(8, "$lbl  $pct%", $lx + $bx + 6, $ly + $bx, 'text');
			$ly += 21;
		}
	}
	/** Legend for dataset 1 / dataset 2 (bar, line, dot charts) */
	private function _drawLegendDatasets() {
		if (!$this->graphic_2_exists && empty($this->graphic_1) && empty($this->graphic_2)) return;
		list(,, $x2,) = $this->_area();
		$lx = $x2 + 18;
		$ly = (int)$this->pad['top'] + 10;
		$bx = 14;
		if (!empty($this->graphic_1)) {
			imagefilledrectangle($this->img, $lx, $ly, $lx+$bx, $ly+$bx, $this->c['fill_1']);
			imagerectangle($this->img, $lx, $ly, $lx+$bx, $ly+$bx, $this->c['border_1']);
			$this->_text(8, $this->graphic_1, $lx+$bx+6, $ly+$bx, 'text');
			$ly += 22;
		}
		if (!empty($this->graphic_2) && $this->graphic_2_exists) {
			imagefilledrectangle($this->img, $lx, $ly, $lx+$bx, $ly+$bx, $this->c['fill_2']);
			imagerectangle($this->img, $lx, $ly, $lx+$bx, $ly+$bx, $this->c['border_2']);
			$this->_text(8, $this->graphic_2, $lx+$bx+6, $ly+$bx, 'text');
		}
	}
	/* ================================================================
	*  Text rendering helpers
	* ================================================================ */
	/**
	* Draw text with TTF if available, otherwise GD built-in font.
	*
	* @param int    $fs   Font size (pt for TTF; maps to GD size 1-5 as fallback)
	* @param string $txt  Text to render
	* @param int    $x    Left pixel
	* @param int    $y    Baseline pixel (TTF) or top (GD)
	* @param string $ckey Key in $this->c[] color palette
	*/
	private function _text($fs, $txt, $x, $y, $ckey) {
		$txt = $this->_encodeText($txt);
		$col = isset($this->c[$ckey]) ? $this->c[$ckey] : $this->c['text'];
		if ($this->fontOk) {
			imagettftext($this->img, $fs, 0, (int)$x, (int)$y, $col, $this->fontfile, $txt);
		} else {
			$gd = $fs >= 11 ? 5 : ($fs >= 9 ? 4 : ($fs >= 8 ? 3 : 2));
			imagestring($this->img, $gd, (int)$x, (int)($y - imagefontheight($gd)), $txt, $col);
		}
	}
	/** Approximate rendered text width in pixels */
	private function _textWidth($fs, $txt) {
		$txt = $this->_encodeText($txt);
		if ($this->fontOk) {
			$box = imagettfbbox($fs, 0, $this->fontfile, $txt);
			return max(1, abs($box[2] - $box[0]));
		}
		$gd = $fs >= 11 ? 5 : ($fs >= 9 ? 4 : ($fs >= 8 ? 3 : 2));
		return imagefontwidth($gd) * strlen($txt);
	}
	private function _textHeight($fs) {
		if ($this->fontOk) {
			$box = imagettfbbox($fs, 0, $this->fontfile, 'Ag');
			return max(1, abs($box[1] - $box[7]));
		}
		$gd = $fs >= 11 ? 5 : ($fs >= 9 ? 4 : ($fs >= 8 ? 3 : 2));
		return imagefontheight($gd);
	}
	/** Convert text to UTF-8 and apply RTL shaping if needed */
	private function _encodeText($txt) {
		if (!empty($this->encoding) && $this->encoding !== 'UTF-8') {
			if (function_exists('iconv'))
				$txt = (string)@iconv($this->encoding, 'UTF-8//TRANSLIT', $txt);
			elseif (function_exists('mb_convert_encoding'))
				$txt = (string)@mb_convert_encoding($txt, 'UTF-8', $this->encoding);
		}
		if (function_exists('is_arabic') && is_arabic($txt))
			$txt = arabic($txt, 'we');
		elseif (function_exists('is_hebrew') && is_hebrew($txt))
			$txt = hebrew($txt);
		return $txt;
	}
	/* ================================================================
	*  Numeric formatting
	* ================================================================ */
	private function _fmt($v) {
		if ($this->latin_notation)
			return number_format((float)$v, (int)$this->dec1, ',', '.');
		return number_format((float)$v, (int)$this->dec1, '.', ',');
	}
	private function _truncate($s, $maxChars) {
		$len = function_exists('mb_strlen') ? mb_strlen($s) : strlen($s);
		if ($len <= $maxChars) return $s;
		return (function_exists('mb_substr') ? mb_substr($s, 0, $maxChars-1) : substr($s, 0, $maxChars-1)) . '...';
	}
	/* ================================================================
	*  Legacy compatibility shims
	* ================================================================ */
	public function number_formated($number, $dec_size = 1) {
		$old = $this->dec1; $this->dec1 = (int)$dec_size;
		$r   = $this->_fmt($number);
		$this->dec1 = $old;
		return $r;
	}
	public function number_float($number) {
		if ($this->latin_notation) $number = str_replace('.', '', $number);
		return (float)str_replace(',', '', $number);
	}
	public function string_width($string, $size) {
		$ptSize = $size <= 3 ? 8 : ($size <= 4 ? 10 : 12);
		return $this->_textWidth($ptSize, $string);
	}
	public function string_height($size) {
		return $size <= 1 ? 8 : ($size <= 3 ? 12 : 14);
	}
	public function calculate_higher_value() {
		$this->higher_value     = $this->_niceMax($this->biggest_y);
		$this->highest_value    = $this->higher_value;
		$this->higher_value_str = $this->_fmt($this->higher_value);
	}
	/** @deprecated No-op; kept for API compatibility. */
	public function draw_credits() {}
}


