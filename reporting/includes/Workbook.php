<?php
/*
*  Module written/ported by Xavier Noguer <xnoguer@rezebra.com>
*
*  The majority of this is _NOT_ my code.  I simply ported it from the
*  PERL Spreadsheet::WriteExcel module.
*
*  The author of the Spreadsheet::WriteExcel module is John McNamara
*  <jmcnamara@cpan.org>
*
*  I _DO_ maintain this code, and John McNamara has nothing to do with the
*  porting of this code to PHP.  Any questions directly related to this
*  class library should be directed to me.
*
*  License Information:
*
*    Spreadsheet_Excel_Writer:  A library for generating Excel Spreadsheets
*    Copyright (c) 2002-2003 Xavier Noguer xnoguer@rezebra.com
*
*    This library is free software; you can redistribute it and/or
*    modify it under the terms of the GNU Lesser General Public
*    License as published by the Free Software Foundation; either
*    version 2.1 of the License, or (at your option) any later version.
*
*    This library is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*    Lesser General Public License for more details.
*
*    You should have received a copy of the GNU Lesser General Public
*    License along with this library; if not, write to the Free Software
*    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/**
* @const SPREADSHEET_EXCEL_WRITER_ADD token identifier for character "+"
*/
define('SPREADSHEET_EXCEL_WRITER_ADD', "+");

/**
* @const SPREADSHEET_EXCEL_WRITER_SUB token identifier for character "-"
*/
define('SPREADSHEET_EXCEL_WRITER_SUB', "-");

/**
* @const SPREADSHEET_EXCEL_WRITER_MUL token identifier for character "*"
*/
define('SPREADSHEET_EXCEL_WRITER_MUL', "*");

/**
* @const SPREADSHEET_EXCEL_WRITER_DIV token identifier for character "/"
*/
define('SPREADSHEET_EXCEL_WRITER_DIV', "/");

/**
* @const SPREADSHEET_EXCEL_WRITER_OPEN token identifier for character "("
*/
define('SPREADSHEET_EXCEL_WRITER_OPEN', "(");

/**
* @const SPREADSHEET_EXCEL_WRITER_CLOSE token identifier for character ")"
*/
define('SPREADSHEET_EXCEL_WRITER_CLOSE', ")");

/**
* @const SPREADSHEET_EXCEL_WRITER_COMA token identifier for character ","
*/
define('SPREADSHEET_EXCEL_WRITER_COMA', ",");

/**
* @const SPREADSHEET_EXCEL_WRITER_SEMICOLON token identifier for character ";"
*/
define('SPREADSHEET_EXCEL_WRITER_SEMICOLON', ";");

/**
* @const SPREADSHEET_EXCEL_WRITER_GT token identifier for character ">"
*/
define('SPREADSHEET_EXCEL_WRITER_GT', ">");

/**
* @const SPREADSHEET_EXCEL_WRITER_LT token identifier for character "<"
*/
define('SPREADSHEET_EXCEL_WRITER_LT', "<");

/**
* @const SPREADSHEET_EXCEL_WRITER_LE token identifier for character "<="
*/
define('SPREADSHEET_EXCEL_WRITER_LE', "<=");

/**
* @const SPREADSHEET_EXCEL_WRITER_GE token identifier for character ">="
*/
define('SPREADSHEET_EXCEL_WRITER_GE', ">=");

/**
* @const SPREADSHEET_EXCEL_WRITER_EQ token identifier for character "="
*/
define('SPREADSHEET_EXCEL_WRITER_EQ', "=");

/**
* @const SPREADSHEET_EXCEL_WRITER_NE token identifier for character "<>"
*/
define('SPREADSHEET_EXCEL_WRITER_NE', "<>");

define('PpsType_Root', 5);
define('PpsType_Dir', 1);
define('PpsType_File', 2);
define('DataSizeSmall', 0x1000);
define('LongIntSize', 4);
define('PpsSize', 0x80);

function Asc2Ucs($sAsc) 
{
    return implode("\x00", (preg_split('//', $sAsc, -1, PREG_SPLIT_NO_EMPTY)))."\x00";
}

function Ucs2Asc($sUcs) 
{
    $chars=explode("\x00", $sUcs);
    array_pop($chars);
    return implode("", $chars);
}

function OLEDate2Local($sDateTime) 
{
}

#------------------------------------------------------------------------------
# Localtime->OLE Date
#------------------------------------------------------------------------------
function LocalDate2OLE($raDate) 
{
}

function _leapYear($iYear) 
{
    return ((($iYear % 4)==0) && (($iYear % 100) || ($iYear % 400)==0)) ? 1 : 0;
}

function _yearDays($iYear) 
{
    return _leapYear($iYear) ? 366 : 365;
}

function _monthDays($iMon, $iYear) 
{
    if ($iMon == 1 || $iMon ==  3 || $iMon ==  5 || $iMon == 7 ||
        $iMon == 8 || $iMon == 10 || $iMon == 12) 
        return 31;
    elseif ($iMon == 4 || $iMon == 6 || $iMon == 9 || $iMon == 11) 
        return 30;
    elseif ($iMon == 2) 
        return _leapYear($iYear) ? 29 : 28;
}

/*
 * This is the OLE::Storage_Lite Perl package ported to PHP
 * OLE::Storage_Lite was written by Kawai Takanori, kwitknr@cpan.org
 */

class ole_pps 
{
    var $No;
    var $Name;
    var $Type;
    var $PrevPps;
    var $NextPps;
    var $DirPps;
    var $Time1st;
    var $Time2nd;
    var $StartBlock;
    var $Size;
    var $Data;
    var $Child;
    var $_PPS_FILE;

    #------------------------------------------------------------------------------
    # _new (OLE::Storage_Lite::PPS)
    #   for OLE::Storage_Lite
    #------------------------------------------------------------------------------
    function __construct($iNo, $sNm, $iType, $iPrev, $iNext, $iDir,
                     $raTime1st, $raTime2nd, $iStart, $iSize,
                     $sData=false, $raChild=false) 
    {
        #1. Constructor for OLE::Storage_Lite

        $this->No         = $iNo;
        $this->Name       = $sNm;
        $this->Type       = $iType;
        $this->PrevPps    = $iPrev;
        $this->NextPps    = $iNext;
        $this->DirPps     = $iDir;
        $this->Time1st    = $raTime1st;
        $this->Time2nd    = $raTime2nd;
        $this->StartBlock = $iStart;
        $this->Size       = $iSize;
        $this->Data       = $sData;
        $this->Child      = $raChild;
        $this->_PPS_FILE  = NULL;
    }

    #------------------------------------------------------------------------------
    # _DataLen (OLE::Storage_Lite::PPS)
    # Check for update
    #------------------------------------------------------------------------------
    function _DataLen() 
    {
        if ($this->Data===false) 
            return 0;

        if ($this->_PPS_FILE) 
            return filesize($this->_PPS_FILE);

        return strlen($this->Data);
    }

    #------------------------------------------------------------------------------
    # _makeSmallData (OLE::Storage_Lite::PPS)
    #------------------------------------------------------------------------------
    function _makeSmallData(&$aList, $rhInfo) 
    {
        //my ($sRes);
        $FILE = $rhInfo->_FILEH_;
        $iSmBlk = 0;
        $sRes = '';

        for ($c=0;$c<sizeof($aList);$c++) 
        {
            $oPps=&$aList[$c];

            #1. Make SBD, small data string

            if ($oPps->Type==PpsType_File) 
            {
                if ($oPps->Size<=0) 
                    continue;

                if($oPps->Size < $rhInfo->_SMALL_SIZE) 
                {
                    $iSmbCnt = floor($oPps->Size / $rhInfo->_SMALL_BLOCK_SIZE) +
                               (($oPps->Size % $rhInfo->_SMALL_BLOCK_SIZE) ? 1 : 0);
                    #1.1 Add to SBD
                    for ($i = 0; $i<($iSmbCnt-1); $i++) 
                        fputs($FILE, pack("V", $i+$iSmBlk+1));
                    fputs($FILE, pack("V", -2));

                    #1.2 Add to Data String(this will be written for RootEntry)
                    #Check for update
                    if ($oPps->_PPS_FILE) 
                    {
                        //my $sBuff;
                        fseek($oPps->_PPS_FILE, 0, SEEK_SET); #To The Top
                        while ($sBuff=fread($oPps->_PPS_FILE, 4096)) 
                            $sRes .= $sBuff;
                    } 
                    else 
                        $sRes .= $oPps->Data;
                    if($oPps->Size % $rhInfo->_SMALL_BLOCK_SIZE) 
                    {
                        $sRes .= (str_repeat("\x00",
                                  ($rhInfo->_SMALL_BLOCK_SIZE -
                                  ($oPps->Size % $rhInfo->_SMALL_BLOCK_SIZE))));
                    }
                    #1.3 Set for PPS
                    $oPps->StartBlock = $iSmBlk;
                    $iSmBlk += $iSmbCnt;
                }
            }

        }
        $iSbCnt = floor($rhInfo->_BIG_BLOCK_SIZE / LongIntSize);
        if ($iSmBlk  % $iSbCnt) 
            fputs($FILE, str_repeat(pack("V", -1), $iSbCnt - ($iSmBlk % $iSbCnt)));
        #2. Write SBD with adjusting length for block
        return $sRes;
    }

    #------------------------------------------------------------------------------
    # _savePpsWk (OLE::Storage_Lite::PPS)
    #------------------------------------------------------------------------------
    function _savePpsWk($rhInfo) 
    {
        #1. Write PPS
        $FILE=$rhInfo->_FILEH_;
        fputs($FILE,
              $this->Name.
              str_repeat("\x00", 64 - strlen($this->Name)).      #  64
              pack("v", strlen($this->Name) + 2).                #  66
              pack("c", $this->Type).                            #  67
              pack("c", 0x00). #UK                               #  68
              pack("V", $this->PrevPps). #Prev                   #  72
              pack("V", $this->NextPps). #Next                   #  76
              pack("V", $this->DirPps).  #Dir                    #  80
              "\x00\x09\x02\x00".                                #  84
              "\x00\x00\x00\x00".                                #  88
              "\xc0\x00\x00\x00".                                #  92
              "\x00\x00\x00\x46".                                #  96
              "\x00\x00\x00\x00".                                # 100
			  "\x00\x00\x00\x00\x00\x00\x00\x00".
			  "\x00\x00\x00\x00\x00\x00\x00\x00".
              pack("V", ($this->StartBlock!==false) ?
                        $this->StartBlock : 0).                  # 120
              pack("V", ($this->Size!==false) ?
                        $this->Size : 0).                        # 124
              pack("V", 0)                                       # 128
        );
    }

}

/*
 * This is the OLE::Storage_Lite Perl package ported to PHP
 * OLE::Storage_Lite was written by Kawai Takanori, kwitknr@cpan.org
 */

class ole_pps_file extends ole_pps 
{
    function __construct($sNm, $sData=false, $sFile=false) 
    {
        $this->No         = false;
        $this->Name       = $sNm;
        $this->Type       = PpsType_File;
        $this->PrevPps    = false;
        $this->NextPps    = false;
        $this->DirPps     = false;
        $this->Time1st    = false;
        $this->Time2nd    = false;
        $this->StartBlock = false;
        $this->Size       = false;
        $this->Data       = ($sFile===false) ? $sData : '';
        $this->Child      = false;

        if ($sFile!==false) 
        {
            if (is_ressource($sFile)) 
                $this->_PPS_FILE=$sFile;
            elseif ($sFile=="") 
            {
                $fname=tempnam("php_ole");
                $this->_PPS_FILE=fopen($fname, "r+b");
            } 
            else 
            {
                $fname=$sFile;
                $this->_PPS_FILE=fopen($fname, "r+b");
            }

            if ($sData!==false) 
                fputs($this->_PPS_FILE, $sData);
        }
    }

    function append ($sData) 
    {
        if ($this->_PPS_FILE) 
            fputs($this->_PPS_FILE, $sData);
        else 
            $this->Data.=$sData;
    }
}

/*
 * This is the OLE::Storage_Lite Perl package ported to PHP
 * OLE::Storage_Lite was written by Kawai Takanori, kwitknr@cpan.org
 */

class ole_pps_root extends ole_pps 
{
    function __construct($raTime1st=false, $raTime2nd=false, $raChild=false) 
    {
        $this->No         = false;
        $this->Name       = Asc2Ucs('Root Entry');
        $this->Type       = PpsType_Root;
        $this->PrevPps    = false;
        $this->NextPps    = false;
        $this->DirPps     = false;
        $this->Time1st    = $raTime1st;
        $this->Time2nd    = $raTime2nd;
        $this->StartBlock = false;
        $this->Size       = false;
        $this->Data       = false;
        $this->Child      = $raChild;
    }

	#------------------------------------------------------------------------------
	# save (OLE::Storage_Lite::PPS::Root)
	#------------------------------------------------------------------------------
	function save($sFile, $bNoAs=false, $rhInfo=false) 
	{
		#0.Initial Setting for saving

	  	if (!$rhInfo) 
			$rhInfo=new stdClass();

		$rhInfo->_BIG_BLOCK_SIZE = $rhInfo->_SMALL_BLOCK_SIZE = 0;
	  	$rhInfo->_BIG_BLOCK_SIZE=pow(2, (($rhInfo->_BIG_BLOCK_SIZE) ?
		  	_adjust2($rhInfo->_BIG_BLOCK_SIZE) : 9));
	  	$rhInfo->_SMALL_BLOCK_SIZE=pow(2, (($rhInfo->_SMALL_BLOCK_SIZE) ?
			_adjust2($rhInfo->_SMALL_BLOCK_SIZE) : 6));
	  	$rhInfo->_SMALL_SIZE = 0x1000;
	  	$rhInfo->_PPS_SIZE = 0x80;

	  	#1.Open File
		#1.1 $sFile is Ref of scalar
	  	if(is_resource($sFile)) 
	  	{
			$oIo=$sFile;
			$rhInfo->_FILEH_ = $oIo;
	  	}
		#1.2 $sFile is a simple filename string
	  	else 
	  	{
			$oIo=fopen("$sFile", "wb");
			$rhInfo->_FILEH_ = $oIo;
	  	}

	  	$iBlk = 0;
		#1. Make an array of PPS (for Save)
	  	$aList=array();
	  	$list=array(&$this);
	  	if($bNoAs) 
			$this->_savePpsSetPnt2($list, $aList, $rhInfo);
	  	else 
			$this->_savePpsSetPnt($list, $aList, $rhInfo);
	  	list($iSBDcnt, $iBBcnt, $iPPScnt) = $this->_calcSize($aList, $rhInfo);
		#2.Save Header
	  	$this->_saveHeader($rhInfo, $iSBDcnt, $iBBcnt, $iPPScnt);

		#3.Make Small Data string (write SBD)
	  	$sSmWk = $this->_makeSmallData($aList, $rhInfo);
	  	$this->Data = $sSmWk;  #Small Datas become RootEntry Data

		#4. Write BB
	  	$iBBlk = $iSBDcnt;
	  	$this->_saveBigData($iBBlk, $aList, $rhInfo);
		#5. Write PPS
	  	$this->_savePps($aList, $rhInfo);
		#6. Write BD and BDList and Adding Header informations
	  	$this->_saveBbd($iSBDcnt, $iBBcnt, $iPPScnt,  $rhInfo); 
		#7.Close File
	  	fclose($rhInfo->_FILEH_);
	}

	#------------------------------------------------------------------------------
	# _calcSize (OLE::Storage_Lite::PPS)
	#------------------------------------------------------------------------------
	function _calcSize(&$raList, $rhInfo) 
	{
		#0. Calculate Basic Setting
  		$iSBDcnt=0;
  		$iBBcnt=0;
  		$iPPScnt = 0;
  		$iSmallLen = 0;
  		$iSBcnt = 0;

  		for ($c=0;$c<sizeof($raList);$c++) 
  		{
      		$oPps=&$raList[$c];

      		if($oPps->Type==PpsType_File) 
      		{
        		$oPps->Size = $oPps->_DataLen();  #Mod
        		if($oPps->Size < $rhInfo->_SMALL_SIZE) 
        		{
        	  		$iSBcnt += floor($oPps->Size / $rhInfo->_SMALL_BLOCK_SIZE) +
        	        	(($oPps->Size % $rhInfo->_SMALL_BLOCK_SIZE) ? 1 : 0);
        		} 
        		else 
        		{
        	  		$iBBcnt += 
        	    		(floor($oPps->Size/ $rhInfo->_BIG_BLOCK_SIZE) +
        	        	(($oPps->Size % $rhInfo->_BIG_BLOCK_SIZE)? 1: 0));
        		}
      		}
  		}	
  		$iSmallLen = $iSBcnt * $rhInfo->_SMALL_BLOCK_SIZE;
  		$iSlCnt = floor($rhInfo->_BIG_BLOCK_SIZE / LongIntSize);
  		$iSBDcnt = floor($iSBcnt / $iSlCnt)+ (($iSBcnt % $iSlCnt) ? 1 : 0);
  		$iBBcnt +=  (floor($iSmallLen/ $rhInfo->_BIG_BLOCK_SIZE) +
    		(( $iSmallLen% $rhInfo->_BIG_BLOCK_SIZE) ? 1 : 0));
  		$iCnt = sizeof($raList);
  		$iBdCnt = $rhInfo->_BIG_BLOCK_SIZE/PpsSize;
  		$iPPScnt = (floor($iCnt/$iBdCnt) + (($iCnt % $iBdCnt) ? 1 : 0));

  		return array($iSBDcnt, $iBBcnt, $iPPScnt);
	}

	#------------------------------------------------------------------------------
	# _adjust2 (OLE::Storage_Lite::PPS::Root)
	#------------------------------------------------------------------------------
	function _adjust2($i2) 
	{
  		$iWk = log($i2)/log(2);
  		return ($iWk > int($iWk)) ? floor($iWk)+1 : $iWk;
	}

	#------------------------------------------------------------------------------
	# _saveHeader (OLE::Storage_Lite::PPS::Root)
	#------------------------------------------------------------------------------
	function _saveHeader($rhInfo, $iSBDcnt, $iBBcnt, $iPPScnt) 
	{
  		$FILE = $rhInfo->_FILEH_;

		#0. Calculate Basic Setting
  		$iBlCnt = $rhInfo->_BIG_BLOCK_SIZE / LongIntSize;
  		$i1stBdL = ($rhInfo->_BIG_BLOCK_SIZE - 0x4C) / LongIntSize;

  		$iBdExL = 0;
  		$iAll = $iBBcnt + $iPPScnt + $iSBDcnt;
  		$iAllW = $iAll;
  		$iBdCntW = floor($iAllW / $iBlCnt) + (($iAllW % $iBlCnt) ? 1 : 0);
  		$iBdCnt = floor(($iAll + $iBdCntW) / $iBlCnt) + ((($iAllW+$iBdCntW) % $iBlCnt) ? 1 : 0);
  		//my $i;

		#0.1 Calculate BD count
  		if ($iBdCnt > $i1stBdL) 
  		{
			// TODO: is do-while correct here?
   			do 
   			{
      			$iBdExL++;
      			$iAllW++;
      			$iBdCntW = floor($iAllW / $iBlCnt) + (($iAllW % $iBlCnt) ? 1 : 0);
      			$iBdCnt = floor(($iAllW + $iBdCntW) / $iBlCnt) + ((($iAllW+$iBdCntW) % $iBlCnt) ? 1 : 0);
    		} 
    		while($iBdCnt > ($iBdExL*$iBlCnt+ $i1stBdL));
  		}

		#1.Save Header
  		fputs($FILE,
            "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1".
            "\x00\x00\x00\x00".
            "\x00\x00\x00\x00".
            "\x00\x00\x00\x00".
            "\x00\x00\x00\x00".
            pack("v", 0x3b).
            pack("v", 0x03).
            pack("v", -2).
            pack("v", 9).
            pack("v", 6).
            pack("v", 0).
            "\x00\x00\x00\x00".
            "\x00\x00\x00\x00".
            pack("V", $iBdCnt).
            pack("V", $iBBcnt+$iSBDcnt). #ROOT START
            pack("V", 0).
            pack("V", 0x1000).
            pack("V", 0).                  #Small Block Depot
            pack("V", 1)
    	);
		#2. Extra BDList Start, Count
  		if($iBdCnt < $i1stBdL) 
  		{
    		fputs($FILE, 
                pack("V", -2).     #Extra BDList Start
                pack("V", 0)       #Extra BDList Count
        	);
  		} 
  		else 
  		{
    		fputs($FILE,
    	        pack("V", $iAll+$iBdCnt).
    	        pack("V", $iBdExL)
    	    );
  		}

		#3. BDList
    	for ($i=0;($i<$i1stBdL) && ($i < $iBdCnt); $i++) 
        	fputs($FILE, pack("V", $iAll+$i));
    	if ($i<$i1stBdL) 
    	{
			// TODO: Check, if str_repeat is binary safe
        	fputs($FILE, str_repeat((pack("V", -1)), ($i1stBdL-$i)));
    	}
	}

	#------------------------------------------------------------------------------
	# _saveBigData (OLE::Storage_Lite::PPS)
	#------------------------------------------------------------------------------
	function _saveBigData(&$iStBlk, &$raList, $rhInfo) 
	{

		//return;//!!!

  		$iRes = 0;
  		$FILE = $rhInfo->_FILEH_;

		#1.Write Big (ge 0x1000) Data into Block
  		for ($c=0;$c<sizeof($raList);$c++) 
  		{
    		$oPps=&$raList[$c];
    		if($oPps->Type!=PpsType_Dir) 
    		{
				#print "PPS: $oPps DEF:", defined($oPps->{Data}), "\n";
        		$oPps->Size = $oPps->_DataLen();  #Mod
        		if(($oPps->Size >= $rhInfo->_SMALL_SIZE) ||
            		(($oPps->Type == PpsType_Root) && $oPps->Data!==false)) 
            	{
            		#1.1 Write Data
            		#Check for update
            		if($oPps->_PPS_FILE) 
            		{
                		//my $sBuff;
                		$iLen = 0;
                		fseek($oPps->_PPS_FILE, 0, SEEK_SET); #To The Top
                		while ($sBuff=fread($oPps->_PPS_FILE, 4096)) 
                		{
                    		$iLen += length($sBuff);
                    		fputs($FILE, $sBuff);           #Check for update
                		}
            		} 
            		else 
                		fputs($FILE, $oPps->Data);
            		if ($oPps->Size % $rhInfo->_BIG_BLOCK_SIZE) 
            		{
						// TODO: Check, if str_repeat() is binary safe
              			fputs($FILE, str_repeat("\x00", 
                        	($rhInfo->_BIG_BLOCK_SIZE - 
                            ($oPps->Size % $rhInfo->_BIG_BLOCK_SIZE)))
                    	);
            		}
            		#1.2 Set For PPS
            		$oPps->StartBlock = $iStBlk;
            		$iStBlk += 
                    	(floor($oPps->Size/ $rhInfo->_BIG_BLOCK_SIZE) +
                        (($oPps->Size % $rhInfo->_BIG_BLOCK_SIZE) ? 1 : 0));
        		}
    		}
  		}
	}

	#------------------------------------------------------------------------------
	# _savePps (OLE::Storage_Lite::PPS::Root)
	#------------------------------------------------------------------------------
	function _savePps(&$raList, $rhInfo) 
	{
		#0. Initial
  		$FILE = $rhInfo->_FILEH_;
		#2. Save PPS
  		for ($c=0;$c<sizeof($raList);$c++) 
  		{
    		$oItem=&$raList[$c];
    		$oItem->_savePpsWk($rhInfo);
  		}
		#3. Adjust for Block
  		$iCnt = sizeof($raList);
  		$iBCnt = $rhInfo->_BIG_BLOCK_SIZE / $rhInfo->_PPS_SIZE;
  		if($iCnt % $iBCnt) 
    		fputs($FILE, str_repeat("\x00", (($iBCnt - ($iCnt % $iBCnt)) * $rhInfo->_PPS_SIZE)));
  		return (floor($iCnt / $iBCnt) + (($iCnt % $iBCnt) ? 1 : 0));
	}

	#------------------------------------------------------------------------------
	# _savePpsSetPnt2 (OLE::Storage_Lite::PPS::Root)
	#  For Test
	#------------------------------------------------------------------------------
	function _savePpsSetPnt2(&$aThis, &$raList, $rhInfo) 
	{
		#1. make Array as Children-Relations
		#1.1 if No Children
  		if (!is_array($aThis) || sizeof($aThis)==0) 
      		return 0xFFFFFFFF;
  		elseif (sizeof($aThis)==1) 
  		{
			#1.2 Just Only one
      		array_push($raList, $aThis[0]);
      		$aThis[0]->No = sizeof($raList)-1;
      		$aThis[0]->PrevPps = 0xFFFFFFFF;
      		$aThis[0]->NextPps = 0xFFFFFFFF;
      		$aThis[0]->DirPps = $this->_savePpsSetPnt2($aThis[0]->Child, $raList, $rhInfo);
      		return $aThis[0]->No;
  		} 
  		else 
  		{
			#1.3 Array
      		$iCnt = sizeof($aThis);
			#1.3.1 Define Center
      		$iPos = 0; #int($iCnt/ 2);     #$iCnt 

      		$aWk = $aThis;
      		$aPrev = (sizeof($aThis) > 2) ? array_splice($aWk, 1, 1) : array(); #$iPos);
      		$aNext = array_splice($aWk, 1); #, $iCnt - $iPos -1);
      		$aThis[$iPos]->PrevPps = $this->_savePpsSetPnt2($aPrev, $raList, $rhInfo);
      		array_push($raList, $aThis[$iPos]);
      		$aThis[$iPos]->No = sizeof($raList)-1;

			#1.3.2 Devide a array into Previous,Next
      		$aThis[$iPos]->NextPps = $this->_savePpsSetPnt2($aNext, $raList, $rhInfo);
      		$aThis[$iPos]->DirPps = $this->_savePpsSetPnt2($aThis[$iPos]->Child, $raList, $rhInfo);
      		return $aThis[$iPos]->No;
  		}
	}

	#------------------------------------------------------------------------------
	# _savePpsSetPnt2 (OLE::Storage_Lite::PPS::Root)
	#  For Test
	#------------------------------------------------------------------------------
	function _savePpsSetPnt2s(&$aThis, &$raList, $rhInfo) 
	{
		#1. make Array as Children-Relations
		#1.1 if No Children
  		if (!is_array($aThis) || sizeof($aThis)==0) 
      		return 0xFFFFFFFF;
  		elseif (sizeof($aThis)==1) 
  		{
			#1.2 Just Only one
      		array_push($raList, $aThis[0]);
      		$aThis[0]->No = sizeof($raList)-1;
      		$aThis[0]->PrevPps = 0xFFFFFFFF;
      		$aThis[0]->NextPps = 0xFFFFFFFF;
      		$aThis[0]->DirPps = $this->_savePpsSetPnt2($aThis[0]->Child, $raList, $rhInfo);
      		return $aThis[0]->No;
  		} 
  		else 
  		{
			#1.3 Array
      		$iCnt = sizeof($aThis);
			#1.3.1 Define Center
      		$iPos = 0; #int($iCnt/ 2);     #$iCnt 
      		array_push($raList, $aThis[$iPos]);
      		$aThis[$iPos]->No = sizeof($raList)-1;
      		$aWk = $aThis;
			#1.3.2 Devide a array into Previous,Next
      		$aPrev = array_splice($aWk, 0, $iPos);
      		$aNext = array_splice($aWk, 1, $iCnt - $iPos - 1);
      		$aThis[$iPos]->PrevPps = $this->_savePpsSetPnt2($aPrev, $raList, $rhInfo);
      		$aThis[$iPos]->NextPps = $this->_savePpsSetPnt2($aNext, $raList, $rhInfo);
      		$aThis[$iPos]->DirPps = $this->_savePpsSetPnt2($aThis[$iPos]->Child, $raList, $rhInfo);
      		return $aThis[$iPos]->No;
  		}
	}

	#------------------------------------------------------------------------------
	# _savePpsSetPnt (OLE::Storage_Lite::PPS::Root)
	#------------------------------------------------------------------------------
	function _savePpsSetPnt(&$aThis, &$raList, $rhInfo) 
	{

		//print "yyy type: ".gettype($aThis)."<br>\n";
		//print "yyy name: ".$aThis[0]->Name."<br>\n";

		#1. make Array as Children-Relations
		#1.1 if No Children
  		if (!is_array($aThis) || sizeof($aThis)==0) 
  		{
      		return 0xFFFFFFFF;
  		} 
  		elseif (sizeof($aThis)==1) 
  		{
			#1.2 Just Only one
      		array_push($raList, $aThis[0]);
      		$aThis[0]->No = sizeof($raList)-1;
      		$aThis[0]->PrevPps = 0xFFFFFFFF;
      		$aThis[0]->NextPps = 0xFFFFFFFF;
      		$aThis[0]->DirPps = $this->_savePpsSetPnt($aThis[0]->Child, $raList, $rhInfo);
      		return $aThis[0]->No;
  		} 
  		else 
  		{	
			#1.3 Array
      		$iCnt = sizeof($aThis);
			#1.3.1 Define Center
      		$iPos = floor($iCnt/2);     #$iCnt 
      		array_push($raList, $aThis[$iPos]);
      		$aThis[$iPos]->No = sizeof($raList)-1;
      		$aWk = $aThis;
			#1.3.2 Devide a array into Previous,Next
      		$aPrev = splice($aWk, 0, $iPos);
      		$aNext = splice($aWk, 1, $iCnt - $iPos - 1);
      		$aThis[$iPos]->PrevPps = $this->_savePpsSetPnt($aPrev, $raList, $rhInfo);
      		$aThis[$iPos]->NextPps = $this->_savePpsSetPnt($aNext, $raList, $rhInfo);
      		$aThis[$iPos]->DirPps = $this->_savePpsSetPnt($aThis[$iPos]->Child, $raList, $rhInfo);
      		return $aThis[$iPos]->No;
  		}
	}

	#------------------------------------------------------------------------------
	# _savePpsSetPnt (OLE::Storage_Lite::PPS::Root)
	#------------------------------------------------------------------------------
	function _savePpsSetPnt1(&$aThis, &$raList, $rhInfo) 
	{
		#1. make Array as Children-Relations
		#1.1 if No Children
		if (!is_array($aThis) || sizeof($aThis)==0) 
		{
			return 0xFFFFFFFF;
		} 
		elseif (sizeof($aThis)==1) 
		{
			#1.2 Just Only one
			array_push($raList, $aThis[0]);
			$aThis[0]->No = sizeof($raList)-1;
			$aThis[0]->PrevPps = 0xFFFFFFFF;
			$aThis[0]->NextPps = 0xFFFFFFFF;
			$aThis[0]->DirPps = $this->_savePpsSetPnt($aThis[0]->Child, $raList, $rhInfo);
			return $aThis[0]->No;
		} 
		else 
		{
			#1.3 Array
			$iCnt = sizeof($aThis);
			#1.3.1 Define Center
			$iPos = floor($iCnt / 2);     #$iCnt 
			array_push($raList, $aThis[$iPos]);
			$aThis[$iPos]->No = sizeof($raList)-1;
			$aWk = $aThis;
			#1.3.2 Devide a array into Previous,Next
			$aPrev = splice($aWk, 0, $iPos);
			$aNext = splice($aWk, 1, $iCnt - $iPos - 1);
			$aThis[$iPos]->PrevPps = $this->_savePpsSetPnt($aPrev, $raList, $rhInfo);
			$aThis[$iPos]->NextPps = $this->_savePpsSetPnt($aNext, $raList, $rhInfo);
			$aThis[$iPos]->DirPps = $this->_savePpsSetPnt($aThis[$iPos]->Child, $raList, $rhInfo);
			return $aThis[$iPos]->No;
		}
	}

	#------------------------------------------------------------------------------
	# _saveBbd (OLE::Storage_Lite)
	#------------------------------------------------------------------------------
	function _saveBbd($iSbdSize, $iBsize, $iPpsCnt, $rhInfo) 
	{
 	 	$FILE = $rhInfo->_FILEH_;
		#0. Calculate Basic Setting
  		$iBbCnt = $rhInfo->_BIG_BLOCK_SIZE / LongIntSize;
  		$i1stBdL = ($rhInfo->_BIG_BLOCK_SIZE - 0x4C) / LongIntSize;

  		$iBdExL = 0;
  		$iAll = $iBsize + $iPpsCnt + $iSbdSize;
  		$iAllW = $iAll;
  		$iBdCntW = floor($iAllW / $iBbCnt) + (($iAllW % $iBbCnt) ? 1 : 0);
  		$iBdCnt = floor(($iAll + $iBdCntW) / $iBbCnt) + ((($iAllW+$iBdCntW) % $iBbCnt)? 1: 0);
  		//my $i;
		#0.1 Calculate BD count
  		if ($iBdCnt >$i1stBdL) 
  		{
			// TODO: do-while correct here?
    		do 
    		{
      			$iBdExL++;
      			$iAllW++;
      			$iBdCntW = floor($iAllW / $iBbCnt) + (($iAllW % $iBbCnt) ? 1 : 0);
      			$iBdCnt = floor(($iAllW + $iBdCntW) / $iBbCnt) + ((($iAllW+$iBdCntW) % $iBbCnt) ? 1 : 0);
    		} 
    		while ($iBdCnt > ($iBdExL*$iBbCnt+$i1stBdL));
  		}

		#1. Making BD
		#1.1 Set for SBD
  		if($iSbdSize > 0) 
  		{
    		for ($i = 0; $i<($iSbdSize-1); $i++) 
      			fputs($FILE, pack("V", $i+1));
    		fputs($FILE, pack("V", -2));
  		}
		#1.2 Set for B
  		for ($i = 0; $i<($iBsize-1); $i++) 
      		fputs($FILE, pack("V", $i+$iSbdSize+1));
  		fputs($FILE, pack("V", -2));

		#1.3 Set for PPS
  		for ($i = 0; $i<($iPpsCnt-1); $i++) 
      		fputs($FILE, pack("V", $i+$iSbdSize+$iBsize+1));
  		fputs($FILE, pack("V", -2));
		#1.4 Set for BBD itself ( 0xFFFFFFFD : BBD)
  		for ($i=0; $i<$iBdCnt;$i++) 
    		fputs($FILE, pack("V", 0xFFFFFFFD));
		#1.5 Set for ExtraBDList
  		for ($i=0; $i<$iBdExL;$i++) 
    		fputs($FILE, pack("V", 0xFFFFFFFC));
		#1.6 Adjust for Block
  		if(($iAllW + $iBdCnt) % $iBbCnt) 
    		fputs($FILE, str_repeat(pack("V", -1), ($iBbCnt - (($iAllW + $iBdCnt) % $iBbCnt))));

		#2.Extra BDList
  		if($iBdCnt > $i1stBdL)  
  		{
    		$iN=0;
    		$iNb=0;
    		for ($i=$i1stBdL;$i<$iBdCnt; $i++, $iN++) 
    		{
      			if($iN>=($iBbCnt-1)) 
      			{
          			$iN = 0;
          			$iNb++;
          			fputs($FILE, pack("V", $iAll+$iBdCnt+$iNb));
      			}
      			fputs($FILE, pack("V", $iBsize+$iSbdSize+$iPpsCnt+$i));
    		}
    		if(($iBdCnt-$i1stBdL) % ($iBbCnt-1)) 
      			fputs($FILE, str_repeat(pack("V", -1), (($iBbCnt-1) - (($iBdCnt-$i1stBdL) % ($iBbCnt-1)))));
    		fputs($FILE, pack("V", -2));
  		}
	}
}

