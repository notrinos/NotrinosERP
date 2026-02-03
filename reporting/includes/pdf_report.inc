<?php
/**********************************************************************
	Copyright (C) FrontAccounting, LLC.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
/*
	TODO:
	. add StartReport/EndReport handlers for better bulk report support, with
	. email/printer destination option should be selected on class creation instead
	 of End()
	. add/use setter function for Header2 parameters (currently passed globally)
	. in report files pass already prepared options to SetCommonData() to avoid need for
	 selection inside FrontReport generic class.
*/
include_once(dirname(__FILE__).'/class.pdf.inc');
include_once(dirname(__FILE__).'/printer_class.inc');
include_once($path_to_root . '/reporting/includes/reporting.inc');
include_once($path_to_root . '/admin/db/company_db.inc');
include_once($path_to_root . '/admin/db/fiscalyears_db.inc');
include_once($path_to_root . '/admin/db/printers_db.inc');
include_once($path_to_root . '/includes/ui/ui_globals.inc');
include_once($path_to_root . '/config.php');

class FrontReport extends Cpdf {
	var $size;
	var $company;
	var $user;
	var $host;
	var $fiscal_year;
	var $title;
	var $filename;
	var $pageWidth;
	var $pageHeight;
	var $topMargin;
	var $bottomMargin;
	var $leftMargin;
	var $rightMargin;
	var $endLine;
	var $lineHeight;
	//var $rtl;

	var $row;
	var $cols;
	var $params;
	var $headers;
	var $aligns;
	var $headers2;
	var $aligns2;
	var $cols2;
	var $pageNumber;
	var $fontSize;
	var $oldFontSize;
	var $currency;
	var $companyLogoEnable;  // select whether to use a company logo graphic in some header templates
	var $scaleLogoWidth;
	var $footerEnable;  // select whether to print a page footer or not
	var $footerText;  // store user-generated footer text
	var $headerTmpl;  // store the name of the currently selected header template
	var $tmplSize; // pdf header template size in pages

	var $rep_id;
	var $formData; // common data used for printing headers footers etc.
	var $contactData; // contact data for sending emials/reportlanguage selection
	
	var $dest;	// destination: email or printer
	
	function __construct($title, $filename, $size = 'A4', $fontsize = 9, $orientation = 'P', $margins = NULL, $excelColWidthFactor = NULL) {
		global $page_security;

		$this->rep_id = $_POST['REP_ID'];	// FIXME
		
		if (!$_SESSION['wa_current_user']->can_access_page($page_security)) {
			display_error(_('The security settings on your account do not permit you to print this report'));
			end_page();
			exit;
		}
		// Page margins - if user-specified, use those.  Otherwise, use defaults below.
		if (isset($margins)) {
			$this->topMargin = $margins['top'];
			$this->bottomMargin = $margins['bottom'];
			$this->leftMargin = $margins['left'];
			$this->rightMargin = $margins['right'];
		}
		// Page orientation - P: portrait, L: landscape
		$orientation = strtoupper($orientation);
		// Page size name
		switch (strtoupper($size)) {
			default:
			case 'A4':
				// Portrait
				if ($orientation == 'P') {
					$this->pageWidth=595;
					$this->pageHeight=842;
					if (!isset($margins)) {
						$this->topMargin=40;
						$this->bottomMargin=30;
						$this->leftMargin=40;
						$this->rightMargin=30;
					}
				}
				else { // Landscape
					$this->pageWidth=842;
					$this->pageHeight=595;
					if (!isset($margins)) {
						$this->topMargin=30;
						$this->bottomMargin=30;
						$this->leftMargin=40;
						$this->rightMargin=30;
					}
			  }
			  break;
			case 'A3':
				// Portrait
				if ($orientation == 'P') {
					$this->pageWidth=842;
					$this->pageHeight=1190;
					if (!isset($margins)) {
						$this->topMargin=50;
						$this->bottomMargin=50;
						$this->leftMargin=50;
						$this->rightMargin=40;
					}
				}
				else { // Landscape
					$this->pageWidth=1190;
					$this->pageHeight=842;
					if (!isset($margins)) {
						$this->topMargin=50;
						$this->bottomMargin=50;
						$this->leftMargin=50;
						$this->rightMargin=40;
					}
				}
				break;
			case 'LETTER':
				// Portrait
				if ($orientation == 'P') {
					$this->pageWidth=612;
					$this->pageHeight=792;
					if (!isset($margins)) {
						$this->topMargin=30;
						$this->bottomMargin=30;
						$this->leftMargin=30;
						$this->rightMargin=25;
					}
				}
				else {// Landscape
					$this->pageWidth=792;
				  $this->pageHeight=612;
					if (!isset($margins)) {
						$this->topMargin=30;
						$this->bottomMargin=30;
						$this->leftMargin=30;
						$this->rightMargin=25;
					}
				}
				break;
			case 'LEGAL':
				// Portrait
				if ($orientation == 'P') {
					$this->pageWidth=612;
					$this->pageHeight=1008;
					if (!isset($margins)) {
						$this->topMargin=50;
						$this->bottomMargin=40;
						$this->leftMargin=30;
						$this->rightMargin=25;
					}
				}
				else {// Landscape
					$this->pageWidth=1008;
					$this->pageHeight=612;
					if (!isset($margins)) {
						$this->topMargin=50;
						$this->bottomMargin=40;
						$this->leftMargin=30;
						$this->rightMargin=25;
					}
				}
				break;
		}
		$this->size = array(0, 0, $this->pageWidth, $this->pageHeight);
		$this->title = $title;
		$this->filename = $filename.'.pdf';
		$this->pageNumber = 0;
		$this->endLine = $this->pageWidth - $this->rightMargin;
		$this->lineHeight = 12;
		$this->fontSize = $fontsize;
		$this->oldFontSize = 0;
		$this->row = $this->pageHeight - $this->topMargin;
		$this->currency = '';
		$this->scaleLogoWidth = false; // if Logo, scale on width (else height).
		$this->SetHeaderType('Header'); // default

		parent::__construct($size, $_SESSION['language']->code, $orientation);
	}
	
