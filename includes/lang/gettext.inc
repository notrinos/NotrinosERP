<?php
/* vim: set expandtab tabstop=4 shiftwidth=4: */
//  
//  Copyright (c) 2003 Laurent Bedubourg
//  
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//  
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//  
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  
//  Authors: Laurent Bedubourg <laurent.bedubourg@free.fr>
//  

define('GETTEXT_NATIVE', 1);
define('GETTEXT_PHP', 2);

function get_text_init($managerType = GETTEXT_NATIVE) {
	global $GetText;
	if (!isset($GetText)) {

        if ($managerType == GETTEXT_NATIVE) 
        {
            if (function_exists('gettext')) 
            {
                $GetText = new gettext_native_support();
                return;
            }
        }
        // fail back to php support 
		$GetText = new gettext_php_support();
	}
}

function raise_error($str) {
	error_log($str);
	return 1;
}

function is_error($err) {
    return $err > 0;
}

/**
* Interface to gettext native support.
*
* @author Laurent Bedubourg <laurent.bedubourg@free.fr>
* @access private
*/
class gettext_native_support 
{
    var $_interpolation_vars = array();
    var $domain_path;

    /**
     * Set gettext language code.
     * @throws GetText_Error
     */
    function set_language($lang_code, $encoding)
    {
        putenv("LANG=$lang_code");
        putenv("LC_ALL=$lang_code");
        putenv("LANGUAGE=$lang_code");

		// cover a couple of country/encoding variants 
		$up = strtoupper($encoding);
		$low = strtolower($encoding);
		$lshort = strtr($up, '-','');
		$ushort = strtr($low, '-','');

		if ($lang_code == 'C')
			$set = setlocale(LC_ALL,'C');
		else
        	$set = setlocale(LC_ALL, $lang_code.".".$encoding, 
				$lang_code.".".$up, $lang_code.".".$low,
				$lang_code.".".$ushort,	$lang_code.".".$lshort);

        setlocale(LC_NUMERIC, 'C'); // important for numeric presentation etc.
        if ($set === false) 
        {
			if (strtoupper(substr(PHP_OS, 0, 3)) === 'WIN') // don't do this test if server is WIN
				return 0;
            $str = sprintf('language code "%s", encoding "%s" not supported by your system',
                $lang_code, $encoding);
			return raise_error("1 " . $str);
        }
    }
    /**
	 *	Check system support for given language nedded for gettext.
	 */
	function check_support($lang_code, $encoding)
    {
		if (strtoupper(substr(PHP_OS, 0, 3)) === 'WIN') // don't do this test if server is WIN
			return true;
		$old = setlocale(LC_CTYPE, '0'); // LC_MESSAGES does not exist on Win
		$up = strtoupper($encoding);
		$low = strtolower($encoding);
		$lshort = strtr($up, '-','');
		$ushort = strtr($low, '-','');

        $test = setlocale(LC_ALL,
			$lang_code.".".$encoding, 
			$lang_code.".".$up,
			$lang_code.".".$low,
			$lang_code.".".$ushort,
			$lang_code.".".$lshort) !== false;
		setlocale(LC_ALL, $old);
		setlocale(LC_NUMERIC, 'C');
		return $test;
	}
    /**
     * Add a translation domain.
     */
    function add_domain($domain, $path=false, $version='')
    {
        if ($path === false) 
	        $path = $this->domain_path;
        if ($path === false) 
	        $path = "./locale";
	    if ($domain == "")
	    	$domain = "?";
		if ($version) {
	// To avoid need for apache server restart after change of *.mo file
	// we have to include file version as part of filename.
	// This is alternative naming convention: $domain = $version.'/'.$domain;
			$domain .= '-'.$version;
		}
        bindtextdomain($domain, $path);
        //bind_textdomain_codeset($domain, $encoding);
        textdomain($domain);
    }

    /**
     * Retrieve translation for specified key.
     *
     * @access private
     */
    function _get_translation($key)
    {
        return gettext($key);
    }
    

    /**
     * Reset interpolation variables.
     */
    function reset()
    {
        $this->_interpolation_vars = array();
    }
    
    /**
     * Set an interpolation variable.
     */
    function set_var($key, $value)
    {
        $this->_interpolation_vars[$key] = $value;
    }

    /**
     * Set an associative array of interpolation variables.
     */
    function set_vars($hash)
    {
        $this->_interpolation_vars = array_merge($this->_interpolation_vars,
                                                $hash);
    }
    