/**
* Class for writing Excel BIFF records.
*
* From "MICROSOFT EXCEL BINARY FILE FORMAT" by Mark O'Brien (Microsoft Corporation):
*
* BIFF (BInary File Format) is the file format in which Excel documents are
* saved on disk.  A BIFF file is a complete description of an Excel document.
* BIFF files consist of sequences of variable-length records. There are many
* different types of BIFF records.  For example, one record type describes a
* formula entered into a cell; one describes the size and location of a
* window into a document; another describes a picture format.
*
* @author   Xavier Noguer <xnoguer@php.net>
* @category FileFormats
* @package  Spreadsheet_Excel_Writer
*/

class Spreadsheet_Excel_Writer_BIFFwriter
{
    /**
    * The BIFF/Excel version (5).
    * @var integer
    */
    var $_BIFF_version = 0x0500;

    /**
    * The byte order of this architecture. 0 => little endian, 1 => big endian
    * @var integer
    */
    var $_byte_order;

    /**
    * The string containing the data of the BIFF stream
    * @var string
    */
    var $_data;

    /**
    * The size of the data in bytes. Should be the same as strlen($this->_data)
    * @var integer
    */
    var $_datasize;

    /**
    * The maximun length for a BIFF record. See _addContinue()
    * @var integer
    * @see _addContinue()
    */
    var $_limit;

    /**
    * Constructor
    *
    * @access public
    */
    function __construct()
    {
        $this->_byte_order = '';
        $this->_data       = '';
        $this->_datasize   = 0;
        $this->_limit      = 2080;
        // Set the byte order
        $this->_setByteOrder();
    }

    /**
    * Determine the byte order and store it as class data to avoid
    * recalculating it for each call to new().
    *
    * @access private
    */
    function _setByteOrder()
    {
        // Check if "pack" gives the required IEEE 64bit float
        $teststr = pack("d", 1.2345);
        $number  = pack("C8", 0x8D, 0x97, 0x6E, 0x12, 0x83, 0xC0, 0xF3, 0x3F);
        if ($number == $teststr) {
            $byte_order = 0;    // Little Endian
        } elseif ($number == strrev($teststr)){
            $byte_order = 1;    // Big Endian
        } else {
            // Give up. I'll fix this in a later version.
            die("Required floating point format ".
                                     "not supported on this platform.");
        }
        $this->_byte_order = $byte_order;
    }

    /**
    * General storage function
    *
    * @param string $data binary data to prepend
    * @access private
    */
    function _prepend($data)
    {
        if (strlen($data) > $this->_limit) {
            $data = $this->_addContinue($data);
        }
        $this->_data      = $data.$this->_data;
        $this->_datasize += strlen($data);
    }

    /**
    * General storage function
    *
    * @param string $data binary data to append
    * @access private
    */
    function _append($data)
    {
        if (strlen($data) > $this->_limit) {
            $data = $this->_addContinue($data);
        }
        $this->_data      = $this->_data.$data;
        $this->_datasize += strlen($data);
    }

    /**
    * Writes Excel BOF record to indicate the beginning of a stream or
    * sub-stream in the BIFF file.
    *
    * @param  integer $type Type of BIFF file to write: 0x0005 Workbook,
    *                       0x0010 Worksheet.
    * @access private
    */
    function _storeBof($type)
    {
        $record  = 0x0809;        // Record identifier

        // According to the SDK $build and $year should be set to zero.
        // However, this throws a warning in Excel 5. So, use magic numbers.
        if ($this->_BIFF_version == 0x0500) {
            $length  = 0x0008;
            $unknown = '';
            $build   = 0x096C;
            $year    = 0x07C9;
        } elseif ($this->_BIFF_version == 0x0600) {
            $length  = 0x0010;
            $unknown = pack("VV", 0x00000041, 0x00000006); //unknown last 8 bytes for BIFF8
            $build   = 0x0DBB;
            $year    = 0x07CC;
        }
        $version = $this->_BIFF_version;

        $header  = pack("vv",   $record, $length);
        $data    = pack("vvvv", $version, $type, $build, $year);
        $this->_prepend($header . $data . $unknown);
    }

    /**
    * Writes Excel EOF record to indicate the end of a BIFF stream.
    *
    * @access private
    */
    function _storeEof()
    {
        $record    = 0x000A;   // Record identifier
        $length    = 0x0000;   // Number of bytes to follow
        $header    = pack("vv", $record, $length);
        $this->_append($header);
    }

    /**
    * Excel limits the size of BIFF records. In Excel 5 the limit is 2084 bytes. In
    * Excel 97 the limit is 8228 bytes. Records that are longer than these limits
    * must be split up into CONTINUE blocks.
    *
    * This function takes a long BIFF record and inserts CONTINUE records as
    * necessary.
    *
    * @param  string  $data The original binary data to be written
    * @return string        A very convenient string of continue blocks
    * @access private
    */
    function _addContinue($data)
    {
        $limit  = $this->_limit;
        $record = 0x003C;         // Record identifier

        // The first 2080/8224 bytes remain intact. However, we have to change
        // the length field of the record.
        $tmp = substr($data, 0, 2).pack("v", $limit-4).substr($data, 4, $limit - 4);

        $header = pack("vv", $record, $limit);  // Headers for continue records

        // Retrieve chunks of 2080/8224 bytes +4 for the header.
        $data_length = strlen($data);
        for ($i = $limit; $i <  ($data_length - $limit); $i += $limit) {
            $tmp .= $header;
            $tmp .= substr($data, $i, $limit);
        }

        // Retrieve the last chunk of data
        $header  = pack("vv", $record, strlen($data) - $i);
        $tmp    .= $header;
        $tmp    .= substr($data, $i, strlen($data) - $i);

        return $tmp;
    }
}

/*
FIXME: change prefixes
*/
define("OP_BETWEEN",    0x00);
define("OP_NOTBETWEEN", 0x01);
define("OP_EQUAL",      0x02);
define("OP_NOTEQUAL",   0x03);
define("OP_GT",         0x04);
define("OP_LT",         0x05);
define("OP_GTE",        0x06);
define("OP_LTE",        0x07);

/**
* Baseclass for generating Excel DV records (validations)
*
* @author   Herman Kuiper
* @category FileFormats
* @package  Spreadsheet_Excel_Writer
*/
class Spreadsheet_Excel_Writer_Validator
{
   var $_type;
   var $_style;
   var $_fixedList;
   var $_blank;
   var $_incell;
   var $_showprompt;
   var $_showerror;
   var $_title_prompt;
   var $_descr_prompt;
   var $_title_error;
   var $_descr_error;
   var $_operator;
   var $_formula1;
   var $_formula2;
    /**
    * The parser from the workbook. Used to parse validation formulas also
    * @var Spreadsheet_Excel_Writer_Parser
    */
    var $_parser;

    function __construct(&$parser)
    {
        $this->_parser       = $parser;
        $this->_type         = 0x01; // FIXME: add method for setting datatype
        $this->_style        = 0x00;
        $this->_fixedList    = false;
        $this->_blank        = false;
        $this->_incell       = false;
        $this->_showprompt   = false;
        $this->_showerror    = true;
        $this->_title_prompt = "\x00";
        $this->_descr_prompt = "\x00";
        $this->_title_error  = "\x00";
        $this->_descr_error  = "\x00";
        $this->_operator     = 0x00; // default is equal
        $this->_formula1    = '';
        $this->_formula2    = '';
    }

   function setPrompt($promptTitle = "\x00", $promptDescription = "\x00", $showPrompt = true)
   {
      $this->_showprompt = $showPrompt;
      $this->_title_prompt = $promptTitle;
      $this->_descr_prompt = $promptDescription;
   }

   function setError($errorTitle = "\x00", $errorDescription = "\x00", $showError = true)
   {
      $this->_showerror = $showError;
      $this->_title_error = $errorTitle;
      $this->_descr_error = $errorDescription;
   }

   function allowBlank()
   {
      $this->_blank = true;
   }

   function onInvalidStop()
   {
      $this->_style = 0x00;
   }

    function onInvalidWarn()
    {
        $this->_style = 0x01;
    }

    function onInvalidInfo()
    {
        $this->_style = 0x02;
    }

    function setFormula1($formula)
    {
        // Parse the formula using the parser in Parser.php
        $this->_parser->parse($formula);

        $this->_formula1 = $this->_parser->toReversePolish();
        return true;
    }

    function setFormula2($formula)
    {
        // Parse the formula using the parser in Parser.php
        $this->_parser->parse($formula);

        $this->_formula2 = $this->_parser->toReversePolish();
        return true;
    }

    function _getOptions()
    {
        $options = $this->_type;
        $options |= $this->_style << 3;
        if ($this->_fixedList) {
            $options |= 0x80;
        }
        if ($this->_blank) {
            $options |= 0x100;
        }
        if (!$this->_incell) {
            $options |= 0x200;
        }
        if ($this->_showprompt) {
            $options |= 0x40000;
        }
        if ($this->_showerror) {
            $options |= 0x80000;
        }
      $options |= $this->_operator << 20;

      return $options;
   }

   function _getData()
   {
      $title_prompt_len = strlen($this->_title_prompt);
      $descr_prompt_len = strlen($this->_descr_prompt);
      $title_error_len = strlen($this->_title_error);
      $descr_error_len = strlen($this->_descr_error);

      $formula1_size = strlen($this->_formula1);
      $formula2_size = strlen($this->_formula2);

      $data  = pack("V", $this->_getOptions());
      $data .= pack("vC", $title_prompt_len, 0x00) . $this->_title_prompt;
      $data .= pack("vC", $title_error_len, 0x00) . $this->_title_error;
      $data .= pack("vC", $descr_prompt_len, 0x00) . $this->_descr_prompt;
      $data .= pack("vC", $descr_error_len, 0x00) . $this->_descr_error;

      $data .= pack("vv", $formula1_size, 0x0000) . $this->_formula1;
      $data .= pack("vv", $formula2_size, 0x0000) . $this->_formula2;

      return $data;
   }
}

/**
* Class for generating Excel XF records (formats)
*
* @author   Xavier Noguer <xnoguer@rezebra.com>
* @category FileFormats
* @package  Spreadsheet_Excel_Writer
*/

class Spreadsheet_Excel_Writer_Format
{
    /**
    * The index given by the workbook when creating a new format.
    * @var integer
    */
    var $_xf_index;

    /**
    * Index to the FONT record.
    * @var integer
    */
    var $font_index;

    /**
    * The font name (ASCII).
    * @var string
    */
    var $_font_name;

    /**
    * Height of font (1/20 of a point)
    * @var integer
    */
    var $_size;

    /**
    * Bold style
    * @var integer
    */
    var $_bold;

    /**
    * Bit specifiying if the font is italic.
    * @var integer
    */
    var $_italic;

    /**
    * Index to the cell's color
    * @var integer
    */
    var $_color;

    /**
    * The text underline property
    * @var integer
    */
    var $_underline;

    /**
    * Bit specifiying if the font has strikeout.
    * @var integer
    */
    var $_font_strikeout;

    /**
    * Bit specifiying if the font has outline.
    * @var integer
    */
    var $_font_outline;

    /**
    * Bit specifiying if the font has shadow.
    * @var integer
    */
    var $_font_shadow;

    /**
    * 2 bytes specifiying the script type for the font.
    * @var integer
    */
    var $_font_script;

    /**
    * Byte specifiying the font family.
    * @var integer
    */
    var $_font_family;

    /**
    * Byte specifiying the font charset.
    * @var integer
    */
    var $_font_charset;

    /**
    * An index (2 bytes) to a FORMAT record (number format).
    * @var integer
    */
    var $_num_format;

    /**
    * Bit specifying if formulas are hidden.
    * @var integer
    */
    var $_hidden;

    /**
    * Bit specifying if the cell is locked.
    * @var integer
    */
    var $_locked;

    /**
    * The three bits specifying the text horizontal alignment.
    * @var integer
    */
    var $_text_h_align;

    /**
    * Bit specifying if the text is wrapped at the right border.
    * @var integer
    */
    var $_text_wrap;

    /**
    * The three bits specifying the text vertical alignment.
    * @var integer
    */
    var $_text_v_align;

    /**
    * 1 bit, apparently not used.
    * @var integer
    */
    var $_text_justlast;

    /**
    * The two bits specifying the text rotation.
    * @var integer
    */
    var $_rotation;

    /**
    * The cell's foreground color.
    * @var integer
    */
    var $_fg_color;

    /**
    * The cell's background color.
    * @var integer
    */
    var $_bg_color;

    /**
    * The cell's background fill pattern.
    * @var integer
    */
    var $_pattern;

    /**
    * Style of the bottom border of the cell
    * @var integer
    */
    var $_bottom;

    /**
    * Color of the bottom border of the cell.
    * @var integer
    */
    var $_bottom_color;

    /**
    * Style of the top border of the cell
    * @var integer
    */
    var $_top;

    /**
    * Color of the top border of the cell.
    * @var integer
    */
    var $_top_color;

    /**
    * Style of the left border of the cell
    * @var integer
    */
    var $_left;

    /**
    * Color of the left border of the cell.
    * @var integer
    */
    var $_left_color;

    /**
    * Style of the right border of the cell
    * @var integer
    */
    var $_right;

    /**
    * Color of the right border of the cell.
    * @var integer
    */
    var $_right_color;

    /**
    * Constructor
    *
    * @access private
    * @param integer $index the XF index for the format.
    * @param array   $properties array with properties to be set on initialization.
    */
    function __construct($BIFF_version, $index = 0, $properties =  array())
    {
        $this->_xf_index       = $index;
        $this->_BIFF_version   = $BIFF_version;
        $this->font_index      = 0;
        $this->_font_name      = 'Arial';
        $this->_size           = 10;
        $this->_bold           = 0x0190;
        $this->_italic         = 0;
        $this->_color          = 0x7FFF;
        $this->_underline      = 0;
        $this->_font_strikeout = 0;
        $this->_font_outline   = 0;
        $this->_font_shadow    = 0;
        $this->_font_script    = 0;
        $this->_font_family    = 0;
        $this->_font_charset   = 0;

        $this->_num_format     = 0;

        $this->_hidden         = 0;
        $this->_locked         = 0;

        $this->_text_h_align   = 0;
        $this->_text_wrap      = 0;
        $this->_text_v_align   = 2;
        $this->_text_justlast  = 0;
        $this->_rotation       = 0;

        $this->_fg_color       = 0x40;
        $this->_bg_color       = 0x41;

        $this->_pattern        = 0;

        $this->_bottom         = 0;
        $this->_top            = 0;
        $this->_left           = 0;
        $this->_right          = 0;
        $this->_diag           = 0;

        $this->_bottom_color   = 0x40;
        $this->_top_color      = 0x40;
        $this->_left_color     = 0x40;
        $this->_right_color    = 0x40;
        $this->_diag_color     = 0x40;

        // Set properties passed to Spreadsheet_Excel_Writer_Workbook::addFormat()
        foreach ($properties as $property => $value)
        {
            if (method_exists($this, 'set'.ucwords($property))) {
                $method_name = 'set'.ucwords($property);
                $this->$method_name($value);
            }
        }
    }


    /**
    * Generate an Excel BIFF XF record (style or cell).
    *
    * @param string $style The type of the XF record ('style' or 'cell').
    * @return string The XF record
    */
    function getXf($style)
    {
        // Set the type of the XF record and some of the attributes.
        if ($style == 'style') {
            $style = 0xFFF5;
        } else {
            $style   = $this->_locked;
            $style  |= $this->_hidden << 1;
        }

        // Flags to indicate if attributes have been set.
        $atr_num     = ($this->_num_format != 0)?1:0;
        $atr_fnt     = ($this->font_index != 0)?1:0;
        $atr_alc     = ($this->_text_wrap)?1:0;
        $atr_bdr     = ($this->_bottom   ||
                        $this->_top      ||
                        $this->_left     ||
                        $this->_right)?1:0;
        $atr_pat     = (($this->_fg_color != 0x40) ||
                        ($this->_bg_color != 0x41) ||
                        $this->_pattern)?1:0;
        $atr_prot    = $this->_locked | $this->_hidden;

        // Zero the default border colour if the border has not been set.
        if ($this->_bottom == 0) {
            $this->_bottom_color = 0;
        }
        if ($this->_top  == 0) {
            $this->_top_color = 0;
        }
        if ($this->_right == 0) {
            $this->_right_color = 0;
        }
        if ($this->_left == 0) {
            $this->_left_color = 0;
        }
        if ($this->_diag == 0) {
            $this->_diag_color = 0;
        }

        $record         = 0x00E0;              // Record identifier
        if ($this->_BIFF_version == 0x0500) {
            $length         = 0x0010;              // Number of bytes to follow
        }
        if ($this->_BIFF_version == 0x0600) {
            $length         = 0x0014;
        }

        $ifnt           = $this->font_index;   // Index to FONT record
        $ifmt           = $this->_num_format;  // Index to FORMAT record
        if ($this->_BIFF_version == 0x0500) {
            $align          = $this->_text_h_align;       // Alignment
            $align         |= $this->_text_wrap     << 3;
            $align         |= $this->_text_v_align  << 4;
            $align         |= $this->_text_justlast << 7;
            $align         |= $this->_rotation      << 8;
            $align         |= $atr_num                << 10;
            $align         |= $atr_fnt                << 11;
            $align         |= $atr_alc                << 12;
            $align         |= $atr_bdr                << 13;
            $align         |= $atr_pat                << 14;
            $align         |= $atr_prot               << 15;

            $icv            = $this->_fg_color;       // fg and bg pattern colors
            $icv           |= $this->_bg_color      << 7;

            $fill           = $this->_pattern;        // Fill and border line style
            $fill          |= $this->_bottom        << 6;
            $fill          |= $this->_bottom_color  << 9;

            $border1        = $this->_top;            // Border line style and color
            $border1       |= $this->_left          << 3;
            $border1       |= $this->_right         << 6;
            $border1       |= $this->_top_color     << 9;

            $border2        = $this->_left_color;     // Border color
            $border2       |= $this->_right_color   << 7;

            $header      = pack("vv",       $record, $length);
            $data        = pack("vvvvvvvv", $ifnt, $ifmt, $style, $align,
                                            $icv, $fill,
                                            $border1, $border2);
        } elseif ($this->_BIFF_version == 0x0600) {
            $align          = $this->_text_h_align;       // Alignment
            $align         |= $this->_text_wrap     << 3;
            $align         |= $this->_text_v_align  << 4;
            $align         |= $this->_text_justlast << 7;

            $used_attrib    = $atr_num              << 2;
            $used_attrib   |= $atr_fnt              << 3;
            $used_attrib   |= $atr_alc              << 4;
            $used_attrib   |= $atr_bdr              << 5;
            $used_attrib   |= $atr_pat              << 6;
            $used_attrib   |= $atr_prot             << 7;

            $icv            = $this->_fg_color;      // fg and bg pattern colors
            $icv           |= $this->_bg_color      << 7;

            $border1        = $this->_left;          // Border line style and color
            $border1       |= $this->_right         << 4;
            $border1       |= $this->_top           << 8;
            $border1       |= $this->_bottom        << 12;
            $border1       |= $this->_left_color    << 16;
            $border1       |= $this->_right_color   << 23;
            $diag_tl_to_rb = 0; // FIXME: add method
            $diag_tr_to_lb = 0; // FIXME: add method
            $border1       |= $diag_tl_to_rb        << 30;
            $border1       |= $diag_tr_to_lb        << 31;

            $border2        = $this->_top_color;    // Border color
            $border2       |= $this->_bottom_color   << 7;
            $border2       |= $this->_diag_color     << 14;
            $border2       |= $this->_diag           << 21;
            $border2       |= $this->_pattern        << 26;

            $header      = pack("vv",       $record, $length);

            $rotation      = 0x00;
            $biff8_options = 0x00;
            $data  = pack("vvvC", $ifnt, $ifmt, $style, $align);
            $data .= pack("CCC", $rotation, $biff8_options, $used_attrib);
            $data .= pack("VVv", $border1, $border2, $icv);
        }

        return($header . $data);
    }

    /**
    * Generate an Excel BIFF FONT record.
    *
    * @return string The FONT record
    */
    function getFont()
    {
        $dyHeight   = $this->_size * 20;    // Height of font (1/20 of a point)
        $icv        = $this->_color;        // Index to color palette
        $bls        = $this->_bold;         // Bold style
        $sss        = $this->_font_script;  // Superscript/subscript
        $uls        = $this->_underline;    // Underline
        $bFamily    = $this->_font_family;  // Font family
        $bCharSet   = $this->_font_charset; // Character set
        $encoding   = 0;                    // TODO: Unicode support

        $cch        = strlen($this->_font_name); // Length of font name
        $record     = 0x31;                      // Record identifier
        if ($this->_BIFF_version == 0x0500) {
            $length     = 0x0F + $cch;            // Record length
        } elseif ($this->_BIFF_version == 0x0600) {
            $length     = 0x10 + $cch;
        }
        $reserved   = 0x00;                // Reserved
        $grbit      = 0x00;                // Font attributes
        if ($this->_italic) {
            $grbit     |= 0x02;
        }
        if ($this->_font_strikeout) {
            $grbit     |= 0x08;
        }
        if ($this->_font_outline) {
            $grbit     |= 0x10;
        }
        if ($this->_font_shadow) {
            $grbit     |= 0x20;
        }

        $header  = pack("vv",         $record, $length);
        if ($this->_BIFF_version == 0x0500) {
            $data    = pack("vvvvvCCCCC", $dyHeight, $grbit, $icv, $bls,
                                          $sss, $uls, $bFamily,
                                          $bCharSet, $reserved, $cch);
        } elseif ($this->_BIFF_version == 0x0600) {
            $data    = pack("vvvvvCCCCCC", $dyHeight, $grbit, $icv, $bls,
                                           $sss, $uls, $bFamily,
                                           $bCharSet, $reserved, $cch, $encoding);
        }
        return($header . $data . $this->_font_name);
    }

    /**
    * Returns a unique hash key for a font.
    * Used by Spreadsheet_Excel_Writer_Workbook::_storeAllFonts()
    *
    * The elements that form the key are arranged to increase the probability of
    * generating a unique key. Elements that hold a large range of numbers
    * (eg. _color) are placed between two binary elements such as _italic
    *
    * @return string A key for this font
    */
    function getFontKey()
    {
        $key  = "$this->_font_name$this->_size";
        $key .= "$this->_font_script$this->_underline";
        $key .= "$this->_font_strikeout$this->_bold$this->_font_outline";
        $key .= "$this->_font_family$this->_font_charset";
        $key .= "$this->_font_shadow$this->_color$this->_italic";
        $key  = str_replace(' ', '_', $key);
        return ($key);
    }

    /**
    * Returns the index used by Spreadsheet_Excel_Writer_Worksheet::_XF()
    *
    * @return integer The index for the XF record
    */
    function getXfIndex()
    {
        return($this->_xf_index);
    }

    /**
    * Used in conjunction with the set_xxx_color methods to convert a color
    * string into a number. Color range is 0..63 but we will restrict it
    * to 8..63 to comply with Gnumeric. Colors 0..7 are repeated in 8..15.
    *
    * @access private
    * @param string $name_color name of the color (i.e.: 'blue', 'red', etc..). Optional.
    * @return integer The color index
    */
    function _getColor($name_color = '')
    {
        $colors = array(
                        'aqua'    => 0x0F,
                        'cyan'    => 0x0F,
                        'black'   => 0x08,
                        'blue'    => 0x0C,
                        'brown'   => 0x10,
                        'magenta' => 0x0E,
                        'fuchsia' => 0x0E,
                        'gray'    => 0x17,
                        'grey'    => 0x17,
                        'green'   => 0x11,
                        'lime'    => 0x0B,
                        'navy'    => 0x12,
                        'orange'  => 0x35,
                        'purple'  => 0x14,
                        'red'     => 0x0A,
                        'silver'  => 0x16,
                        'white'   => 0x09,
                        'yellow'  => 0x0D
                       );

        // Return the default color, 0x7FFF, if undef,
        if ($name_color == '') {
            return(0x7FFF);
        }

        // or the color string converted to an integer,
        if (isset($colors[$name_color])) {
            return($colors[$name_color]);
        }

        // or the default color if string is unrecognised,
        if (preg_match("/\D/",$name_color)) {
            return(0x7FFF);
        }

        // or an index < 8 mapped into the correct range,
        if ($name_color < 8) {
            return($name_color + 8);
        }

        // or the default color if arg is outside range,
        if ($name_color > 63) {
            return(0x7FFF);
        }

        // or an integer in the valid range
        return($name_color);
    }

    /**
    * Set cell alignment.
    *
    * @access public
    * @param string $location alignment for the cell ('left', 'right', etc...).
    */
    function setAlign($location)
    {
        if (preg_match("/\d/",$location)) {
            return;                      // Ignore numbers
        }

        $location = strtolower($location);

        if ($location == 'left') {
            $this->_text_h_align = 1;
        }
        if ($location == 'centre') {
            $this->_text_h_align = 2;
        }
        if ($location == 'center') {
            $this->_text_h_align = 2;
        }
        if ($location == 'right') {
            $this->_text_h_align = 3;
        }
        if ($location == 'fill') {
            $this->_text_h_align = 4;
        }
        if ($location == 'justify') {
            $this->_text_h_align = 5;
        }
        if ($location == 'merge') {
            $this->_text_h_align = 6;
        }
        if ($location == 'equal_space') { // For T.K.
            $this->_text_h_align = 7;
        }
        if ($location == 'top') {
            $this->_text_v_align = 0;
        }
        if ($location == 'vcentre') {
            $this->_text_v_align = 1;
        }
        if ($location == 'vcenter') {
            $this->_text_v_align = 1;
        }
        if ($location == 'bottom') {
            $this->_text_v_align = 2;
        }
        if ($location == 'vjustify') {
            $this->_text_v_align = 3;
        }
        if ($location == 'vequal_space') { // For T.K.
            $this->_text_v_align = 4;
        }
    }

    /**
    * Set cell horizontal alignment.
    *
    * @access public
    * @param string $location alignment for the cell ('left', 'right', etc...).
    */
    function setHAlign($location)
    {
        if (preg_match("/\d/",$location)) {
            return;                      // Ignore numbers
        }
    
        $location = strtolower($location);
    
        if ($location == 'left') {
            $this->_text_h_align = 1;
        }
        if ($location == 'centre') {
            $this->_text_h_align = 2;
        }
        if ($location == 'center') {
            $this->_text_h_align = 2;
        }
        if ($location == 'right') {
            $this->_text_h_align = 3;
        }
        if ($location == 'fill') {
            $this->_text_h_align = 4;
        }
        if ($location == 'justify') {
            $this->_text_h_align = 5;
        }
        if ($location == 'merge') {
            $this->_text_h_align = 6;
        }
        if ($location == 'equal_space') { // For T.K.
            $this->_text_h_align = 7;
        }
    }

    /**
    * Set cell vertical alignment.
    *
    * @access public
    * @param string $location alignment for the cell ('top', 'vleft', 'vright', etc...).
    */
    function setVAlign($location)
    {
        if (preg_match("/\d/",$location)) {
            return;                      // Ignore numbers
        }
    
        $location = strtolower($location);
 
        if ($location == 'top') {
            $this->_text_v_align = 0;
        }
        if ($location == 'vcentre') {
            $this->_text_v_align = 1;
        }
        if ($location == 'vcenter') {
            $this->_text_v_align = 1;
        }
        if ($location == 'bottom') {
            $this->_text_v_align = 2;
        }
        if ($location == 'vjustify') {
            $this->_text_v_align = 3;
        }
        if ($location == 'vequal_space') { // For T.K.
            $this->_text_v_align = 4;
        }
    }

    /**
    * This is an alias for the unintuitive setAlign('merge')
    *
    * @access public
    */
    function setMerge()
    {
        $this->setAlign('merge');
    }

    /**
    * Sets the boldness of the text.
    * Bold has a range 100..1000.
    * 0 (400) is normal. 1 (700) is bold.
    *
    * @access public
    * @param integer $weight Weight for the text, 0 maps to 400 (normal text),
                             1 maps to 700 (bold text). Valid range is: 100-1000.
                             It's Optional, default is 1 (bold).
    */
    function setBold($weight = 1)
    {
        if ($weight == 1) {
            $weight = 0x2BC;  // Bold text
        }
        if ($weight == 0) {
            $weight = 0x190;  // Normal text
        }
        if ($weight <  0x064) {
            $weight = 0x190;  // Lower bound
        }
        if ($weight >  0x3E8) {
            $weight = 0x190;  // Upper bound
        }
        $this->_bold = $weight;
    }


    /************************************
    * FUNCTIONS FOR SETTING CELLS BORDERS
    */

    /**
    * Sets the width for the bottom border of the cell
    *
    * @access public
    * @param integer $style style of the cell border. 1 => thin, 2 => thick.
    */
    function setBottom($style)
    {
        $this->_bottom = $style;
    }

    /**
    * Sets the width for the top border of the cell
    *
    * @access public
    * @param integer $style style of the cell top border. 1 => thin, 2 => thick.
    */
    function setTop($style)
    {
        $this->_top = $style;
    }

    /**
    * Sets the width for the left border of the cell
    *
    * @access public
    * @param integer $style style of the cell left border. 1 => thin, 2 => thick.
    */
    function setLeft($style)
    {
        $this->_left = $style;
    }

    /**
    * Sets the width for the right border of the cell
    *
    * @access public
    * @param integer $style style of the cell right border. 1 => thin, 2 => thick.
    */
    function setRight($style)
    {
        $this->_right = $style;
    }


    /**
    * Set cells borders to the same style
    *
    * @access public
    * @param integer $style style to apply for all cell borders. 1 => thin, 2 => thick.
    */
    function setBorder($style)
    {
        $this->setBottom($style);
        $this->setTop($style);
        $this->setLeft($style);
        $this->setRight($style);
    }


    /*******************************************
    * FUNCTIONS FOR SETTING CELLS BORDERS COLORS
    */

    /**
    * Sets all the cell's borders to the same color
    *
    * @access public
    * @param mixed $color The color we are setting. Either a string (like 'blue'),
    *                     or an integer (range is [8...63]).
    */
    function setBorderColor($color)
    {
        $this->setBottomColor($color);
        $this->setTopColor($color);
        $this->setLeftColor($color);
        $this->setRightColor($color);
    }

    /**
    * Sets the cell's bottom border color
    *
    * @access public
    * @param mixed $color either a string (like 'blue'), or an integer (range is [8...63]).
    */
    function setBottomColor($color)
    {
        $value = $this->_getColor($color);
        $this->_bottom_color = $value;
    }

    /**
    * Sets the cell's top border color
    *
    * @access public
    * @param mixed $color either a string (like 'blue'), or an integer (range is [8...63]).
    */
    function setTopColor($color)
    {
        $value = $this->_getColor($color);
        $this->_top_color = $value;
    }

    /**
    * Sets the cell's left border color
    *
    * @access public
    * @param mixed $color either a string (like 'blue'), or an integer (range is [8...63]).
    */
    function setLeftColor($color)
    {
        $value = $this->_getColor($color);
        $this->_left_color = $value;
    }

    /**
    * Sets the cell's right border color
    *
    * @access public
    * @param mixed $color either a string (like 'blue'), or an integer (range is [8...63]).
    */
    function setRightColor($color)
    {
        $value = $this->_getColor($color);
        $this->_right_color = $value;
    }


    /**
    * Sets the cell's foreground color
    *
    * @access public
    * @param mixed $color either a string (like 'blue'), or an integer (range is [8...63]).
    */
    function setFgColor($color)
    {
        $value = $this->_getColor($color);
        $this->_fg_color = $value;
        if ($this->_pattern == 0) { // force color to be seen
            $this->_pattern = 1;
        }
    }

    /**
    * Sets the cell's background color
    *
    * @access public
    * @param mixed $color either a string (like 'blue'), or an integer (range is [8...63]).
    */
    function setBgColor($color)
    {
        $value = $this->_getColor($color);
        $this->_bg_color = $value;
        if ($this->_pattern == 0) { // force color to be seen
            $this->_pattern = 1;
        }
    }

    /**
    * Sets the cell's color
    *
    * @access public
    * @param mixed $color either a string (like 'blue'), or an integer (range is [8...63]).
    */
    function setColor($color)
    {
        $value = $this->_getColor($color);
        $this->_color = $value;
    }

    /**
    * Sets the fill pattern attribute of a cell
    *
    * @access public
    * @param integer $arg Optional. Defaults to 1. Meaningful values are: 0-18,
    *                     0 meaning no background.
    */
    function setPattern($arg = 1)
    {
        $this->_pattern = $arg;
    }

    /**
    * Sets the underline of the text
    *
    * @access public
    * @param integer $underline The value for underline. Possible values are:
    *                          1 => underline, 2 => double underline.
    */
    function setUnderline($underline)
    {
        $this->_underline = $underline;
    }

    /**
    * Sets the font style as italic
    *
    * @access public
    */
    function setItalic()
    {
        $this->_italic = 1;
    }

    /**
    * Sets the font size
    *
    * @access public
    * @param integer $size The font size (in pixels I think).
    */
    function setSize($size)
    {
        $this->_size = $size;
    }

    /**
    * Sets text wrapping
    *
    * @access public
    */
    function setTextWrap()
    {
        $this->_text_wrap = 1;
    }