	/*
	 * Select the font and style to use for following output until
	 * it's changed again.
	 * 
	 * $style is either:
	 *   * a special case string (for backwards compatible with older code):
	 *     * bold
	 *     * italic
	 *   * or a case-insensitive string where each char represents a style choice
	 *     and you can use more than one or none at all.  Possible choices:
	 *      * empty string: regular
	 *      * B: bold
	 *      * I: italic
	 *      * U: underline
	 *      * D: line trough (aka 'strike through')
	 * $fontname should be a standard PDF font (like 'times', 'helvetica' or 'courier')
	 *   or one that's been installed on your system (see TCPDF docs for details).
	 *   An empty string can also be used which will retain the font currently in use if
	 *   you just want to change the style.
	 */
	function Font($style = '', $fontname = '') {
		$this->selectFont($fontname, $style);
	}

	function Info($params, $cols, $headers, $aligns, $cols2 = null, $headers2 = null, $aligns2 = null, $companylogoenable = false, $footerenable = false, $footertext = '') {
		global $SysPrefs, $version;

		$this->addInfo('Title', $this->title);
		$this->addInfo('Subject', $this->title);
		$this->addInfo('Author', $SysPrefs->app_title . ' ' . $version);
		$this->addInfo('Creator',$SysPrefs->power_by . ' - ' . $SysPrefs->power_url);
		$year = get_current_fiscalyear();
		if ($year['closed'] == 0)
			$how = _('Active');
		else
			$how = _('Closed');
		$this->fiscal_year = sql2date($year['begin']) . ' - ' . sql2date($year['end']) . '  ' . '(' . $how . ')';
		$this->company = get_company_prefs();
		$this->user = $_SESSION['wa_current_user']->name;
		$this->host = $_SERVER['SERVER_NAME'];
		$this->params = $params;
		$this->cols = $cols;
		for ($i = 0; $i < count($this->cols); $i++)
			$this->cols[$i] += $this->leftMargin;
		$this->headers = $headers;
		$this->aligns = $aligns;
		$this->cols2 = $cols2;
		if ($this->cols2 != null) {
			for ($i = 0; $i < count($this->cols2); $i++)
				$this->cols2[$i] += $this->leftMargin;
		}
		$this->headers2 = $headers2;
		$this->aligns2 = $aligns2;

		// Set whether to display company logo in some header templates
		$this->companyLogoEnable = $companylogoenable;
		
		// Store footer settings
		$this->footerEnable = $footerenable;
		$this->footerText = $footertext;	
	}
	//
	//	Header for listings
	//
	function Header() {
		global $SysPrefs;
		
		$companyCol = $this->endLine - 150;
		$titleCol = $this->leftMargin + 100;

		$this->row = $this->pageHeight - $this->topMargin;

		$this->SetDrawColor(128, 128, 128);
		$this->Line($this->row + 5, 1);

		$this->NewLine();

		$this->fontSize += 4;
		$this->Font('bold');
		$this->Text($this->leftMargin, $this->title, $companyCol);
		$this->Font();
		$this->fontSize -= 4;
		$logo = company_path() . '/images/' . $this->company['coy_logo'];
		if (!empty($SysPrefs->prefs['company_logo_report']) && $this->company['coy_logo'] != '' && file_exists($logo)) {
			$size = getimagesize($logo);
			$height = $size[0] > 150 ? $size[1] * 150 / $size[0] : 30; 
			$this->row -= ($height / 2);
			$this->AddImage($logo, $companyCol, $this->row, 0, $height);
			$this->row -= 6;
		}
		else {
			$this->Text($companyCol, $this->company['coy_name']);
			$this->row -= ($this->lineHeight + 4);
		}
		$str = _('Print Out Date') . ':';
		$this->Text($this->leftMargin, $str, $titleCol);
		$str = Today() . '   ' . Now();
		if ($this->company['time_zone'])
			$str .= ' ' . date('O') . ' GMT';
		$this->Text($titleCol, $str, $companyCol);
		$this->Text($companyCol, $this->host);

		$this->NewLine();
		$str = _('Fiscal Year') . ':';
		$this->Text($this->leftMargin, $str, $titleCol);
		$str = $this->fiscal_year;
		$this->Text($titleCol, $str, $companyCol);
		$this->Text($companyCol, $this->user);
		for ($i = 1; $i < count_array($this->params); $i++) {
			if ($this->params[$i]['from'] != '') {
				$this->NewLine();
				$str = $this->params[$i]['text'] . ':';
				$this->Text($this->leftMargin, $str, $titleCol);
				$str = $this->params[$i]['from'];
				if ($this->params[$i]['to'] != '')
					$str .= ' - ' . $this->params[$i]['to'];
				$this->Text($titleCol, $str, $companyCol);
			}
		}
		if ($this->params[0] != '') {// Comments
			$this->NewLine();
			$str = _('Comments') . ':';
			$this->Text($this->leftMargin, $str, $titleCol);
			$this->Font('bold');
			$this->Text($titleCol, $this->params[0], $this->endLine - 35);
			$this->Font();
		}
		$str = _('Page') . ' ' . $this->pageNumber;
		$this->Text($this->endLine - 38, $str);
		$this->Line($this->row - 5, 1);

		$this->row -= ($this->lineHeight + 6);
		$this->Font('italic');
		if ($this->headers2 != null) {
			$count = count_array($this->headers2);
			for ($i = 0; $i < $count; $i++)
				$this->TextCol2($i, $i + 1,	$this->headers2[$i]);
			$this->NewLine();
		}
		$count = count_array($this->headers);
		for ($i = 0; $i < $count; $i++)
			$this->TextCol($i, $i + 1, $this->headers[$i]);
		$this->Font();
		$this->Line($this->row - 5, 1);

		$this->NewLine(2);
	}
	/*
		Transition function 
	*/
	function SetCommonData($myrow, $branch, $sales_order, $bankaccount, $doctype, $contacts) {
		$this->pageNumber = 0;
		$this->formData = array();
		$datnames = array( 
		'myrow' => array('ord_date', 'date_', 'tran_date', 
			'order_no','reference', 'id', 'trans_no', 'name', 'location_name',
			'delivery_address', 'supp_name', 'address',
			'DebtorName', 'supp_account_no', 'wo_ref', 'debtor_ref','type', 'trans_no', 
			'StockItemName', 'tax_id', 'order_', 'delivery_date', 'units_issued',
			'due_date', 'required_by', 'payment_terms', 'curr_code',
			'ov_freight', 'ov_gst', 'ov_amount', 'prepaid', 'requisition_no', 'contact'),
		'branch' => array('br_address', 'br_name', 'salesman', 'disable_branch'),
		'sales_order' => array('deliver_to', 'delivery_address', 'customer_ref'),
		'bankaccount' => array('bank_name', 'bank_account_number', 'payment_service')
		);

		foreach($datnames as $var => $fields) {
			if (isset($$var)) {
				foreach($fields as $locname) {
					if (isset(${$var}[$locname]) && (${$var}[$locname]!==null))
						$this->formData[$locname] = ${$var}[$locname];
				}
			}
		}
		$this->formData['doctype'] = $doctype;
		$this->formData['document_amount'] = @$this->formData['ov_amount']+@$this->formData['ov_freight']+@$this->formData['ov_gst'];
		if (count($contacts)) {
			if (!is_array($contacts[0]))
				$contacts = array($contacts); // change to array when single contact passed
			$this->contactData = $contacts;
			// as report is currently generated once despite number of email recipients
			// we select language for the first recipient as report language
			$this->formData['rep_lang'] = $contacts[0]['lang'];
		}
	}
	/*
		Set header handler
	*/
	function SetHeaderType($name) {
		$this->headerTmpl = $name;
	}
	/*
		Header for sales/purchase documents
	*/
	function Header2() {
		global $dflt_lang; // FIXME should be passed as params

		$this->SetLang(@$this->formData['rep_lang'] ? $this->formData['rep_lang'] : $dflt_lang);
		$doctype = $this->formData['doctype'];
		$header2type = true;

		$lang = user_language();
		$this->SetLang(@$this->formData['rep_lang'] ? $this->formData['rep_lang'] : ( $lang ? $lang : $dflt_lang));

		 // leave layout files names without path to enable including
		 // modified versions from company/x/reporting directory
		include('includes/doctext.inc');
		include('includes/header2.inc');

		$this->row = $temp;
	}