    /**
     * Retrieve translation for specified key.
     *
     * @param  string $key  -- gettext msgid
     * @throws GetText_Error
     */
    function gettext($key)
    {
    	$value = $this->_get_translation($key);
        if ($value === false) {
            $str = sprintf('Unable to locate gettext key "%s"', $key);
			return raise_error("2 " . $str);
        }
        
        while (preg_match('/\$\{(.*?)\}/sm', $value, $m)) {
            list($src, $var) = $m;

            // retrieve variable to interpolate in context, throw an exception
            // if not found.
            $var2 = $this->_get_var($var);
            if ($var2 === false) {
                $str = sprintf('Interpolation error, var "%s" not set', $var);
                //$err = new GetText_Error($str);
                //return PEAR::raise_error($err);
                return raise_error("3 " . $str);
            }
            $value = str_replace($src, $var2, $value);
        }
        return $value;
    }

    /**
     * Retrieve an interpolation variable value.
     * 
     * @return mixed
     * @access private
     */
    function _get_var($name)
    {
        if (!array_key_exists($name, $this->_interpolation_vars)) {
            return false;
        }
        return $this->_interpolation_vars[$name];
    }
}


/**
* Implementation of get_text support for PHP.
*
* This implementation is abble to cache .po files into php files returning the
* domain translation hashtable.
*
* @access private
* @author Laurent Bedubourg <laurent.bedubourg@free.fr>
*/
class gettext_php_support extends gettext_native_support
{
    var $_path     = 'locale/';
    var $_lang_code = false;
    var $_domains  = array();
    var $_end      = -1;
    var $_jobs     = array();

    /**
     * Set the translation domain.
     *
     * @param  string $lang_code -- language code
     * @throws GetText_Error
     */
    function set_language($lang_code, $encoding)
    {
        // if language already set, try to reload domains
        if ($this->_lang_code !== false and $this->_lang_code != $lang_code) 
        {
            foreach ($this->_domains as $domain) 
            {
                $this->_jobs[] = array($domain->name, $domain->path);
            }
            $this->_domains = array();
            $this->_end = -1;
        }
        
        $this->_lang_code = $lang_code;

        // this allow us to set the language code after 
        // domain list.
        while (count($this->_jobs) > 0) 
        {
            list($domain, $path) = array_shift($this->_jobs);
            $err = $this->add_domain($domain, $path);
            // error raised, break jobs
			if (is_error($err)) 
			{
                return $err;
            }            
        }
    }
    /**
	 *	Check system support for given language (dummy).
	 */
	function check_support($lang_code, $encoding)
    {
		return true;
    }
    /**
     * Add a translation domain.
     *
     * @param string $domain        -- Domain name
     * @param string $path optional -- Repository path
     * @throws GetText_Error
     */
    function add_domain($domain, $path = false, $version ='')
    {   
        if ($path === false) 
	      $path = $this->domain_path;
        if ($path === false) 
	        $path = "./locale";

    	if ($version) {
			$domain .= '-'.$version;
		}

        if (array_key_exists($domain, $this->_domains)) 
        { 
            return; 
        }
        
        if (!$this->_lang_code) 
        { 
            $this->_jobs[] = array($domain, $path); 
            return;
        }
        // Don't fill the domains with false data, it increased the error.log
       	if (strpos($domain, $this->_lang_code) === false)
        	return;
 
        $err = $this->_load_domain($domain, $path);
        if ($err != 0) 
        {
            return $err;
        }

        $this->_end++;
    }

    /**
     * Load a translation domain file.
     *
     * This method cache the translation hash into a php file unless
     * GETTEXT_NO_CACHE is defined.
     * 
     * @param  string $domain        -- Domain name
     * @param  string $path optional -- Repository
     * @throws GetText_Error
     * @access private
     */
    function _load_domain($domain, $path = "./locale")
    {
        $src_domain = $path . "/$this->_lang_code/LC_MESSAGES/$domain.po";
        $php_domain = $path . "/$this->_lang_code/LC_MESSAGES/$domain.php";
        
        if (!file_exists($src_domain)) 
        {
            $str = sprintf('Domain file "%s" not found.', $src_domain);
			return raise_error("4 " . $str);
        }
        
        $d = new gettext_domain();
        $d->name = $domain;
        $d->path = $path;
        if (!file_exists($php_domain) || (filemtime($php_domain) < filemtime($src_domain))) 
        {
            
            // parse and compile translation table
            $parser = new gettext_php_support_parser();
            $hash   = $parser->parse($src_domain);
            if (!defined('GETTEXT_NO_CACHE')) 
            {
                $comp = new gettext_php_support_compiler();
                $err  = $comp->compile($hash, $src_domain);
    			if (is_error($err)) 
    			{
                    return $err;
                } 
            }
            $d->_keys = $hash;
        } 
        else 
        {
            $d->_keys = include $php_domain;
        }
        $this->_domains[] = &$d;
    }
    
