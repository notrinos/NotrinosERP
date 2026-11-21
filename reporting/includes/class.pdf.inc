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
	This class is an extension to the tcpdf class using a syntax that the original reports were written in
	(the R &OS pdf.php class) - due to easily portation this wrapper class
	was written to allow the same code base to use the more functional tcpdf.class by Nicola Asuni

*	Wrapper for use R&OSpdf API with tcpdf.org class
*/

include_once(dirname(__FILE__).'/tcpdf.php');

class Cpdf extends \setasign\Fpdi\Tcpdf\Fpdi {

	/**
	 * Left and right cell padding in points, kept for reports that read it.
	 * @var float
	 */
	public $cMargin = 0;


	function __construct($pageSize='A4', $lang=null, $pageOrientation='P') {
		parent::__construct($pageOrientation, 'pt', $pageSize);//, $uni, $enc);
		$this->tcpdflink = false; // no library credit line on the last page
		$this->SetLang($lang);
		$this->setPrintHeader(false);
		$this->setPrintFooter(false);
		$this->setPDFVersion("1.3");
		$this->setAutoPageBreak(0);
		$this->SetLineWidth(1);
		$this->SetCellPadding(0);
	}

	/**
	 * Sets the left and right cell padding (the report "cell margin").
	 *
	 * @param float $pad Padding in points.
	 * @return void
	 */
	function SetCellPadding($pad) {
		$this->cMargin = $pad;
		$this->setCellPaddings($pad, 0, $pad, 0);
	}

	/**
	 * Kept for reports written for the old library; the total page count alias
	 * returned by getAliasNbPages() is always replaced on output.
	 *
	 * @return void
	 */
	function AliasNbPages() {
	}
	/*
		Set lamguage for next report
	*/
	function SetLang($code=null) {
		global $installed_languages, $dflt_lang, $path_to_root, $local_path_to_root, $GetText;

		if (!$code)
			$code = $dflt_lang;
		else
			install_hooks($code);

		$lang = array_search_value($code, $installed_languages, 'code');
		$GetText->set_language($lang['code'], strtoupper($lang['encoding']));

		// $local_path_to_root is  set inside find_custom_file.
		// Select extension domain if po file is provided
		// otherwise use global translation.
		if (file_exists($local_path_to_root.'/lang/'.$lang['code'].'/LC_MESSAGES/'.$lang['code'].'.po'))
			$GetText->add_domain($lang['code'], $local_path_to_root . "/lang");
		else
			$GetText->add_domain($lang['code'], $path_to_root . "/lang", @$lang['version']);
		// re-read translated sys names.
		include($path_to_root.'/includes/sysnames.inc');

		$l = array('a_meta_charset' => strtoupper($lang['encoding']), 
			'a_meta_dir' => @$lang['rtl'] ? 'rtl' : 'ltr',
			'a_meta_language' => $code, 'w_page' => 'page');

		if (!isset($l['a_meta_charset']))
			$l = array('a_meta_charset' => 'ISO-8859-1', 'a_meta_dir' => 'ltr', 'a_meta_language' => 'en_GB', 'w_page' => 'page');
		$enc = $l['a_meta_charset'];
		$uni = ($enc == 'UTF-8' || $enc == 'GB2312' ? true : false);
		if ($uni)
			$this->raiseMemoryLimit(48 * 1024 * 1024);

		$this->isunicode = $uni;
		$this->setLanguageArray($l);
		$this->encoding = strtoupper($lang['encoding']);

		$this->FontFamily = 'helvetica';
		$this->FontStyle = '';
		$this->FontSizePt = 12;
	}


	// $fontname should be a standard PDF font (like 'times', 'helvetica' or 'courier')
	// or one that's been installed on your system.  An empty string can also be used
	// which will retain the font currently in use.
	// $style is either:
	//   * a special case string:
	//     * bold
	//     * italic
	//   * or a case-insensitive string where each char represents a style choice
	// and you can use more than one or none at all.  Possible choices:
	//      * empty string: regular
	//      * B: bold
	//      * I: italic
	//      * U: underline
	//      * D: line trough (aka "strike through")
	function selectFont($fontname, $style = '') {
		// Parse the style - check for special cases, otherwise leave as-is
		if ($style == 'italic')
			$style = 'i';
		elseif ($style == 'bold')
			$style = 'b';

		// Parse the fontname
		if ($fontname != '')
			$fontname = basename($fontname);
		if ($fontname == '') {
			if ($this->isunicode) {
				switch ($this->l['a_meta_language']) {
					case "ar_EG" : 	$fontname = "ae_tholoth"; 	break;
					case "zh_CN" : 	$fontname = "gbsn00lp"; 	break;
					case "zh_TW" : 	$fontname = "chinese_traditional_cid0"; 	break;
					default :      	$fontname = "dejavusans"; 		break;
				}
			}
			elseif ($this->encoding === "ISO-8859-2") {
				switch ($this->l['a_meta_language']) {
					default :		$fontname = "freesans"; 	break;
				}
			}
			elseif ($this->encoding === "ISO-8859-5") {
				switch ($this->l['a_meta_language']) {
					default :		$fontname = "freesans5"; 	break;
				}
			}
			elseif ($this->encoding === "ISO-8859-13") {
				switch ($this->l['a_meta_language']) {
					default :        $fontname = "freesans13";     break;
				}
			}
			// else use built-in adobe fonts helvetica.
		}
		$this->SetFont($fontname, $style);
	}

