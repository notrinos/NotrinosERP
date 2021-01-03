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
	Read content of remote url via http.
	Does not require curl php extension nor allow_url_fopen=1.
*/
function url_get_contents($url, $timeout=10)
{
	// get the host name and url path
	$parsedUrl = parse_url($url);

	if (@$parsedUrl['scheme'] == 'file')
		return file_get_contents($parsedUrl['path']);

	$host = $parsedUrl['host'];

	if (isset($parsedUrl['path'])) {
		$path = $parsedUrl['path'];
	} else {
		// the url is pointing to the host like http://www.mysite.com
		$path = '/';
	}

	if (isset($parsedUrl['query'])) {
		$path .= '?' . $parsedUrl['query'];
	}

	if (isset($parsedUrl['port'])) {
		$port = $parsedUrl['port'];
	} else {
		// most sites use port 80
		$port = '80';
	}

	$response = '';
	// connect to the remote server
	$fp = @fsockopen($host, $port, $errno, $errstr, $timeout );
	if( !$fp ) {
		return null;
	} else {
		// send the necessary headers to get the file
		fputs($fp, "GET $path HTTP/1.0\r\n" .
			"Host: $host\r\n".
			(isset($parsedUrl['pass']) ? "Authorization: Basic ".
				base64_encode($parsedUrl['user'].':'.$parsedUrl['pass'])."\r\n" : '').
			"User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.0.3) Gecko/20060426 Firefox/1.5.0.3\r\n" .
			"Accept: */*\r\n" .
			"Accept-Language: en-us,en;q=0.5\r\n" .
			"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n" .
			"Connection: close\r\n" .
			"Referer: http://$host\r\n\r\n");

		// retrieve the response from the remote server

		$response = stream_get_contents($fp);

		if (!strpos($response, "200 OK\r\n"))
			return null;
		// strip the headers
		$pos = strpos($response, "\r\n\r\n");

		$response = substr($response, $pos + 4);
	}

	// return the file content
	return $response;
}

function url_copy($from, $to, $timeout=10)
{
	$f = fopen($to, 'wb');
	if (!$f || !fwrite($f, url_get_contents($from, $timeout)))
		return false;
	fclose($f);
	return true;
}