    /**
     * Implementation of gettext message retrieval.
     */
    function _get_translation($key)
    {
        for ($i = $this->_end; $i >= 0; $i--) 
        {
            if ($this->_domains[$i]->has_key($key)) 
            {
                return $this->_domains[$i]->get($key);
            }
        }
        return $key;
    }
}

/**
* Class representing a domain file for a specified language.
*
* @access private
* @author Laurent Bedubourg <laurent.bedubourg@free.fr>
*/
class gettext_domain
{
    var $name;
    var $path;

    var $_keys = array();

    function has_key($key)
    {
        return array_key_exists($key, $this->_keys);
    }

    function get($key)
    {
        return $this->_keys[$key];
    }
}

/**
* This class is used to parse gettext '.po' files into php associative arrays.
*
* @access private
* @author Laurent Bedubourg <laurent.bedubourg@free.fr>
*/
class gettext_php_support_parser 
{
    var $_hash = array();
    var $_current_key;
    var $_current_value;
    
    /**
     * Parse specified .po file.
     *
     * @return hashtable
     * @throws GetText_Error
     */
    function parse($file)
    {
        $this->_hash = array();
        $this->_current_key = false;
        $this->_current_value = "";
        
        if (!file_exists($file)) 
        {
            $str = sprintf('Unable to locate file "%s"', $file);
			return raise_error($str);
        }
        $i = 0;
        $lines = file($file);
        foreach ($lines as $line) 
        {
            $this->_parse_line($line, ++$i);
        }
        $this->_store_key();

        return $this->_hash;
    }

    /**
     * Parse one po line.
     *
     * @access private
     */
    function _parse_line($line, $nbr)
    {
        $line = str_replace("\\\"", "'", $line); // Should be inside preg_match, but I couldn't find the solution. This works.
        if (preg_match('/^\s*?#/', $line)) { return; }
        if (preg_match('/^\s*?msgid \"(.*?)(?!<\\\)\"/', $line, $m)) {
            $this->_store_key();
            $this->_current_key = $m[1];
            return;
        }
        if (preg_match('/^\s*?msgstr \"(.*?)(?!<\\\)\"/', $line, $m)) {
            $this->_current_value .= $m[1];
            return;
        }
        if (preg_match('/^\s*?\"(.*?)(?!<\\\)\"/', $line, $m)) {
            $this->_current_value .= $m[1];
            return;
        }
    }

    /**
     * Store last key/value pair into building hashtable.
     *
     * @access private
     */
    function _store_key()
    {
        if ($this->_current_key === false) return;
        $this->_current_value = str_replace('\\n', "\n", $this->_current_value);
        $this->_hash[$this->_current_key] = $this->_current_value;
        $this->_current_key = false;
        $this->_current_value = "";
    }
}


/**
* This class write a php file from a gettext hashtable.
*
* The produced file return the translation hashtable on include.
* 
* @throws GetText_Error
* @access private
* @author Laurent Bedubourg <laurent.bedubourg@free.fr>
*/
class gettext_php_support_compiler 
{
    /**
     * Write hash in an includable php file.
     */
    function compile(&$hash, $source_path)
    {
        $dest_path = preg_replace('/\.po$/', '.php', $source_path);
        $fp = @fopen($dest_path, "w");
        if (!$fp) 
        {
            $str = sprintf('Unable to open "%s" in write mode.', $dest_path);
			return raise_error($str);
        }
        fwrite($fp, '<?php' . "\n");
        fwrite($fp, 'return array(' . "\n");
        foreach ($hash as $key => $value) 
        {
            $key   = str_replace("'", "\\'", $key);
            $value = str_replace("'", "\\'", $value);
            fwrite($fp, '    \'' . $key . '\' => \'' . $value . "',\n");
        }
        fwrite($fp, ');' . "\n");
        fclose($fp);
    }
}

/*
	Set current gettext domain path
*/
function set_ext_domain($path='') {
	global $path_to_root, $GetText;
	static $domain_stack = array('');

	if ($path)	// save path on domain stack
		array_unshift($domain_stack,  $path);
	else
	{
		array_shift($domain_stack);
		$path = $domain_stack[0];
	}

	$lang_path = $path_to_root . ($path ? '/' : '') .$path.'/lang';
	// ignore change when extension does not provide translation structure and test for valid gettext.
	if (file_exists($lang_path) && isset($GetText))
		$GetText->add_domain($_SESSION['language']->code,
			$lang_path, $path ? '' : $_SESSION['language']->version);
}