	// Alternate header style which also supports a simple footer
	function Header3() {
		// Turn off cell padding for the main report header, restoring the current setting later
		$oldcMargin = $this->cMargin;
		$this->SetCellPadding(0);

		// Set some constants which control header item layout
		// only set them once or the PHP interpreter gets angry
		if ($this->pageNumber == 1) {
			define('COMPANY_WIDTH', 150);
			define('LOGO_HEIGHT', 50);
			define('LOGO_Y_POS_ADJ_FACTOR', 0.74);
			define('LABEL_WIDTH', 80);
			define('PAGE_NUM_WIDTH', 60);
			define('TITLE_FONT_SIZE', 14);
			define('HEADER1_FONT_SIZE', 10);
			define('HEADER2_FONT_SIZE', 9);
			define('FOOTER_FONT_SIZE', 10);
			define('FOOTER_MARGIN', 4);
		}
		// Set some variables which control header item layout
		$companyCol = $this->endLine - COMPANY_WIDTH;
		$headerFieldCol = $this->leftMargin + LABEL_WIDTH;
		$pageNumCol = $this->endLine - PAGE_NUM_WIDTH;
		$footerCol = $this->leftMargin + PAGE_NUM_WIDTH; 
		$footerRow = $this->bottomMargin - FOOTER_MARGIN;

		$this->row = $this->pageHeight - $this->topMargin;

		// Set the color of dividing lines we'll draw
		$oldDrawColor = $this->GetDrawColor();
		$this->SetDrawColor(128, 128, 128);

		// Tell TCPDF that we want to use its alias system to track the total number of pages
		$this->AliasNbPages();
		
		// Footer
		if ($this->footerEnable) {
			$this->Line($footerRow, 1);
			$prevFontSize = $this->fontSize;
			$this->fontSize = FOOTER_FONT_SIZE;
			$this->TextWrap($footerCol, $footerRow - ($this->fontSize + 1), $pageNumCol - $footerCol, $this->footerText, $align = 'center', $border = 0, $fill = 0, $link = NULL, $stretch = 1);
			$this->TextWrap($pageNumCol, $footerRow - ($this->fontSize + 1), PAGE_NUM_WIDTH, _('Page') . ' ' . $this->pageNumber . '/' . $this->getAliasNbPages(), $align = 'right', $border = 0, $fill = 0, $link = NULL, $stretch = 1);
			$this->fontSize = $prevFontSize;
		}

		//
		// Header
		//
		
		// Print gray line across the page
		$this->Line($this->row + 8, 1);

		$this->NewLine();

		// Print the report title nice and big
		$oldFontSize = $this->fontSize;
		$this->fontSize = TITLE_FONT_SIZE;
		$this->Font('B');
		$this->Text($this->leftMargin, $this->title, $companyCol);
		$this->fontSize = HEADER1_FONT_SIZE;

		// Print company logo if present and requested, or else just print company name
		// Build a string specifying the location of the company logo file
		$logo = company_path() . '/images/' . $this->company['coy_logo'];
		if ($this->companyLogoEnable && ($this->company['coy_logo'] != '') && file_exists($logo)) {
			// Width being zero means that the image will be scaled to the specified height
			// keeping its aspect ratio intact.
			if ($this->scaleLogoWidth)
				$this->AddImage($logo, $companyCol, $this->row + 15, COMPANY_WIDTH, 0);
			else	
				$this->AddImage($logo, $companyCol, $this->row - (LOGO_HEIGHT * LOGO_Y_POS_ADJ_FACTOR), 0, LOGO_HEIGHT);
		}
		else
			$this->Text($companyCol, $this->company['coy_name']);

		// Dimension 1 - optional
		// - only print if available and not blank
		if (count($this->params) > 3)
			if ($this->params[3]['from'] != '') {
				$this->NewLine(1, 0, $this->fontSize + 2);
				$str = $this->params[3]['text'] . ':';
				$this->Text($this->leftMargin, $str, $headerFieldCol);
				$str = $this->params[3]['from'];
				$this->Text($headerFieldCol, $str, $companyCol);
			}

		// Dimension 2 - optional
		// - only print if available and not blank
		if (count($this->params) > 4)
			if ($this->params[4]['from'] != '') {
				$this->NewLine(1, 0, $this->fontSize + 2);
				$str = $this->params[4]['text'] . ':';
				$this->Text($this->leftMargin, $str, $headerFieldCol);
				$str = $this->params[4]['from'];
				$this->Text($headerFieldCol, $str, $companyCol);
			}

		// Tags - optional
		// if present, it's an array of tag names
		if (count($this->params) > 5)
			if ($this->params[5]['from'] != '') {
				$this->NewLine(1, 0, $this->fontSize + 2);
				$str = $this->params[5]['text'] . ':';
				$this->Text($this->leftMargin, $str, $headerFieldCol);
				$str = '';
				for ($i = 0; $i < count($this->params[5]['from']); $i++) {
					if($i != 0)
						$str .= ', ';
					$str .= $this->params[5]['from'][$i];
				}
				$this->Text($headerFieldCol, $str, $companyCol);
			}

		// Report Date - time period covered
		// - can specify a range, or just the end date (and the report contents
		//   should make it obvious what the beginning date is)
		$this->NewLine(1, 0, $this->fontSize + 2);
		$str = _('Report Period') . ':';
		$this->Text($this->leftMargin, $str, $headerFieldCol);
		$str = '';
		if (isset($this->params[1]['from']) && $this->params[1]['from'] != '')
			$str = $this->params[1]['from'] . ' - ';
		$str .= $this->params[1]['to'];
		$this->Text($headerFieldCol, $str, $companyCol);

		// Turn off Bold
		$this->Font();
		
		$this->NewLine(1, 0, $this->fontSize + 1);

		// Make the remaining report headings a little less important
		$this->fontSize = HEADER2_FONT_SIZE;

		// Timestamp of when this copy of the report was generated
		$str = _('Generated At') . ':';
		$this->Text($this->leftMargin, $str, $headerFieldCol);
		$str = Today() . '   ' . Now();
		if ($this->company['time_zone'])
			$str .= ' ' . date('O') . ' GMT';
		$this->Text($headerFieldCol, $str, $companyCol);

		// Name of the user that generated this copy of the report
		$this->NewLine(1, 0, $this->fontSize + 1);
		$str = _('Generated By') . ':';
		$this->Text($this->leftMargin, $str, $headerFieldCol);
		$str = $this->user;
		$this->Text($headerFieldCol, $str, $companyCol);

		// Display any user-generated comments for this copy of the report
		if ($this->params[0] != '') {// Comments
			$this->NewLine(1, 0, $this->fontSize + 1);
			$str = _('Comments') . ':';
			$this->Text($this->leftMargin, $str, $headerFieldCol);
			$this->Font('B');
			$this->Text($headerFieldCol, $this->params[0], $companyCol, 0, 0, 'left', 0, 0, $link=NULL, 1);
			$this->Font();
		}

		// Add page numbering to header if footer is turned off
		if (!$this->footerEnable) {
			$str = _('Page') . ' ' . $this->pageNumber . '/' . $this->getAliasNbPages();
			$this->Text($pageNumCol, $str, 0, 0, 0, 'right', 0, 0, NULL, 1);
		}
		
		// Print gray line across the page
		$this->Line($this->row - 5, 1);

		// Restore font size to user-defined size
		$this->fontSize = $oldFontSize;

		// restore user-specified cell padding for column headers
		$this->SetCellPadding($oldcMargin);

		// scoot down the page a bit
		$oldLineHeight = $this->lineHeight;
		$this->lineHeight = $this->fontSize + 1;
		$this->row -= ($this->lineHeight + 6);
		$this->lineHeight = $oldLineHeight;

		// Print the column headers!
		$this->Font('I');
		if ($this->headers2 != null) {
			$count = count($this->headers2);
			for ($i = 0; $i < $count; $i++)
				$this->TextCol2($i, $i + 1,	$this->headers2[$i], $corr=0, $r=0, $border=0, $fill=0, $link=NULL, $stretch=1);
			$this->NewLine();
		}
		$count = count($this->headers);
		for ($i = 0; $i < $count; $i++)
			$this->TextCol($i, $i + 1, $this->headers[$i], $corr=0, $r=0, $border=0, $fill=0, $link=NULL, $stretch=1);
		$this->Font();

		$this->NewLine(2);

		// restore user-specified draw color
		$this->SetDrawColor($oldDrawColor[0], $oldDrawColor[1], $oldDrawColor[2]);		
	}