    /**
    * Sets the orientation of the text
    *
    * @access public
    * @param integer $angle The rotation angle for the text (clockwise). Possible
                            values are: 0, 90, 270 and -1 for stacking top-to-bottom.
    */
    function setTextRotation($angle)
    {
        switch ($angle)
        {
            case 0:
                $this->_rotation = 0;
                break;
            case 90:
                $this->_rotation = 3;
                break;
            case 270:
                $this->_rotation = 2;
                break;
            case -1:
                $this->_rotation = 1;
                break;
            default :
                $this->_rotation = 0;
                break;
        }
    }

    /**
    * Sets the numeric format.
    * It can be date, time, currency, etc...
    *
    * @access public
    * @param integer $num_format The numeric format.
    */
    function setNumFormat($num_format)
    {
        $this->_num_format = $num_format;
    }

    /**
    * Sets font as strikeout.
    *
    * @access public
    */
    function setStrikeOut()
    {
        $this->_font_strikeout = 1;
    }

    /**
    * Sets outlining for a font.
    *
    * @access public
    */
    function setOutLine()
    {
        $this->_font_outline = 1;
    }

    /**
    * Sets font as shadow.
    *
    * @access public
    */
    function setShadow()
    {
        $this->_font_shadow = 1;
    }

    /**
    * Sets the script type of the text
    *
    * @access public
    * @param integer $script The value for script type. Possible values are:
    *                        1 => superscript, 2 => subscript.
    */
    function setScript($script)
    {
        $this->_font_script = $script;
    }

     /**
     * Locks a cell.
     *
     * @access public
     */
     function setLocked()
     {
         $this->_locked = 1;
     }

    /**
    * Unlocks a cell. Useful for unprotecting particular cells of a protected sheet.
    *
    * @access public
    */
    function setUnLocked()
    {
        $this->_locked = 0;
    }

    /**
    * Sets the font family name.
    *
    * @access public
    * @param string $fontfamily The font family name. Possible values are:
    *                           'Times New Roman', 'Arial', 'Courier'.
    */
    function setFontFamily($font_family)
    {
        $this->_font_name = $font_family;
    }
}

/**
* Class for parsing Excel formulas
*
* @author   Xavier Noguer <xnoguer@rezebra.com>
* @category FileFormats
* @package  Spreadsheet_Excel_Writer
*/

class Spreadsheet_Excel_Writer_Parser
{
    /**
    * The index of the character we are currently looking at
    * @var integer
    */
    var $_current_char;

    /**
    * The token we are working on.
    * @var string
    */
    var $_current_token;

    /**
    * The formula to parse
    * @var string
    */
    var $_formula;

    /**
    * The character ahead of the current char
    * @var string
    */
    var $_lookahead;

    /**
    * The parse tree to be generated
    * @var string
    */
    var $_parse_tree;

    /**
    * The byte order. 1 => big endian, 0 => little endian.
    * @var integer
    */
    var $_byte_order;

    /**
    * Array of external sheets
    * @var array
    */
    var $_ext_sheets;

    /**
    * Array of sheet references in the form of REF structures
    * @var array
    */
    var $_references;

    /**
    * The BIFF version for the workbook
    * @var integer
    */
    var $_BIFF_version;

    /**
    * The class constructor
    *
    * @param integer $byte_order The byte order (Little endian or Big endian) of the architecture
                                 (optional). 1 => big endian, 0 (default) little endian.
    */
    function __construct($byte_order, $biff_version)
    {
        $this->_current_char  = 0;
        $this->_BIFF_version  = $biff_version;
        $this->_current_token = '';       // The token we are working on.
        $this->_formula       = '';       // The formula to parse.
        $this->_lookahead     = '';       // The character ahead of the current char.
        $this->_parse_tree    = '';       // The parse tree to be generated.
        $this->_initializeHashes();      // Initialize the hashes: ptg's and function's ptg's
        $this->_byte_order = $byte_order; // Little Endian or Big Endian
        $this->_ext_sheets = array();
        $this->_references = array();
    }

    /**
    * Initialize the ptg and function hashes.
    *
    * @access private
    */
    function _initializeHashes()
    {
        // The Excel ptg indices
        $this->ptg = array(
            'ptgExp'       => 0x01,
            'ptgTbl'       => 0x02,
            'ptgAdd'       => 0x03,
            'ptgSub'       => 0x04,
            'ptgMul'       => 0x05,
            'ptgDiv'       => 0x06,
            'ptgPower'     => 0x07,
            'ptgConcat'    => 0x08,
            'ptgLT'        => 0x09,
            'ptgLE'        => 0x0A,
            'ptgEQ'        => 0x0B,
            'ptgGE'        => 0x0C,
            'ptgGT'        => 0x0D,
            'ptgNE'        => 0x0E,
            'ptgIsect'     => 0x0F,
            'ptgUnion'     => 0x10,
            'ptgRange'     => 0x11,
            'ptgUplus'     => 0x12,
            'ptgUminus'    => 0x13,
            'ptgPercent'   => 0x14,
            'ptgParen'     => 0x15,
            'ptgMissArg'   => 0x16,
            'ptgStr'       => 0x17,
            'ptgAttr'      => 0x19,
            'ptgSheet'     => 0x1A,
            'ptgEndSheet'  => 0x1B,
            'ptgErr'       => 0x1C,
            'ptgBool'      => 0x1D,
            'ptgInt'       => 0x1E,
            'ptgNum'       => 0x1F,
            'ptgArray'     => 0x20,
            'ptgFunc'      => 0x21,
            'ptgFuncVar'   => 0x22,
            'ptgName'      => 0x23,
            'ptgRef'       => 0x24,
            'ptgArea'      => 0x25,
            'ptgMemArea'   => 0x26,
            'ptgMemErr'    => 0x27,
            'ptgMemNoMem'  => 0x28,
            'ptgMemFunc'   => 0x29,
            'ptgRefErr'    => 0x2A,
            'ptgAreaErr'   => 0x2B,
            'ptgRefN'      => 0x2C,
            'ptgAreaN'     => 0x2D,
            'ptgMemAreaN'  => 0x2E,
            'ptgMemNoMemN' => 0x2F,
            'ptgNameX'     => 0x39,
            'ptgRef3d'     => 0x3A,
            'ptgArea3d'    => 0x3B,
            'ptgRefErr3d'  => 0x3C,
            'ptgAreaErr3d' => 0x3D,
            'ptgArrayV'    => 0x40,
            'ptgFuncV'     => 0x41,
            'ptgFuncVarV'  => 0x42,
            'ptgNameV'     => 0x43,
            'ptgRefV'      => 0x44,
            'ptgAreaV'     => 0x45,
            'ptgMemAreaV'  => 0x46,
            'ptgMemErrV'   => 0x47,
            'ptgMemNoMemV' => 0x48,
            'ptgMemFuncV'  => 0x49,
            'ptgRefErrV'   => 0x4A,
            'ptgAreaErrV'  => 0x4B,
            'ptgRefNV'     => 0x4C,
            'ptgAreaNV'    => 0x4D,
            'ptgMemAreaNV' => 0x4E,
            'ptgMemNoMemN' => 0x4F,
            'ptgFuncCEV'   => 0x58,
            'ptgNameXV'    => 0x59,
            'ptgRef3dV'    => 0x5A,
            'ptgArea3dV'   => 0x5B,
            'ptgRefErr3dV' => 0x5C,
            'ptgAreaErr3d' => 0x5D,
            'ptgArrayA'    => 0x60,
            'ptgFuncA'     => 0x61,
            'ptgFuncVarA'  => 0x62,
            'ptgNameA'     => 0x63,
            'ptgRefA'      => 0x64,
            'ptgAreaA'     => 0x65,
            'ptgMemAreaA'  => 0x66,
            'ptgMemErrA'   => 0x67,
            'ptgMemNoMemA' => 0x68,
            'ptgMemFuncA'  => 0x69,
            'ptgRefErrA'   => 0x6A,
            'ptgAreaErrA'  => 0x6B,
            'ptgRefNA'     => 0x6C,
            'ptgAreaNA'    => 0x6D,
            'ptgMemAreaNA' => 0x6E,
            'ptgMemNoMemN' => 0x6F,
            'ptgFuncCEA'   => 0x78,
            'ptgNameXA'    => 0x79,
            'ptgRef3dA'    => 0x7A,
            'ptgArea3dA'   => 0x7B,
            'ptgRefErr3dA' => 0x7C,
            'ptgAreaErr3d' => 0x7D
            );

        // Thanks to Michael Meeks and Gnumeric for the initial arg values.
        //
        // The following hash was generated by "function_locale.pl" in the distro.
        // Refer to function_locale.pl for non-English function names.
        //
        // The array elements are as follow:
        // ptg:   The Excel function ptg code.
        // args:  The number of arguments that the function takes:
        //           >=0 is a fixed number of arguments.
        //           -1  is a variable  number of arguments.
        // class: The reference, value or array class of the function args.
        // vol:   The function is volatile.
        //
        $this->_functions = array(
              // function                  ptg  args  class  vol
              'COUNT'           => array(   0,   -1,    0,    0 ),
              'IF'              => array(   1,   -1,    1,    0 ),
              'ISNA'            => array(   2,    1,    1,    0 ),
              'ISERROR'         => array(   3,    1,    1,    0 ),
              'SUM'             => array(   4,   -1,    0,    0 ),
              'AVERAGE'         => array(   5,   -1,    0,    0 ),
              'MIN'             => array(   6,   -1,    0,    0 ),
              'MAX'             => array(   7,   -1,    0,    0 ),
              'ROW'             => array(   8,   -1,    0,    0 ),
              'COLUMN'          => array(   9,   -1,    0,    0 ),
              'NA'              => array(  10,    0,    0,    0 ),
              'NPV'             => array(  11,   -1,    1,    0 ),
              'STDEV'           => array(  12,   -1,    0,    0 ),
              'DOLLAR'          => array(  13,   -1,    1,    0 ),
              'FIXED'           => array(  14,   -1,    1,    0 ),
              'SIN'             => array(  15,    1,    1,    0 ),
              'COS'             => array(  16,    1,    1,    0 ),
              'TAN'             => array(  17,    1,    1,    0 ),
              'ATAN'            => array(  18,    1,    1,    0 ),
              'PI'              => array(  19,    0,    1,    0 ),
              'SQRT'            => array(  20,    1,    1,    0 ),
              'EXP'             => array(  21,    1,    1,    0 ),
              'LN'              => array(  22,    1,    1,    0 ),
              'LOG10'           => array(  23,    1,    1,    0 ),
              'ABS'             => array(  24,    1,    1,    0 ),
              'INT'             => array(  25,    1,    1,    0 ),
              'SIGN'            => array(  26,    1,    1,    0 ),
              'ROUND'           => array(  27,    2,    1,    0 ),
              'LOOKUP'          => array(  28,   -1,    0,    0 ),
              'INDEX'           => array(  29,   -1,    0,    1 ),
              'REPT'            => array(  30,    2,    1,    0 ),
              'MID'             => array(  31,    3,    1,    0 ),
              'LEN'             => array(  32,    1,    1,    0 ),
              'VALUE'           => array(  33,    1,    1,    0 ),
              'TRUE'            => array(  34,    0,    1,    0 ),
              'FALSE'           => array(  35,    0,    1,    0 ),
              'AND'             => array(  36,   -1,    0,    0 ),
              'OR'              => array(  37,   -1,    0,    0 ),
              'NOT'             => array(  38,    1,    1,    0 ),
              'MOD'             => array(  39,    2,    1,    0 ),
              'DCOUNT'          => array(  40,    3,    0,    0 ),
              'DSUM'            => array(  41,    3,    0,    0 ),
              'DAVERAGE'        => array(  42,    3,    0,    0 ),
              'DMIN'            => array(  43,    3,    0,    0 ),
              'DMAX'            => array(  44,    3,    0,    0 ),
              'DSTDEV'          => array(  45,    3,    0,    0 ),
              'VAR'             => array(  46,   -1,    0,    0 ),
              'DVAR'            => array(  47,    3,    0,    0 ),
              'TEXT'            => array(  48,    2,    1,    0 ),
              'LINEST'          => array(  49,   -1,    0,    0 ),
              'TREND'           => array(  50,   -1,    0,    0 ),
              'LOGEST'          => array(  51,   -1,    0,    0 ),
              'GROWTH'          => array(  52,   -1,    0,    0 ),
              'PV'              => array(  56,   -1,    1,    0 ),
              'FV'              => array(  57,   -1,    1,    0 ),
              'NPER'            => array(  58,   -1,    1,    0 ),
              'PMT'             => array(  59,   -1,    1,    0 ),
              'RATE'            => array(  60,   -1,    1,    0 ),
              'MIRR'            => array(  61,    3,    0,    0 ),
              'IRR'             => array(  62,   -1,    0,    0 ),
              'RAND'            => array(  63,    0,    1,    1 ),
              'MATCH'           => array(  64,   -1,    0,    0 ),
              'DATE'            => array(  65,    3,    1,    0 ),
              'TIME'            => array(  66,    3,    1,    0 ),
              'DAY'             => array(  67,    1,    1,    0 ),
              'MONTH'           => array(  68,    1,    1,    0 ),
              'YEAR'            => array(  69,    1,    1,    0 ),
              'WEEKDAY'         => array(  70,   -1,    1,    0 ),
              'HOUR'            => array(  71,    1,    1,    0 ),
              'MINUTE'          => array(  72,    1,    1,    0 ),
              'SECOND'          => array(  73,    1,    1,    0 ),
              'NOW'             => array(  74,    0,    1,    1 ),
              'AREAS'           => array(  75,    1,    0,    1 ),
              'ROWS'            => array(  76,    1,    0,    1 ),
              'COLUMNS'         => array(  77,    1,    0,    1 ),
              'OFFSET'          => array(  78,   -1,    0,    1 ),
              'SEARCH'          => array(  82,   -1,    1,    0 ),
              'TRANSPOSE'       => array(  83,    1,    1,    0 ),
              'TYPE'            => array(  86,    1,    1,    0 ),
              'ATAN2'           => array(  97,    2,    1,    0 ),
              'ASIN'            => array(  98,    1,    1,    0 ),
              'ACOS'            => array(  99,    1,    1,    0 ),
              'CHOOSE'          => array( 100,   -1,    1,    0 ),
              'HLOOKUP'         => array( 101,   -1,    0,    0 ),
              'VLOOKUP'         => array( 102,   -1,    0,    0 ),
              'ISREF'           => array( 105,    1,    0,    0 ),
              'LOG'             => array( 109,   -1,    1,    0 ),
              'CHAR'            => array( 111,    1,    1,    0 ),
              'LOWER'           => array( 112,    1,    1,    0 ),
              'UPPER'           => array( 113,    1,    1,    0 ),
              'PROPER'          => array( 114,    1,    1,    0 ),
              'LEFT'            => array( 115,   -1,    1,    0 ),
              'RIGHT'           => array( 116,   -1,    1,    0 ),
              'EXACT'           => array( 117,    2,    1,    0 ),
              'TRIM'            => array( 118,    1,    1,    0 ),
              'REPLACE'         => array( 119,    4,    1,    0 ),
              'SUBSTITUTE'      => array( 120,   -1,    1,    0 ),
              'CODE'            => array( 121,    1,    1,    0 ),
              'FIND'            => array( 124,   -1,    1,    0 ),
              'CELL'            => array( 125,   -1,    0,    1 ),
              'ISERR'           => array( 126,    1,    1,    0 ),
              'ISTEXT'          => array( 127,    1,    1,    0 ),
              'ISNUMBER'        => array( 128,    1,    1,    0 ),
              'ISBLANK'         => array( 129,    1,    1,    0 ),
              'T'               => array( 130,    1,    0,    0 ),
              'N'               => array( 131,    1,    0,    0 ),
              'DATEVALUE'       => array( 140,    1,    1,    0 ),
              'TIMEVALUE'       => array( 141,    1,    1,    0 ),
              'SLN'             => array( 142,    3,    1,    0 ),
              'SYD'             => array( 143,    4,    1,    0 ),
              'DDB'             => array( 144,   -1,    1,    0 ),
              'INDIRECT'        => array( 148,   -1,    1,    1 ),
              'CALL'            => array( 150,   -1,    1,    0 ),
              'CLEAN'           => array( 162,    1,    1,    0 ),
              'MDETERM'         => array( 163,    1,    2,    0 ),
              'MINVERSE'        => array( 164,    1,    2,    0 ),
              'MMULT'           => array( 165,    2,    2,    0 ),
              'IPMT'            => array( 167,   -1,    1,    0 ),
              'PPMT'            => array( 168,   -1,    1,    0 ),
              'COUNTA'          => array( 169,   -1,    0,    0 ),
              'PRODUCT'         => array( 183,   -1,    0,    0 ),
              'FACT'            => array( 184,    1,    1,    0 ),
              'DPRODUCT'        => array( 189,    3,    0,    0 ),
              'ISNONTEXT'       => array( 190,    1,    1,    0 ),
              'STDEVP'          => array( 193,   -1,    0,    0 ),
              'VARP'            => array( 194,   -1,    0,    0 ),
              'DSTDEVP'         => array( 195,    3,    0,    0 ),
              'DVARP'           => array( 196,    3,    0,    0 ),
              'TRUNC'           => array( 197,   -1,    1,    0 ),
              'ISLOGICAL'       => array( 198,    1,    1,    0 ),
              'DCOUNTA'         => array( 199,    3,    0,    0 ),
              'ROUNDUP'         => array( 212,    2,    1,    0 ),
              'ROUNDDOWN'       => array( 213,    2,    1,    0 ),
              'RANK'            => array( 216,   -1,    0,    0 ),
              'ADDRESS'         => array( 219,   -1,    1,    0 ),
              'DAYS360'         => array( 220,   -1,    1,    0 ),
              'TODAY'           => array( 221,    0,    1,    1 ),
              'VDB'             => array( 222,   -1,    1,    0 ),
              'MEDIAN'          => array( 227,   -1,    0,    0 ),
              'SUMPRODUCT'      => array( 228,   -1,    2,    0 ),
              'SINH'            => array( 229,    1,    1,    0 ),
              'COSH'            => array( 230,    1,    1,    0 ),
              'TANH'            => array( 231,    1,    1,    0 ),
              'ASINH'           => array( 232,    1,    1,    0 ),
              'ACOSH'           => array( 233,    1,    1,    0 ),
              'ATANH'           => array( 234,    1,    1,    0 ),
              'DGET'            => array( 235,    3,    0,    0 ),
              'INFO'            => array( 244,    1,    1,    1 ),
              'DB'              => array( 247,   -1,    1,    0 ),
              'FREQUENCY'       => array( 252,    2,    0,    0 ),
              'ERROR.TYPE'      => array( 261,    1,    1,    0 ),
              'REGISTER.ID'     => array( 267,   -1,    1,    0 ),
              'AVEDEV'          => array( 269,   -1,    0,    0 ),
              'BETADIST'        => array( 270,   -1,    1,    0 ),
              'GAMMALN'         => array( 271,    1,    1,    0 ),
              'BETAINV'         => array( 272,   -1,    1,    0 ),
              'BINOMDIST'       => array( 273,    4,    1,    0 ),
              'CHIDIST'         => array( 274,    2,    1,    0 ),
              'CHIINV'          => array( 275,    2,    1,    0 ),
              'COMBIN'          => array( 276,    2,    1,    0 ),
              'CONFIDENCE'      => array( 277,    3,    1,    0 ),
              'CRITBINOM'       => array( 278,    3,    1,    0 ),
              'EVEN'            => array( 279,    1,    1,    0 ),
              'EXPONDIST'       => array( 280,    3,    1,    0 ),
              'FDIST'           => array( 281,    3,    1,    0 ),
              'FINV'            => array( 282,    3,    1,    0 ),
              'FISHER'          => array( 283,    1,    1,    0 ),
              'FISHERINV'       => array( 284,    1,    1,    0 ),
              'FLOOR'           => array( 285,    2,    1,    0 ),
              'GAMMADIST'       => array( 286,    4,    1,    0 ),
              'GAMMAINV'        => array( 287,    3,    1,    0 ),
              'CEILING'         => array( 288,    2,    1,    0 ),
              'HYPGEOMDIST'     => array( 289,    4,    1,    0 ),
              'LOGNORMDIST'     => array( 290,    3,    1,    0 ),
              'LOGINV'          => array( 291,    3,    1,    0 ),
              'NEGBINOMDIST'    => array( 292,    3,    1,    0 ),
              'NORMDIST'        => array( 293,    4,    1,    0 ),
              'NORMSDIST'       => array( 294,    1,    1,    0 ),
              'NORMINV'         => array( 295,    3,    1,    0 ),
              'NORMSINV'        => array( 296,    1,    1,    0 ),
              'STANDARDIZE'     => array( 297,    3,    1,    0 ),
              'ODD'             => array( 298,    1,    1,    0 ),
              'PERMUT'          => array( 299,    2,    1,    0 ),
              'POISSON'         => array( 300,    3,    1,    0 ),
              'TDIST'           => array( 301,    3,    1,    0 ),
              'WEIBULL'         => array( 302,    4,    1,    0 ),
              'SUMXMY2'         => array( 303,    2,    2,    0 ),
              'SUMX2MY2'        => array( 304,    2,    2,    0 ),
              'SUMX2PY2'        => array( 305,    2,    2,    0 ),
              'CHITEST'         => array( 306,    2,    2,    0 ),
              'CORREL'          => array( 307,    2,    2,    0 ),
              'COVAR'           => array( 308,    2,    2,    0 ),
              'FORECAST'        => array( 309,    3,    2,    0 ),
              'FTEST'           => array( 310,    2,    2,    0 ),
              'INTERCEPT'       => array( 311,    2,    2,    0 ),
              'PEARSON'         => array( 312,    2,    2,    0 ),
              'RSQ'             => array( 313,    2,    2,    0 ),
              'STEYX'           => array( 314,    2,    2,    0 ),
              'SLOPE'           => array( 315,    2,    2,    0 ),
              'TTEST'           => array( 316,    4,    2,    0 ),
              'PROB'            => array( 317,   -1,    2,    0 ),
              'DEVSQ'           => array( 318,   -1,    0,    0 ),
              'GEOMEAN'         => array( 319,   -1,    0,    0 ),
              'HARMEAN'         => array( 320,   -1,    0,    0 ),
              'SUMSQ'           => array( 321,   -1,    0,    0 ),
              'KURT'            => array( 322,   -1,    0,    0 ),
              'SKEW'            => array( 323,   -1,    0,    0 ),
              'ZTEST'           => array( 324,   -1,    0,    0 ),
              'LARGE'           => array( 325,    2,    0,    0 ),
              'SMALL'           => array( 326,    2,    0,    0 ),
              'QUARTILE'        => array( 327,    2,    0,    0 ),
              'PERCENTILE'      => array( 328,    2,    0,    0 ),
              'PERCENTRANK'     => array( 329,   -1,    0,    0 ),
              'MODE'            => array( 330,   -1,    2,    0 ),
              'TRIMMEAN'        => array( 331,    2,    0,    0 ),
              'TINV'            => array( 332,    2,    1,    0 ),
              'CONCATENATE'     => array( 336,   -1,    1,    0 ),
              'POWER'           => array( 337,    2,    1,    0 ),
              'RADIANS'         => array( 342,    1,    1,    0 ),
              'DEGREES'         => array( 343,    1,    1,    0 ),
              'SUBTOTAL'        => array( 344,   -1,    0,    0 ),
              'SUMIF'           => array( 345,   -1,    0,    0 ),
              'COUNTIF'         => array( 346,    2,    0,    0 ),
              'COUNTBLANK'      => array( 347,    1,    0,    0 ),
              'ROMAN'           => array( 354,   -1,    1,    0 )
              );
    }

    /**
    * Convert a token to the proper ptg value.
    *
    * @access private
    * @param mixed $token The token to convert.
    * @return mixed the converted token on success. Die if the token
    *               is not recognized
    */
    function _convert($token)
    {
        if (preg_match("/^\"[^\"]{0,255}\"$/", $token)) {
            return $this->_convertString($token);

        } elseif (is_numeric($token)) {
            return $this->_convertNumber($token);

        // match references like A1 or $A$1
        } elseif (preg_match('/^\$?([A-Ia-i]?[A-Za-z])\$?(\d+)$/',$token)) {
            return $this->_convertRef2d($token);

        // match external references like Sheet1!A1 or Sheet1:Sheet2!A1
        } elseif (preg_match("/^\w+(\:\w+)?\![A-Ia-i]?[A-Za-z](\d+)$/u",$token)) {
            return $this->_convertRef3d($token);

        // match external references like 'Sheet1'!A1 or 'Sheet1:Sheet2'!A1
        } elseif (preg_match("/^'[\w -]+(\:[\w -]+)?'\![A-Ia-i]?[A-Za-z](\d+)$/u",$token)) {
            return $this->_convertRef3d($token);

        // match ranges like A1:B2
        } elseif (preg_match("/^(\$)?[A-Ia-i]?[A-Za-z](\$)?(\d+)\:(\$)?[A-Ia-i]?[A-Za-z](\$)?(\d+)$/",$token)) {
            return $this->_convertRange2d($token);

        // match ranges like A1..B2
        } elseif (preg_match("/^(\$)?[A-Ia-i]?[A-Za-z](\$)?(\d+)\.\.(\$)?[A-Ia-i]?[A-Za-z](\$)?(\d+)$/",$token)) {
            return $this->_convertRange2d($token);

        // match external ranges like Sheet1!A1 or Sheet1:Sheet2!A1:B2
        } elseif (preg_match("/^\w+(\:\w+)?\!([A-Ia-i]?[A-Za-z])?(\d+)\:([A-Ia-i]?[A-Za-z])?(\d+)$/u",$token)) {
            return $this->_convertRange3d($token);

        // match external ranges like 'Sheet1'!A1 or 'Sheet1:Sheet2'!A1:B2
        } elseif (preg_match("/^'[\w -]+(\:[\w -]+)?'\!([A-Ia-i]?[A-Za-z])?(\d+)\:([A-Ia-i]?[A-Za-z])?(\d+)$/u",$token)) {
            return $this->_convertRange3d($token);

        // operators (including parentheses)
        } elseif (isset($this->ptg[$token])) {
            return pack("C", $this->ptg[$token]);

        // commented so argument number can be processed correctly. See toReversePolish().
        /*elseif (preg_match("/[A-Z0-9\xc0-\xdc\.]+/",$token))
        {
            return($this->_convertFunction($token,$this->_func_args));
        }*/

        // if it's an argument, ignore the token (the argument remains)
        } elseif ($token == 'arg') {
            return '';
        }
        // TODO: use real error codes
        die("Unknown token $token");
    }

    /**
    * Convert a number token to ptgInt or ptgNum
    *
    * @access private
    * @param mixed $num an integer or double for conversion to its ptg value
    */
    function _convertNumber($num)
    {
        // Integer in the range 0..2**16-1
        if ((preg_match("/^\d+$/", $num)) and ($num <= 65535)) {
            return pack("Cv", $this->ptg['ptgInt'], $num);
        } else { // A float
            if ($this->_byte_order) { // if it's Big Endian
                $num = strrev($num);
            }
            return pack("Cd", $this->ptg['ptgNum'], $num);
        }
    }

    /**
    * Convert a string token to ptgStr
    *
    * @access private
    * @param string $string A string for conversion to its ptg value.
    * @return mixed the converted token on success. PEAR_Error if the string
    *               is longer than 255 characters.
    */
    function _convertString($string)
    {
        // chop away beggining and ending quotes
        $string = substr($string, 1, strlen($string) - 2);
        if (strlen($string) > 255) {
            die("String is too long");
        }

        if ($this->_BIFF_version == 0x0500) {
            return pack("CC", $this->ptg['ptgStr'], strlen($string)).$string;
        } elseif ($this->_BIFF_version == 0x0600) {
            $encoding = 0;   // TODO: Unicode support
            return pack("CCC", $this->ptg['ptgStr'], strlen($string), $encoding).$string;
        }
    }

    /**
    * Convert a function to a ptgFunc or ptgFuncVarV depending on the number of
    * args that it takes.
    *
    * @access private
    * @param string  $token    The name of the function for convertion to ptg value.
    * @param integer $num_args The number of arguments the function receives.
    * @return string The packed ptg for the function
    */
    function _convertFunction($token, $num_args)
    {
        $args     = $this->_functions[$token][1];
        $volatile = $this->_functions[$token][3];

        // Fixed number of args eg. TIME($i,$j,$k).
        if ($args >= 0) {
            return pack("Cv", $this->ptg['ptgFuncV'], $this->_functions[$token][0]);
        }
        // Variable number of args eg. SUM($i,$j,$k, ..).
        if ($args == -1) {
            return pack("CCv", $this->ptg['ptgFuncVarV'], $num_args, $this->_functions[$token][0]);
        }
    }

    /**
    * Convert an Excel range such as A1:D4 to a ptgRefV.
    *
    * @access private
    * @param string $range An Excel range in the A1:A2 or A1..A2 format.
    */
    function _convertRange2d($range)
    {
        $class = 2; // as far as I know, this is magick.

        // Split the range into 2 cell refs
        if (preg_match("/^([A-Ia-i]?[A-Za-z])(\d+)\:([A-Ia-i]?[A-Za-z])(\d+)$/", $range)) {
            list($cell1, $cell2) = preg_split('/:/', $range);
        } elseif (preg_match("/^([A-Ia-i]?[A-Za-z])(\d+)\.\.([A-Ia-i]?[A-Za-z])(\d+)$/", $range)) {
            list($cell1, $cell2) = preg_split('/\.\./', $range);

        } else {
            // TODO: use real error codes
            die("Unknown range separator");
        }

        // Convert the cell references
        $cell_array1 = $this->_cellToPackedRowcol($cell1);
        list($row1, $col1) = $cell_array1;
        $cell_array2 = $this->_cellToPackedRowcol($cell2);
        list($row2, $col2) = $cell_array2;

        // The ptg value depends on the class of the ptg.
        if ($class == 0) {
            $ptgArea = pack("C", $this->ptg['ptgArea']);
        } elseif ($class == 1) {
            $ptgArea = pack("C", $this->ptg['ptgAreaV']);
        } elseif ($class == 2) {
            $ptgArea = pack("C", $this->ptg['ptgAreaA']);
        } else {
            // TODO: use real error codes
            die("Unknown class $class");
        }
        return $ptgArea . $row1 . $row2 . $col1. $col2;
    }

    /**
    * Convert an Excel 3d range such as "Sheet1!A1:D4" or "Sheet1:Sheet2!A1:D4" to
    * a ptgArea3d.
    *
    * @access private
    * @param string $token An Excel range in the Sheet1!A1:A2 format.
    * @return mixed The packed ptgArea3d token on success, PEAR_Error on failure.
    */
    function _convertRange3d($token)
    {
        $class = 2; // as far as I know, this is magick.

        // Split the ref at the ! symbol
        list($ext_ref, $range) = preg_split('/!/', $token);

        // Convert the external reference part (different for BIFF8)
        if ($this->_BIFF_version == 0x0500) {
            $ext_ref = $this->_packExtRef($ext_ref);
        } elseif ($this->_BIFF_version == 0x0600) {
             $ext_ref = $this->_getRefIndex($ext_ref);
        }

        // Split the range into 2 cell refs
        list($cell1, $cell2) = preg_split('/:/', $range);

        // Convert the cell references
        if (preg_match("/^(\$)?[A-Ia-i]?[A-Za-z](\$)?(\d+)$/", $cell1)) {
            $cell_array1 = $this->_cellToPackedRowcol($cell1);
            list($row1, $col1) = $cell_array1;
            $cell_array2 = $this->_cellToPackedRowcol($cell2);
            list($row2, $col2) = $cell_array2;
        } else { // It's a rows range (like 26:27)
             $cells_array = $this->_rangeToPackedRange($cell1.':'.$cell2);
             list($row1, $col1, $row2, $col2) = $cells_array;
        }

        // The ptg value depends on the class of the ptg.
        if ($class == 0) {
            $ptgArea = pack("C", $this->ptg['ptgArea3d']);
        } elseif ($class == 1) {
            $ptgArea = pack("C", $this->ptg['ptgArea3dV']);
        } elseif ($class == 2) {
            $ptgArea = pack("C", $this->ptg['ptgArea3dA']);
        } else {
            die("Unknown class $class");
        }

        return $ptgArea . $ext_ref . $row1 . $row2 . $col1. $col2;
    }

    /**
    * Convert an Excel reference such as A1, $B2, C$3 or $D$4 to a ptgRefV.
    *
    * @access private
    * @param string $cell An Excel cell reference
    * @return string The cell in packed() format with the corresponding ptg
    */
    function _convertRef2d($cell)
    {
        $class = 2; // as far as I know, this is magick.

        // Convert the cell reference
        $cell_array = $this->_cellToPackedRowcol($cell);
        list($row, $col) = $cell_array;

        // The ptg value depends on the class of the ptg.
        if ($class == 0) {
            $ptgRef = pack("C", $this->ptg['ptgRef']);
        } elseif ($class == 1) {
            $ptgRef = pack("C", $this->ptg['ptgRefV']);
        } elseif ($class == 2) {
            $ptgRef = pack("C", $this->ptg['ptgRefA']);
        } else {
            // TODO: use real error codes
            die("Unknown class $class");
        }
        return $ptgRef.$row.$col;
    }

    /**
    * Convert an Excel 3d reference such as "Sheet1!A1" or "Sheet1:Sheet2!A1" to a
    * ptgRef3d.
    *
    * @access private
    * @param string $cell An Excel cell reference
    * @return mixed The packed ptgRef3d token on success, PEAR_Error on failure.
    */
    function _convertRef3d($cell)
    {
        $class = 2; // as far as I know, this is magick.

        // Split the ref at the ! symbol
        list($ext_ref, $cell) = preg_split('/!/', $cell);

        // Convert the external reference part (different for BIFF8)
        if ($this->_BIFF_version == 0x0500) {
            $ext_ref = $this->_packExtRef($ext_ref);
        } elseif ($this->_BIFF_version == 0x0600) {
            $ext_ref = $this->_getRefIndex($ext_ref);
        }

        // Convert the cell reference part
        list($row, $col) = $this->_cellToPackedRowcol($cell);

        // The ptg value depends on the class of the ptg.
        if ($class == 0) {
            $ptgRef = pack("C", $this->ptg['ptgRef3d']);
        } elseif ($class == 1) {
            $ptgRef = pack("C", $this->ptg['ptgRef3dV']);
        } elseif ($class == 2) {
            $ptgRef = pack("C", $this->ptg['ptgRef3dA']);
        } else {
            die("Unknown class $class");
        }

        return $ptgRef . $ext_ref. $row . $col;
    }

