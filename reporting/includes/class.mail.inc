<?php
/*
    Name:           eMail
    Description:    Simple sending eMail in text and HTML with CC, BCC and attachment
    Version:        1.0
    last modified:  2004-05-14

    Autor:          Daniel Käfer
    Homepage:       http://www.danielkaefer.de

    Leave this header in this file!
------------------------------------------------------------------ 
 Updated: 2010-10-25
 Updated by: Michael Hahn (MPH)
 
 Problem: The Suhosin patch for PHP was blocking the email before it ever reached
 the email server due to double line feeds (\n) in the header of the email.
 Also, the body of the message was included in the header. This would also
 trip the security measure everytime it spotted a double line feed.
 Fix: Remove any double line feed from the header info and seperate the body of 
 the message from the header.
 Other updates: I'm not sure about RFC compliance but, every other email I've look at had
 certain information included in double quotes. More than likely to avoid
 erroneous file naming. I tried to emulate this mindset.
 Added line length and EOL char for file chunking. For some reason without
 it there were extra line feeds in the chunked file.

 * Lots of fixes for FA
*/

class email
{
    var $to = array();
    var $cc = array();
    var $bcc = array();
    var $attachment = array();
    var $boundary = "";
    var $header = "";
    var $subject = "";
    var $body = "";
	var $charset = 'ISO-8859-1';
	var $add_params;
	
    function __construct($name, $mail)
    {
        $this->boundary = md5(uniqid(time()));
		$this->header = "From: $name <$mail>\n";
		$bcc = get_company_pref('bcc_email');
		if ($bcc)
			$this->bcc[] = $bcc;
    }

    function to($mail)
    {
    	$this->to[] = $mail;
    }

    function cc($mail)
    {
    	$this->cc[] = $mail;
    }

    function bcc($mail)
    {
    	$this->bcc[] = $mail;
    }

    function attachment($file, $filename=null)
    {
    	if (!isset($filename))
    		$filename = basename($file);
    	$this->attachment[$filename] = $file;
    }

    function subject($subject)
    {
    	$this->subject = $subject;
    }

    function text($text)
    {
        $this->body = "--$this->boundary\n";
	    $this->body .= "Content-Type: text/plain; charset=\"{$this->charset}\"\n";
	    $this->body .= "Content-Transfer-Encoding: 8bit\n\n";
	    $this->body .= $text."\n";
    }

    function html($html)
    {
        $this->body = "--$this->boundary\n";
	    $this->body .= "Content-Type: text/html; charset=\"{$this->charset}\"\n";
	    $this->body .= "Content-Transfer-Encoding: quoted-printable\n\n";
	    $this->body .= "<html><body>\n".$html."\n</body></html>\n";
    }

	function mime_type($filename)
	{
		$file = basename($filename, '.zip');
		if ($filename == $file . '.zip') return 'application/x-zip-compressed';
		$file = basename($filename, '.pdf');
		if ($filename == $file . '.pdf') return 'application/pdf';
		$file = basename($filename, '.xls'); 
		if ($filename == $file . '.xls') return 'application/vnd.ms-excel'; 
		$file = basename($filename, '.csv');
		if ($filename == $file . '.csv') return 'application/vnd.ms-excel';
		$file = basename($filename, '.tar');
		if ($filename == $file . '.tar') return 'application/x-tar';
		$file = basename($filename, '.tar.gz');
		if ($filename == $file . '.tar.gz') return 'application/x-tar-gz';
		$file = basename($filename, '.tgz');
		if ($filename == $file . '.tgz') return 'application/x-tar-gz';
		$file = basename($filename, '.gz');
		if ($filename == $file . '.gz') return 'application/x-gzip';
		$file = basename($filename, '.html');
		if ($filename == $file . '.html') return 'text/html';
		return 'application/unknown';
	}

	function send()
    {
        // Add CC Recipients 
		if (!empty($this->cc)) 
			$this->header .= "Cc: " . implode(", ", $this->cc) . "\n" ; 

		// Add BCC Recipients 
		if (!empty($this->bcc)) 
			$this->header .= "Bcc: " . implode(", ", $this->bcc) . "\n" ; 
		$this->header .= "Content-Type: multipart/mixed;\n boundary=\"$this->boundary\"\n";

        // Add Attachments
        if (!empty($this->attachment))
        {
            foreach ($this->attachment as $filename => $file)
            {
                $file = fread(fopen($file, "r"), filesize($file));
				$this->body .= "--".$this->boundary."\n";
				$this->body .= "Content-Type: " .$this->mime_type($file). "; name=\"".$filename."\"\n";
				$this->body .= "Content-Transfer-Encoding: base64\n";
				$this->body .= "Content-Disposition: attachment; filename=\"".$filename."\"\n\n";
				$this->body .= chunk_split(base64_encode($file),"72","\n");
                $file = "";
            }
        }
		$this->body .= "--".$this->boundary."--\n";

		$ret = 0;
        foreach($this->to as $mail)
        {
			if (mail($mail, $this->subject, $this->body, $this->header, $this->add_params))
				$ret++;
        }
        return $ret;
    }
}