	/**
	 * Format a numeric string date into something nicer looking.
	 *
	 * @param string $date Date string to be formatted.
	 * @param int $input_format Format of the input string.  Possible values are:<ul><li>0: user's default (default)</li></ul>
	 * @param int $output_format Format of the output string.  Possible values are:<ul><li>0: Month (word) Day (numeric), 4-digit Year - Example: January 1, 2000 (default)</li><li>1: Month 4-digit Year - Example: January 2000</li><li>2: Month Abbreviation 4-digit Year - Example: Jan 2000</li></ul>
	 * @access public
	 */
	function DatePrettyPrint($date, $input_format = 0, $output_format = 0) {
		if ($date != '') {
			$date = date2sql($date);
			$year = (int) (substr($date, 0, 4));
			$month = (int) (substr($date, 5, 2));
			$day = (int) (substr($date, 8, 2));
			if ($output_format == 0)
				return(date('F j, Y', mktime(12, 0, 0, $month, $day, $year)));
			elseif ($output_format == 1)
				return(date('F Y', mktime(12, 0, 0, $month, $day, $year)));
			elseif ($output_format == 2)
				return(date('M Y', mktime(12, 0, 0, $month, $day, $year)));
		}
		else
			return $date;
	}

	function AddImage($logo, $x, $y, $w, $h) {
		if (strpos($logo, '.png') || strpos($logo, '.PNG'))
			$this->addPngFromFile($logo, $x, $y, $w, $h);
		else
			$this->addJpegFromFile($logo, $x, $y, $w, $h);
	}

