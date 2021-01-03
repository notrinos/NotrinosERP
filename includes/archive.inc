<?php
/*--------------------------------------------------
 | TAR/GZIP/BZIP2/ZIP ARCHIVE CLASSES 2.1
 | By Devin Doucette
 | Copyright (c) 2005 Devin Doucette
 | Email: darksnoopy@shaw.ca
 +--------------------------------------------------
 | Email bugs/suggestions to darksnoopy@shaw.ca
 +--------------------------------------------------
 | This script has been created and released under
 | the GNU GPL and is free to use and redistribute
 | only if this copyright statement is not removed
 +--------------------------------------------------
 | FrontAccounting bugfixes/changes added.
 +--------------------------------------------------*/

class archive
{
	function __construct($name)
	{
		$this->options = array (
			'basedir' => ".",
			'name' => $name,
			'prepend' => "",
			'inmemory' => 0,
			'overwrite' => 0,
			'recurse' => 1,
			'storepaths' => 1,
			'followlinks' => 0,
			'level' => 3,
			'method' => 1,
			'sfx' => "",
			'type' => "",
			'comment' => ""
		);
		$this->files = array ();
		$this->exclude = array ();
		$this->storeonly = array ();
		$this->error = array ();
	}

	function set_options($options)
	{
		foreach ($options as $key => $value)
			$this->options[$key] = $value;
		if (!empty ($this->options['basedir']))
		{
			$this->options['basedir'] = str_replace("\\", "/", $this->options['basedir']);
			$this->options['basedir'] = preg_replace("/\/+/", "/", $this->options['basedir']);
			$this->options['basedir'] = preg_replace("/\/$/", "", $this->options['basedir']);
		}
		if (!empty ($this->options['name']))
		{
			$this->options['name'] = str_replace("\\", "/", $this->options['name']);
//			$this->options['name'] = preg_replace("/\/+/", "/", $this->options['name']);
		}
		if (!empty ($this->options['prepend']))
		{
			$this->options['prepend'] = str_replace("\\", "/", $this->options['prepend']);
			$this->options['prepend'] = preg_replace("/^(\.*\/+)+/", "", $this->options['prepend']);
			$this->options['prepend'] = preg_replace("/\/+/", "/", $this->options['prepend']);
			$this->options['prepend'] = preg_replace("/\/$/", "", $this->options['prepend']) . "/";
		}
	}

	function create_archive()
	{
		$this->make_list();

		if ($this->options['inmemory'] == 0)
		{
			if ($this->options['overwrite'] == 0 && 
				file_exists($this->options['name'] 
				. ($this->options['type'] == "pkg" || $this->options['type'] == "gzip" ||$this->options['type'] == "bzip" ? ".tmp" : "")))
			{
				$this->error[] = "File {$this->options['name']} already exists.";
				return 0;
			}
			else if (!($this->archive = @fopen($this->options['name'].".tmp", "wb+")))
			{
				$this->error[] = "Could not open {$this->options['name']} for writing.".':'.getcwd();
				return 0;
			}
		}
		else
			$this->archive = "";

		if (!$this->create_tar())
		{
			$this->error[] = "Could not create package file.";
			return 0;
		}
		if (!$this->create_pkg())
		{
			$this->error[] = "Could not compress package file.";
			return 0;
		}

		if ($this->options['inmemory'] == 0)
		{
			fclose($this->archive);
			unlink($this->options['name'] . ".tmp");
		}
	}

	function add_data($data)
	{
		if ($this->options['inmemory'] == 0)
			fwrite($this->archive, $data);
		else
			$this->archive .= $data;
	}

	function make_list()
	{
		if (!empty ($this->exclude)) {
			foreach ($this->files as $key => $value)
				foreach ($this->exclude as $current)
					if ($value['name'] == $current['name']) 
						unset ($this->files[$key]);
		}
		if (!empty ($this->storeonly))
			foreach ($this->files as $key => $value)
				foreach ($this->storeonly as $current)
					if ($value['name'] == $current['name'])
						$this->files[$key]['method'] = 0;
		unset ($this->exclude, $this->storeonly);
	}

	function add_files($list)
	{
		$temp = $this->list_files($list);
		foreach ($temp as $current)
			$this->files[] = $current;
	}

	function exclude_files($list)
	{
		$temp = $this->list_files($list);
		foreach ($temp as $current)
			$this->exclude[] = $current;
	}

	function store_files($list)
	{
		$temp = $this->list_files($list);
		foreach ($temp as $current)
			$this->storeonly[] = $current;
	}