	/**
	 * Raises the PHP memory limit for unicode reports; never lowers it.
	 *
	 * @param int $bytes Minimum memory limit in bytes.
	 * @return void
	 */
	function raiseMemoryLimit($bytes) {
		$current = trim((string) ini_get('memory_limit'));
		if ($current === '' || $current === '-1')
			return;
		$value = (int) $current;
		switch (strtolower(substr($current, -1))) {
			case 'g': $value *= 1024;
			case 'm': $value *= 1024;
			case 'k': $value *= 1024;
		}
		if ($value < $bytes)
			ini_set('memory_limit', (string) $bytes);
	}

	function Header1() {
	}

	function Footer() {
	}

	function newPage() {
		parent::AddPage();
	}

	function line($x1, $y1, $x2, $y2, $style = array()) {
		parent::Line($x1, $this->h-$y1, $x2, $this->h-$y2, $style);
	}

	function rectangle($x, $y, $w, $h, $style='', $border_style=array(), $fill_color=array()) {
		parent::Rect($x, $this->h-$y, $w, $h, $style, $border_style, $fill_color);
	}


	function addText($xb, $yb, $size, $txt) {
		if ($this->isunicode && $this->encoding != 'UTF-8')
			$txt = iconv($this->encoding, 'UTF-8', $txt);
		$this->SetFontSize($size);
		$this->Text($xb, $this->h-$yb, $txt);
	}

	function addInfo($label, $value) {
		if (in_array($label, array( 'Title', 'Subject', 'Author', 'Creator'))) {
			$seter = "Set{$label}";
			$this->$seter($value);
		}
	}

	function addJpegFromFile($img, $x, $y, $w=0, $h=0) {
		$this->Image($img, $x, $this->h-$y-$h, $w, $h);
	}

	function addPngFromFile($img, $x, $y, $w=0, $h=0) {
		$this->Image($img, $x, $this->h-$y-$h, $w, $h);
	}
	/*
	* Next Two functions are adopted from R&OS pdf class
	*/

	/*
	* draw a part of an ellipse
	*/
	function partEllipse($x0, $y0, $astart, $afinish, $r1, $r2=0, $angle=0, $nSeg=8) {
		$this->ellipse($x0, $y0, $r1, $r2, $angle, $nSeg, $astart, $afinish, 0);
	}

	/*
	* draw an ellipse
	* note that the part and filled ellipse are just special cases of this function
	*
	* draws an ellipse in the current line style
	* centered at $x0,$y0, radii $r1,$r2
	* if $r2 is not set, then a circle is drawn
	* nSeg is not allowed to be less than 2, as this will simply draw a line (and will even draw a
	* pretty crappy shape at 2, as we are approximating with bezier curves.
	*/
	function ellipse($x0, $y0, $r1, $r2=0, $angle=0, $nSeg=8, $astart=0, $afinish=360, $close=1, $fill=0, $dummy=null) {
		parent::Ellipse($x0, $y0, $r1, $r2, $angle, $astart, $afinish, ($close ? 'C' : ''), array(), array(), $nSeg);
	}

	function Stream($fname='') {
		parent::Output($fname, 'I');
	}

	function calcTextWrap($txt, $width, $spacebreak=false) {
		$ret = "";
		$txt2 = $txt;
		$w = $this->GetStringWidth($txt);
		if ($w > $width && $w > 0 && $width != 0) {
			$n = strlen($txt);
			$k = intval($n * $width / $w);
			if ($k > 0 && $k < $n) {
				$txt2 = substr($txt, 0, $k);
				if ($spacebreak && (($pos = strrpos($txt2, " ")) !== false)) {
					$txt2 = substr($txt2, 0, $pos);
					$ret = substr($txt, $pos+1);
				}
				else
					$ret = substr($txt, $k);
			}
		}
		return array($txt2, $ret);
	}

	function addTextWrap($xb, $yb, $w, $h, $txt, $align='left', $border=0, $fill=0, $link=NULL, $stretch=1, $spacebreak=false) {
		$ret = "";
		if (!$this->rtl) {
			if ($align == 'right')
				$align = 'R';
			elseif ($align == 'left')
				$align = 'L';
			elseif ($align == 'center')
				$align = 'C';
			elseif ($align == 'justify')
				$align = 'J';
		}
		else {
			if ($align == 'right')
				$align = 'R';// This may need to be 'L'
			elseif ($align == 'left')
				$align = 'R';
			elseif ($align == 'center')
				$align = 'C';
			elseif ($align == 'justify')
				$align = 'J';
		}

		$txt = parent::unhtmlentities($txt);
		// If horizontal scaling was requested, check to see if we're trying to scale
		// too much.  If so, cut back string first and then scale it.
		$maxScaleFactor = 1.4;
		if ($stretch == 1 || $stretch == 2)
			$txt = $this->calcTextWrap($txt, $w * $maxScaleFactor, $spacebreak);
		// Wrap text if stretching isn't turned on
		else
			$txt = $this->calcTextWrap($txt, $w, $spacebreak);
		$ret = $txt[1];
		$txt = $txt[0];
		$this->SetXY($xb, $this->h - $yb - $h);

		if ($this->isunicode && $this->encoding != "UTF-8")
			$txt = iconv($this->encoding, "UTF-8", $txt);
		$this->Cell($w, $h, $txt, $border, 0, $align, $fill, $link, $stretch);
		return $ret;
	}

	function Text($x, $y, $txt, $stroke=0, $clip=false, $ffill=true, $border=0, $ln=0, $align='', $fill=false, $link='', $stretch=0, $ignore_min_height=false, $calign='T', $valign='M', $rtloff=false) {
		parent::Text($x, $y, parent::unhtmlentities($txt), $stroke, $clip, $ffill, $border, $ln, $align, $fill, $link, $stretch, $ignore_min_height, $calign, $valign, $rtloff);
	}
}