	// Get current draw color setting from TCPDF object; returns array of RGB numbers
	function GetDrawColor() {
		// Convert the TCPDF stored DrawColor string into an array of strings
		$colorFields = explode(' ', $this->DrawColor);

		// Test last value: G == grayscale, single number; RG == RGB, 3 numbers
		if ($colorFields[count($colorFields) - 1] == 'G')
			// Convert a grayscale string value to the equivalent RGB value
			$drawColor = array((float) $colorFields[0], (float) $colorFields[0], (float) $colorFields[0]);
		else
			// Convert RGB string values to the a numeric array
			$drawColor = array((float) $colorFields[0], (float) $colorFields[1], (float) $colorFields[2]);
		
		return $drawColor;
	}
	
	// Get current cell padding setting from TCPDF object
	function GetCellPadding() {
		return $this->cMargin;
	}

	// Set desired cell padding (aka 'cell margin')
	// Seems to be just left and right margins...
	function SetCellPadding($pad) {
		parent::SetCellPadding($pad);
	}
	
	function Text($c, $txt, $n=0, $corr=0, $r=0, $align='left', $border=0, $fill=0, $link=NULL, $stretch=1) {
		if ($n == 0)
			$n = $this->pageWidth - $this->rightMargin;

		return $this->TextWrap($c, $this->row - $r, $n - $c + $corr, $txt, $align, $border, $fill, $link, $stretch);
	}