	function list_files($list)
	{
		if (!is_array ($list))
		{
			$temp = $list;
			$list = array ($temp);
			unset ($temp);
		}

		$files = array ();

		$pwd = getcwd();
		chdir($this->options['basedir']);

		foreach ($list as $current)
		{
			$current = str_replace("\\", "/", $current);
			$current = preg_replace("/\/+/", "/", $current);
			$current = preg_replace("/\/$/", "", $current);
			if (strstr($current, "*"))
			{
				$regex = preg_replace("/([\\\^\$\.\[\]\|\(\)\?\+\{\}\/])/", "\\\\\\1", $current);
				$regex = str_replace("*", ".*", $regex);
				$dir = strstr($current, "/") ? substr($current, 0, strrpos($current, "/")) : ".";
				$temp = $this->parse_dir($dir);
				foreach ($temp as $current2)
					if (preg_match("/^{$regex}$/i", $current2['name']))
						$files[] = $current2;
				unset ($regex, $dir, $temp, $current);
			}
			else if (@is_dir($current))
			{
				$temp = $this->parse_dir($current);
				foreach ($temp as $file)
					$files[] = $file;
				unset ($temp, $file);
			}
			else if (@file_exists($current))
				$files[] = array ('name' => $current, 'name2' => $this->options['prepend'] .
					preg_replace("/(\.+\/+)+/", "", ($this->options['storepaths'] == 0 && strstr($current, "/")) ?
					substr($current, strrpos($current, "/") + 1) : $current),
					'type' => @is_link($current) && $this->options['followlinks'] == 0 ? 2 : 0,
					'ext' => substr($current, strrpos($current, ".")), 'stat' => stat($current));
		}

		chdir($pwd);

		unset ($current, $pwd);
		return $files;
	}

	function parse_dir($dirname)
	{
		if ($this->options['storepaths'] == 1 && !preg_match("/^(\.+\/*)+$/", $dirname))
			$files = array (array ('name' => $dirname, 'name2' => $this->options['prepend'] .
				preg_replace("/(\.+\/+)+/", "", ($this->options['storepaths'] == 0 && strstr($dirname, "/")) ?
				substr($dirname, strrpos($dirname, "/") + 1) : $dirname), 'type' => 5, 'stat' => stat($dirname)));
		else
			$files = array ();
		$dir = @opendir($dirname);

		while ($file = @readdir($dir))
		{
			$fullname = $dirname . "/" . $file;
			if ($file == "." || $file == "..")
				continue;
			else if (@is_dir($fullname))
			{
				if (empty ($this->options['recurse']))
					continue;
				$temp = $this->parse_dir($fullname);
				foreach ($temp as $file2)
					$files[] = $file2;
			}
			else if (@file_exists($fullname))
				$files[] = array ('name' => $fullname, 'name2' => $this->options['prepend'] .
					preg_replace("/(\.+\/+)+/", "", ($this->options['storepaths'] == 0 && strstr($fullname, "/")) ?
					substr($fullname, strrpos($fullname, "/") + 1) : $fullname),
					'type' => @is_link($fullname) && $this->options['followlinks'] == 0 ? 2 : 0,
					'ext' => substr($file, strrpos($file, ".")), 'stat' => stat($fullname));
		}

		@closedir($dir);

		return $files;
	}
}

class tar_file extends archive
{
	function __construct($name)
	{
		parent::__construct($name);
		$this->options['type'] = "tar";
	}