    /**
    * Convert the sheet name part of an external reference, for example "Sheet1" or
    * "Sheet1:Sheet2", to a packed structure.
    *
    * @access private
    * @param string $ext_ref The name of the external reference
    * @return string The reference index in packed() format
    */
    function _packExtRef($ext_ref)
    {
        $ext_ref = preg_replace("/^'/", '', $ext_ref); // Remove leading  ' if any.
        $ext_ref = preg_replace("/'$/", '', $ext_ref); // Remove trailing ' if any.

        // Check if there is a sheet range eg., Sheet1:Sheet2.
        if (preg_match("/:/", $ext_ref)) {
            list($sheet_name1, $sheet_name2) = preg_split('/:/', $ext_ref);

            $sheet1 = $this->_getSheetIndex($sheet_name1);
            if ($sheet1 == -1) {
                die("Unknown sheet name $sheet_name1 in formula");
            }
            $sheet2 = $this->_getSheetIndex($sheet_name2);
            if ($sheet2 == -1) {
                die("Unknown sheet name $sheet_name2 in formula");
            }

            // Reverse max and min sheet numbers if necessary
            if ($sheet1 > $sheet2) {
                list($sheet1, $sheet2) = array($sheet2, $sheet1);
            }
        } else { // Single sheet name only.
            $sheet1 = $this->_getSheetIndex($ext_ref);
            if ($sheet1 == -1) {
                die("Unknown sheet name $ext_ref in formula");
            }
            $sheet2 = $sheet1;
        }

        // References are stored relative to 0xFFFF.
        $offset = -1 - $sheet1;

        return pack('vdvv', $offset, 0x00, $sheet1, $sheet2);
    }

    /**
    * Look up the REF index that corresponds to an external sheet name
    * (or range). If it doesn't exist yet add it to the workbook's references
    * array. It assumes all sheet names given must exist.
    *
    * @access private
    * @param string $ext_ref The name of the external reference
    * @return mixed The reference index in packed() format on success,
    *               PEAR_Error on failure
    */
    function _getRefIndex($ext_ref)
    {
        $ext_ref = preg_replace("/^'/", '', $ext_ref); // Remove leading  ' if any.
        $ext_ref = preg_replace("/'$/", '', $ext_ref); // Remove trailing ' if any.

        // Check if there is a sheet range eg., Sheet1:Sheet2.
        if (preg_match("/:/", $ext_ref)) {
            list($sheet_name1, $sheet_name2) = preg_split('/:/', $ext_ref);

            $sheet1 = $this->_getSheetIndex($sheet_name1);
            if ($sheet1 == -1) {
                die("Unknown sheet name $sheet_name1 in formula");
            }
            $sheet2 = $this->_getSheetIndex($sheet_name2);
            if ($sheet2 == -1) {
                die("Unknown sheet name $sheet_name2 in formula");
            }

            // Reverse max and min sheet numbers if necessary
            if ($sheet1 > $sheet2) {
                list($sheet1, $sheet2) = array($sheet2, $sheet1);
            }
        } else { // Single sheet name only.
            $sheet1 = $this->_getSheetIndex($ext_ref);
            if ($sheet1 == -1) {
                die("Unknown sheet name $ext_ref in formula");
            }
            $sheet2 = $sheet1;
        }

        // assume all references belong to this document
        $supbook_index = 0x00;
        $ref = pack('vvv', $supbook_index, $sheet1, $sheet2);
        $total_references = count($this->_references);
        $index = -1;
        for ($i = 0; $i < $total_references; $i++) {
            if ($ref == $this->_references[$i]) {
                $index = $i;
                break;
            }
        }
        // if REF was not found add it to references array
        if ($index == -1) {
            $this->_references[$total_references] = $ref;
            $index = $total_references;
        }

        return pack('v', $index);
    }

    /**
    * Look up the index that corresponds to an external sheet name. The hash of
    * sheet names is updated by the addworksheet() method of the
    * Spreadsheet_Excel_Writer_Workbook class.
    *
    * @access private
    * @return integer The sheet index, -1 if the sheet was not found
    */
    function _getSheetIndex($sheet_name)
    {
        if (!isset($this->_ext_sheets[$sheet_name])) {
            return -1;
        } else {
            return $this->_ext_sheets[$sheet_name];
        }
    }

    /**
    * This method is used to update the array of sheet names. It is
    * called by the addWorksheet() method of the
    * Spreadsheet_Excel_Writer_Workbook class.
    *
    * @access public
    * @see Spreadsheet_Excel_Writer_Workbook::addWorksheet()
    * @param string  $name  The name of the worksheet being added
    * @param integer $index The index of the worksheet being added
    */
    function setExtSheet($name, $index)
    {
        $this->_ext_sheets[$name] = $index;
    }

    /**
    * pack() row and column into the required 3 or 4 byte format.
    *
    * @access private
    * @param string $cell The Excel cell reference to be packed
    * @return array Array containing the row and column in packed() format
    */
    function _cellToPackedRowcol($cell)
    {
        $cell = strtoupper($cell);
        list($row, $col, $row_rel, $col_rel) = $this->_cellToRowcol($cell);
        if ($col >= 256) {
            die("Column in: $cell greater than 255");
        }
        // FIXME: change for BIFF8
        if ($row >= 16384) {
            die("Row in: $cell greater than 16384 ");
        }

        // Set the high bits to indicate if row or col are relative.
        if ($this->_BIFF_version == 0x0500) {
            $row    |= $col_rel << 14;
            $row    |= $row_rel << 15;
            $col     = pack('C', $col);
        } elseif ($this->_BIFF_version == 0x0600) {
            $col    |= $col_rel << 14;
            $col    |= $row_rel << 15;
            $col     = pack('v', $col);
        }
        $row     = pack('v', $row);

        return array($row, $col);
    }

    /**
    * pack() row range into the required 3 or 4 byte format.
    * Just using maximum col/rows, which is probably not the correct solution
    *
    * @access private
    * @param string $range The Excel range to be packed
    * @return array Array containing (row1,col1,row2,col2) in packed() format
    */
    function _rangeToPackedRange($range)
    {
        preg_match('/(\$)?(\d+)\:(\$)?(\d+)/', $range, $match);
        // return absolute rows if there is a $ in the ref
        $row1_rel = empty($match[1]) ? 1 : 0;
        $row1     = $match[2];
        $row2_rel = empty($match[3]) ? 1 : 0;
        $row2     = $match[4];
        // Convert 1-index to zero-index
        $row1--;
        $row2--;
        // Trick poor inocent Excel
        $col1 = 0;
        $col2 = 16383; // FIXME: maximum possible value for Excel 5 (change this!!!)

        // FIXME: this changes for BIFF8
        if (($row1 >= 16384) or ($row2 >= 16384)) {
            die("Row in: $range greater than 16384 ");
        }

        // Set the high bits to indicate if rows are relative.
        if ($this->_BIFF_version == 0x0500) {
            $row1    |= $row1_rel << 14; // FIXME: probably a bug
            $row2    |= $row2_rel << 15;
            $col1     = pack('C', $col1);
            $col2     = pack('C', $col2);
        } elseif ($this->_BIFF_version == 0x0600) {
            $col1    |= $row1_rel << 15;
            $col2    |= $row2_rel << 15;
            $col1     = pack('v', $col1);
            $col2     = pack('v', $col2);
        }
        $row1     = pack('v', $row1);
        $row2     = pack('v', $row2);

        return array($row1, $col1, $row2, $col2);
    }

    /**
    * Convert an Excel cell reference such as A1 or $B2 or C$3 or $D$4 to a zero
    * indexed row and column number. Also returns two (0,1) values to indicate
    * whether the row or column are relative references.
    *
    * @access private
    * @param string $cell The Excel cell reference in A1 format.
    * @return array
    */
    function _cellToRowcol($cell)
    {
        preg_match('/(\$)?([A-I]?[A-Z])(\$)?(\d+)/',$cell,$match);
        // return absolute column if there is a $ in the ref
        $col_rel = empty($match[1]) ? 1 : 0;
        $col_ref = $match[2];
        $row_rel = empty($match[3]) ? 1 : 0;
        $row     = $match[4];

        // Convert base26 column string to a number.
        $expn   = strlen($col_ref) - 1;
        $col    = 0;
        $col_ref_length = strlen($col_ref);
        for ($i = 0; $i < $col_ref_length; $i++) {
            $col += (ord($col_ref[$i]) - ord('A') + 1) * pow(26, $expn);
            $expn--;
        }

        // Convert 1-index to zero-index
        $row--;
        $col--;

        return array($row, $col, $row_rel, $col_rel);
    }

    /**
    * Advance to the next valid token.
    *
    * @access private
    */
    function _advance()
    {
        $i = $this->_current_char;
        $formula_length = strlen($this->_formula);
        // eat up white spaces
        if ($i < $formula_length) {
            while ($this->_formula[$i] == " ") {
                $i++;
            }

            if ($i < ($formula_length - 1)) {
                $this->_lookahead = $this->_formula[$i+1];
            }
            $token = '';
        }

        while ($i < $formula_length) {
            $token .= $this->_formula[$i];
            if ($i < ($formula_length - 1)) {
                $this->_lookahead = $this->_formula[$i+1];
            } else {
                $this->_lookahead = '';
            }

            if ($this->_match($token) != '') {
                //if ($i < strlen($this->_formula) - 1) {
                //    $this->_lookahead = $this->_formula{$i+1};
                //}
                $this->_current_char = $i + 1;
                $this->_current_token = $token;
                return 1;
            }

            if ($i < ($formula_length - 2)) {
                $this->_lookahead = $this->_formula[$i+2];
            } else { // if we run out of characters _lookahead becomes empty
                $this->_lookahead = '';
            }
            $i++;
        }
        //die("Lexical error ".$this->_current_char);
    }

    /**
    * Checks if it's a valid token.
    *
    * @access private
    * @param mixed $token The token to check.
    * @return mixed       The checked token or false on failure
    */
    function _match($token)
    {
        switch($token) {
            case SPREADSHEET_EXCEL_WRITER_ADD:
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_SUB:
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_MUL:
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_DIV:
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_OPEN:
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_CLOSE:
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_COMA:
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_SEMICOLON:
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_GT:
                if ($this->_lookahead == '=') { // it's a GE token
                    break;
                }
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_LT:
                // it's a LE or a NE token
                if (($this->_lookahead == '=') or ($this->_lookahead == '>')) {
                    break;
                }
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_GE:
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_LE:
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_EQ:
                return $token;
                break;
            case SPREADSHEET_EXCEL_WRITER_NE:
                return $token;
                break;
            default:
                // if it's a reference
                if (preg_match('/^\$?[A-Ia-i]?[A-Za-z]\$?[0-9]+$/',$token) and
                   !preg_match("/[0-9]/",$this->_lookahead) and 
                   ($this->_lookahead != ':') and ($this->_lookahead != '.') and
                   ($this->_lookahead != '!'))
                {
                    return $token;
                }
                // If it's an external reference (Sheet1!A1 or Sheet1:Sheet2!A1)
                elseif (preg_match("/^\w+(\:\w+)?\![A-Ia-i]?[A-Za-z][0-9]+$/u",$token) and
                       !preg_match("/[0-9]/",$this->_lookahead) and
                       ($this->_lookahead != ':') and ($this->_lookahead != '.'))
                {
                    return $token;
                }
                // If it's an external reference ('Sheet1'!A1 or 'Sheet1:Sheet2'!A1)
                elseif (preg_match("/^'[\w -]+(\:[\w -]+)?'\![A-Ia-i]?[A-Za-z][0-9]+$/u",$token) and
                       !preg_match("/[0-9]/",$this->_lookahead) and
                       ($this->_lookahead != ':') and ($this->_lookahead != '.'))
                {
                    return $token;
                }
                // if it's a range (A1:A2)
                elseif (preg_match("/^(\$)?[A-Ia-i]?[A-Za-z](\$)?[0-9]+:(\$)?[A-Ia-i]?[A-Za-z](\$)?[0-9]+$/",$token) and 
                       !preg_match("/[0-9]/",$this->_lookahead))
                {
                    return $token;
                }
                // if it's a range (A1..A2)
                elseif (preg_match("/^(\$)?[A-Ia-i]?[A-Za-z](\$)?[0-9]+\.\.(\$)?[A-Ia-i]?[A-Za-z](\$)?[0-9]+$/",$token) and 
                       !preg_match("/[0-9]/",$this->_lookahead))
                {
                    return $token;
                }
                // If it's an external range like Sheet1!A1 or Sheet1:Sheet2!A1:B2
                elseif (preg_match("/^\w+(\:\w+)?\!([A-Ia-i]?[A-Za-z])?[0-9]+:([A-Ia-i]?[A-Za-z])?[0-9]+$/u",$token) and
                       !preg_match("/[0-9]/",$this->_lookahead))
                {
                    return $token;
                }
                // If it's an external range like 'Sheet1'!A1 or 'Sheet1:Sheet2'!A1:B2
                elseif (preg_match("/^'[\w -]+(\:[\w -]+)?'\!([A-Ia-i]?[A-Za-z])?[0-9]+:([A-Ia-i]?[A-Za-z])?[0-9]+$/u",$token) and
                       !preg_match("/[0-9]/",$this->_lookahead))
                {
                    return $token;
                }
                // If it's a number (check that it's not a sheet name or range)
                elseif (is_numeric($token) and 
                        (!is_numeric($token.$this->_lookahead) or ($this->_lookahead == '')) and
                        ($this->_lookahead != '!') and ($this->_lookahead != ':'))
                {
                    return $token;
                }
                // If it's a string (of maximum 255 characters)
                elseif (preg_match("/^\"[^\"]{0,255}\"$/",$token))
                {
                    return $token;
                }
                // if it's a function call
                elseif (preg_match("/^[A-Z0-9\xc0-\xdc\.]+$/i",$token) and ($this->_lookahead == "("))
                {
                    return $token;
                }
                return '';
        }
    }

    /**
    * The parsing method. It parses a formula.
    *
    * @access public
    * @param string $formula The formula to parse, without the initial equal
    *                        sign (=).
    * @return mixed true on success, PEAR_Error on failure
    */
    function parse($formula)
    {
        $this->_current_char = 0;
        $this->_formula      = $formula;
        $this->_lookahead    = $formula[1];
        $this->_advance();
        $this->_parse_tree   = $this->_condition();
    }

    /**
    * It parses a condition. It assumes the following rule:
    * Cond -> Expr [(">" | "<") Expr]
    *
    * @access private
    * @return mixed The parsed ptg'd tree on success, PEAR_Error on failure
    */
    function _condition()
    {
        $result = $this->_expression();
        if ($this->_current_token == SPREADSHEET_EXCEL_WRITER_LT) {
            $this->_advance();
            $result2 = $this->_expression();
            $result = $this->_createTree('ptgLT', $result, $result2);
        } elseif ($this->_current_token == SPREADSHEET_EXCEL_WRITER_GT) {
            $this->_advance();
            $result2 = $this->_expression();
            $result = $this->_createTree('ptgGT', $result, $result2);
        } elseif ($this->_current_token == SPREADSHEET_EXCEL_WRITER_LE) {
            $this->_advance();
            $result2 = $this->_expression();
            $result = $this->_createTree('ptgLE', $result, $result2);
        } elseif ($this->_current_token == SPREADSHEET_EXCEL_WRITER_GE) {
            $this->_advance();
            $result2 = $this->_expression();
            $result = $this->_createTree('ptgGE', $result, $result2);
        } elseif ($this->_current_token == SPREADSHEET_EXCEL_WRITER_EQ) {
            $this->_advance();
            $result2 = $this->_expression();
            $result = $this->_createTree('ptgEQ', $result, $result2);
        } elseif ($this->_current_token == SPREADSHEET_EXCEL_WRITER_NE) {
            $this->_advance();
            $result2 = $this->_expression();
            $result = $this->_createTree('ptgNE', $result, $result2);
        }
        return $result;
    }

    /**
    * It parses a expression. It assumes the following rule:
    * Expr -> Term [("+" | "-") Term]
    *      -> "string"
    *      -> "-" Term
    *
    * @access private
    * @return mixed The parsed ptg'd tree on success, PEAR_Error on failure
    */
    function _expression()
    {
        // If it's a string return a string node
        if (preg_match("/^\"[^\"]{0,255}\"$/", $this->_current_token)) {
            $result = $this->_createTree($this->_current_token, '', '');
            $this->_advance();
            return $result;
        } elseif ($this->_current_token == SPREADSHEET_EXCEL_WRITER_SUB) {
            // catch "-" Term
            $this->_advance();
            $result2 = $this->_expression();
            $result = $this->_createTree('ptgUminus', $result2, '');
            return $result;
        }
        $result = $this->_term();
        while (($this->_current_token == SPREADSHEET_EXCEL_WRITER_ADD) or
               ($this->_current_token == SPREADSHEET_EXCEL_WRITER_SUB)) {
        /**/
            if ($this->_current_token == SPREADSHEET_EXCEL_WRITER_ADD) {
                $this->_advance();
                $result2 = $this->_term();
                $result = $this->_createTree('ptgAdd', $result, $result2);
            } else {
                $this->_advance();
                $result2 = $this->_term();
                $result = $this->_createTree('ptgSub', $result, $result2);
            }
        }
        return $result;
    }

    /**
    * This function just introduces a ptgParen element in the tree, so that Excel
    * doesn't get confused when working with a parenthesized formula afterwards.
    *
    * @access private
    * @see _fact()
    * @return array The parsed ptg'd tree
    */
    function _parenthesizedExpression()
    {
        $result = $this->_createTree('ptgParen', $this->_expression(), '');
        return $result;
    }

    /**
    * It parses a term. It assumes the following rule:
    * Term -> Fact [("*" | "/") Fact]
    *
    * @access private
    * @return mixed The parsed ptg'd tree on success, PEAR_Error on failure
    */
    function _term()
    {
        $result = $this->_fact();
        while (($this->_current_token == SPREADSHEET_EXCEL_WRITER_MUL) or
               ($this->_current_token == SPREADSHEET_EXCEL_WRITER_DIV)) {
        /**/
            if ($this->_current_token == SPREADSHEET_EXCEL_WRITER_MUL) {
                $this->_advance();
                $result2 = $this->_fact();
                $result = $this->_createTree('ptgMul', $result, $result2);
            } else {
                $this->_advance();
                $result2 = $this->_fact();
                $result = $this->_createTree('ptgDiv', $result, $result2);
            }
        }
        return $result;
    }

    /**
    * It parses a factor. It assumes the following rule:
    * Fact -> ( Expr )
    *       | CellRef
    *       | CellRange
    *       | Number
    *       | Function
    *
    * @access private
    * @return mixed The parsed ptg'd tree on success, PEAR_Error on failure
    */
    function _fact()
    {
        if ($this->_current_token == SPREADSHEET_EXCEL_WRITER_OPEN) {
            $this->_advance();         // eat the "("
            $result = $this->_parenthesizedExpression();
            if ($this->_current_token != SPREADSHEET_EXCEL_WRITER_CLOSE) {
                die("')' token expected.");
            }
            $this->_advance();         // eat the ")"
            return $result;
        }
        // if it's a reference
        if (preg_match('/^\$?[A-Ia-i]?[A-Za-z]\$?[0-9]+$/',$this->_current_token))
        {
            $result = $this->_createTree($this->_current_token, '', '');
            $this->_advance();
            return $result;
        }
        // If it's an external reference (Sheet1!A1 or Sheet1:Sheet2!A1)
        elseif (preg_match("/^\w+(\:\w+)?\![A-Ia-i]?[A-Za-z][0-9]+$/u",$this->_current_token))
        {
            $result = $this->_createTree($this->_current_token, '', '');
            $this->_advance();
            return $result;
        }
        // If it's an external reference ('Sheet1'!A1 or 'Sheet1:Sheet2'!A1)
        elseif (preg_match("/^'[\w -]+(\:[\w -]+)?'\![A-Ia-i]?[A-Za-z][0-9]+$/u",$this->_current_token))
        {
            $result = $this->_createTree($this->_current_token, '', '');
            $this->_advance();
            return $result;
        }
        // if it's a range
        elseif (preg_match("/^(\$)?[A-Ia-i]?[A-Za-z](\$)?[0-9]+:(\$)?[A-Ia-i]?[A-Za-z](\$)?[0-9]+$/",$this->_current_token) or 
                preg_match("/^(\$)?[A-Ia-i]?[A-Za-z](\$)?[0-9]+\.\.(\$)?[A-Ia-i]?[A-Za-z](\$)?[0-9]+$/",$this->_current_token))
        {
            $result = $this->_current_token;
            $this->_advance();
            return $result;
        }
        // If it's an external range (Sheet1!A1 or Sheet1!A1:B2)
        elseif (preg_match("/^\w+(\:\w+)?\!([A-Ia-i]?[A-Za-z])?[0-9]+:([A-Ia-i]?[A-Za-z])?[0-9]+$/u",$this->_current_token))
        {
            $result = $this->_current_token;
            $this->_advance();
            return $result;
        }
        // If it's an external range ('Sheet1'!A1 or 'Sheet1'!A1:B2)
        elseif (preg_match("/^'[\w -]+(\:[\w -]+)?'\!([A-Ia-i]?[A-Za-z])?[0-9]+:([A-Ia-i]?[A-Za-z])?[0-9]+$/u",$this->_current_token))
        {
            $result = $this->_current_token;
            $this->_advance();
            return $result;
        }
        elseif (is_numeric($this->_current_token))
        {
            $result = $this->_createTree($this->_current_token, '', '');
            $this->_advance();
            return $result;
        }
        // if it's a function call
        elseif (preg_match("/^[A-Z0-9\xc0-\xdc\.]+$/i",$this->_current_token))
        {
            $result = $this->_func();
            return $result;
        }
        die("Syntax error: ".$this->_current_token.
                                 ", lookahead: ".$this->_lookahead.
                                 ", current char: ".$this->_current_char);
    }

    /**
    * It parses a function call. It assumes the following rule:
    * Func -> ( Expr [,Expr]* )
    *
    * @access private
    * @return mixed The parsed ptg'd tree on success, PEAR_Error on failure
    */
    function _func()
    {
        $num_args = 0; // number of arguments received
        $function = strtoupper($this->_current_token);
        $result   = ''; // initialize result
        $this->_advance();
        $this->_advance();         // eat the "("
        while ($this->_current_token != ')') {
        /**/
            if ($num_args > 0) {
                if ($this->_current_token == SPREADSHEET_EXCEL_WRITER_COMA or
                    $this->_current_token == SPREADSHEET_EXCEL_WRITER_SEMICOLON)
                {
                    $this->_advance();  // eat the "," or ";"
                } else {
                    die("Syntax error: comma expected in ".
                                      "function $function, arg #{$num_args}");
                }
                $result2 = $this->_condition();
                $result = $this->_createTree('arg', $result, $result2);
            } else { // first argument
                $result2 = $this->_condition();
                $result = $this->_createTree('arg', '', $result2);
            }
            $num_args++;
        }
        if (!isset($this->_functions[$function])) {
            die("Function $function() doesn't exist");
        }
        $args = $this->_functions[$function][1];
        // If fixed number of args eg. TIME($i,$j,$k). Check that the number of args is valid.
        if (($args >= 0) and ($args != $num_args)) {
            die("Incorrect number of arguments in function $function() ");
        }

        $result = $this->_createTree($function, $result, $num_args);
        $this->_advance();         // eat the ")"
        return $result;
    }

    /**
    * Creates a tree. In fact an array which may have one or two arrays (sub-trees)
    * as elements.
    *
    * @access private
    * @param mixed $value The value of this node.
    * @param mixed $left  The left array (sub-tree) or a final node.
    * @param mixed $right The right array (sub-tree) or a final node.
    * @return array A tree
    */
    function _createTree($value, $left, $right)
    {
        return array('value' => $value, 'left' => $left, 'right' => $right);
    }

    /**
    * Builds a string containing the tree in reverse polish notation (What you
    * would use in a HP calculator stack).
    * The following tree:
    *
    *    +
    *   / \
    *  2   3
    *
    * produces: "23+"
    *
    * The following tree:
    *
    *    +
    *   / \
    *  3   *
    *     / \
    *    6   A1
    *
    * produces: "36A1*+"
    *
    * In fact all operands, functions, references, etc... are written as ptg's
    *
    * @access public
    * @param array $tree The optional tree to convert.
    * @return string The tree in reverse polish notation
    */
    function toReversePolish($tree = array())
    {
        $polish = ""; // the string we are going to return
        if (empty($tree)) { // If it's the first call use _parse_tree
            $tree = $this->_parse_tree;
        }
        if (is_array($tree['left'])) {
            $converted_tree = $this->toReversePolish($tree['left']);
            $polish .= $converted_tree;
        } elseif ($tree['left'] != '') { // It's a final node
            $converted_tree = $this->_convert($tree['left']);
            $polish .= $converted_tree;
        }
        if (is_array($tree['right'])) {
            $converted_tree = $this->toReversePolish($tree['right']);
            $polish .= $converted_tree;
        } elseif ($tree['right'] != '') { // It's a final node
            $converted_tree = $this->_convert($tree['right']);
            $polish .= $converted_tree;
        }
        // if it's a function convert it here (so we can set it's arguments)
        if (preg_match("/^[A-Z0-9\xc0-\xdc\.]+$/",$tree['value']) and
            !preg_match('/^([A-Ia-i]?[A-Za-z])(\d+)$/',$tree['value']) and
            !preg_match("/^[A-Ia-i]?[A-Za-z](\d+)\.\.[A-Ia-i]?[A-Za-z](\d+)$/",$tree['value']) and
            !is_numeric($tree['value']) and
            !isset($this->ptg[$tree['value']]))
        {
            // left subtree for a function is always an array.
            if ($tree['left'] != '') {
                $left_tree = $this->toReversePolish($tree['left']);
            } else {
                $left_tree = '';
            }
            // add it's left subtree and return.
            return $left_tree.$this->_convertFunction($tree['value'], $tree['right']);
        } else {
            $converted_tree = $this->_convert($tree['value']);
        }
        $polish .= $converted_tree;
        return $polish;
    }
}

/**
* Class for generating Excel Spreadsheets
*
* @author   Xavier Noguer <xnoguer@rezebra.com>
* @category FileFormats
* @package  Spreadsheet_Excel_Writer
*/

class Spreadsheet_Excel_Writer_Worksheet extends Spreadsheet_Excel_Writer_BIFFwriter
{
    /**
    * Name of the Worksheet
    * @var string
    */
    var $name;

    /**
    * Index for the Worksheet
    * @var integer
    */
    var $index;

    /**
    * Reference to the (default) Format object for URLs
    * @var object Format
    */
    var $_url_format;

    /**
    * Reference to the parser used for parsing formulas
    * @var object Format
    */
    var $_parser;

    /**
    * Filehandle to the temporary file for storing data
    * @var resource
    */
    var $_filehandle;

    /**
    * Boolean indicating if we are using a temporary file for storing data
    * @var bool
    */
    var $_using_tmpfile;

    /**
    * Maximum number of rows for an Excel spreadsheet (BIFF5)
    * @var integer
    */
    var $_xls_rowmax;

    /**
    * Maximum number of columns for an Excel spreadsheet (BIFF5)
    * @var integer
    */
    var $_xls_colmax;

    /**
    * Maximum number of characters for a string (LABEL record in BIFF5)
    * @var integer
    */
    var $_xls_strmax;

    /**
    * First row for the DIMENSIONS record
    * @var integer
    * @see _storeDimensions()
    */
    var $_dim_rowmin;

    /**
    * Last row for the DIMENSIONS record
    * @var integer
    * @see _storeDimensions()
    */
    var $_dim_rowmax;

    /**
    * First column for the DIMENSIONS record
    * @var integer
    * @see _storeDimensions()
    */
    var $_dim_colmin;

    /**
    * Last column for the DIMENSIONS record
    * @var integer
    * @see _storeDimensions()
    */
    var $_dim_colmax;

    /**
    * Array containing format information for columns
    * @var array
    */
    var $_colinfo;

    /**
    * Array containing the selected area for the worksheet
    * @var array
    */
    var $_selection;

    /**
    * Array containing the panes for the worksheet
    * @var array
    */
    var $_panes;

    /**
    * The active pane for the worksheet
    * @var integer
    */
    var $_active_pane;

    /**
    * Bit specifying if panes are frozen
    * @var integer
    */
    var $_frozen;

    /**
    * Bit specifying if the worksheet is selected
    * @var integer
    */
    var $selected;

    /**
    * The paper size (for printing) (DOCUMENT!!!)
    * @var integer
    */
    var $_paper_size;

    /**
    * Bit specifying paper orientation (for printing). 0 => landscape, 1 => portrait
    * @var integer
    */
    var $_orientation;

    /**
    * The page header caption
    * @var string
    */
    var $_header;

    /**
    * The page footer caption
    * @var string
    */
    var $_footer;

    /**
    * The horizontal centering value for the page
    * @var integer
    */
    var $_hcenter;

    /**
    * The vertical centering value for the page
    * @var integer
    */
    var $_vcenter;

    /**
    * The margin for the header
    * @var float
    */
    var $_margin_head;

    /**
    * The margin for the footer
    * @var float
    */
    var $_margin_foot;

    /**
    * The left margin for the worksheet in inches
    * @var float
    */
    var $_margin_left;

    /**
    * The right margin for the worksheet in inches
    * @var float
    */
    var $_margin_right;

    /**
    * The top margin for the worksheet in inches
    * @var float
    */
    var $_margin_top;

    /**
    * The bottom margin for the worksheet in inches
    * @var float
    */
    var $_margin_bottom;

    /**
    * First row to reapeat on each printed page
    * @var integer
    */
    var $title_rowmin;

    /**
    * Last row to reapeat on each printed page
    * @var integer
    */
    var $title_rowmax;

    /**
    * First column to reapeat on each printed page
    * @var integer
    */
    var $title_colmin;

    /**
    * First row of the area to print
    * @var integer
    */
    var $print_rowmin;

    /**
    * Last row to of the area to print
    * @var integer
    */
    var $print_rowmax;

    /**
    * First column of the area to print
    * @var integer
    */
    var $print_colmin;

    /**
    * Last column of the area to print
    * @var integer
    */
    var $print_colmax;

    /**
    * Whether to use outline.
    * @var integer
    */
    var $_outline_on;

    /**
    * Auto outline styles.
    * @var bool
    */
    var $_outline_style;

    /**
    * Whether to have outline summary below.
    * @var bool
    */
    var $_outline_below;

    /**
    * Whether to have outline summary at the right.
    * @var bool
    */
    var $_outline_right;

    /**
    * Outline row level.
    * @var integer
    */
    var $_outline_row_level;

    /**
    * Whether to fit to page when printing or not.
    * @var bool
    */
    var $_fit_page;

    /**
    * Number of pages to fit wide
    * @var integer
    */
    var $_fit_width;

    /**
    * Number of pages to fit high
    * @var integer
    */
    var $_fit_height;

    /**
    * Reference to the total number of strings in the workbook
    * @var integer
    */
    var $_str_total;

    /**
    * Reference to the number of unique strings in the workbook
    * @var integer
    */
    var $_str_unique;

    /**
    * Reference to the array containing all the unique strings in the workbook
    * @var array
    */
    var $_str_table;

    /**
    * Merged cell ranges
    * @var array
    */
    var $_merged_ranges;

    /**
    * Charset encoding currently used when calling writeString()
    * @var string
    */
    var $_input_encoding;

    /**
    * Constructor
    *
    * @param string  $name         The name of the new worksheet
    * @param integer $index        The index of the new worksheet
    * @param mixed   &$activesheet The current activesheet of the workbook we belong to
    * @param mixed   &$firstsheet  The first worksheet in the workbook we belong to
    * @param mixed   &$url_format  The default format for hyperlinks
    * @param mixed   &$parser      The formula parser created for the Workbook
    * @access private
    */
    function 					__construct($BIFF_version, $name,
                                                $index, &$activesheet,
                                                &$firstsheet, &$str_total,
                                                &$str_unique, &$str_table,
                                                &$url_format, &$parser)
    {
        // It needs to call its parent's constructor explicitly
        parent::__construct();
        $this->_BIFF_version   = $BIFF_version;
        $rowmax                = 65536; // 16384 in Excel 5
        $colmax                = 256;

        $this->name            = $name;
        $this->index           = $index;
        $this->activesheet     = &$activesheet;
        $this->firstsheet      = &$firstsheet;
        $this->_str_total      = &$str_total;
        $this->_str_unique     = &$str_unique;
        $this->_str_table      = &$str_table;
        $this->_url_format     = &$url_format;
        $this->_parser         = &$parser;

        //$this->ext_sheets      = array();
        $this->_filehandle     = '';
        $this->_using_tmpfile  = true;
        //$this->fileclosed      = 0;
        //$this->offset          = 0;
        $this->_xls_rowmax     = $rowmax;
        $this->_xls_colmax     = $colmax;
        $this->_xls_strmax     = 255;
        $this->_dim_rowmin     = $rowmax + 1;
        $this->_dim_rowmax     = 0;
        $this->_dim_colmin     = $colmax + 1;
        $this->_dim_colmax     = 0;
        $this->_colinfo        = array();
        $this->_selection      = array(0,0,0,0);
        $this->_panes          = array();
        $this->_active_pane    = 3;
        $this->_frozen         = 0;
        $this->selected        = 0;

        $this->_paper_size      = 0x0;
        $this->_orientation     = 0x1;
        $this->_header          = '';
        $this->_footer          = '';
        $this->_hcenter         = 0;
        $this->_vcenter         = 0;
        $this->_margin_head     = 0.50;
        $this->_margin_foot     = 0.50;
        $this->_margin_left     = 0.75;
        $this->_margin_right    = 0.75;
        $this->_margin_top      = 1.00;
        $this->_margin_bottom   = 1.00;

        $this->title_rowmin     = null;
        $this->title_rowmax     = null;
        $this->title_colmin     = null;
        $this->title_colmax     = null;
        $this->print_rowmin     = null;
        $this->print_rowmax     = null;
        $this->print_colmin     = null;
        $this->print_colmax     = null;

        $this->_print_gridlines  = 1;
        $this->_screen_gridlines = 1;
        $this->_print_headers    = 0;

        $this->_fit_page        = 0;
        $this->_fit_width       = 0;
        $this->_fit_height      = 0;

        $this->_hbreaks         = array();
        $this->_vbreaks         = array();

        $this->_protect         = 0;
        $this->_password        = null;

        $this->col_sizes        = array();
        $this->_row_sizes        = array();

        $this->_zoom            = 100;
        $this->_print_scale     = 100;

        $this->_outline_row_level = 0;
        $this->_outline_style     = 0;
        $this->_outline_below     = 1;
        $this->_outline_right     = 1;
        $this->_outline_on        = 1;

        $this->_merged_ranges     = array();
        
		$this->_rtl				  = 0; 	// Added by Joe Hunt 2009-03-05 for arabic languages
        $this->_input_encoding    = '';

        $this->_dv                = array();

        $this->_initialize();
    }