	function TextWrap($xpos, $ypos, $len, $str, $align = 'left', $border = 0, $fill = 0, $link = NULL, $stretch = 1, $spacebreak=false) {
		$str = strtr($str === null ? '' : $str, array("\r"=>''));

		if ($this->fontSize != $this->oldFontSize) {
			$this->SetFontSize($this->fontSize);
			$this->oldFontSize = $this->fontSize;
		}
		return $this->addTextWrap($xpos, $ypos, $len, $this->fontSize, $str, $align, $border, $fill, $link, $stretch, $spacebreak);
	}

	function TextCol($c, $n, $txt, $corr=0, $r=0, $border=0, $fill=0, $link=NULL, $stretch=1) {
		return $this->TextWrap($this->cols[$c], $this->row - $r, $this->cols[$n] - $this->cols[$c] + $corr, $txt, $this->aligns[$c], $border, $fill, $link, $stretch);
	}
	
	function AmountCol($c, $n, $txt, $dec=0, $corr=0, $r=0, $border=0, $fill=0, $link=NULL, $stretch=1, $color_red=false) {
		if ($color_red && $txt < 0)
			$this->SetTextColor(255, 0, 0);
		$ret = $this->TextCol($c, $n, number_format2($txt, $dec), $corr, $r, $border, $fill, $link, $stretch);
		if ($color_red && $txt < 0)
			$this->SetTextColor(0, 0, 0);
		return $ret;	
	}

	function AmountCol2($c, $n, $txt, $dec=0, $corr=0, $r=0, $border=0, $fill=0, $link=NULL, $stretch=1, $color_red=false, $amount_locale = 'en_US.UTF-8', $amount_format = '%(!.2n') {
		setlocale(LC_MONETARY, $amount_locale);
		if ($color_red && $txt < 0)
			$this->SetTextColor(255, 0, 0);
		$ret = $this->TextCol($c, $n, price_format($txt), $corr, $r, $border, $fill, $link, $stretch);
		if ($color_red && $txt < 0)
			$this->SetTextColor(0, 0, 0);
		return $ret;	
	}
	
	function DateCol($c, $n, $txt, $conv=false, $corr=0, $r=0, $border=0, $fill=0, $link=NULL, $stretch=1) {
		if ($conv)
			$txt = sql2date($txt);
		return $this->TextCol($c, $n, $txt, $corr, $r, $border, $fill, $link, $stretch);
	}

	function TextCol2($c, $n, $txt, $corr=0, $r=0, $border=0, $fill=0, $link=NULL, $stretch=1) {
		return $this->TextWrap($this->cols2[$c], $this->row - $r, $this->cols2[$n] - $this->cols2[$c] + $corr, $txt, $this->aligns2[$c], $border, $fill, $link, $stretch);
	}

	function TextColLines($c, $n, $txt, $corr=0, $r=0, $border=0, $fill=0, $link=NULL, $stretch=0) {
		$this->row -= $r;
		$this->TextWrapLines($this->cols[$c], $this->cols[$n] - $this->cols[$c] + $corr, $txt, $this->aligns[$c], $border, $fill, $link, $stretch, true);
	}

	function TextWrapLines($c, $width, $txt, $align='left', $border=0, $fill=0, $link=NULL, $stretch=0, $spacebreak=true) {
		$str = explode("\n", $txt === null ? '' : $txt);

		for ($i = 0; $i < count($str); $i++) {
			$l = $str[$i];
			do {
				$l = $this->TextWrap($c, $this->row , $width, $l, $align, $border, $fill, $link, $stretch, $spacebreak);
				$this->row -= $this->lineHeight;
			}
			while ($l != '');
		}
	}

	/**
	 * Expose the underlying calcTextWrap() function in this API.
	 */
	function TextWrapCalc($txt, $width, $spacebreak=false) {
		return $this->calcTextWrap($txt, $width, $spacebreak);
	}
	