	function create_tar()
	{
		$pwd = getcwd();
		chdir($this->options['basedir']);
		$files = $this->files;
		foreach ($files as $current)
		{
			if ($current['name'] == $this->options['name'])
				continue;
			if (strlen($current['name2']) > 99)
			{
				$path = substr($current['name2'], 0, strpos($current['name2'], "/", strlen($current['name2']) - 100) + 1);
				$current['name2'] = substr($current['name2'], strlen($path));
				if (strlen($path) > 154 || strlen($current['name2']) > 99)
				{
					$this->error[] = "Could not add {$path}{$current['name2']} to archive because the filename is too long.";
					continue;
				}
			}
			$block = pack("a100a8a8a8a12a12a8a1a100a6a2a32a32a8a8a155a12", $current['name2'], sprintf("%07o", 
				$current['stat'][2]), sprintf("%07o", $current['stat'][4]), sprintf("%07o", $current['stat'][5]), 
				sprintf("%011o", $current['type'] == 2 ? 0 : $current['stat'][7]), sprintf("%011o", $current['stat'][9]), 
				"        ", $current['type'], $current['type'] == 2 ? @readlink($current['name']) : "", "ustar ", " ", 
				"Unknown", "Unknown", "", "", !empty ($path) ? $path : "", "");

			$checksum = 0;
			for ($i = 0; $i < 512; $i++)
				$checksum += ord(substr($block, $i, 1));
			$checksum = pack("a8", sprintf("%07o", $checksum));
			$block = substr_replace($block, $checksum, 148, 8);

			if ($current['type'] == 2 || $current['stat'][7] == 0)
				{ $this->add_data($block); 
				}
			else if ($fp = @fopen($current['name'], "rb"))
			{
				$this->add_data($block);

				while ($temp = fread($fp, 1048576)) {
					$this->add_data($temp);
				}
				if ($current['stat'][7] % 512 > 0)
				{
					$temp = "";
					for ($i = 0; $i < 512 - $current['stat'][7] % 512; $i++)
						$temp .= "\0";
					$this->add_data($temp);
				}
				fclose($fp);
			}
			else
				$this->error[] = "Could not open file {$current['name']} for reading. It was not added.";
		}

		$this->add_data(pack("a1024", ""));

		chdir($pwd);

		return 1;
	}
	/*
		Extract files to base directory.
		When $dry_run!=0 no file is written,
		but list of existing files to be written is returned.
	*/
	function extract_files($dry_run = false)
	{
	$flist = array();
		if ($fp = $this->open_archive())
		{
			$pwd = getcwd();
			// display_error($pwd.':'.$this->options['basedir']);
			chdir($this->options['basedir']);
			if ($this->options['inmemory'] == 1)
				$this->files = array();

			while ($block = fread($fp, 512))
			{
				$temp = unpack("a100name/a8mode/a8uid/a8gid/a12size/a12mtime/a8checksum/a1type/a100symlink/a6magic/a2temp/a32temp/a32temp/a8temp/a8temp/a155prefix/a12temp", $block);
				$file = array (
					'name' => rtrim($temp['prefix']) . rtrim($temp['name']),
					'stat' => array (
						2 => octdec($temp['mode']),
						4 => octdec($temp['uid']),
						5 => octdec($temp['gid']),
						7 => octdec($temp['size']),
						9 => octdec($temp['mtime']),
					),
					'checksum' => octdec($temp['checksum']),
					'type' => $temp['type'],
					'magic' => $temp['magic'],
				);
				if ($file['checksum'] == 0x00000000)
					break;
				else if (substr($file['magic'], 0, 5) != "ustar")
				{
					$this->error[] = "This script does not support extracting this type of tar file.";
					break;
				}
				$block = substr_replace($block, "        ", 148, 8);
				$checksum = 0;
				for ($i = 0; $i < 512; $i++)
					$checksum += ord(substr($block, $i, 1));
				if ($file['checksum'] != $checksum) {
					$this->error[] = "Could not extract from {$this->options['name']}, it is corrupt.";
					break;
				}
				if ($dry_run || $this->options['inmemory'] == 1)
				{
					if ($file['type'] == 0) {
						if($dry_run) {
							$tst = check_write($file['name']);
							if (!$tst) {
								$this->error[] = "Could not open {$this->options['basedir']}/{$file['name']} for writing.";
								break;
							} else {
								$flist[] = $file['name'];
								if ( $tst < 0 && $this->options['overwrite'] == 0)
									$this->error[] = "{$file['name']} already exists.";
							}
							fseek($fp, ($file['stat'][7] + 511) & ~511, SEEK_CUR); // skip data
						} else {
							$dat = fread($fp, $file['stat'][7]);
							$tail = $file['stat'][7] % 512;
							if ($tail)
								fread($fp, 512 - $tail);
							$file['data'] = $dat;
						}
					}
					unset ($file['checksum'], $file['magic']);
					$this->files[] = $file; 
				}
				else if ($file['type'] == 5)
				{
					if (!is_dir($file['name'])) {
						mkdir($file['name'], $file['stat'][2]);
					}
				}
				else if ($this->options['overwrite'] == 0 && file_exists($file['name']))
				{
					$this->error[] = "{$file['name']} already exists.";
					continue;
				}
				else if ($file['type'] == 2)
				{
					symlink($temp['symlink'], $file['name']);
					@chmod($file['name'], $file['stat'][2]);
				}
				else if ($new = @fopen($file['name'], "wb"))
				{
					fwrite($new, fread($fp, $file['stat'][7]));
					if ($file['stat'][7] % 512)
						fread($fp, 512 - $file['stat'][7] % 512);
					fclose($new);
					@chmod($file['name'], $file['stat'][2]);
				}
				else
				{
					$this->error[] = "Could not open {$file['name']} for writing.";
					continue;
				}
//				@chown($file['name'], $file['stat'][4]);
//				@chgrp($file['name'], $file['stat'][5]);
				if (!$dry_run) {
					@touch($file['name'], $file['stat'][9]);
				}
				unset ($file);
			}
			chdir($pwd);
		}
		else
			$this->error[] = "Could not open file {$this->options['name']}";

		return $flist;
	}

	function open_archive()
	{
		return @fopen($this->options['name'], "rb");
	}
}

class gzip_file extends tar_file
{
	function __construct($name)
	{
		parent::__construct($name);
		$this->options['type'] = "gzip";
	}

	function create_gzip()
	{
		if ($this->options['inmemory'] == 0)
		{
			if ($fp = gzopen($this->options['name'], "wb{$this->options['level']}"))
			{
				fseek($this->archive, 0);
				while ($temp = fread($this->archive, 1048576))
					gzwrite($fp, $temp);
				gzclose($fp);
			}
			else
			{
				$this->error[] = "Could not open {$this->options['name']} for writing.";
				return 0;
			}
		}
		else
			$this->archive = gzencode($this->archive, $this->options['level']);

		return 1;
	}

	function open_archive()
	{
		return @gzopen($this->options['name'], "rb");
	}
}