    /**
    * Open a tmp file to store the majority of the Worksheet data. If this fails,
    * for example due to write permissions, store the data in memory. This can be
    * slow for large files.
    *
    * @access private
    */
    function _initialize()
    {
        // Open tmp file for storing Worksheet data
        $fh = tmpfile();
        if ($fh) {
            // Store filehandle
            $this->_filehandle = $fh;
        } else {
            // If tmpfile() fails store data in memory
            $this->_using_tmpfile = false;
        }
    }

    /**
    * Add data to the beginning of the workbook (note the reverse order)
    * and to the end of the workbook.
    *
    * @access public
    * @see Spreadsheet_Excel_Writer_Workbook::storeWorkbook()
    * @param array $sheetnames The array of sheetnames from the Workbook this
    *                          worksheet belongs to
    */
    function close($sheetnames)
    {
        $num_sheets = count($sheetnames);

        /***********************************************
        * Prepend in reverse order!!
        */

        // Prepend the sheet dimensions
        $this->_storeDimensions();

        // Prepend the sheet password
        $this->_storePassword();

        // Prepend the sheet protection
        $this->_storeProtect();

        // Prepend the page setup
        $this->_storeSetup();

        /* FIXME: margins are actually appended */
        // Prepend the bottom margin
        $this->_storeMarginBottom();

        // Prepend the top margin
        $this->_storeMarginTop();

        // Prepend the right margin
        $this->_storeMarginRight();

        // Prepend the left margin
        $this->_storeMarginLeft();

        // Prepend the page vertical centering
        $this->_storeVcenter();

        // Prepend the page horizontal centering
        $this->_storeHcenter();

        // Prepend the page footer
        $this->_storeFooter();

        // Prepend the page header
        $this->_storeHeader();

        // Prepend the vertical page breaks
        $this->_storeVbreak();

        // Prepend the horizontal page breaks
        $this->_storeHbreak();

        // Prepend WSBOOL
        $this->_storeWsbool();

        // Prepend GRIDSET
        $this->_storeGridset();

         //  Prepend GUTS
        if ($this->_BIFF_version == 0x0500) {
            $this->_storeGuts();
        }

        // Prepend PRINTGRIDLINES
        $this->_storePrintGridlines();

        // Prepend PRINTHEADERS
        $this->_storePrintHeaders();

        // Prepend EXTERNSHEET references
        if ($this->_BIFF_version == 0x0500) {
            for ($i = $num_sheets; $i > 0; $i--) {
                $sheetname = $sheetnames[$i-1];
                $this->_storeExternsheet($sheetname);
            }
        }

        // Prepend the EXTERNCOUNT of external references.
        if ($this->_BIFF_version == 0x0500) {
            $this->_storeExterncount($num_sheets);
        }

        // Prepend the COLINFO records if they exist
        if (!empty($this->_colinfo)) {
            $colcount = count($this->_colinfo);
            for ($i = 0; $i < $colcount; $i++) {
                $this->_storeColinfo($this->_colinfo[$i]);
            }
            $this->_storeDefcol();
        }

        // Prepend the BOF record
        $this->_storeBof(0x0010);

        /*
        * End of prepend. Read upwards from here.
        ***********************************************/

        // Append
        $this->_storeWindow2();
        $this->_storeZoom();
        if (!empty($this->_panes)) {
            $this->_storePanes($this->_panes);
        }
        $this->_storeSelection($this->_selection);
        $this->_storeMergedCells();
        /* TODO: add data validity */
        /*if ($this->_BIFF_version == 0x0600) {
            $this->_storeDataValidity();
        }*/
        $this->_storeEof();
    }

    /**
    * Retrieve the worksheet name.
    * This is usefull when creating worksheets without a name.
    *
    * @access public
    * @return string The worksheet's name
    */
    function getName()
    {
        return $this->name;
    }

    /**
    * Retrieves data from memory in one chunk, or from disk in $buffer
    * sized chunks.
    *
    * @return string The data
    */
    function getData()
    {
        $buffer = 4096;

        // Return data stored in memory
        if (isset($this->_data)) {
            $tmp   = $this->_data;
            unset($this->_data);
            $fh    = $this->_filehandle;
            if ($this->_using_tmpfile) {
                fseek($fh, 0);
            }
            return $tmp;
        }
        // Return data stored on disk
        if ($this->_using_tmpfile) {
            if ($tmp = fread($this->_filehandle, $buffer)) {
                return $tmp;
            }
        }

        // No data to return
        return '';
    }

    /**
    * Sets a merged cell range
    *
    * @access public
    * @param integer $first_row First row of the area to merge
    * @param integer $first_col First column of the area to merge
    * @param integer $last_row  Last row of the area to merge
    * @param integer $last_col  Last column of the area to merge
    */
    function setMerge($first_row, $first_col, $last_row, $last_col)
    {
        if (($last_row < $first_row) || ($last_col < $first_col)) {
            return;
        }
        // don't check rowmin, rowmax, etc... because we don't know when this
        // is going to be called
        $this->_merged_ranges[] = array($first_row, $first_col, $last_row, $last_col);
    }

    /**
    * Set this worksheet as a selected worksheet,
    * i.e. the worksheet has its tab highlighted.
    *
    * @access public
    */
    function select()
    {
        $this->selected = 1;
    }

    /**
    * Set this worksheet as the active worksheet,
    * i.e. the worksheet that is displayed when the workbook is opened.
    * Also set it as selected.
    *
    * @access public
    */
    function activate()
    {
        $this->selected = 1;
        $this->activesheet = $this->index;
    }

    /**
    * Set this worksheet as the first visible sheet.
    * This is necessary when there are a large number of worksheets and the
    * activated worksheet is not visible on the screen.
    *
    * @access public
    */
    function setFirstSheet()
    {
        $this->firstsheet = $this->index;
    }

    /**
    * Set the worksheet protection flag
    * to prevent accidental modification and to
    * hide formulas if the locked and hidden format properties have been set.
    *
    * @access public
    * @param string $password The password to use for protecting the sheet.
    */
    function protect($password)
    {
        $this->_protect   = 1;
        $this->_password  = $this->_encodePassword($password);
    }

    /**
    * Set the width of a single column or a range of columns.
    *
    * @access public
    * @param integer $firstcol first column on the range
    * @param integer $lastcol  last column on the range
    * @param integer $width    width to set
    * @param mixed   $format   The optional XF format to apply to the columns
    * @param integer $hidden   The optional hidden atribute
    * @param integer $level    The optional outline level
    */
    function setColumn($firstcol, $lastcol, $width, $format = null, $hidden = 0, $level = 0)
    {
        $this->_colinfo[] = array($firstcol, $lastcol, $width, &$format, $hidden, $level);

        // Set width to zero if column is hidden
        $width = ($hidden) ? 0 : $width;

        for ($col = $firstcol; $col <= $lastcol; $col++) {
            $this->col_sizes[$col] = $width;
        }
    }

    /**
    * Set which cell or cells are selected in a worksheet
    *
    * @access public
    * @param integer $first_row    first row in the selected quadrant
    * @param integer $first_column first column in the selected quadrant
    * @param integer $last_row     last row in the selected quadrant
    * @param integer $last_column  last column in the selected quadrant
    */
    function setSelection($first_row,$first_column,$last_row,$last_column)
    {
        $this->_selection = array($first_row,$first_column,$last_row,$last_column);
    }

    /**
    * Set panes and mark them as frozen.
    *
    * @access public
    * @param array $panes This is the only parameter received and is composed of the following:
    *                     0 => Vertical split position,
    *                     1 => Horizontal split position
    *                     2 => Top row visible
    *                     3 => Leftmost column visible
    *                     4 => Active pane
    */
    function freezePanes($panes)
    {
        $this->_frozen = 1;
        $this->_panes  = $panes;
    }

    /**
    * Set panes and mark them as unfrozen.
    *
    * @access public
    * @param array $panes This is the only parameter received and is composed of the following:
    *                     0 => Vertical split position,
    *                     1 => Horizontal split position
    *                     2 => Top row visible
    *                     3 => Leftmost column visible
    *                     4 => Active pane
    */
    function thawPanes($panes)
    {
        $this->_frozen = 0;
        $this->_panes  = $panes;
    }

    /**
    * Set the page orientation as portrait.
    *
    * @access public
    */
    function setPortrait()
    {
        $this->_orientation = 1;
    }

    /**
    * Set the page orientation as landscape.
    *
    * @access public
    */
    function setLandscape()
    {
        $this->_orientation = 0;
    }

    /**
    * Set the paper type. Ex. 1 = US Letter, 9 = A4
    *
    * @access public
    * @param integer $size The type of paper size to use
    */
    function setPaper($size = 0)
    {
        $this->_paper_size = $size;
    }


    /**
    * Set the page header caption and optional margin.
    *
    * @access public
    * @param string $string The header text
    * @param float  $margin optional head margin in inches.
    */
    function setHeader($string,$margin = 0.50)
    {
        if (strlen($string) >= 255) {
            //carp 'Header string must be less than 255 characters';
            return;
        }
        $this->_header      = $string;
        $this->_margin_head = $margin;
    }

    /**
    * Set the page footer caption and optional margin.
    *
    * @access public
    * @param string $string The footer text
    * @param float  $margin optional foot margin in inches.
    */
    function setFooter($string,$margin = 0.50)
    {
        if (strlen($string) >= 255) {
            //carp 'Footer string must be less than 255 characters';
            return;
        }
        $this->_footer      = $string;
        $this->_margin_foot = $margin;
    }

    /**
    * Center the page horinzontally.
    *
    * @access public
    * @param integer $center the optional value for centering. Defaults to 1 (center).
    */
    function centerHorizontally($center = 1)
    {
        $this->_hcenter = $center;
    }

    /**
    * Center the page vertically.
    *
    * @access public
    * @param integer $center the optional value for centering. Defaults to 1 (center).
    */
    function centerVertically($center = 1)
    {
        $this->_vcenter = $center;
    }

    /**
    * Set all the page margins to the same value in inches.
    *
    * @access public
    * @param float $margin The margin to set in inches
    */
    function setMargins($margin)
    {
        $this->setMarginLeft($margin);
        $this->setMarginRight($margin);
        $this->setMarginTop($margin);
        $this->setMarginBottom($margin);
    }

    /**
    * Set the left and right margins to the same value in inches.
    *
    * @access public
    * @param float $margin The margin to set in inches
    */
    function setMargins_LR($margin)
    {
        $this->setMarginLeft($margin);
        $this->setMarginRight($margin);
    }

    /**
    * Set the top and bottom margins to the same value in inches.
    *
    * @access public
    * @param float $margin The margin to set in inches
    */
    function setMargins_TB($margin)
    {
        $this->setMarginTop($margin);
        $this->setMarginBottom($margin);
    }

    /**
    * Set the left margin in inches.
    *
    * @access public
    * @param float $margin The margin to set in inches
    */
    function setMarginLeft($margin = 0.75)
    {
        $this->_margin_left = $margin;
    }

    /**
    * Set the right margin in inches.
    *
    * @access public
    * @param float $margin The margin to set in inches
    */
    function setMarginRight($margin = 0.75)
    {
        $this->_margin_right = $margin;
    }

    /**
    * Set the top margin in inches.
    *
    * @access public
    * @param float $margin The margin to set in inches
    */
    function setMarginTop($margin = 1.00)
    {
        $this->_margin_top = $margin;
    }

    /**
    * Set the bottom margin in inches.
    *
    * @access public
    * @param float $margin The margin to set in inches
    */
    function setMarginBottom($margin = 1.00)
    {
        $this->_margin_bottom = $margin;
    }

    /**
    * Set the rows to repeat at the top of each printed page.
    *
    * @access public
    * @param integer $first_row First row to repeat
    * @param integer $last_row  Last row to repeat. Optional.
    */
    function repeatRows($first_row, $last_row = null)
    {
        $this->title_rowmin  = $first_row;
        if (isset($last_row)) { //Second row is optional
            $this->title_rowmax  = $last_row;
        } else {
            $this->title_rowmax  = $first_row;
        }
    }

    /**
    * Set the columns to repeat at the left hand side of each printed page.
    *
    * @access public
    * @param integer $first_col First column to repeat
    * @param integer $last_col  Last column to repeat. Optional.
    */
    function repeatColumns($first_col, $last_col = null)
    {
        $this->title_colmin  = $first_col;
        if (isset($last_col)) { // Second col is optional
            $this->title_colmax  = $last_col;
        } else {
            $this->title_colmax  = $first_col;
        }
    }

    /**
    * Set the area of each worksheet that will be printed.
    *
    * @access public
    * @param integer $first_row First row of the area to print
    * @param integer $first_col First column of the area to print
    * @param integer $last_row  Last row of the area to print
    * @param integer $last_col  Last column of the area to print
    */
    function printArea($first_row, $first_col, $last_row, $last_col)
    {
        $this->print_rowmin  = $first_row;
        $this->print_colmin  = $first_col;
        $this->print_rowmax  = $last_row;
        $this->print_colmax  = $last_col;
    }


    /**
    * Set the option to hide gridlines on the printed page.
    *
    * @access public
    */
    function hideGridlines()
    {
        $this->_print_gridlines = 0;
    }

    /**
    * Set the option to hide gridlines on the worksheet (as seen on the screen).
    *
    * @access public
    */
    function hideScreenGridlines()
    {
        $this->_screen_gridlines = 0;
    }

    /**
    * Set the option to print the row and column headers on the printed page.
    *
    * @access public
    * @param integer $print Whether to print the headers or not. Defaults to 1 (print).
    */
    function printRowColHeaders($print = 1)
    {
        $this->_print_headers = $print;
    }

    /**
    * Set the vertical and horizontal number of pages that will define the maximum area printed.
    * It doesn't seem to work with OpenOffice.
    *
    * @access public
    * @param  integer $width  Maximun width of printed area in pages
    * @param  integer $height Maximun heigth of printed area in pages
    * @see setPrintScale()
    */
    function fitToPages($width, $height)
    {
        $this->_fit_page      = 1;
        $this->_fit_width     = $width;
        $this->_fit_height    = $height;
    }

    /**
    * Store the horizontal page breaks on a worksheet (for printing).
    * The breaks represent the row after which the break is inserted.
    *
    * @access public
    * @param array $breaks Array containing the horizontal page breaks
    */
    function setHPagebreaks($breaks)
    {
        foreach ($breaks as $break) {
            array_push($this->_hbreaks, $break);
        }
    }

    /**
    * Store the vertical page breaks on a worksheet (for printing).
    * The breaks represent the column after which the break is inserted.
    *
    * @access public
    * @param array $breaks Array containing the vertical page breaks
    */
    function setVPagebreaks($breaks)
    {
        foreach ($breaks as $break) {
            array_push($this->_vbreaks, $break);
        }
    }


    /**
    * Set the worksheet zoom factor.
    *
    * @access public
    * @param integer $scale The zoom factor
    */
    function setZoom($scale = 100)
    {
        // Confine the scale to Excel's range
        if ($scale < 10 || $scale > 400) {
            $scale = 100;
        }

        $this->_zoom = floor($scale);
    }

    /**
    * Set the scale factor for the printed page.
    * It turns off the "fit to page" option
    *
    * @access public
    * @param integer $scale The optional scale factor. Defaults to 100
    */
    function setPrintScale($scale = 100)
    {
        // Confine the scale to Excel's range
        if ($scale < 10 || $scale > 400) {
            $scale = 100;
        }

        // Turn off "fit to page" option
        $this->_fit_page = 0;

        $this->_print_scale = floor($scale);
    }

    /**
    * Map to the appropriate write method acording to the token recieved.
    *
    * @access public
    * @param integer $row    The row of the cell we are writing to
    * @param integer $col    The column of the cell we are writing to
    * @param mixed   $token  What we are writing
    * @param mixed   $format The optional format to apply to the cell
    */
    function write($row, $col, $token, $format = null)
    {
        // Check for a cell reference in A1 notation and substitute row and column
        /*if ($_[0] =~ /^\D/) {
            @_ = $this->_substituteCellref(@_);
    }*/

        if (preg_match("/^([+-]?)(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?$/", $token)) {
            // Match number
            return $this->writeNumber($row, $col, $token, $format);
        } elseif (preg_match("/^[fh]tt?p:\/\//", $token)) {
            // Match http or ftp URL
            return $this->writeUrl($row, $col, $token, '', $format);
        } elseif (preg_match("/^mailto:/", $token)) {
            // Match mailto:
            return $this->writeUrl($row, $col, $token, '', $format);
        } elseif (preg_match("/^(?:in|ex)ternal:/", $token)) {
            // Match internal or external sheet link
            return $this->writeUrl($row, $col, $token, '', $format);
        } elseif (preg_match("/^=/", $token)) {
            // Match formula
            return $this->writeFormula($row, $col, $token, $format);
        } elseif (preg_match("/^@/", $token)) {
            // Match formula
            return $this->writeFormula($row, $col, $token, $format);
        } elseif ($token == '') {
            // Match blank
            return $this->writeBlank($row, $col, $format);
        } else {
            // Default: match string
            return $this->writeString($row, $col, $token, $format);
        }
    }

    /**
    * Write an array of values as a row
    *
    * @access public
    * @param integer $row    The row we are writing to
    * @param integer $col    The first col (leftmost col) we are writing to
    * @param array   $val    The array of values to write
    * @param mixed   $format The optional format to apply to the cell
    * @return mixed PEAR_Error on failure
    */

    function writeRow($row, $col, $val, $format = null)
    {
        $retval = '';
        if (is_array($val)) {
            foreach ($val as $v) {
                if (is_array($v)) {
                    $this->writeCol($row, $col, $v, $format);
                } else {
                    $this->write($row, $col, $v, $format);
                }
                $col++;
            }
        } else {
            die('$val needs to be an array');
        }
        return($retval);
    }

    /**
    * Write an array of values as a column
    *
    * @access public
    * @param integer $row    The first row (uppermost row) we are writing to
    * @param integer $col    The col we are writing to
    * @param array   $val    The array of values to write
    * @param mixed   $format The optional format to apply to the cell
    * @return mixed PEAR_Error on failure
    */

    function writeCol($row, $col, $val, $format = null)
    {
        $retval = '';
        if (is_array($val)) {
            foreach ($val as $v) {
                $this->write($row, $col, $v, $format);
                $row++;
            }
        } else {
            die('$val needs to be an array');
        }
        return($retval);
    }

    /**
    * Returns an index to the XF record in the workbook
    *
    * @access private
    * @param mixed &$format The optional XF format
    * @return integer The XF record index
    */
    function _XF(&$format)
    {
        if ($format) {
            return($format->getXfIndex());
        } else {
            return(0x0F);
        }
    }


    /******************************************************************************
    *******************************************************************************
    *
    * Internal methods
    */


    /**
    * Store Worksheet data in memory using the parent's class append() or to a
    * temporary file, the default.
    *
    * @access private
    * @param string $data The binary data to append
    */
    function _append($data)
    {
        if ($this->_using_tmpfile) {
            // Add CONTINUE records if necessary
            if (strlen($data) > $this->_limit) {
                $data = $this->_addContinue($data);
            }
            fwrite($this->_filehandle, $data);
            $this->_datasize += strlen($data);
        } else {
            parent::_append($data);
        }
    }

    /**
    * Substitute an Excel cell reference in A1 notation for  zero based row and
    * column values in an argument list.
    *
    * Ex: ("A4", "Hello") is converted to (3, 0, "Hello").
    *
    * @access private
    * @param string $cell The cell reference. Or range of cells.
    * @return array
    */
    function _substituteCellref($cell)
    {
        $cell = strtoupper($cell);

        // Convert a column range: 'A:A' or 'B:G'
        if (preg_match("/([A-I]?[A-Z]):([A-I]?[A-Z])/", $cell, $match)) {
            list($no_use, $col1) =  $this->_cellToRowcol($match[1] .'1'); // Add a dummy row
            list($no_use, $col2) =  $this->_cellToRowcol($match[2] .'1'); // Add a dummy row
            return(array($col1, $col2));
        }

        // Convert a cell range: 'A1:B7'
        if (preg_match("/\$?([A-I]?[A-Z]\$?\d+):\$?([A-I]?[A-Z]\$?\d+)/", $cell, $match)) {
            list($row1, $col1) =  $this->_cellToRowcol($match[1]);
            list($row2, $col2) =  $this->_cellToRowcol($match[2]);
            return(array($row1, $col1, $row2, $col2));
        }

        // Convert a cell reference: 'A1' or 'AD2000'
        if (preg_match("/\$?([A-I]?[A-Z]\$?\d+)/", $cell)) {
            list($row1, $col1) =  $this->_cellToRowcol($match[1]);
            return(array($row1, $col1));
        }

        // TODO use real error codes
        die("Unknown cell reference $cell");
    }

    /**
    * Convert an Excel cell reference in A1 notation to a zero based row and column
    * reference; converts C1 to (0, 2).
    *
    * @access private
    * @param string $cell The cell reference.
    * @return array containing (row, column)
    */
    function _cellToRowcol($cell)
    {
        preg_match("/\$?([A-I]?[A-Z])\$?(\d+)/",$cell,$match);
        $col     = $match[1];
        $row     = $match[2];

        // Convert base26 column string to number
        $chars = preg_split('//', $col);
        $expn  = 0;
        $col   = 0;

        while ($chars) {
            $char = array_pop($chars);        // LS char first
            $col += (ord($char) -ord('A') +1) * pow(26,$expn);
            $expn++;
        }

        // Convert 1-index to zero-index
        $row--;
        $col--;

        return(array($row, $col));
    }

    /**
    * Based on the algorithm provided by Daniel Rentz of OpenOffice.
    *
    * @access private
    * @param string $plaintext The password to be encoded in plaintext.
    * @return string The encoded password
    */
    function _encodePassword($plaintext)
    {
        $password = 0x0000;
        $i        = 1;       // char position

        // split the plain text password in its component characters
        $chars = preg_split('//', $plaintext, -1, PREG_SPLIT_NO_EMPTY);
        foreach ($chars as $char) {
            $value        = ord($char) << $i;   // shifted ASCII value
            $rotated_bits = $value >> 15;       // rotated bits beyond bit 15
            $value       &= 0x7fff;             // first 15 bits
            $password    ^= ($value | $rotated_bits);
            $i++;
        }

        $password ^= strlen($plaintext);
        $password ^= 0xCE4B;

        return($password);
    }

    /**
    * This method sets the properties for outlining and grouping. The defaults
    * correspond to Excel's defaults.
    *
    * @param bool $visible
    * @param bool $symbols_below
    * @param bool $symbols_right
    * @param bool $auto_style
    */
    function setOutline($visible = true, $symbols_below = true, $symbols_right = true, $auto_style = false)
    {
        $this->_outline_on    = $visible;
        $this->_outline_below = $symbols_below;
        $this->_outline_right = $symbols_right;
        $this->_outline_style = $auto_style;

        // Ensure this is a boolean vale for Window2
        if ($this->_outline_on) {
            $this->_outline_on = 1;
        }
     }

    /******************************************************************************
    *******************************************************************************
    *
    * BIFF RECORDS
    */


    /**
    * Write a double to the specified row and column (zero indexed).
    * An integer can be written as a double. Excel will display an
    * integer. $format is optional.
    *
    * Returns  0 : normal termination
    *         -2 : row or column out of range
    *
    * @access public
    * @param integer $row    Zero indexed row
    * @param integer $col    Zero indexed column
    * @param float   $num    The number to write
    * @param mixed   $format The optional XF format
    * @return integer
    */
    function writeNumber($row, $col, $num, $format = null)
    {
        $record    = 0x0203;                 // Record identifier
        $length    = 0x000E;                 // Number of bytes to follow

        $xf        = $this->_XF($format);    // The cell format

        // Check that row and col are valid and store max and min values
        if ($row >= $this->_xls_rowmax) {
            return(-2);
        }
        if ($col >= $this->_xls_colmax) {
            return(-2);
        }
        if ($row <  $this->_dim_rowmin)  {
            $this->_dim_rowmin = $row;
        }
        if ($row >  $this->_dim_rowmax)  {
            $this->_dim_rowmax = $row;
        }
        if ($col <  $this->_dim_colmin)  {
            $this->_dim_colmin = $col;
        }
        if ($col >  $this->_dim_colmax)  {
            $this->_dim_colmax = $col;
        }

        $header    = pack("vv",  $record, $length);
        $data      = pack("vvv", $row, $col, $xf);
        $xl_double = pack("d",   $num);
        if ($this->_byte_order) { // if it's Big Endian
            $xl_double = strrev($xl_double);
        }

        $this->_append($header.$data.$xl_double);
        return(0);
    }

    /**
    * Write a string to the specified row and column (zero indexed).
    * NOTE: there is an Excel 5 defined limit of 255 characters.
    * $format is optional.
    * Returns  0 : normal termination
    *         -2 : row or column out of range
    *         -3 : long string truncated to 255 chars
    *
    * @access public
    * @param integer $row    Zero indexed row
    * @param integer $col    Zero indexed column
    * @param string  $str    The string to write
    * @param mixed   $format The XF format for the cell
    * @return integer
    */
    function writeString($row, $col, $str, $format = null)
    {
        if ($this->_BIFF_version == 0x0600) {
            return $this->writeStringBIFF8($row, $col, $str, $format);
        }
        $strlen    = strlen($str);
        $record    = 0x0204;                   // Record identifier
        $length    = 0x0008 + $strlen;         // Bytes to follow
        $xf        = $this->_XF($format);      // The cell format

        $str_error = 0;

        // Check that row and col are valid and store max and min values
        if ($row >= $this->_xls_rowmax) {
            return(-2);
        }
        if ($col >= $this->_xls_colmax) {
            return(-2);
        }
        if ($row <  $this->_dim_rowmin) {
            $this->_dim_rowmin = $row;
        }
        if ($row >  $this->_dim_rowmax) {
            $this->_dim_rowmax = $row;
        }
        if ($col <  $this->_dim_colmin) {
            $this->_dim_colmin = $col;
        }
        if ($col >  $this->_dim_colmax) {
            $this->_dim_colmax = $col;
        }

        if ($strlen > $this->_xls_strmax) { // LABEL must be < 255 chars
            $str       = substr($str, 0, $this->_xls_strmax);
            $length    = 0x0008 + $this->_xls_strmax;
            $strlen    = $this->_xls_strmax;
            $str_error = -3;
        }

        $header    = pack("vv",   $record, $length);
        $data      = pack("vvvv", $row, $col, $xf, $strlen);
        $this->_append($header . $data . $str);
        return($str_error);
    }

    /**
    * Sets Input Encoding for writing strings
    *
    * @access public
    * @param string $encoding The encoding. Ex: 'UTF-16LE', 'utf-8', 'ISO-859-7'
    */
    function setInputEncoding($encoding)
    {
         if ($encoding != 'UTF-16LE' && !function_exists('iconv')) {
             die("Using an input encoding other than UTF-16LE requires PHP support for iconv");
         }
         $this->_input_encoding = $encoding;
    }

    /** added 2009-03-05 by Joe Hunt, FA for arabic languages */
    function setRTL()
    {
    	$this->_rtl = 1;
    }	

    /**
    * Write a string to the specified row and column (zero indexed).
    * This is the BIFF8 version (no 255 chars limit).
    * $format is optional.
    * Returns  0 : normal termination
    *         -2 : row or column out of range
    *         -3 : long string truncated to 255 chars
    *
    * @access public
    * @param integer $row    Zero indexed row
    * @param integer $col    Zero indexed column
    * @param string  $str    The string to write
    * @param mixed   $format The XF format for the cell
    * @return integer
    */
    function writeStringBIFF8($row, $col, $str, $format = null)
    {
        if ($this->_input_encoding == 'UTF-16LE')
        {
            $strlen = function_exists('mb_strlen') ? mb_strlen($str, 'UTF-16LE') : (strlen($str) / 2);
            $encoding  = 0x1;
        }
        elseif ($this->_input_encoding != '')
        {
            $str = iconv($this->_input_encoding, 'UTF-16LE', $str);
            $strlen = function_exists('mb_strlen') ? mb_strlen($str, 'UTF-16LE') : (strlen($str) / 2);
            $encoding  = 0x1;
        }
        else
        {
            $strlen    = strlen($str);
            $encoding  = 0x0;
        }
        $record    = 0x00FD;                   // Record identifier
        $length    = 0x000A;                   // Bytes to follow
        $xf        = $this->_XF($format);      // The cell format

        $str_error = 0;

        // Check that row and col are valid and store max and min values
        if ($this->_checkRowCol($row, $col) == false) {
            return -2;
        }

        $str = pack('vC', $strlen, $encoding).$str;

        /* check if string is already present */
        if (!isset($this->_str_table[$str])) {
            $this->_str_table[$str] = $this->_str_unique++;
        }
        $this->_str_total++;

        $header    = pack('vv',   $record, $length);
        $data      = pack('vvvV', $row, $col, $xf, $this->_str_table[$str]);
        $this->_append($header.$data);
        return $str_error;
    }

    /**
    * Check row and col before writing to a cell, and update the sheet's
    * dimensions accordingly
    *
    * @access private
    * @param integer $row    Zero indexed row
    * @param integer $col    Zero indexed column
    * @return boolean true for success, false if row and/or col are grester
    *                 then maximums allowed.
    */
    function _checkRowCol($row, $col)
    {
        if ($row >= $this->_xls_rowmax) {
            return false;
        }
        if ($col >= $this->_xls_colmax) {
            return false;
        }
        if ($row <  $this->_dim_rowmin) {
            $this->_dim_rowmin = $row;
        }
        if ($row >  $this->_dim_rowmax) {
            $this->_dim_rowmax = $row;
        }
        if ($col <  $this->_dim_colmin) {
            $this->_dim_colmin = $col;
        }
        if ($col >  $this->_dim_colmax) {
            $this->_dim_colmax = $col;
        }
        return true;
    }

    /**
    * Writes a note associated with the cell given by the row and column.
    * NOTE records don't have a length limit.
    *
    * @access public
    * @param integer $row    Zero indexed row
    * @param integer $col    Zero indexed column
    * @param string  $note   The note to write
    */
    function writeNote($row, $col, $note)
    {
        $note_length    = strlen($note);
        $record         = 0x001C;                // Record identifier
        $max_length     = 2048;                  // Maximun length for a NOTE record
        //$length      = 0x0006 + $note_length;    // Bytes to follow

        // Check that row and col are valid and store max and min values
        if ($row >= $this->_xls_rowmax) {
            return(-2);
        }
        if ($col >= $this->_xls_colmax) {
            return(-2);
        }
        if ($row <  $this->_dim_rowmin) {
            $this->_dim_rowmin = $row;
        }
        if ($row >  $this->_dim_rowmax) {
            $this->_dim_rowmax = $row;
        }
        if ($col <  $this->_dim_colmin) {
            $this->_dim_colmin = $col;
        }
        if ($col >  $this->_dim_colmax) {
            $this->_dim_colmax = $col;
        }

        // Length for this record is no more than 2048 + 6
        $length    = 0x0006 + min($note_length, 2048);
        $header    = pack("vv",   $record, $length);
        $data      = pack("vvv", $row, $col, $note_length);
        $this->_append($header . $data . substr($note, 0, 2048));

        for ($i = $max_length; $i < $note_length; $i += $max_length) {
            $chunk  = substr($note, $i, $max_length);
            $length = 0x0006 + strlen($chunk);
            $header = pack("vv",   $record, $length);
            $data   = pack("vvv", -1, 0, strlen($chunk));
            $this->_append($header.$data.$chunk);
        }
        return(0);
    }

    /**
    * Write a blank cell to the specified row and column (zero indexed).
    * A blank cell is used to specify formatting without adding a string
    * or a number.
    *
    * A blank cell without a format serves no purpose. Therefore, we don't write
    * a BLANK record unless a format is specified.
    *
    * Returns  0 : normal termination (including no format)
    *         -1 : insufficient number of arguments
    *         -2 : row or column out of range
    *
    * @access public
    * @param integer $row    Zero indexed row
    * @param integer $col    Zero indexed column
    * @param mixed   $format The XF format
    */
    function writeBlank($row, $col, $format)
    {
        // Don't write a blank cell unless it has a format
        if (!$format) {
            return(0);
        }

        $record    = 0x0201;                 // Record identifier
        $length    = 0x0006;                 // Number of bytes to follow
        $xf        = $this->_XF($format);    // The cell format

        // Check that row and col are valid and store max and min values
        if ($row >= $this->_xls_rowmax) {
            return(-2);
        }
        if ($col >= $this->_xls_colmax) {
            return(-2);
        }
        if ($row <  $this->_dim_rowmin) {
            $this->_dim_rowmin = $row;
        }
        if ($row >  $this->_dim_rowmax) {
            $this->_dim_rowmax = $row;
        }
        if ($col <  $this->_dim_colmin) {
            $this->_dim_colmin = $col;
        }
        if ($col >  $this->_dim_colmax) {
            $this->_dim_colmax = $col;
        }

        $header    = pack("vv",  $record, $length);
        $data      = pack("vvv", $row, $col, $xf);
        $this->_append($header . $data);
        return 0;
    }