	/**
	 * Sets the line drawing style.
	 * 
	 * Takes an associative array as arg so you don't need to specify all values.
	 * 
	 * Array keys:
	 * width (float) - the thickness of the line in user units
	 * cap (string) - the type of cap to put on the line, values can be 'butt','round','square'
	 *    where the diffference between 'square' and 'butt' is that 'square' projects a flat end past the
	 *    end of the line.
	 * join (string) - can be 'miter', 'round', 'bevel'
	 * dash (mixed) - Dash pattern. Is 0 (without dash) or string with series of length values, which are the
	 *        lengths of the on and off dashes. For example: '2' represents 2 on, 2 off, 2 on, 2 off, ...;
	 *        '2,1' is 2 on, 1 off, 2 on, 1 off, ... 
	 * phase (integer) - a modifier on the dash pattern which is used to shift the point at which the pattern starts.
	 * color (array) - draw color.  Format: array(GREY), or array(R,G,B) or array(C,M,Y,K).
	 */
	function SetLineStyle($style) {
		parent::SetLineStyle($style);
	}

	/**
	 * Sets the line drawing width.
	 */
	function SetLineWidth($width) {
		parent::SetLineWidth($width);
	}
	
	function LineTo($from, $row, $to, $row2) {
		parent::line($from, $row, $to, $row2);
	}

	function Line($row, $height = 0, $dummy1=null, $dummy2=null, $dummy3=null) {
		$oldLineWidth = $this->GetLineWidth();
		$this->SetLineWidth($height + 1);
		parent::line($this->pageWidth - $this->rightMargin, $row ,$this->leftMargin, $row);
		$this->SetLineWidth($oldLineWidth);
	}

	/**
	* Underlines the contents of a cell, but not the cell padding area.
	* Primarily useful for the last line before a 'totals' line.
	* @param int $c Column number to underline.
	* @param int $r Print the underline(s) this number of rows below the current position.  Can be negative in order to go up.
	* @param int $type Type of underlining to draw.  Possible values are:<ul><li>1: single underline (default)</li><li>2: double underline</li></ul>
	* @param int $linewidth Thickness of the line to draw.  Default value of zero will use the current line width defined for this document.
	* @param array $style Line style. Array like for {@link SetLineStyle SetLineStyle}. Default value: default line style (empty array).
	* @access public
	* @see SetLineWidth(), SetDrawColor(), SetLineStyle()
	*/
	function UnderlineCell($c, $r = 0, $type = 1, $linewidth = 0, $style = array()) {
		// If line width was specified, save current setting so we can reset it
		if ($linewidth != 0) {
			$oldLineWidth = $this->GetLineWidth();
			$this->SetLineWidth($linewidth);
		}

		// Figure out how far down to move the line based on current font size
		// Calculate this because printing underline directly at $this->row goes on top
		// of the parts of characters that 'hang down', like the bottom of commas &
		// lowercase letter 'g', etc.
		if ($this->fontSize < 10)
			$y_adj = 2;
		else
			$y_adj = 3; 
		parent::line($this->cols[$c] + $this->cMargin, $this->row - $r - $y_adj, $this->cols[$c + 1] - $this->cMargin, $this->row - $r - $y_adj, $style);

		// Double underline, far enough below the first underline so as not to overlap
		// the first underline (depends on current line thickness (aka 'line width')
		if ($type == 2)
			parent::line($this->cols[$c] + $this->cMargin, $this->row - $r - $y_adj - ($this->GetLineWidth() + 2), $this->cols[$c + 1] - $this->cMargin, $this->row - $r - $y_adj - ($this->GetLineWidth() + 2), $style);

		// If line width was specified, reset it back to the original setting
		if ($linewidth != 0)
			$this->SetLineWidth($oldLineWidth);
	}
	
	function NewLine($l=1, $np=0, $h = NULL) {
		// If the line height wasn't specified, use the current setting
		if ($h == NULL)
			$h = $this->lineHeight;

		// Move one line down the page
		$this->row -= ($l * $h);

		// Check to see if we're at the bottom and should insert a page break
		if ($this->row < $this->bottomMargin + ($np * $h))
			$this->NewPage();
	}

	function NewPage() {
		if ($this->pageNumber==0) {
			// check if there is pdf header template for this report
			// and set if it is found
			$tmpl_pdf = find_custom_file('/reporting/forms/'.$this->headerTmpl.'.pdf');
			if ($tmpl_pdf)
				$this->tmplSize = $this->setSourceFile($tmpl_pdf);
		}

		$this->pageNumber++;
		parent::newPage();

		if ($this->tmplSize) {
			$this->row = $this->pageHeight - $this->topMargin; // reset row
			$id = $this->importPage(min($this->pageNumber, $this->tmplSize));
			$this->useTemplate($id);
		}

		// include related php file if any
		$tmpl_php = find_custom_file('/reporting/forms/'.$this->headerTmpl.'.php');
		if ($tmpl_php)
			include($tmpl_php);

		if (method_exists($this, $this->headerTmpl))	// draw predefined page layout if any
			$this->{$this->headerTmpl}();
	}

	function End($email=0, $subject='') {
		global $SysPrefs, $path_to_root;

		if (!empty($SysPrefs->prefs['print_dialog_direct']))		
			$this->includeJS('print();'); // force to open print dialog

		if ($SysPrefs->pdf_debug == 1) {
			$pdfcode = $this->Output('','S');
			$pdfcode = str_replace("\n", "\n<br>", html_specials_encode($pdfcode));
			echo '<html><body>';
			echo trim($pdfcode);
			echo '</body></html>';
		}
		else {
			$dir =  company_path(). '/pdf_files';
			//save the file
			if (!file_exists($dir))
				mkdir ($dir,0777);
			
			// do not use standard filenames or your sensitive company data 
			// are world readable
			$fname = $dir.'/'.random_id().'.pdf';
			$this->Output($fname, 'F');

			if ($email == 1) {
				$contactData = array();
				if ($this->contactData)
					foreach($this->contactData as $contact)
						if (!empty($contact['email']))
							$contactData[] = $contact;

				if(!count($contactData)) {
					$this->SetLang(user_language());
					display_warning(sprintf(_("You have no email contact defined for this type of document for '%s'."), $this->formData['recipient_name']));
				}
				else {
					$sent = $try = 0;
					$emails = '';
					if(!$subject)
						$subject = $this->formData['document_name'] . ' '. $this->formData['document_number'];
					foreach($contactData as $contact) {
						if (!isset($contact['email'])) 
							continue;
						$emailtype = true;
						$this->SetLang($contact['lang']);
						$coy_name = @html_entity_decode($this->company['coy_name'], ENT_QUOTES, $_SESSION['language']->encoding=='iso-8859-2' ? 'ISO-8859-1' : $_SESSION['language']->encoding);

						require_once($path_to_root . '/reporting/includes/class.mail.inc');
						$mail = new email(str_replace(",", "", $coy_name),
							$this->company['email']);
						$mail->charset = $this->encoding;
						
						$msg = _('Dear') . ' ' . $contact['name2'] . ",\n\n" 
							. _('Attached you will find ') . ' ' . $subject ."\n\n";

						if (isset($this->formData['payment_service'])) {
							$amt = number_format($this->formData['document_amount'], user_price_dec());
							$service = $this->formData['payment_service'];
							$url = payment_link($service, array(
									'company_email' => $this->company['email'],
									'amount' => $amt,
									'currency' => $this->formData['curr_code'],
									'comment' => $this->title . ' ' . $this->formData['document_number']
									));
							if ($url)
								$msg.= _('You can pay through'). " $service: $url\n\n";
						}

						$msg .= _('Kindest regards') . "\n\n";
						$sender = $this->user . "\n" . $coy_name . "\n" . $this->company['postal_address'] . "\n" . $this->company['email'] . "\n" . $this->company['phone'];
    					$mail->to(str_replace(",", "", $contact['name'].' '.$contact['name2']), $contact['email']);
    					$try++;
						$mail->subject($subject);
						$mail->text($msg . $sender);
						$mail->attachment($fname, $this->filename);
						$emails .= " " . $contact['email'];
						if ($mail->send()) $sent++;
					} // foreach contact
					unlink($fname);
					$this->SetLang(user_language());
					if (!$try)
						display_warning(sprintf(_("You have no email contact defined for this type of document for '%s'."), $this->formData['recipient_name']));
					elseif (!$sent)
						display_warning($this->title.' '.$this->formData['document_number'].'. '._('Sending document by email failed').'. '._('Email:').$emails);
					else
						display_notification($this->title.' '.$this->formData['document_number'].' '._('has been sent by email to destination.').' '._('Email:').$emails);
				}
			}
			else {
				$printer = get_report_printer(user_print_profile(), $this->rep_id);
				if ($printer == false) {
					if (in_ajax()) {
						global $Ajax;

						if (user_rep_popup()) 
							$Ajax->popup($fname); // when embeded pdf viewer used
						else
							$Ajax->redirect($fname); // otherwise use faster method
					}
					else {
						header('Content-type: application/pdf');
						header('Content-Disposition: inline; filename='.$this->filename);
						header('Expires: 0');
						header('Cache-Control: must-revalidate, post-check=0, pre-check=0');
						header('Pragma: public');
						$this->Stream($this->filename);
					}
				}
				else { // send report to network printer
					$prn = new remote_printer($printer['queue'],$printer['host'],
						$printer['port'], $printer['timeout']);
					$error = $prn->print_file($fname);
					if ($error)
						display_error($error);
					else
						display_notification(_('Report has been sent to network printer ').$printer['name']);
				}
			}
			// first have a look through the directory, 
			// and remove old temporary pdfs
			if ($d = @opendir($dir)) {
				while (($file = readdir($d)) !== false) {
					if (!is_file($dir.'/'.$file) || $file == 'index.php') continue;
				// then check to see if this one is too old
					$ftime = filemtime($dir.'/'.$file);
				// seems 3 min is enough for any report download, isn't it?
					if (time()-$ftime > 180){
						unlink($dir.'/'.$file);
					}
				}
				closedir($d);
			}
		}
	}
}