    /**
    * Write a formula to the specified row and column (zero indexed).
    * The textual representation of the formula is passed to the parser in
    * Parser.php which returns a packed binary string.
    *
    * Returns  0 : normal termination
    *         -1 : formula errors (bad formula)
    *         -2 : row or column out of range
    *
    * @access public
    * @param integer $row     Zero indexed row
    * @param integer $col     Zero indexed column
    * @param string  $formula The formula text string
    * @param mixed   $format  The optional XF format
    * @return integer
    */
    function writeFormula($row, $col, $formula, $format = null)
    {
        $record    = 0x0006;     // Record identifier

        // Excel normally stores the last calculated value of the formula in $num.
        // Clearly we are not in a position to calculate this a priori. Instead
        // we set $num to zero and set the option flags in $grbit to ensure
        // automatic calculation of the formula when the file is opened.
        //
        $xf        = $this->_XF($format); // The cell format
        $num       = 0x00;                // Current value of formula
        $grbit     = 0x03;                // Option flags
        $unknown   = 0x0000;              // Must be zero


        // Check that row and col are valid and store max and min values
        if ($this->_checkRowCol($row, $col) == false) {
            return -2;
        }

        // Strip the '=' or '@' sign at the beginning of the formula string
        if (preg_match("/^=/", $formula)) {
            $formula = preg_replace("/(^=)/", "", $formula);
        } elseif (preg_match("/^@/", $formula)) {
            $formula = preg_replace("/(^@)/", "", $formula);
        } else {
            // Error handling
            $this->writeString($row, $col, 'Unrecognised character for formula');
            return -1;
        }

        // Parse the formula using the parser in Parser.php
        $this->_parser->parse($formula);
 
        $formula = $this->_parser->toReversePolish();
 
        $formlen    = strlen($formula);    // Length of the binary string
        $length     = 0x16 + $formlen;     // Length of the record data

        $header    = pack("vv",      $record, $length);
        $data      = pack("vvvdvVv", $row, $col, $xf, $num,
                                     $grbit, $unknown, $formlen);

        $this->_append($header . $data . $formula);
        return 0;
    }

    /**
    * Write a hyperlink.
    * This is comprised of two elements: the visible label and
    * the invisible link. The visible label is the same as the link unless an
    * alternative string is specified. The label is written using the
    * writeString() method. Therefore the 255 characters string limit applies.
    * $string and $format are optional.
    *
    * The hyperlink can be to a http, ftp, mail, internal sheet (not yet), or external
    * directory url.
    *
    * Returns  0 : normal termination
    *         -2 : row or column out of range
    *         -3 : long string truncated to 255 chars
    *
    * @access public
    * @param integer $row    Row
    * @param integer $col    Column
    * @param string  $url    URL string
    * @param string  $string Alternative label
    * @param mixed   $format The cell format
    * @return integer
    */
    function writeUrl($row, $col, $url, $string = '', $format = null)
    {
        // Add start row and col to arg list
        return($this->_writeUrlRange($row, $col, $row, $col, $url, $string, $format));
    }

    /**
    * This is the more general form of writeUrl(). It allows a hyperlink to be
    * written to a range of cells. This function also decides the type of hyperlink
    * to be written. These are either, Web (http, ftp, mailto), Internal
    * (Sheet1!A1) or external ('c:\temp\foo.xls#Sheet1!A1').
    *
    * @access private
    * @see writeUrl()
    * @param integer $row1   Start row
    * @param integer $col1   Start column
    * @param integer $row2   End row
    * @param integer $col2   End column
    * @param string  $url    URL string
    * @param string  $string Alternative label
    * @param mixed   $format The cell format
    * @return integer
    */

    function _writeUrlRange($row1, $col1, $row2, $col2, $url, $string = '', $format = null)
    {

        // Check for internal/external sheet links or default to web link
        if (preg_match('[^internal:]', $url)) {
            return($this->_writeUrlInternal($row1, $col1, $row2, $col2, $url, $string, $format));
        }
        if (preg_match('[^external:]', $url)) {
            return($this->_writeUrlExternal($row1, $col1, $row2, $col2, $url, $string, $format));
        }
        return($this->_writeUrlWeb($row1, $col1, $row2, $col2, $url, $string, $format));
    }


    /**
    * Used to write http, ftp and mailto hyperlinks.
    * The link type ($options) is 0x03 is the same as absolute dir ref without
    * sheet. However it is differentiated by the $unknown2 data stream.
    *
    * @access private
    * @see writeUrl()
    * @param integer $row1   Start row
    * @param integer $col1   Start column
    * @param integer $row2   End row
    * @param integer $col2   End column
    * @param string  $url    URL string
    * @param string  $str    Alternative label
    * @param mixed   $format The cell format
    * @return integer
    */
    function _writeUrlWeb($row1, $col1, $row2, $col2, $url, $str, $format = null)
    {
        $record      = 0x01B8;                       // Record identifier
        $length      = 0x00000;                      // Bytes to follow

        if (!$format) {
            $format = $this->_url_format;
        }

        // Write the visible label using the writeString() method.
        if ($str == '') {
            $str = $url;
        }
        $str_error = $this->writeString($row1, $col1, $str, $format);
        if (($str_error == -2) || ($str_error == -3)) {
            return $str_error;
        }

        // Pack the undocumented parts of the hyperlink stream
        $unknown1    = pack("H*", "D0C9EA79F9BACE118C8200AA004BA90B02000000");
        $unknown2    = pack("H*", "E0C9EA79F9BACE118C8200AA004BA90B");

        // Pack the option flags
        $options     = pack("V", 0x03);

        // Convert URL to a null terminated wchar string
        $url         = join("\0", preg_split("''", $url, -1, PREG_SPLIT_NO_EMPTY));
        $url         = $url . "\0\0\0";

        // Pack the length of the URL
        $url_len     = pack("V", strlen($url));

        // Calculate the data length
        $length      = 0x34 + strlen($url);

        // Pack the header data
        $header      = pack("vv",   $record, $length);
        $data        = pack("vvvv", $row1, $row2, $col1, $col2);

        // Write the packed data
        $this->_append($header . $data .
                       $unknown1 . $options .
                       $unknown2 . $url_len . $url);
        return($str_error);
    }

    /**
    * Used to write internal reference hyperlinks such as "Sheet1!A1".
    *
    * @access private
    * @see writeUrl()
    * @param integer $row1   Start row
    * @param integer $col1   Start column
    * @param integer $row2   End row
    * @param integer $col2   End column
    * @param string  $url    URL string
    * @param string  $str    Alternative label
    * @param mixed   $format The cell format
    * @return integer
    */
    function _writeUrlInternal($row1, $col1, $row2, $col2, $url, $str, $format = null)
    {
        $record      = 0x01B8;                       // Record identifier
        $length      = 0x00000;                      // Bytes to follow

        if (!$format) {
            $format = $this->_url_format;
        }

        // Strip URL type
        $url = preg_replace('/^internal:/', '', $url);

        // Write the visible label
        if ($str == '') {
            $str = $url;
        }
        $str_error = $this->writeString($row1, $col1, $str, $format);
        if (($str_error == -2) || ($str_error == -3)) {
            return $str_error;
        }

        // Pack the undocumented parts of the hyperlink stream
        $unknown1    = pack("H*", "D0C9EA79F9BACE118C8200AA004BA90B02000000");

        // Pack the option flags
        $options     = pack("V", 0x08);

        // Convert the URL type and to a null terminated wchar string
        $url         = join("\0", preg_split("''", $url, -1, PREG_SPLIT_NO_EMPTY));
        $url         = $url . "\0\0\0";

        // Pack the length of the URL as chars (not wchars)
        $url_len     = pack("V", floor(strlen($url)/2));

        // Calculate the data length
        $length      = 0x24 + strlen($url);

        // Pack the header data
        $header      = pack("vv",   $record, $length);
        $data        = pack("vvvv", $row1, $row2, $col1, $col2);

        // Write the packed data
        $this->_append($header . $data .
                       $unknown1 . $options .
                       $url_len . $url);
        return($str_error);
    }

    /**
    * Write links to external directory names such as 'c:\foo.xls',
    * c:\foo.xls#Sheet1!A1', '../../foo.xls'. and '../../foo.xls#Sheet1!A1'.
    *
    * Note: Excel writes some relative links with the $dir_long string. We ignore
    * these cases for the sake of simpler code.
    *
    * @access private
    * @see writeUrl()
    * @param integer $row1   Start row
    * @param integer $col1   Start column
    * @param integer $row2   End row
    * @param integer $col2   End column
    * @param string  $url    URL string
    * @param string  $str    Alternative label
    * @param mixed   $format The cell format
    * @return integer
    */
    function _writeUrlExternal($row1, $col1, $row2, $col2, $url, $str, $format = null)
    {
        // Network drives are different. We will handle them separately
        // MS/Novell network drives and shares start with \\
        if (preg_match('[^external:\\\\]', $url)) {
            return; //($this->_writeUrlExternal_net($row1, $col1, $row2, $col2, $url, $str, $format));
        }
    
        $record      = 0x01B8;                       // Record identifier
        $length      = 0x00000;                      // Bytes to follow
    
        if (!$format) {
            $format = $this->_url_format;
        }
    
        // Strip URL type and change Unix dir separator to Dos style (if needed)
        //
        $url = preg_replace('/^external:/', '', $url);
        $url = preg_replace('/\//', "\\", $url);
    
        // Write the visible label
        if ($str == '') {
            $str = preg_replace('/\#/', ' - ', $url);
        }
        $str_error = $this->writeString($row1, $col1, $str, $format);
        if (($str_error == -2) or ($str_error == -3)) {
            return $str_error;
        }
    
        // Determine if the link is relative or absolute:
        //   relative if link contains no dir separator, "somefile.xls"
        //   relative if link starts with up-dir, "..\..\somefile.xls"
        //   otherwise, absolute
        
        $absolute    = 0x02; // Bit mask
        if (!preg_match("/\\\/", $url)) {
            $absolute    = 0x00;
        }
        if (preg_match("/^\.\.\\\/", $url)) {
            $absolute    = 0x00;
        }
        $link_type               = 0x01 | $absolute;
    
        // Determine if the link contains a sheet reference and change some of the
        // parameters accordingly.
        // Split the dir name and sheet name (if it exists)
        /*if (preg_match("/\#/", $url)) {
            list($dir_long, $sheet) = preg_split("/\#/", $url);
        } else {
            $dir_long = $url;
        }
    
        if (isset($sheet)) {
            $link_type |= 0x08;
            $sheet_len  = pack("V", strlen($sheet) + 0x01);
            $sheet      = join("\0", preg_split('//', $sheet));
            $sheet     .= "\0\0\0";
        } else {
            $sheet_len   = '';
            $sheet       = '';
        }*/
        $dir_long = $url;
        if (preg_match("/\#/", $url)) {
            $link_type |= 0x08;
        }


    
        // Pack the link type
        $link_type   = pack("V", $link_type);
    
        // Calculate the up-level dir count e.g.. (..\..\..\ == 3)
        $up_count    = preg_match_all("/\.\.\\\/", $dir_long, $useless);
        $up_count    = pack("v", $up_count);
    
        // Store the short dos dir name (null terminated)
        $dir_short   = preg_replace("/\.\.\\\/", '', $dir_long) . "\0";
    
        // Store the long dir name as a wchar string (non-null terminated)
        //$dir_long       = join("\0", preg_split('//', $dir_long));
        $dir_long       = $dir_long . "\0";
    
        // Pack the lengths of the dir strings
        $dir_short_len = pack("V", strlen($dir_short)      );
        $dir_long_len  = pack("V", strlen($dir_long)       );
        $stream_len    = pack("V", 0);//strlen($dir_long) + 0x06);
    
        // Pack the undocumented parts of the hyperlink stream
        $unknown1 = pack("H*",'D0C9EA79F9BACE118C8200AA004BA90B02000000'       );
        $unknown2 = pack("H*",'0303000000000000C000000000000046'               );
        $unknown3 = pack("H*",'FFFFADDE000000000000000000000000000000000000000');
        $unknown4 = pack("v",  0x03                                            );
    
        // Pack the main data stream
        $data        = pack("vvvv", $row1, $row2, $col1, $col2) .
                          $unknown1     .
                          $link_type    .
                          $unknown2     .
                          $up_count     .
                          $dir_short_len.
                          $dir_short    .
                          $unknown3     .
                          $stream_len   .
                          $dir_long_len .
                          $unknown4     .
                          $dir_long     .
                          $sheet_len    .
                          $sheet        ;
    
        // Pack the header data
        $length   = strlen($data);
        $header   = pack("vv", $record, $length);
    
        // Write the packed data
        $this->_append($header. $data);
        return($str_error);
    }


    /**
    * This method is used to set the height and format for a row.
    *
    * @access public
    * @param integer $row    The row to set
    * @param integer $height Height we are giving to the row.
    *                        Use null to set XF without setting height
    * @param mixed   $format XF format we are giving to the row
    * @param bool    $hidden The optional hidden attribute
    * @param integer $level  The optional outline level for row, in range [0,7]
    */
    function setRow($row, $height, $format = null, $hidden = false, $level = 0)
    {
        $record      = 0x0208;               // Record identifier
        $length      = 0x0010;               // Number of bytes to follow

        $colMic      = 0x0000;               // First defined column
        $colMac      = 0x0000;               // Last defined column
        $irwMac      = 0x0000;               // Used by Excel to optimise loading
        $reserved    = 0x0000;               // Reserved
        $grbit       = 0x0000;               // Option flags
        $ixfe        = $this->_XF($format);  // XF index

        // set _row_sizes so _sizeRow() can use it
        $this->_row_sizes[$row] = $height;

        // Use setRow($row, null, $XF) to set XF format without setting height
        if ($height != null) {
            $miyRw = $height * 20;  // row height
        } else {
            $miyRw = 0xff;          // default row height is 256
        }

        $level = max(0, min($level, 7));  // level should be between 0 and 7
        $this->_outline_row_level = max($level, $this->_outline_row_level);


        // Set the options flags. fUnsynced is used to show that the font and row
        // heights are not compatible. This is usually the case for WriteExcel.
        // The collapsed flag 0x10 doesn't seem to be used to indicate that a row
        // is collapsed. Instead it is used to indicate that the previous row is
        // collapsed. The zero height flag, 0x20, is used to collapse a row.

        $grbit |= $level;
        if ($hidden) {
            $grbit |= 0x0020;
        }
        $grbit |= 0x0040; // fUnsynced
        if ($format) {
            $grbit |= 0x0080;
        }
        $grbit |= 0x0100;

        $header   = pack("vv",       $record, $length);
        $data     = pack("vvvvvvvv", $row, $colMic, $colMac, $miyRw,
                                     $irwMac,$reserved, $grbit, $ixfe);
        $this->_append($header.$data);
    }

    /**
    * Writes Excel DIMENSIONS to define the area in which there is data.
    *
    * @access private
    */
    function _storeDimensions()
    {
        $record    = 0x0200;                 // Record identifier
        $row_min   = $this->_dim_rowmin;     // First row
        $row_max   = $this->_dim_rowmax + 1; // Last row plus 1
        $col_min   = $this->_dim_colmin;     // First column
        $col_max   = $this->_dim_colmax + 1; // Last column plus 1
        $reserved  = 0x0000;                 // Reserved by Excel

        if ($this->_BIFF_version == 0x0500) {
            $length    = 0x000A;               // Number of bytes to follow
            $data      = pack("vvvvv", $row_min, $row_max,
                                       $col_min, $col_max, $reserved);
        } elseif ($this->_BIFF_version == 0x0600) {
            $length    = 0x000E;
            $data      = pack("VVvvv", $row_min, $row_max,
                                       $col_min, $col_max, $reserved);
        }
        $header = pack("vv", $record, $length);
        $this->_prepend($header.$data);
    }

    /**
    * Write BIFF record Window2.
    *
    * @access private
    */
    function _storeWindow2()
    {
        $record         = 0x023E;     // Record identifier
        if ($this->_BIFF_version == 0x0500) {
            $length         = 0x000A;     // Number of bytes to follow
        } elseif ($this->_BIFF_version == 0x0600) {
            $length         = 0x0012;
        }

        $grbit          = 0x00B6;     // Option flags
        $rwTop          = 0x0000;     // Top row visible in window
        $colLeft        = 0x0000;     // Leftmost column visible in window


        // The options flags that comprise $grbit
        $fDspFmla       = 0;                     // 0 - bit
        $fDspGrid       = $this->_screen_gridlines; // 1
        $fDspRwCol      = 1;                     // 2
        $fFrozen        = $this->_frozen;        // 3
        $fDspZeros      = 1;                     // 4
        $fDefaultHdr    = 1;                     // 5
        $fArabic        = $this->_rtl;           // 6
        $fDspGuts       = $this->_outline_on;    // 7
        $fFrozenNoSplit = 0;                     // 0 - bit
        $fSelected      = $this->selected;       // 1
        $fPaged         = 1;                     // 2

        $grbit             = $fDspFmla;
        $grbit            |= $fDspGrid       << 1;
        $grbit            |= $fDspRwCol      << 2;
        $grbit            |= $fFrozen        << 3;
        $grbit            |= $fDspZeros      << 4;
        $grbit            |= $fDefaultHdr    << 5;
        $grbit            |= $fArabic        << 6;
        $grbit            |= $fDspGuts       << 7;
        $grbit            |= $fFrozenNoSplit << 8;
        $grbit            |= $fSelected      << 9;
        $grbit            |= $fPaged         << 10;

        $header  = pack("vv",   $record, $length);
        $data    = pack("vvv", $grbit, $rwTop, $colLeft);
        // FIXME !!!
        if ($this->_BIFF_version == 0x0500) {
            $rgbHdr         = 0x00000000; // Row/column heading and gridline color
            $data .= pack("V", $rgbHdr);
        } elseif ($this->_BIFF_version == 0x0600) {
            $rgbHdr       = 0x0040; // Row/column heading and gridline color index
            $zoom_factor_page_break = 0x0000;
            $zoom_factor_normal     = 0x0000;
            $data .= pack("vvvvV", $rgbHdr, 0x0000, $zoom_factor_page_break, $zoom_factor_normal, 0x00000000);
        }
        $this->_append($header.$data);
    }

    /**
    * Write BIFF record DEFCOLWIDTH if COLINFO records are in use.
    *
    * @access private
    */
    function _storeDefcol()
    {
        $record   = 0x0055;      // Record identifier
        $length   = 0x0002;      // Number of bytes to follow
        $colwidth = 0x0008;      // Default column width

        $header   = pack("vv", $record, $length);
        $data     = pack("v",  $colwidth);
        $this->_prepend($header . $data);
    }

    /**
    * Write BIFF record COLINFO to define column widths
    *
    * Note: The SDK says the record length is 0x0B but Excel writes a 0x0C
    * length record.
    *
    * @access private
    * @param array $col_array This is the only parameter received and is composed of the following:
    *                0 => First formatted column,
    *                1 => Last formatted column,
    *                2 => Col width (8.43 is Excel default),
    *                3 => The optional XF format of the column,
    *                4 => Option flags.
    *                5 => Optional outline level
    */
    function _storeColinfo($col_array)
    {
        if (isset($col_array[0])) {
            $colFirst = $col_array[0];
        }
        if (isset($col_array[1])) {
            $colLast = $col_array[1];
        }
        if (isset($col_array[2])) {
            $coldx = $col_array[2];
        } else {
            $coldx = 8.43;
        }
        if (isset($col_array[3])) {
            $format = $col_array[3];
        } else {
            $format = 0;
        }
        if (isset($col_array[4])) {
            $grbit = $col_array[4];
        } else {
            $grbit = 0;
        }
        if (isset($col_array[5])) {
            $level = $col_array[5];
        } else {
            $level = 0;
        }
        $record   = 0x007D;          // Record identifier
        $length   = 0x000B;          // Number of bytes to follow

        $coldx   += 0.72;            // Fudge. Excel subtracts 0.72 !?
        $coldx   *= 256;             // Convert to units of 1/256 of a char

        $ixfe     = $this->_XF($format);
        $reserved = 0x00;            // Reserved

        $level = max(0, min($level, 7));
        $grbit |= $level << 8;

        $header   = pack("vv",     $record, $length);
        $data     = pack("vvvvvC", $colFirst, $colLast, $coldx,
                                   $ixfe, $grbit, $reserved);
        $this->_prepend($header.$data);
    }

    /**
    * Write BIFF record SELECTION.
    *
    * @access private
    * @param array $array array containing ($rwFirst,$colFirst,$rwLast,$colLast)
    * @see setSelection()
    */
    function _storeSelection($array)
    {
        list($rwFirst,$colFirst,$rwLast,$colLast) = $array;
        $record   = 0x001D;                  // Record identifier
        $length   = 0x000F;                  // Number of bytes to follow

        $pnn      = $this->_active_pane;     // Pane position
        $rwAct    = $rwFirst;                // Active row
        $colAct   = $colFirst;               // Active column
        $irefAct  = 0;                       // Active cell ref
        $cref     = 1;                       // Number of refs

        if (!isset($rwLast)) {
            $rwLast   = $rwFirst;       // Last  row in reference
        }
        if (!isset($colLast)) {
            $colLast  = $colFirst;      // Last  col in reference
        }

        // Swap last row/col for first row/col as necessary
        if ($rwFirst > $rwLast) {
            list($rwFirst, $rwLast) = array($rwLast, $rwFirst);
        }

        if ($colFirst > $colLast) {
            list($colFirst, $colLast) = array($colLast, $colFirst);
        }

        $header   = pack("vv",         $record, $length);
        $data     = pack("CvvvvvvCC",  $pnn, $rwAct, $colAct,
                                       $irefAct, $cref,
                                       $rwFirst, $rwLast,
                                       $colFirst, $colLast);
        $this->_append($header . $data);
    }

    /**
    * Store the MERGEDCELLS record for all ranges of merged cells
    *
    * @access private
    */
    function _storeMergedCells()
    {
        // if there are no merged cell ranges set, return
        if (count($this->_merged_ranges) == 0) {
            return;
        }
        $record   = 0x00E5;
        $length   = 2 + count($this->_merged_ranges) * 8;

        $header   = pack('vv', $record, $length);
        $data     = pack('v',  count($this->_merged_ranges));
        foreach ($this->_merged_ranges as $range) {
            $data .= pack('vvvv', $range[0], $range[2], $range[1], $range[3]);
        }
        $this->_append($header . $data);
    }

    /**
    * Write BIFF record EXTERNCOUNT to indicate the number of external sheet
    * references in a worksheet.
    *
    * Excel only stores references to external sheets that are used in formulas.
    * For simplicity we store references to all the sheets in the workbook
    * regardless of whether they are used or not. This reduces the overall
    * complexity and eliminates the need for a two way dialogue between the formula
    * parser the worksheet objects.
    *
    * @access private
    * @param integer $count The number of external sheet references in this worksheet
    */
    function _storeExterncount($count)
    {
        $record = 0x0016;          // Record identifier
        $length = 0x0002;          // Number of bytes to follow

        $header = pack("vv", $record, $length);
        $data   = pack("v",  $count);
        $this->_prepend($header . $data);
    }

    /**
    * Writes the Excel BIFF EXTERNSHEET record. These references are used by
    * formulas. A formula references a sheet name via an index. Since we store a
    * reference to all of the external worksheets the EXTERNSHEET index is the same
    * as the worksheet index.
    *
    * @access private
    * @param string $sheetname The name of a external worksheet
    */
    function _storeExternsheet($sheetname)
    {
        $record    = 0x0017;         // Record identifier

        // References to the current sheet are encoded differently to references to
        // external sheets.
        //
        if ($this->name == $sheetname) {
            $sheetname = '';
            $length    = 0x02;  // The following 2 bytes
            $cch       = 1;     // The following byte
            $rgch      = 0x02;  // Self reference
        } else {
            $length    = 0x02 + strlen($sheetname);
            $cch       = strlen($sheetname);
            $rgch      = 0x03;  // Reference to a sheet in the current workbook
        }

        $header = pack("vv",  $record, $length);
        $data   = pack("CC", $cch, $rgch);
        $this->_prepend($header . $data . $sheetname);
    }

    /**
    * Writes the Excel BIFF PANE record.
    * The panes can either be frozen or thawed (unfrozen).
    * Frozen panes are specified in terms of an integer number of rows and columns.
    * Thawed panes are specified in terms of Excel's units for rows and columns.
    *
    * @access private
    * @param array $panes This is the only parameter received and is composed of the following:
    *                     0 => Vertical split position,
    *                     1 => Horizontal split position
    *                     2 => Top row visible
    *                     3 => Leftmost column visible
    *                     4 => Active pane
    */
    function _storePanes($panes)
    {
        $y       = $panes[0];
        $x       = $panes[1];
        $rwTop   = $panes[2];
        $colLeft = $panes[3];
        if (count($panes) > 4) { // if Active pane was received
            $pnnAct = $panes[4];
        } else {
            $pnnAct = null;
        }
        $record  = 0x0041;       // Record identifier
        $length  = 0x000A;       // Number of bytes to follow

        // Code specific to frozen or thawed panes.
        if ($this->_frozen) {
            // Set default values for $rwTop and $colLeft
            if (!isset($rwTop)) {
                $rwTop   = $y;
            }
            if (!isset($colLeft)) {
                $colLeft = $x;
            }
        } else {
            // Set default values for $rwTop and $colLeft
            if (!isset($rwTop)) {
                $rwTop   = 0;
            }
            if (!isset($colLeft)) {
                $colLeft = 0;
            }

            // Convert Excel's row and column units to the internal units.
            // The default row height is 12.75
            // The default column width is 8.43
            // The following slope and intersection values were interpolated.
            //
            $y = 20*$y      + 255;
            $x = 113.879*$x + 390;
        }


        // Determine which pane should be active. There is also the undocumented
        // option to override this should it be necessary: may be removed later.
        //
        if (!isset($pnnAct)) {
            if ($x != 0 && $y != 0) {
                $pnnAct = 0; // Bottom right
            }
            if ($x != 0 && $y == 0) {
                $pnnAct = 1; // Top right
            }
            if ($x == 0 && $y != 0) {
                $pnnAct = 2; // Bottom left
            }
            if ($x == 0 && $y == 0) {
                $pnnAct = 3; // Top left
            }
        }

        $this->_active_pane = $pnnAct; // Used in _storeSelection

        $header     = pack("vv",    $record, $length);
        $data       = pack("vvvvv", $x, $y, $rwTop, $colLeft, $pnnAct);
        $this->_append($header . $data);
    }

    /**
    * Store the page setup SETUP BIFF record.
    *
    * @access private
    */
    function _storeSetup()
    {
        $record       = 0x00A1;                  // Record identifier
        $length       = 0x0022;                  // Number of bytes to follow

        $iPaperSize   = $this->_paper_size;    // Paper size
        $iScale       = $this->_print_scale;   // Print scaling factor
        $iPageStart   = 0x01;                 // Starting page number
        $iFitWidth    = $this->_fit_width;    // Fit to number of pages wide
        $iFitHeight   = $this->_fit_height;   // Fit to number of pages high
        $grbit        = 0x00;                 // Option flags
        $iRes         = 0x0258;               // Print resolution
        $iVRes        = 0x0258;               // Vertical print resolution
        $numHdr       = $this->_margin_head;  // Header Margin
        $numFtr       = $this->_margin_foot;   // Footer Margin
        $iCopies      = 0x01;                 // Number of copies

        $fLeftToRight = 0x0;                     // Print over then down
        $fLandscape   = $this->_orientation;     // Page orientation
        $fNoPls       = 0x0;                     // Setup not read from printer
        $fNoColor     = 0x0;                     // Print black and white
        $fDraft       = 0x0;                     // Print draft quality
        $fNotes       = 0x0;                     // Print notes
        $fNoOrient    = 0x0;                     // Orientation not set
        $fUsePage     = 0x0;                     // Use custom starting page

        $grbit           = $fLeftToRight;
        $grbit          |= $fLandscape    << 1;
        $grbit          |= $fNoPls        << 2;
        $grbit          |= $fNoColor      << 3;
        $grbit          |= $fDraft        << 4;
        $grbit          |= $fNotes        << 5;
        $grbit          |= $fNoOrient     << 6;
        $grbit          |= $fUsePage      << 7;

        $numHdr = pack("d", $numHdr);
        $numFtr = pack("d", $numFtr);
        if ($this->_byte_order) { // if it's Big Endian
            $numHdr = strrev($numHdr);
            $numFtr = strrev($numFtr);
        }

        $header = pack("vv", $record, $length);
        $data1  = pack("vvvvvvvv", $iPaperSize,
                                   $iScale,
                                   $iPageStart,
                                   $iFitWidth,
                                   $iFitHeight,
                                   $grbit,
                                   $iRes,
                                   $iVRes);
        $data2  = $numHdr.$numFtr;
        $data3  = pack("v", $iCopies);
        $this->_prepend($header . $data1 . $data2 . $data3);
    }

    /**
    * Store the header caption BIFF record.
    *
    * @access private
    */
    function _storeHeader()
    {
        $record  = 0x0014;               // Record identifier

        $str      = $this->_header;       // header string
        $cch      = strlen($str);         // Length of header string
        if ($this->_BIFF_version == 0x0600) {
            $encoding = 0x0;                  // TODO: Unicode support
            $length   = 3 + $cch;             // Bytes to follow
        } else {
            $length  = 1 + $cch;             // Bytes to follow
        }

        $header   = pack("vv", $record, $length);
        if ($this->_BIFF_version == 0x0600) {
            $data     = pack("vC",  $cch, $encoding);
        } else {
            $data      = pack("C",  $cch);
        }

        $this->_prepend($header.$data.$str);
    }

    /**
    * Store the footer caption BIFF record.
    *
    * @access private
    */
    function _storeFooter()
    {
        $record  = 0x0015;               // Record identifier

        $str      = $this->_footer;       // Footer string
        $cch      = strlen($str);         // Length of footer string
        if ($this->_BIFF_version == 0x0600) {
            $encoding = 0x0;                  // TODO: Unicode support
            $length   = 3 + $cch;             // Bytes to follow
        } else {
            $length  = 1 + $cch;
        }

        $header    = pack("vv", $record, $length);
        if ($this->_BIFF_version == 0x0600) {
            $data      = pack("vC",  $cch, $encoding);
        } else {
            $data      = pack("C",  $cch);
        }

        $this->_prepend($header . $data . $str);
    }

    /**
    * Store the horizontal centering HCENTER BIFF record.
    *
    * @access private
    */
    function _storeHcenter()
    {
        $record   = 0x0083;              // Record identifier
        $length   = 0x0002;              // Bytes to follow

        $fHCenter = $this->_hcenter;     // Horizontal centering

        $header    = pack("vv", $record, $length);
        $data      = pack("v",  $fHCenter);

        $this->_prepend($header.$data);
    }

    /**
    * Store the vertical centering VCENTER BIFF record.
    *
    * @access private
    */
    function _storeVcenter()
    {
        $record   = 0x0084;              // Record identifier
        $length   = 0x0002;              // Bytes to follow

        $fVCenter = $this->_vcenter;     // Horizontal centering

        $header    = pack("vv", $record, $length);
        $data      = pack("v",  $fVCenter);
        $this->_prepend($header . $data);
    }

    /**
    * Store the LEFTMARGIN BIFF record.
    *
    * @access private
    */
    function _storeMarginLeft()
    {
        $record  = 0x0026;                   // Record identifier
        $length  = 0x0008;                   // Bytes to follow

        $margin  = $this->_margin_left;       // Margin in inches

        $header    = pack("vv",  $record, $length);
        $data      = pack("d",   $margin);
        if ($this->_byte_order) { // if it's Big Endian
            $data = strrev($data);
        }

        $this->_prepend($header . $data);
    }

    /**
    * Store the RIGHTMARGIN BIFF record.
    *
    * @access private
    */
    function _storeMarginRight()
    {
        $record  = 0x0027;                   // Record identifier
        $length  = 0x0008;                   // Bytes to follow

        $margin  = $this->_margin_right;      // Margin in inches

        $header    = pack("vv",  $record, $length);
        $data      = pack("d",   $margin);
        if ($this->_byte_order) { // if it's Big Endian
            $data = strrev($data);
        }

        $this->_prepend($header . $data);
    }

    /**
    * Store the TOPMARGIN BIFF record.
    *
    * @access private
    */
    function _storeMarginTop()
    {
        $record  = 0x0028;                   // Record identifier
        $length  = 0x0008;                   // Bytes to follow

        $margin  = $this->_margin_top;        // Margin in inches

        $header    = pack("vv",  $record, $length);
        $data      = pack("d",   $margin);
        if ($this->_byte_order) { // if it's Big Endian
            $data = strrev($data);
        }

        $this->_prepend($header . $data);
    }

    /**
    * Store the BOTTOMMARGIN BIFF record.
    *
    * @access private
    */
    function _storeMarginBottom()
    {
        $record  = 0x0029;                   // Record identifier
        $length  = 0x0008;                   // Bytes to follow

        $margin  = $this->_margin_bottom;     // Margin in inches

        $header    = pack("vv",  $record, $length);
        $data      = pack("d",   $margin);
        if ($this->_byte_order) { // if it's Big Endian
            $data = strrev($data);
        }

        $this->_prepend($header . $data);
    }

    /**
    * Merges the area given by its arguments.
    * This is an Excel97/2000 method. It is required to perform more complicated
    * merging than the normal setAlign('merge').
    *
    * @access public
    * @param integer $first_row First row of the area to merge
    * @param integer $first_col First column of the area to merge
    * @param integer $last_row  Last row of the area to merge
    * @param integer $last_col  Last column of the area to merge
    */
    function mergeCells($first_row, $first_col, $last_row, $last_col)
    {
        $record  = 0x00E5;                   // Record identifier
        $length  = 0x000A;                   // Bytes to follow
        $cref     = 1;                       // Number of refs

        // Swap last row/col for first row/col as necessary
        if ($first_row > $last_row) {
            list($first_row, $last_row) = array($last_row, $first_row);
        }

        if ($first_col > $last_col) {
            list($first_col, $last_col) = array($last_col, $first_col);
        }

        $header   = pack("vv",    $record, $length);
        $data     = pack("vvvvv", $cref, $first_row, $last_row,
                                  $first_col, $last_col);

        $this->_append($header.$data);
    }

    /**
    * Write the PRINTHEADERS BIFF record.
    *
    * @access private
    */
    function _storePrintHeaders()
    {
        $record      = 0x002a;                   // Record identifier
        $length      = 0x0002;                   // Bytes to follow

        $fPrintRwCol = $this->_print_headers;     // Boolean flag

        $header      = pack("vv", $record, $length);
        $data        = pack("v", $fPrintRwCol);
        $this->_prepend($header . $data);
    }

    /**
    * Write the PRINTGRIDLINES BIFF record. Must be used in conjunction with the
    * GRIDSET record.
    *
    * @access private
    */
    function _storePrintGridlines()
    {
        $record      = 0x002b;                    // Record identifier
        $length      = 0x0002;                    // Bytes to follow

        $fPrintGrid  = $this->_print_gridlines;    // Boolean flag

        $header      = pack("vv", $record, $length);
        $data        = pack("v", $fPrintGrid);
        $this->_prepend($header . $data);
    }

    /**
    * Write the GRIDSET BIFF record. Must be used in conjunction with the
    * PRINTGRIDLINES record.
    *
    * @access private
    */
    function _storeGridset()
    {
        $record      = 0x0082;                        // Record identifier
        $length      = 0x0002;                        // Bytes to follow

        $fGridSet    = !($this->_print_gridlines);     // Boolean flag

        $header      = pack("vv",  $record, $length);
        $data        = pack("v",   $fGridSet);
        $this->_prepend($header . $data);
    }

    /**
    * Write the GUTS BIFF record. This is used to configure the gutter margins
    * where Excel outline symbols are displayed. The visibility of the gutters is
    * controlled by a flag in WSBOOL.
    *
    * @see _storeWsbool()
    * @access private
    */
    function _storeGuts()
    {
        $record      = 0x0080;   // Record identifier
        $length      = 0x0008;   // Bytes to follow

        $dxRwGut     = 0x0000;   // Size of row gutter
        $dxColGut    = 0x0000;   // Size of col gutter

        $row_level   = $this->_outline_row_level;
        $col_level   = 0;

        // Calculate the maximum column outline level. The equivalent calculation
        // for the row outline level is carried out in setRow().
        $colcount = count($this->_colinfo);
        for ($i = 0; $i < $colcount; $i++) {
           // Skip cols without outline level info.
           if (count_array($col_level) >= 6) {
              $col_level = max($this->_colinfo[$i][5], $col_level);
           }
        }

        // Set the limits for the outline levels (0 <= x <= 7).
        $col_level = max(0, min($col_level, 7));

        // The displayed level is one greater than the max outline levels
        if ($row_level) {
            $row_level++;
        }
        if ($col_level) {
            $col_level++;
        }

        $header      = pack("vv",   $record, $length);
        $data        = pack("vvvv", $dxRwGut, $dxColGut, $row_level, $col_level);

        $this->_prepend($header.$data);
    }


    /**
    * Write the WSBOOL BIFF record, mainly for fit-to-page. Used in conjunction
    * with the SETUP record.
    *
    * @access private
    */
    function _storeWsbool()
    {
        $record      = 0x0081;   // Record identifier
        $length      = 0x0002;   // Bytes to follow
        $grbit       = 0x0000;

        // The only option that is of interest is the flag for fit to page. So we
        // set all the options in one go.
        //
        /*if ($this->_fit_page) {
            $grbit = 0x05c1;
        } else {
            $grbit = 0x04c1;
        }*/
        // Set the option flags
        $grbit |= 0x0001;                           // Auto page breaks visible
        if ($this->_outline_style) {
            $grbit |= 0x0020; // Auto outline styles
        }
        if ($this->_outline_below) {
            $grbit |= 0x0040; // Outline summary below
        }
        if ($this->_outline_right) {
            $grbit |= 0x0080; // Outline summary right
        }
        if ($this->_fit_page) {
            $grbit |= 0x0100; // Page setup fit to page
        }
        if ($this->_outline_on) {
            $grbit |= 0x0400; // Outline symbols displayed
        }

        $header      = pack("vv", $record, $length);
        $data        = pack("v",  $grbit);
        $this->_prepend($header . $data);
    }

    /**
    * Write the HORIZONTALPAGEBREAKS BIFF record.
    *
    * @access private
    */
    function _storeHbreak()
    {
        // Return if the user hasn't specified pagebreaks
        if (empty($this->_hbreaks)) {
            return;
        }

        // Sort and filter array of page breaks
        $breaks = $this->_hbreaks;
        sort($breaks, SORT_NUMERIC);
        if ($breaks[0] == 0) { // don't use first break if it's 0
            array_shift($breaks);
        }

        $record  = 0x001b;               // Record identifier
        $cbrk    = count($breaks);       // Number of page breaks
        if ($this->_BIFF_version == 0x0600) {
            $length  = 2 + 6*$cbrk;      // Bytes to follow
        } else {
            $length  = 2 + 2*$cbrk;      // Bytes to follow
        }

        $header  = pack("vv", $record, $length);
        $data    = pack("v",  $cbrk);

        // Append each page break
        foreach ($breaks as $break) {
            if ($this->_BIFF_version == 0x0600) {
                $data .= pack("vvv", $break, 0x0000, 0x00ff);
            } else {
                $data .= pack("v", $break);
            }
        }

        $this->_prepend($header.$data);
    }


    /**
    * Write the VERTICALPAGEBREAKS BIFF record.
    *
    * @access private
    */
    function _storeVbreak()
    {
        // Return if the user hasn't specified pagebreaks
        if (empty($this->_vbreaks)) {
            return;
        }

        // 1000 vertical pagebreaks appears to be an internal Excel 5 limit.
        // It is slightly higher in Excel 97/200, approx. 1026
        $breaks = array_slice($this->_vbreaks,0,1000);

        // Sort and filter array of page breaks
        sort($breaks, SORT_NUMERIC);
        if ($breaks[0] == 0) { // don't use first break if it's 0
            array_shift($breaks);
        }

        $record  = 0x001a;               // Record identifier
        $cbrk    = count($breaks);       // Number of page breaks
        if ($this->_BIFF_version == 0x0600) {
            $length  = 2 + 6*$cbrk;      // Bytes to follow
        } else {
            $length  = 2 + 2*$cbrk;      // Bytes to follow
        }

        $header  = pack("vv",  $record, $length);
        $data    = pack("v",   $cbrk);

        // Append each page break
        foreach ($breaks as $break) {
            if ($this->_BIFF_version == 0x0600) {
                $data .= pack("vvv", $break, 0x0000, 0xffff);
            } else {
                $data .= pack("v", $break);
            }
        }

        $this->_prepend($header . $data);
    }

    /**
    * Set the Biff PROTECT record to indicate that the worksheet is protected.
    *
    * @access private
    */
    function _storeProtect()
    {
        // Exit unless sheet protection has been specified
        if ($this->_protect == 0) {
            return;
        }

        $record      = 0x0012;             // Record identifier
        $length      = 0x0002;             // Bytes to follow

        $fLock       = $this->_protect;    // Worksheet is protected

        $header      = pack("vv", $record, $length);
        $data        = pack("v",  $fLock);

        $this->_prepend($header.$data);
    }

    /**
    * Write the worksheet PASSWORD record.
    *
    * @access private
    */
    function _storePassword()
    {
        // Exit unless sheet protection and password have been specified
        if (($this->_protect == 0) || (!isset($this->_password))) {
            return;
        }

        $record      = 0x0013;               // Record identifier
        $length      = 0x0002;               // Bytes to follow

        $wPassword   = $this->_password;     // Encoded password

        $header      = pack("vv", $record, $length);
        $data        = pack("v",  $wPassword);

        $this->_prepend($header . $data);
    }


    /**
    * Insert a 24bit bitmap image in a worksheet.
    *
    * @access public
    * @param integer $row     The row we are going to insert the bitmap into
    * @param integer $col     The column we are going to insert the bitmap into
    * @param string  $bitmap  The bitmap filename
    * @param integer $x       The horizontal position (offset) of the image inside the cell.
    * @param integer $y       The vertical position (offset) of the image inside the cell.
    * @param integer $scale_x The horizontal scale
    * @param integer $scale_y The vertical scale
    */
    function insertBitmap($row, $col, $bitmap, $x = 0, $y = 0, $scale_x = 1, $scale_y = 1)
    {
        $bitmap_array = $this->_processBitmap($bitmap);
        if ($this->isError($bitmap_array)) {
            $this->writeString($row, $col, $bitmap_array->getMessage());
            return;
        }
        list($width, $height, $size, $data) = $bitmap_array; //$this->_processBitmap($bitmap);

        // Scale the frame of the image.
        $width  *= $scale_x;
        $height *= $scale_y;

        // Calculate the vertices of the image and write the OBJ record
        $this->_positionImage($col, $row, $x, $y, $width, $height);

        // Write the IMDATA record to store the bitmap data
        $record      = 0x007f;
        $length      = 8 + $size;
        $cf          = 0x09;
        $env         = 0x01;
        $lcb         = $size;

        $header      = pack("vvvvV", $record, $length, $cf, $env, $lcb);
        $this->_append($header.$data);
    }

    /**
    * Calculate the vertices that define the position of the image as required by
    * the OBJ record.
    *
    *         +------------+------------+
    *         |     A      |      B     |
    *   +-----+------------+------------+
    *   |     |(x1,y1)     |            |
    *   |  1  |(A1)._______|______      |
    *   |     |    |              |     |
    *   |     |    |              |     |
    *   +-----+----|    BITMAP    |-----+
    *   |     |    |              |     |
    *   |  2  |    |______________.     |
    *   |     |            |        (B2)|
    *   |     |            |     (x2,y2)|
    *   +---- +------------+------------+
    *
    * Example of a bitmap that covers some of the area from cell A1 to cell B2.
    *
    * Based on the width and height of the bitmap we need to calculate 8 vars:
    *     $col_start, $row_start, $col_end, $row_end, $x1, $y1, $x2, $y2.
    * The width and height of the cells are also variable and have to be taken into
    * account.
    * The values of $col_start and $row_start are passed in from the calling
    * function. The values of $col_end and $row_end are calculated by subtracting
    * the width and height of the bitmap from the width and height of the
    * underlying cells.
    * The vertices are expressed as a percentage of the underlying cell width as
    * follows (rhs values are in pixels):
    *
    *       x1 = X / W *1024
    *       y1 = Y / H *256
    *       x2 = (X-1) / W *1024
    *       y2 = (Y-1) / H *256
    *
    *       Where:  X is distance from the left side of the underlying cell
    *               Y is distance from the top of the underlying cell
    *               W is the width of the cell
    *               H is the height of the cell
    *
    * @access private
    * @note  the SDK incorrectly states that the height should be expressed as a
    *        percentage of 1024.
    * @param integer $col_start Col containing upper left corner of object
    * @param integer $row_start Row containing top left corner of object
    * @param integer $x1        Distance to left side of object
    * @param integer $y1        Distance to top of object
    * @param integer $width     Width of image frame
    * @param integer $height    Height of image frame
    */
    function _positionImage($col_start, $row_start, $x1, $y1, $width, $height)
    {
        // Initialise end cell to the same as the start cell
        $col_end    = $col_start;  // Col containing lower right corner of object
        $row_end    = $row_start;  // Row containing bottom right corner of object

        // Zero the specified offset if greater than the cell dimensions
        if ($x1 >= $this->_sizeCol($col_start)) {
            $x1 = 0;
        }
        if ($y1 >= $this->_sizeRow($row_start)) {
            $y1 = 0;
        }

        $width      = $width  + $x1 -1;
        $height     = $height + $y1 -1;

        // Subtract the underlying cell widths to find the end cell of the image
        while ($width >= $this->_sizeCol($col_end)) {
            $width -= $this->_sizeCol($col_end);
            $col_end++;
        }

        // Subtract the underlying cell heights to find the end cell of the image
        while ($height >= $this->_sizeRow($row_end)) {
            $height -= $this->_sizeRow($row_end);
            $row_end++;
        }

        // Bitmap isn't allowed to start or finish in a hidden cell, i.e. a cell
        // with zero eight or width.
        //
        if ($this->_sizeCol($col_start) == 0) {
            return;
        }
        if ($this->_sizeCol($col_end)   == 0) {
            return;
        }
        if ($this->_sizeRow($row_start) == 0) {
            return;
        }
        if ($this->_sizeRow($row_end)   == 0) {
            return;
        }

        // Convert the pixel values to the percentage value expected by Excel
        $x1 = $x1     / $this->_sizeCol($col_start)   * 1024;
        $y1 = $y1     / $this->_sizeRow($row_start)   *  256;
        $x2 = $width  / $this->_sizeCol($col_end)     * 1024; // Distance to right side of object
        $y2 = $height / $this->_sizeRow($row_end)     *  256; // Distance to bottom of object

        $this->_storeObjPicture($col_start, $x1,
                                 $row_start, $y1,
                                 $col_end, $x2,
                                 $row_end, $y2);
    }

    /**
    * Convert the width of a cell from user's units to pixels. By interpolation
    * the relationship is: y = 7x +5. If the width hasn't been set by the user we
    * use the default value. If the col is hidden we use a value of zero.
    *
    * @access private
    * @param integer $col The column
    * @return integer The width in pixels
    */
    function _sizeCol($col)
    {
        // Look up the cell value to see if it has been changed
        if (isset($this->col_sizes[$col])) {
            if ($this->col_sizes[$col] == 0) {
                return(0);
            } else {
                return(floor(7 * $this->col_sizes[$col] + 5));
            }
        } else {
            return(64);
        }
    }

    /**
    * Convert the height of a cell from user's units to pixels. By interpolation
    * the relationship is: y = 4/3x. If the height hasn't been set by the user we
    * use the default value. If the row is hidden we use a value of zero. (Not
    * possible to hide row yet).
    *
    * @access private
    * @param integer $row The row
    * @return integer The width in pixels
    */
    function _sizeRow($row)
    {
        // Look up the cell value to see if it has been changed
        if (isset($this->_row_sizes[$row])) {
            if ($this->_row_sizes[$row] == 0) {
                return(0);
            } else {
                return(floor(4/3 * $this->_row_sizes[$row]));
            }
        } else {
            return(17);
        }
    }

    /**
    * Store the OBJ record that precedes an IMDATA record. This could be generalise
    * to support other Excel objects.
    *
    * @access private
    * @param integer $colL Column containing upper left corner of object
    * @param integer $dxL  Distance from left side of cell
    * @param integer $rwT  Row containing top left corner of object
    * @param integer $dyT  Distance from top of cell
    * @param integer $colR Column containing lower right corner of object
    * @param integer $dxR  Distance from right of cell
    * @param integer $rwB  Row containing bottom right corner of object
    * @param integer $dyB  Distance from bottom of cell
    */
    function _storeObjPicture($colL,$dxL,$rwT,$dyT,$colR,$dxR,$rwB,$dyB)
    {
        $record      = 0x005d;   // Record identifier
        $length      = 0x003c;   // Bytes to follow

        $cObj        = 0x0001;   // Count of objects in file (set to 1)
        $OT          = 0x0008;   // Object type. 8 = Picture
        $id          = 0x0001;   // Object ID
        $grbit       = 0x0614;   // Option flags

        $cbMacro     = 0x0000;   // Length of FMLA structure
        $Reserved1   = 0x0000;   // Reserved
        $Reserved2   = 0x0000;   // Reserved

        $icvBack     = 0x09;     // Background colour
        $icvFore     = 0x09;     // Foreground colour
        $fls         = 0x00;     // Fill pattern
        $fAuto       = 0x00;     // Automatic fill
        $icv         = 0x08;     // Line colour
        $lns         = 0xff;     // Line style
        $lnw         = 0x01;     // Line weight
        $fAutoB      = 0x00;     // Automatic border
        $frs         = 0x0000;   // Frame style
        $cf          = 0x0009;   // Image format, 9 = bitmap
        $Reserved3   = 0x0000;   // Reserved
        $cbPictFmla  = 0x0000;   // Length of FMLA structure
        $Reserved4   = 0x0000;   // Reserved
        $grbit2      = 0x0001;   // Option flags
        $Reserved5   = 0x0000;   // Reserved


        $header      = pack("vv", $record, $length);
        $data        = pack("V", $cObj);
        $data       .= pack("v", $OT);
        $data       .= pack("v", $id);
        $data       .= pack("v", $grbit);
        $data       .= pack("v", $colL);
        $data       .= pack("v", $dxL);
        $data       .= pack("v", $rwT);
        $data       .= pack("v", $dyT);
        $data       .= pack("v", $colR);
        $data       .= pack("v", $dxR);
        $data       .= pack("v", $rwB);
        $data       .= pack("v", $dyB);
        $data       .= pack("v", $cbMacro);
        $data       .= pack("V", $Reserved1);
        $data       .= pack("v", $Reserved2);
        $data       .= pack("C", $icvBack);
        $data       .= pack("C", $icvFore);
        $data       .= pack("C", $fls);
        $data       .= pack("C", $fAuto);
        $data       .= pack("C", $icv);
        $data       .= pack("C", $lns);
        $data       .= pack("C", $lnw);
        $data       .= pack("C", $fAutoB);
        $data       .= pack("v", $frs);
        $data       .= pack("V", $cf);
        $data       .= pack("v", $Reserved3);
        $data       .= pack("v", $cbPictFmla);
        $data       .= pack("v", $Reserved4);
        $data       .= pack("v", $grbit2);
        $data       .= pack("V", $Reserved5);

        $this->_append($header . $data);
    }

    /**
    * Convert a 24 bit bitmap into the modified internal format used by Windows.
    * This is described in BITMAPCOREHEADER and BITMAPCOREINFO structures in the
    * MSDN library.
    *
    * @access private
    * @param string $bitmap The bitmap to process
    * @return array Array with data and properties of the bitmap
    */
    function _processBitmap($bitmap)
    {
        // Open file.
        $bmp_fd = @fopen($bitmap,"rb");
        if (!$bmp_fd) {
            die("Couldn't import $bitmap");
        }

        // Slurp the file into a string.
        $data = fread($bmp_fd, filesize($bitmap));

        // Check that the file is big enough to be a bitmap.
        if (strlen($data) <= 0x36) {
            die("$bitmap doesn't contain enough data.\n");
        }

        // The first 2 bytes are used to identify the bitmap.
        $identity = unpack("A2ident", $data);
        if ($identity['ident'] != "BM") {
            die("$bitmap doesn't appear to be a valid bitmap image.\n");
        }

        // Remove bitmap data: ID.
        $data = substr($data, 2);

        // Read and remove the bitmap size. This is more reliable than reading
        // the data size at offset 0x22.
        //
        $size_array   = unpack("Vsa", substr($data, 0, 4));
        $size   = $size_array['sa'];
        $data   = substr($data, 4);
        $size  -= 0x36; // Subtract size of bitmap header.
        $size  += 0x0C; // Add size of BIFF header.

        // Remove bitmap data: reserved, offset, header length.
        $data = substr($data, 12);

        // Read and remove the bitmap width and height. Verify the sizes.
        $width_and_height = unpack("V2", substr($data, 0, 8));
        $width  = $width_and_height[1];
        $height = $width_and_height[2];
        $data   = substr($data, 8);
        if ($width > 0xFFFF) {
            die("$bitmap: largest image width supported is 65k.\n");
        }
        if ($height > 0xFFFF) {
            die("$bitmap: largest image height supported is 65k.\n");
        }

        // Read and remove the bitmap planes and bpp data. Verify them.
        $planes_and_bitcount = unpack("v2", substr($data, 0, 4));
        $data = substr($data, 4);
        if ($planes_and_bitcount[2] != 24) { // Bitcount
            die("$bitmap isn't a 24bit true color bitmap.\n");
        }
        if ($planes_and_bitcount[1] != 1) {
            die("$bitmap: only 1 plane supported in bitmap image.\n");
        }

        // Read and remove the bitmap compression. Verify compression.
        $compression = unpack("Vcomp", substr($data, 0, 4));
        $data = substr($data, 4);

        //$compression = 0;
        if ($compression['comp'] != 0) {
            die("$bitmap: compression not supported in bitmap image.\n");
        }

        // Remove bitmap data: data size, hres, vres, colours, imp. colours.
        $data = substr($data, 20);

        // Add the BITMAPCOREHEADER data
        $header  = pack("Vvvvv", 0x000c, $width, $height, 0x01, 0x18);
        $data    = $header . $data;

        return (array($width, $height, $size, $data));
    }

    /**
    * Store the window zoom factor. This should be a reduced fraction but for
    * simplicity we will store all fractions with a numerator of 100.
    *
    * @access private
    */
    function _storeZoom()
    {
        // If scale is 100 we don't need to write a record
        if ($this->_zoom == 100) {
            return;
        }

        $record      = 0x00A0;               // Record identifier
        $length      = 0x0004;               // Bytes to follow

        $header      = pack("vv", $record, $length);
        $data        = pack("vv", $this->_zoom, 100);
        $this->_append($header . $data);
    }

    /**
    * FIXME: add comments
    */
    function setValidation($row1, $col1, $row2, $col2, &$validator)
    {
        $this->_dv[] = $validator->_getData() .
                       pack("vvvvv", 1, $row1, $row2, $col1, $col2);
    }

    /**
    * Store the DVAL and DV records.
    *
    * @access private
    */
    function _storeDataValidity()
    {
        $record      = 0x01b2;      // Record identifier
        $length      = 0x0012;      // Bytes to follow

        $grbit       = 0x0002;      // Prompt box at cell, no cached validity data at DV records
        $horPos      = 0x00000000;  // Horizontal position of prompt box, if fixed position
        $verPos      = 0x00000000;  // Vertical position of prompt box, if fixed position
        $objId       = 0xffffffff;  // Object identifier of drop down arrow object, or -1 if not visible

        $header      = pack('vv', $record, $length);
        $data        = pack('vVVVV', $grbit, $horPos, $verPos, $objId,
                                     count($this->_dv));
        $this->_append($header.$data);

        $record = 0x01be;              // Record identifier
        foreach ($this->_dv as $dv) {
            $length = strlen($dv);      // Bytes to follow
            $header = pack("vv", $record, $length);
            $this->_append($header . $dv);
        }
    }
}

/**
* Class for generating Excel Spreadsheets
*
* @author   Xavier Noguer <xnoguer@rezebra.com>
* @category FileFormats
* @package  Spreadsheet_Excel_Writer
*/

class Spreadsheet_Excel_Writer_Workbook extends Spreadsheet_Excel_Writer_BIFFwriter
{
    /**
    * Filename for the Workbook
    * @var string
    */
    var $_filename;

    /**
    * Formula parser
    * @var object Parser
    */
    var $_parser;

    /**
    * Flag for 1904 date system (0 => base date is 1900, 1 => base date is 1904)
    * @var integer
    */
    var $_1904;

    /**
    * The active worksheet of the workbook (0 indexed)
    * @var integer
    */
    var $_activesheet;

    /**
    * 1st displayed worksheet in the workbook (0 indexed)
    * @var integer
    */
    var $_firstsheet;

    /**
    * Number of workbook tabs selected
    * @var integer
    */
    var $_selected;

    /**
    * Index for creating adding new formats to the workbook
    * @var integer
    */
    var $_xf_index;

    /**
    * Flag for preventing close from being called twice.
    * @var integer
    * @see close()
    */
    var $_fileclosed;

    /**
    * The BIFF file size for the workbook.
    * @var integer
    * @see _calcSheetOffsets()
    */
    var $_biffsize;

    /**
    * The default sheetname for all sheets created.
    * @var string
    */
    var $_sheetname;

    /**
    * The default XF format.
    * @var object Format
    */
    var $_tmp_format;

    /**
    * Array containing references to all of this workbook's worksheets
    * @var array
    */
    var $_worksheets;

    /**
    * Array of sheetnames for creating the EXTERNSHEET records
    * @var array
    */
    var $_sheetnames;

    /**
    * Array containing references to all of this workbook's formats
    * @var array
    */
    var $_formats;

    /**
    * Array containing the colour palette
    * @var array
    */
    var $_palette;

    /**
    * The default format for URLs.
    * @var object Format
    */
    var $_url_format;

    /**
    * The codepage indicates the text encoding used for strings
    * @var integer
    */
    var $_codepage;

    /**
    * The country code used for localization
    * @var integer
    */
    var $_country_code;

    /**
    * The temporary dir for storing the OLE file
    * @var string
    */
    var $_tmp_dir;

    /**
    * number of bytes for sizeinfo of strings
    * @var integer
    */
    var $_string_sizeinfo_size;

    /**
    * Class constructor
    *
    * @param string filename for storing the workbook. "-" for writing to stdout.
    * @access public
    */
    function __construct($filename)
    {
        // It needs to call its parent's constructor explicitly
        parent::__construct();

        $this->_filename         = $filename;
        $this->_parser           = new Spreadsheet_Excel_Writer_Parser($this->_byte_order, $this->_BIFF_version);
        $this->_1904             = 0;
        $this->_activesheet      = 0;
        $this->_firstsheet       = 0;
        $this->_selected         = 0;
        $this->_xf_index         = 16; // 15 style XF's and 1 cell XF.
        $this->_fileclosed       = 0;
        $this->_biffsize         = 0;
        $this->_sheetname        = 'Sheet';
        $this->_tmp_format       = new Spreadsheet_Excel_Writer_Format($this->_BIFF_version);
        $this->_worksheets       = array();
        $this->_sheetnames       = array();
        $this->_formats          = array();
        $this->_palette          = array();
        $this->_codepage         = 0x04E4; // FIXME: should change for BIFF8
        $this->_country_code     = -1;
        $this->_string_sizeinfo  = 3;

        // Add the default format for hyperlinks
        $this->_url_format =& $this->addFormat(array('color' => 'blue', 'underline' => 1));
        $this->_str_total       = 0;
        $this->_str_unique      = 0;
        $this->_str_table       = array();
        $this->_setPaletteXl97();
        $this->_tmp_dir         = '';
    }

    /**
    * Calls finalization methods.
    * This method should always be the last one to be called on every workbook
    *
    * @access public
    * @return mixed true on success. PEAR_Error on failure
    */
    function close()
    {
        if ($this->_fileclosed) { // Prevent close() from being called twice.
            return true;
        }
        $this->_storeWorkbook();
        $this->_fileclosed = 1;
        return true;
    }

    /**
    * An accessor for the _worksheets[] array
    * Returns an array of the worksheet objects in a workbook
    * It actually calls to worksheets()
    *
    * @access public
    * @see worksheets()
    * @return array
    */
    function sheets()
    {
        return $this->worksheets();
    }

    /**
    * An accessor for the _worksheets[] array.
    * Returns an array of the worksheet objects in a workbook
    *
    * @access public
    * @return array
    */
    function worksheets()
    {
        return $this->_worksheets;
    }

    /**
    * Sets the BIFF version.
    * This method exists just to access experimental functionality
    * from BIFF8. It will be deprecated !
    * Only possible value is 8 (Excel 97/2000).
    * For any other value it fails silently.
    *
    * @access public
    * @param integer $version The BIFF version
    */
    function setVersion($version)
    {
        if ($version == 8) { // only accept version 8
            $version = 0x0600;
            $this->_BIFF_version = $version;
            // change BIFFwriter limit for CONTINUE records
            $this->_limit = 8228;
            $this->_tmp_format->_BIFF_version = $version;
            $this->_url_format->_BIFF_version = $version;
            $this->_parser->_BIFF_version = $version;

            $total_worksheets = count($this->_worksheets);
            // change version for all worksheets too
            for ($i = 0; $i < $total_worksheets; $i++) {
                $this->_worksheets[$i]->_BIFF_version = $version;
            }

            $total_formats = count($this->_formats);
            // change version for all formats too
            for ($i = 0; $i < $total_formats; $i++) {
                $this->_formats[$i]->_BIFF_version = $version;
            }
        }
    }

    /**
    * Set the country identifier for the workbook
    *
    * @access public
    * @param integer $code Is the international calling country code for the
    *                      chosen country.
    */
    function setCountry($code)
    {
        $this->_country_code = $code;
    }

    /**
    * Add a new worksheet to the Excel workbook.
    * If no name is given the name of the worksheet will be Sheeti$i, with
    * $i in [1..].
    *
    * @access public
    * @param string $name the optional name of the worksheet
    * @return mixed reference to a worksheet object on success, PEAR_Error
    *               on failure
    */
    function &addWorksheet($name = '')
    {
        $index     = count($this->_worksheets);
        $sheetname = $this->_sheetname;

        if ($name == '') {
            $name = $sheetname.($index+1);
        }

        // Check that sheetname is <= 31 chars (Excel limit before BIFF8).
        if ($this->_BIFF_version != 0x0600)
        {
            if (strlen($name) > 31) {
                die("Sheetname $name must be <= 31 chars");
            }
        }

        // Check that the worksheet name doesn't already exist: a fatal Excel error.
        $total_worksheets = count($this->_worksheets);
        for ($i = 0; $i < $total_worksheets; $i++) {
            if ($this->_worksheets[$i]->getName() == $name) {
                die("Worksheet '$name' already exists");
            }
        }

        $worksheet = new Spreadsheet_Excel_Writer_Worksheet($this->_BIFF_version,
                                   $name, $index,
                                   $this->_activesheet, $this->_firstsheet,
                                   $this->_str_total, $this->_str_unique,
                                   $this->_str_table, $this->_url_format,
                                   $this->_parser);

        $this->_worksheets[$index] = &$worksheet;    // Store ref for iterator
        $this->_sheetnames[$index] = $name;          // Store EXTERNSHEET names
        $this->_parser->setExtSheet($name, $index);  // Register worksheet name with parser
        return $worksheet;
    }

    /**
    * Add a new format to the Excel workbook.
    * Also, pass any properties to the Format constructor.
    *
    * @access public
    * @param array $properties array with properties for initializing the format.
    * @return &Spreadsheet_Excel_Writer_Format reference to an Excel Format
    */
    function &addFormat($properties = array())
    {
        $format = new Spreadsheet_Excel_Writer_Format($this->_BIFF_version, $this->_xf_index, $properties);
        $this->_xf_index += 1;
        $this->_formats[] = &$format;
        return $format;
    }

    /**
     * Create new validator.
     *
     * @access public
     * @return &Spreadsheet_Excel_Writer_Validator reference to a Validator
     */
    function &addValidator()
    {
        include_once 'Spreadsheet/Excel/Writer/Validator.php';
        /* FIXME: check for successful inclusion*/
        $valid = new Spreadsheet_Excel_Writer_Validator($this->_parser);
        return $valid;
    }

    /**
    * Change the RGB components of the elements in the colour palette.
    *
    * @access public
    * @param integer $index colour index
    * @param integer $red   red RGB value [0-255]
    * @param integer $green green RGB value [0-255]
    * @param integer $blue  blue RGB value [0-255]
    * @return integer The palette index for the custom color
    */
    function setCustomColor($index, $red, $green, $blue)
    {
        // Match a HTML #xxyyzz style parameter
        /*if (defined $_[1] and $_[1] =~ /^#(\w\w)(\w\w)(\w\w)/ ) {
            @_ = ($_[0], hex $1, hex $2, hex $3);
        }*/

        // Check that the colour index is the right range
        if ($index < 8 or $index > 64) {
            // TODO: assign real error codes
            die("Color index $index outside range: 8 <= index <= 64");
        }

        // Check that the colour components are in the right range
        if (($red   < 0 or $red   > 255) ||
            ($green < 0 or $green > 255) ||
            ($blue  < 0 or $blue  > 255))
        {
            die("Color component outside range: 0 <= color <= 255");
        }

        $index -= 8; // Adjust colour index (wingless dragonfly)

        // Set the RGB value
        $this->_palette[$index] = array($red, $green, $blue, 0);
        return($index + 8);
    }

    /**
    * Sets the colour palette to the Excel 97+ default.
    *
    * @access private
    */
    function _setPaletteXl97()
    {
        $this->_palette = array(
                           array(0x00, 0x00, 0x00, 0x00),   // 8
                           array(0xff, 0xff, 0xff, 0x00),   // 9
                           array(0xff, 0x00, 0x00, 0x00),   // 10
                           array(0x00, 0xff, 0x00, 0x00),   // 11
                           array(0x00, 0x00, 0xff, 0x00),   // 12
                           array(0xff, 0xff, 0x00, 0x00),   // 13
                           array(0xff, 0x00, 0xff, 0x00),   // 14
                           array(0x00, 0xff, 0xff, 0x00),   // 15
                           array(0x80, 0x00, 0x00, 0x00),   // 16
                           array(0x00, 0x80, 0x00, 0x00),   // 17
                           array(0x00, 0x00, 0x80, 0x00),   // 18
                           array(0x80, 0x80, 0x00, 0x00),   // 19
                           array(0x80, 0x00, 0x80, 0x00),   // 20
                           array(0x00, 0x80, 0x80, 0x00),   // 21
                           array(0xc0, 0xc0, 0xc0, 0x00),   // 22
                           array(0x80, 0x80, 0x80, 0x00),   // 23
                           array(0x99, 0x99, 0xff, 0x00),   // 24
                           array(0x99, 0x33, 0x66, 0x00),   // 25
                           array(0xff, 0xff, 0xcc, 0x00),   // 26
                           array(0xcc, 0xff, 0xff, 0x00),   // 27
                           array(0x66, 0x00, 0x66, 0x00),   // 28
                           array(0xff, 0x80, 0x80, 0x00),   // 29
                           array(0x00, 0x66, 0xcc, 0x00),   // 30
                           array(0xcc, 0xcc, 0xff, 0x00),   // 31
                           array(0x00, 0x00, 0x80, 0x00),   // 32
                           array(0xff, 0x00, 0xff, 0x00),   // 33
                           array(0xff, 0xff, 0x00, 0x00),   // 34
                           array(0x00, 0xff, 0xff, 0x00),   // 35
                           array(0x80, 0x00, 0x80, 0x00),   // 36
                           array(0x80, 0x00, 0x00, 0x00),   // 37
                           array(0x00, 0x80, 0x80, 0x00),   // 38
                           array(0x00, 0x00, 0xff, 0x00),   // 39
                           array(0x00, 0xcc, 0xff, 0x00),   // 40
                           array(0xcc, 0xff, 0xff, 0x00),   // 41
                           array(0xcc, 0xff, 0xcc, 0x00),   // 42
                           array(0xff, 0xff, 0x99, 0x00),   // 43
                           array(0x99, 0xcc, 0xff, 0x00),   // 44
                           array(0xff, 0x99, 0xcc, 0x00),   // 45
                           array(0xcc, 0x99, 0xff, 0x00),   // 46
                           array(0xff, 0xcc, 0x99, 0x00),   // 47
                           array(0x33, 0x66, 0xff, 0x00),   // 48
                           array(0x33, 0xcc, 0xcc, 0x00),   // 49
                           array(0x99, 0xcc, 0x00, 0x00),   // 50
                           array(0xff, 0xcc, 0x00, 0x00),   // 51
                           array(0xff, 0x99, 0x00, 0x00),   // 52
                           array(0xff, 0x66, 0x00, 0x00),   // 53
                           array(0x66, 0x66, 0x99, 0x00),   // 54
                           array(0x96, 0x96, 0x96, 0x00),   // 55
                           array(0x00, 0x33, 0x66, 0x00),   // 56
                           array(0x33, 0x99, 0x66, 0x00),   // 57
                           array(0x00, 0x33, 0x00, 0x00),   // 58
                           array(0x33, 0x33, 0x00, 0x00),   // 59
                           array(0x99, 0x33, 0x00, 0x00),   // 60
                           array(0x99, 0x33, 0x66, 0x00),   // 61
                           array(0x33, 0x33, 0x99, 0x00),   // 62
                           array(0x33, 0x33, 0x33, 0x00),   // 63
                         );
    }

    /**
    * Assemble worksheets into a workbook and send the BIFF data to an OLE
    * storage.
    *
    * @access private
    * @return mixed true on success. PEAR_Error on failure
    */
    function _storeWorkbook()
    {
        // Ensure that at least one worksheet has been selected.
        if ($this->_activesheet == 0) {
            $this->_worksheets[0]->selected = 1;
        }

        // Calculate the number of selected worksheet tabs and call the finalization
        // methods for each worksheet
        $total_worksheets = count($this->_worksheets);
        for ($i = 0; $i < $total_worksheets; $i++) {
            if ($this->_worksheets[$i]->selected) {
                $this->_selected++;
            }
            $this->_worksheets[$i]->close($this->_sheetnames);
        }

        // Add Workbook globals
        $this->_storeBof(0x0005);
        $this->_storeCodepage();
        if ($this->_BIFF_version == 0x0600) {
            $this->_storeWindow1();
        }
        if ($this->_BIFF_version == 0x0500) {
            $this->_storeExterns();    // For print area and repeat rows
        }
        $this->_storeNames();      // For print area and repeat rows
        if ($this->_BIFF_version == 0x0500) {
            $this->_storeWindow1();
        }
        $this->_storeDatemode();
        $this->_storeAllFonts();
        $this->_storeAllNumFormats();
        $this->_storeAllXfs();
        $this->_storeAllStyles();
        $this->_storePalette();
        $this->_calcSheetOffsets();

        // Add BOUNDSHEET records
        for ($i = 0; $i < $total_worksheets; $i++) {
            $this->_storeBoundsheet($this->_worksheets[$i]->name,$this->_worksheets[$i]->offset);
        }

        if ($this->_country_code != -1) {
            $this->_storeCountry();
        }

        if ($this->_BIFF_version == 0x0600) {
            //$this->_storeSupbookInternal();
            /* TODO: store external SUPBOOK records and XCT and CRN records
            in case of external references for BIFF8 */
            //$this->_storeExternsheetBiff8();
            $this->_storeSharedStringsTable();
        }

        // End Workbook globals
        $this->_storeEof();

        // Store the workbook in an OLE container
        $res = $this->_storeOLEFile();
        return true;
    }

    /**
    * Sets the temp dir used for storing the OLE file
    *
    * @access public
    * @param string $dir The dir to be used as temp dir
    * @return true if given dir is valid, false otherwise
    */
    function setTempDir($dir)
    {
        if (is_dir($dir)) {
            $this->_tmp_dir = $dir;
            return true;
        }
        return false;
    }

    /**
    * Store the workbook in an OLE container
    *
    * @access private
    * @return mixed true on success. PEAR_Error on failure
    */
    function _storeOLEFile()
    {
        if($this->_BIFF_version == 0x0600) {
            $OLE = new ole_pps_file(Asc2Ucs('Workbook'));
        } else {
            $OLE = new ole_pps_file(Asc2Ucs('Book'));
        }

        $OLE->append($this->_data);

        $total_worksheets = count($this->_worksheets);
        for ($i = 0; $i < $total_worksheets; $i++) {
            while ($tmp = $this->_worksheets[$i]->getData()) {
                $OLE->append($tmp);
            }
        }

        $root = new ole_pps_root(false, false, array($OLE));

        $root->save($this->_filename);
    }

    /**
    * Calculate offsets for Worksheet BOF records.
    *
    * @access private
    */
    function _calcSheetOffsets()
    {
        if ($this->_BIFF_version == 0x0600) {
            $boundsheet_length = 12;  // fixed length for a BOUNDSHEET record
        } else {
            $boundsheet_length = 11;
        }
        $EOF               = 4;
        $offset            = $this->_datasize;

        if ($this->_BIFF_version == 0x0600) {
            // add the length of the SST
            /* TODO: check this works for a lot of strings (> 8224 bytes) */
            $offset += $this->_calculateSharedStringsSizes();
            if ($this->_country_code != -1) {
                $offset += 8; // adding COUNTRY record
            }
            // add the lenght of SUPBOOK, EXTERNSHEET and NAME records
            //$offset += 8; // FIXME: calculate real value when storing the records
        }
        $total_worksheets = count($this->_worksheets);
        // add the length of the BOUNDSHEET records
        for ($i = 0; $i < $total_worksheets; $i++) {
            $offset += $boundsheet_length + strlen($this->_worksheets[$i]->name);
        }
        $offset += $EOF;

        for ($i = 0; $i < $total_worksheets; $i++) {
            $this->_worksheets[$i]->offset = $offset;
            $offset += $this->_worksheets[$i]->_datasize;
        }
        $this->_biffsize = $offset;
    }

    /**
    * Store the Excel FONT records.
    *
    * @access private
    */
    function _storeAllFonts()
    {
        // tmp_format is added by the constructor. We use this to write the default XF's
        $format = $this->_tmp_format;
        $font   = $format->getFont();

        // Note: Fonts are 0-indexed. According to the SDK there is no index 4,
        // so the following fonts are 0, 1, 2, 3, 5
        //
        for ($i = 1; $i <= 5; $i++){
            $this->_append($font);
        }

        // Iterate through the XF objects and write a FONT record if it isn't the
        // same as the default FONT and if it hasn't already been used.
        //
        $fonts = array();
        $index = 6;                  // The first user defined FONT

        $key = $format->getFontKey(); // The default font from _tmp_format
        $fonts[$key] = 0;             // Index of the default font

        $total_formats = count($this->_formats);
        for ($i = 0; $i < $total_formats; $i++) {
            $key = $this->_formats[$i]->getFontKey();
            if (isset($fonts[$key])) {
                // FONT has already been used
                $this->_formats[$i]->font_index = $fonts[$key];
            } else {
                // Add a new FONT record
                $fonts[$key]        = $index;
                $this->_formats[$i]->font_index = $index;
                $index++;
                $font = $this->_formats[$i]->getFont();
                $this->_append($font);
            }
        }
    }

    /**
    * Store user defined numerical formats i.e. FORMAT records
    *
    * @access private
    */
    function _storeAllNumFormats()
    {
        // Leaning num_format syndrome
        $hash_num_formats = array();
        $num_formats      = array();
        $index = 164;

        // Iterate through the XF objects and write a FORMAT record if it isn't a
        // built-in format type and if the FORMAT string hasn't already been used.
        $total_formats = count($this->_formats);
        for ($i = 0; $i < $total_formats; $i++) {
            $num_format = $this->_formats[$i]->_num_format;

            // Check if $num_format is an index to a built-in format.
            // Also check for a string of zeros, which is a valid format string
            // but would evaluate to zero.
            //
            if (!preg_match("/^0+\d/", $num_format)) {
                if (preg_match("/^\d+$/", $num_format)) { // built-in format
                    continue;
                }
            }

            if (isset($hash_num_formats[$num_format])) {
                // FORMAT has already been used
                $this->_formats[$i]->_num_format = $hash_num_formats[$num_format];
            } else{
                // Add a new FORMAT
                $hash_num_formats[$num_format]  = $index;
                $this->_formats[$i]->_num_format = $index;
                array_push($num_formats,$num_format);
                $index++;
            }
        }

        // Write the new FORMAT records starting from 0xA4
        $index = 164;
        foreach ($num_formats as $num_format) {
            $this->_storeNumFormat($num_format,$index);
            $index++;
        }
    }

    /**
    * Write all XF records.
    *
    * @access private
    */
    function _storeAllXfs()
    {
        // _tmp_format is added by the constructor. We use this to write the default XF's
        // The default font index is 0
        //
        $format = $this->_tmp_format;
        for ($i = 0; $i <= 14; $i++) {
            $xf = $format->getXf('style'); // Style XF
            $this->_append($xf);
        }

        $xf = $format->getXf('cell');      // Cell XF
        $this->_append($xf);

        // User defined XFs
        $total_formats = count($this->_formats);
        for ($i = 0; $i < $total_formats; $i++) {
            $xf = $this->_formats[$i]->getXf('cell');
            $this->_append($xf);
        }
    }

    /**
    * Write all STYLE records.
    *
    * @access private
    */
    function _storeAllStyles()
    {
        $this->_storeStyle();
    }

    /**
    * Write the EXTERNCOUNT and EXTERNSHEET records. These are used as indexes for
    * the NAME records.
    *
    * @access private
    */
    function _storeExterns()
    {
        // Create EXTERNCOUNT with number of worksheets
        $this->_storeExterncount(count($this->_worksheets));

        // Create EXTERNSHEET for each worksheet
        foreach ($this->_sheetnames as $sheetname) {
            $this->_storeExternsheet($sheetname);
        }
    }

    /**
    * Write the NAME record to define the print area and the repeat rows and cols.
    *
    * @access private
    */
    function _storeNames()
    {
        // Create the print area NAME records
        $total_worksheets = count($this->_worksheets);
        for ($i = 0; $i < $total_worksheets; $i++) {
            // Write a Name record if the print area has been defined
            if (isset($this->_worksheets[$i]->print_rowmin)) {
                $this->_storeNameShort(
                    $this->_worksheets[$i]->index,
                    0x06, // NAME type
                    $this->_worksheets[$i]->print_rowmin,
                    $this->_worksheets[$i]->print_rowmax,
                    $this->_worksheets[$i]->print_colmin,
                    $this->_worksheets[$i]->print_colmax
                    );
            }
        }

        // Create the print title NAME records
        $total_worksheets = count($this->_worksheets);
        for ($i = 0; $i < $total_worksheets; $i++) {
            $rowmin = $this->_worksheets[$i]->title_rowmin;
            $rowmax = $this->_worksheets[$i]->title_rowmax;
            $colmin = $this->_worksheets[$i]->title_colmin;
            $colmax = $this->_worksheets[$i]->title_colmax;

            // Determine if row + col, row, col or nothing has been defined
            // and write the appropriate record
            //
            if (isset($rowmin) && isset($colmin)) {
                // Row and column titles have been defined.
                // Row title has been defined.
                $this->_storeNameLong(
                    $this->_worksheets[$i]->index,
                    0x07, // NAME type
                    $rowmin,
                    $rowmax,
                    $colmin,
                    $colmax
                    );
            } elseif (isset($rowmin)) {
                // Row title has been defined.
                $this->_storeNameShort(
                    $this->_worksheets[$i]->index,
                    0x07, // NAME type
                    $rowmin,
                    $rowmax,
                    0x00,
                    0xff
                    );
            } elseif (isset($colmin)) {
                // Column title has been defined.
                $this->_storeNameShort(
                    $this->_worksheets[$i]->index,
                    0x07, // NAME type
                    0x0000,
                    0x3fff,
                    $colmin,
                    $colmax
                    );
            } else {
                // Print title hasn't been defined.
            }
        }
    }




    /******************************************************************************
    *
    * BIFF RECORDS
    *
    */

    /**
    * Stores the CODEPAGE biff record.
    *
    * @access private
    */
    function _storeCodepage()
    {
        $record          = 0x0042;             // Record identifier
        $length          = 0x0002;             // Number of bytes to follow
        $cv              = $this->_codepage;   // The code page

        $header          = pack('vv', $record, $length);
        $data            = pack('v',  $cv);

        $this->_append($header . $data);
    }

    /**
    * Write Excel BIFF WINDOW1 record.
    *
    * @access private
    */
    function _storeWindow1()
    {
        $record    = 0x003D;                 // Record identifier
        $length    = 0x0012;                 // Number of bytes to follow

        $xWn       = 0x0000;                 // Horizontal position of window
        $yWn       = 0x0000;                 // Vertical position of window
        $dxWn      = 0x25BC;                 // Width of window
        $dyWn      = 0x1572;                 // Height of window

        $grbit     = 0x0038;                 // Option flags
        $ctabsel   = $this->_selected;       // Number of workbook tabs selected
        $wTabRatio = 0x0258;                 // Tab to scrollbar ratio

        $itabFirst = $this->_firstsheet;     // 1st displayed worksheet
        $itabCur   = $this->_activesheet;    // Active worksheet

        $header    = pack("vv",        $record, $length);
        $data      = pack("vvvvvvvvv", $xWn, $yWn, $dxWn, $dyWn,
                                       $grbit,
                                       $itabCur, $itabFirst,
                                       $ctabsel, $wTabRatio);
        $this->_append($header . $data);
    }

    /**
    * Writes Excel BIFF BOUNDSHEET record.
    * FIXME: inconsistent with BIFF documentation
    *
    * @param string  $sheetname Worksheet name
    * @param integer $offset    Location of worksheet BOF
    * @access private
    */
    function _storeBoundsheet($sheetname,$offset)
    {
        $record    = 0x0085;                    // Record identifier
/*        
        if ($this->_BIFF_version == 0x0600) 	// Tried to fix the correct handling here, with the
        {										// corrected specification from M$ - Joe Hunt 2009-03-08
        	$encoding_string = $this->_input_encoding;
        	if ($encoding_string == 'UTF-16LE')
        	{
        	    $strlen = function_exists('mb_strlen') ? mb_strlen($sheetname, 'UTF-16LE') : (strlen($sheetname) / 2);
        	    $encoding  = 0x1;
        	}
        	else if ($encoding_string != '')
        	{
        	    $sheetname = iconv($encoding_string, 'UTF-16LE', $sheetname);
        	    $strlen = function_exists('mb_strlen') ? mb_strlen($sheetname, 'UTF-16LE') : (strlen($sheetname) / 2);
        	    $encoding  = 0x1;
        	}
        	if ($strlen % 2 != 0)
        		$strlen++;
        	$encoding  = 0x1;
        	
            //$strlen = strlen($sheetname);
            $length    = 0x08 + $strlen; 		// Number of bytes to follow
        } else {
        	$strlen = strlen($sheetname);
            $length = 0x07 + $strlen; 			// Number of bytes to follow
        }

        $grbit     = 0x0000;                    // Visibility and sheet type
        $cch       = $strlen;        			// Length of sheet name

        $header    = pack("vv",  $record, $length);
        if ($this->_BIFF_version == 0x0600) {
            $data      = pack("VvCC", $offset, $grbit, $cch, $encoding);
        } else {
            $data      = pack("VvC", $offset, $grbit, $cch);
        }
*/        
        if ($this->_BIFF_version == 0x0600) 
        {
            $strlen = strlen($sheetname);
            $length    = 0x08 + $strlen; 		// Number of bytes to follow
        } else {
        	$strlen = strlen($sheetname);
            $length = 0x07 + $strlen; 			// Number of bytes to follow
        }

        $grbit     = 0x0000;                    // Visibility and sheet type
        $cch       = $strlen;        			// Length of sheet name

        $header    = pack("vv",  $record, $length);
        if ($this->_BIFF_version == 0x0600) {
            $data      = pack("Vvv", $offset, $grbit, $cch);
        } else {
            $data      = pack("VvC", $offset, $grbit, $cch);
        }
        $this->_append($header.$data.$sheetname);
    }

    /**
    * Write Internal SUPBOOK record
    *
    * @access private
    */
    function _storeSupbookInternal()
    {
        $record    = 0x01AE;   // Record identifier
        $length    = 0x0004;   // Bytes to follow

        $header    = pack("vv", $record, $length);
        $data      = pack("vv", count($this->_worksheets), 0x0104);
        $this->_append($header . $data);
    }

    /**
    * Writes the Excel BIFF EXTERNSHEET record. These references are used by
    * formulas.
    *
    * @param string $sheetname Worksheet name
    * @access private
    */
    function _storeExternsheetBiff8()
    {
        $total_references = count($this->_parser->_references);
        $record   = 0x0017;                     // Record identifier
        $length   = 2 + 6 * $total_references;  // Number of bytes to follow

        $supbook_index = 0;           // FIXME: only using internal SUPBOOK record
        $header           = pack("vv",  $record, $length);
        $data             = pack('v', $total_references);
        for ($i = 0; $i < $total_references; $i++) {
            $data .= $this->_parser->_references[$i];
        }
        $this->_append($header . $data);
    }

    /**
    * Write Excel BIFF STYLE records.
    *
    * @access private
    */
    function _storeStyle()
    {
        $record    = 0x0293;   // Record identifier
        $length    = 0x0004;   // Bytes to follow

        $ixfe      = 0x8000;   // Index to style XF
        $BuiltIn   = 0x00;     // Built-in style
        $iLevel    = 0xff;     // Outline style level

        $header    = pack("vv",  $record, $length);
        $data      = pack("vCC", $ixfe, $BuiltIn, $iLevel);
        $this->_append($header . $data);
    }


    /**
    * Writes Excel FORMAT record for non "built-in" numerical formats.
    *
    * @param string  $format Custom format string
    * @param integer $ifmt   Format index code
    * @access private
    */
    function _storeNumFormat($format, $ifmt)
    {
        $record    = 0x041E;                      // Record identifier

        if ($this->_BIFF_version == 0x0600) {
            $length    = 5 + strlen($format);      // Number of bytes to follow
            $encoding = 0x0;
        } elseif ($this->_BIFF_version == 0x0500) {
            $length    = 3 + strlen($format);      // Number of bytes to follow
        }

        $cch       = strlen($format);             // Length of format string

        $header    = pack("vv", $record, $length);
        if ($this->_BIFF_version == 0x0600) {
            $data      = pack("vvC", $ifmt, $cch, $encoding);
        } elseif ($this->_BIFF_version == 0x0500) {
            $data      = pack("vC", $ifmt, $cch);
        }
        $this->_append($header . $data . $format);
    }

    /**
    * Write DATEMODE record to indicate the date system in use (1904 or 1900).
    *
    * @access private
    */
    function _storeDatemode()
    {
        $record    = 0x0022;         // Record identifier
        $length    = 0x0002;         // Bytes to follow

        $f1904     = $this->_1904;   // Flag for 1904 date system

        $header    = pack("vv", $record, $length);
        $data      = pack("v", $f1904);
        $this->_append($header . $data);
    }


    /**
    * Write BIFF record EXTERNCOUNT to indicate the number of external sheet
    * references in the workbook.
    *
    * Excel only stores references to external sheets that are used in NAME.
    * The workbook NAME record is required to define the print area and the repeat
    * rows and columns.
    *
    * A similar method is used in Worksheet.php for a slightly different purpose.
    *
    * @param integer $cxals Number of external references
    * @access private
    */
    function _storeExterncount($cxals)
    {
        $record   = 0x0016;          // Record identifier
        $length   = 0x0002;          // Number of bytes to follow

        $header   = pack("vv", $record, $length);
        $data     = pack("v",  $cxals);
        $this->_append($header . $data);
    }


    /**
    * Writes the Excel BIFF EXTERNSHEET record. These references are used by
    * formulas. NAME record is required to define the print area and the repeat
    * rows and columns.
    *
    * A similar method is used in Worksheet.php for a slightly different purpose.
    *
    * @param string $sheetname Worksheet name
    * @access private
    */
    function _storeExternsheet($sheetname)
    {
        $record      = 0x0017;                     // Record identifier
        $length      = 0x02 + strlen($sheetname);  // Number of bytes to follow

        $cch         = strlen($sheetname);         // Length of sheet name
        $rgch        = 0x03;                       // Filename encoding

        $header      = pack("vv",  $record, $length);
        $data        = pack("CC", $cch, $rgch);
        $this->_append($header . $data . $sheetname);
    }


    /**
    * Store the NAME record in the short format that is used for storing the print
    * area, repeat rows only and repeat columns only.
    *
    * @param integer $index  Sheet index
    * @param integer $type   Built-in name type
    * @param integer $rowmin Start row
    * @param integer $rowmax End row
    * @param integer $colmin Start colum
    * @param integer $colmax End column
    * @access private
    */
    function _storeNameShort($index, $type, $rowmin, $rowmax, $colmin, $colmax)
    {
        $record          = 0x0018;       // Record identifier
        $length          = 0x0024;       // Number of bytes to follow

        $grbit           = 0x0020;       // Option flags
        $chKey           = 0x00;         // Keyboard shortcut
        $cch             = 0x01;         // Length of text name
        $cce             = 0x0015;       // Length of text definition
        $ixals           = $index + 1;   // Sheet index
        $itab            = $ixals;       // Equal to ixals
        $cchCustMenu     = 0x00;         // Length of cust menu text
        $cchDescription  = 0x00;         // Length of description text
        $cchHelptopic    = 0x00;         // Length of help topic text
        $cchStatustext   = 0x00;         // Length of status bar text
        $rgch            = $type;        // Built-in name type

        $unknown03       = 0x3b;
        $unknown04       = 0xffff-$index;
        $unknown05       = 0x0000;
        $unknown06       = 0x0000;
        $unknown07       = 0x1087;
        $unknown08       = 0x8005;

        $header             = pack("vv", $record, $length);
        $data               = pack("v", $grbit);
        $data              .= pack("C", $chKey);
        $data              .= pack("C", $cch);
        $data              .= pack("v", $cce);
        $data              .= pack("v", $ixals);
        $data              .= pack("v", $itab);
        $data              .= pack("C", $cchCustMenu);
        $data              .= pack("C", $cchDescription);
        $data              .= pack("C", $cchHelptopic);
        $data              .= pack("C", $cchStatustext);
        $data              .= pack("C", $rgch);
        $data              .= pack("C", $unknown03);
        $data              .= pack("v", $unknown04);
        $data              .= pack("v", $unknown05);
        $data              .= pack("v", $unknown06);
        $data              .= pack("v", $unknown07);
        $data              .= pack("v", $unknown08);
        $data              .= pack("v", $index);
        $data              .= pack("v", $index);
        $data              .= pack("v", $rowmin);
        $data              .= pack("v", $rowmax);
        $data              .= pack("C", $colmin);
        $data              .= pack("C", $colmax);
        $this->_append($header . $data);
    }


    /**
    * Store the NAME record in the long format that is used for storing the repeat
    * rows and columns when both are specified. This shares a lot of code with
    * _storeNameShort() but we use a separate method to keep the code clean.
    * Code abstraction for reuse can be carried too far, and I should know. ;-)
    *
    * @param integer $index Sheet index
    * @param integer $type  Built-in name type
    * @param integer $rowmin Start row
    * @param integer $rowmax End row
    * @param integer $colmin Start colum
    * @param integer $colmax End column
    * @access private
    */
    function _storeNameLong($index, $type, $rowmin, $rowmax, $colmin, $colmax)
    {
        $record          = 0x0018;       // Record identifier
        $length          = 0x003d;       // Number of bytes to follow
        $grbit           = 0x0020;       // Option flags
        $chKey           = 0x00;         // Keyboard shortcut
        $cch             = 0x01;         // Length of text name
        $cce             = 0x002e;       // Length of text definition
        $ixals           = $index + 1;   // Sheet index
        $itab            = $ixals;       // Equal to ixals
        $cchCustMenu     = 0x00;         // Length of cust menu text
        $cchDescription  = 0x00;         // Length of description text
        $cchHelptopic    = 0x00;         // Length of help topic text
        $cchStatustext   = 0x00;         // Length of status bar text
        $rgch            = $type;        // Built-in name type

        $unknown01       = 0x29;
        $unknown02       = 0x002b;
        $unknown03       = 0x3b;
        $unknown04       = 0xffff-$index;
        $unknown05       = 0x0000;
        $unknown06       = 0x0000;
        $unknown07       = 0x1087;
        $unknown08       = 0x8008;

        $header             = pack("vv",  $record, $length);
        $data               = pack("v", $grbit);
        $data              .= pack("C", $chKey);
        $data              .= pack("C", $cch);
        $data              .= pack("v", $cce);
        $data              .= pack("v", $ixals);
        $data              .= pack("v", $itab);
        $data              .= pack("C", $cchCustMenu);
        $data              .= pack("C", $cchDescription);
        $data              .= pack("C", $cchHelptopic);
        $data              .= pack("C", $cchStatustext);
        $data              .= pack("C", $rgch);
        $data              .= pack("C", $unknown01);
        $data              .= pack("v", $unknown02);
        // Column definition
        $data              .= pack("C", $unknown03);
        $data              .= pack("v", $unknown04);
        $data              .= pack("v", $unknown05);
        $data              .= pack("v", $unknown06);
        $data              .= pack("v", $unknown07);
        $data              .= pack("v", $unknown08);
        $data              .= pack("v", $index);
        $data              .= pack("v", $index);
        $data              .= pack("v", 0x0000);
        $data              .= pack("v", 0x3fff);
        $data              .= pack("C", $colmin);
        $data              .= pack("C", $colmax);
        // Row definition
        $data              .= pack("C", $unknown03);
        $data              .= pack("v", $unknown04);
        $data              .= pack("v", $unknown05);
        $data              .= pack("v", $unknown06);
        $data              .= pack("v", $unknown07);
        $data              .= pack("v", $unknown08);
        $data              .= pack("v", $index);
        $data              .= pack("v", $index);
        $data              .= pack("v", $rowmin);
        $data              .= pack("v", $rowmax);
        $data              .= pack("C", 0x00);
        $data              .= pack("C", 0xff);
        // End of data
        $data              .= pack("C", 0x10);
        $this->_append($header . $data);
    }

    /**
    * Stores the COUNTRY record for localization
    *
    * @access private
    */
    function _storeCountry()
    {
        $record          = 0x008C;    // Record identifier
        $length          = 4;         // Number of bytes to follow

        $header = pack('vv',  $record, $length);
        /* using the same country code always for simplicity */
        $data = pack('vv', $this->_country_code, $this->_country_code);
        $this->_append($header . $data);
    }

    /**
    * Stores the PALETTE biff record.
    *
    * @access private
    */
    function _storePalette()
    {
        $aref            = $this->_palette;

        $record          = 0x0092;                 // Record identifier
        $length          = 2 + 4 * count($aref);   // Number of bytes to follow
        $ccv             =         count($aref);   // Number of RGB values to follow
        $data = '';                                // The RGB data

        // Pack the RGB data
        foreach ($aref as $color) {
            foreach ($color as $byte) {
                $data .= pack("C",$byte);
            }
        }

        $header = pack("vvv",  $record, $length, $ccv);
        $this->_append($header . $data);
    }

    /**
    * Calculate
    * Handling of the SST continue blocks is complicated by the need to include an
    * additional continuation byte depending on whether the string is split between
    * blocks or whether it starts at the beginning of the block. (There are also
    * additional complications that will arise later when/if Rich Strings are
    * supported).
    *
    * @access private
    */
    function _calculateSharedStringsSizes()
    {
        /* Iterate through the strings to calculate the CONTINUE block sizes.
           For simplicity we use the same size for the SST and CONTINUE records:
           8228 : Maximum Excel97 block size
             -4 : Length of block header
             -8 : Length of additional SST header information
             -8 : Arbitrary number to keep within _add_continue() limit = 8208
        */
        $continue_limit     = 8208;
        $block_length       = 0;
        $written            = 0;
        $this->_block_sizes = array();
        $continue           = 0;

        foreach (array_keys($this->_str_table) as $string) {
            $string_length = strlen($string);
            $headerinfo    = unpack("vlength/Cencoding", $string);
            $encoding      = $headerinfo["encoding"];
            $split_string  = 0;

            // Block length is the total length of the strings that will be
            // written out in a single SST or CONTINUE block.
            $block_length += $string_length;

            // We can write the string if it doesn't cross a CONTINUE boundary
            if ($block_length < $continue_limit) {
                $written      += $string_length;
                continue;
            }

            // Deal with the cases where the next string to be written will exceed
            // the CONTINUE boundary. If the string is very long it may need to be
            // written in more than one CONTINUE record.
            while ($block_length >= $continue_limit) {

                // We need to avoid the case where a string is continued in the first
                // n bytes that contain the string header information.
                $header_length   = 3; // Min string + header size -1
                $space_remaining = $continue_limit - $written - $continue;


                /* TODO: Unicode data should only be split on char (2 byte)
                boundaries. Therefore, in some cases we need to reduce the
                amount of available
                */
                $align = 0;

                // Only applies to Unicode strings
                if ($encoding == 1) {
                    // Min string + header size -1
                    $header_length = 4;

                    if ($space_remaining > $header_length) {
                        // String contains 3 byte header => split on odd boundary
                        if (!$split_string && $space_remaining % 2 != 1) {
                            $space_remaining--;
                            $align = 1;
                        }
                        // Split section without header => split on even boundary
                        else if ($split_string && $space_remaining % 2 == 1) {
                            $space_remaining--;
                            $align = 1;
                        }

                        $split_string = 1;
                    }
                }


                if ($space_remaining > $header_length) {
                    // Write as much as possible of the string in the current block
                    $written      += $space_remaining;

                    // Reduce the current block length by the amount written
                    $block_length -= $continue_limit - $continue - $align;

                    // Store the max size for this block
                    $this->_block_sizes[] = $continue_limit - $align;

                    // If the current string was split then the next CONTINUE block
                    // should have the string continue flag (grbit) set unless the
                    // split string fits exactly into the remaining space.
                    if ($block_length > 0) {
                        $continue = 1;
                    } else {
                        $continue = 0;
                    }
                } else {
                    // Store the max size for this block
                    $this->_block_sizes[] = $written + $continue;

                    // Not enough space to start the string in the current block
                    $block_length -= $continue_limit - $space_remaining - $continue;
                    $continue = 0;

                }

                // If the string (or substr) is small enough we can write it in the
                // new CONTINUE block. Else, go through the loop again to write it in
                // one or more CONTINUE blocks
                if ($block_length < $continue_limit) {
                    $written = $block_length;
                } else {
                    $written = 0;
                }
            }
        }

        // Store the max size for the last block unless it is empty
        if ($written + $continue) {
            $this->_block_sizes[] = $written + $continue;
        }


        /* Calculate the total length of the SST and associated CONTINUEs (if any).
         The SST record will have a length even if it contains no strings.
         This length is required to set the offsets in the BOUNDSHEET records since
         they must be written before the SST records
        */

        $tmp_block_sizes = array();
        $tmp_block_sizes = $this->_block_sizes;

        $length  = 12;
        if (!empty($tmp_block_sizes)) {
            $length += array_shift($tmp_block_sizes); // SST
        }
        while (!empty($tmp_block_sizes)) {
            $length += 4 + array_shift($tmp_block_sizes); // CONTINUEs
        }

        return $length;
    }

    /**
    * Write all of the workbooks strings into an indexed array.
    * See the comments in _calculate_shared_string_sizes() for more information.
    *
    * The Excel documentation says that the SST record should be followed by an
    * EXTSST record. The EXTSST record is a hash table that is used to optimise
    * access to SST. However, despite the documentation it doesn't seem to be
    * required so we will ignore it.
    *
    * @access private
    */
    function _storeSharedStringsTable()
    {
        $record  = 0x00fc;  // Record identifier
        $length  = 0x0008;  // Number of bytes to follow
        $total   = 0x0000;

        // Iterate through the strings to calculate the CONTINUE block sizes
        $continue_limit = 8208;
        $block_length   = 0;
        $written        = 0;
        $continue       = 0;

        // sizes are upside down
        $tmp_block_sizes = $this->_block_sizes;
        // $tmp_block_sizes = array_reverse($this->_block_sizes);

        // The SST record is required even if it contains no strings. Thus we will
        // always have a length
        //
        if (!empty($tmp_block_sizes)) {
            $length = 8 + array_shift($tmp_block_sizes);
        }
        else {
            // No strings
            $length = 8;
        }

        // Write the SST block header information
        $header      = pack("vv", $record, $length);
        $data        = pack("VV", $this->_str_total, $this->_str_unique);
        $this->_append($header . $data);

        /* TODO: not good for performance */
        foreach (array_keys($this->_str_table) as $string) {

            $string_length = strlen($string);
            $headerinfo    = unpack("vlength/Cencoding", $string);
            $encoding      = $headerinfo["encoding"];
            $split_string  = 0;

            // Block length is the total length of the strings that will be
            // written out in a single SST or CONTINUE block.
            //
            $block_length += $string_length;


            // We can write the string if it doesn't cross a CONTINUE boundary
            if ($block_length < $continue_limit) {
                $this->_append($string);
                $written += $string_length;
                continue;
            }

            // Deal with the cases where the next string to be written will exceed
            // the CONTINUE boundary. If the string is very long it may need to be
            // written in more than one CONTINUE record.
            //
            while ($block_length >= $continue_limit) {

                // We need to avoid the case where a string is continued in the first
                // n bytes that contain the string header information.
                //
                $header_length   = 3; // Min string + header size -1
                $space_remaining = $continue_limit - $written - $continue;


                // Unicode data should only be split on char (2 byte) boundaries.
                // Therefore, in some cases we need to reduce the amount of available
                // space by 1 byte to ensure the correct alignment.
                $align = 0;

                // Only applies to Unicode strings
                if ($encoding == 1) {
                    // Min string + header size -1
                    $header_length = 4;

                    if ($space_remaining > $header_length) {
                        // String contains 3 byte header => split on odd boundary
                        if (!$split_string && $space_remaining % 2 != 1) {
                            $space_remaining--;
                            $align = 1;
                        }
                        // Split section without header => split on even boundary
                        else if ($split_string && $space_remaining % 2 == 1) {
                            $space_remaining--;
                            $align = 1;
                        }

                        $split_string = 1;
                    }
                }


                if ($space_remaining > $header_length) {
                    // Write as much as possible of the string in the current block
                    $tmp = substr($string, 0, $space_remaining);
                    $this->_append($tmp);

                    // The remainder will be written in the next block(s)
                    $string = substr($string, $space_remaining);

                    // Reduce the current block length by the amount written
                    $block_length -= $continue_limit - $continue - $align;

                    // If the current string was split then the next CONTINUE block
                    // should have the string continue flag (grbit) set unless the
                    // split string fits exactly into the remaining space.
                    //
                    if ($block_length > 0) {
                        $continue = 1;
                    } else {
                        $continue = 0;
                    }
                } else {
                    // Not enough space to start the string in the current block
                    $block_length -= $continue_limit - $space_remaining - $continue;
                    $continue = 0;
                }

                // Write the CONTINUE block header
                if (!empty($this->_block_sizes)) {
                    $record  = 0x003C;
                    $length  = array_shift($tmp_block_sizes);

                    $header  = pack('vv', $record, $length);
                    if ($continue) {
                        $header .= pack('C', $encoding);
                    }
                    $this->_append($header);
                }

                // If the string (or substr) is small enough we can write it in the
                // new CONTINUE block. Else, go through the loop again to write it in
                // one or more CONTINUE blocks
                //
                if ($block_length < $continue_limit) {
                    $this->_append($string);
                    $written = $block_length;
                } else {
                    $written = 0;
                }
            }
        }
    }
}
