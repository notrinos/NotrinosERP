<?php
/**
*
*   graph
*   version 1.0
*
*
*
* Author: Carlos Reche/Joe Hunt
* E-mail: carlosreche@yahoo.com/joe.hunt.consulting@gmail.com
* Sorocaba, SP - Brazil/Wellington, New Zealand
*
* Created: Sep 20, 2004
* Last Modification: Sep 20, 2004/Apr 01, 2007
*
*
*
*  Authors' comments:
*
*  graph creates 6 different types of graphics with how many parameters you want. You can
*  change the appearance of the graphics in 3 different skins, and you can still cross data from 2 
*  graphics in only 1! It's a powerful script, and I recommend you read all the instructions 
*  to learn how to use all of this features. Don't worry, it's very simple to use it.
*
*  This script is free. Please keep the credits.
*
*/

/**

   INSTRUNCTIONS OF HOW TO USE THIS SCRIPT  (Please, take a minute to read it. It's important!)


   NOTE: make sure that your PHP is compiled to work with GD Lib.

///// START OF EXAMPLE.PHP /////

<?php

require "class.graphics.inc";
$pg = new graph;

$pg->title = "Sex";
$pg->type  = "5";
$pg->x[0]  = "male";
$pg->y[0]  = "50";
$pg->x[1]  = "female";
$pg->y[1]  = "55";
$pg->display();
?>

In your html file you set it up as:
.....
<img src="example.php" border="1" />
.....

You can supply extra parameters to display(). Ex. $pg->display("test.png") will save the image to a file.
Ex. $pg->display("", true) will paint a border around the image. It might be suitable if you choose to save to
file for later presentation.

///// END OF EXAMPLE.PHP /////



   Here is a list of all parameters you may set:

   title      =>  Title of the graphic
   axis_x     =>  Name of values from Axis X
   axis_y     =>  Name of values from Axis Y
   graphic_1  =>  Name of Graphic_1 (only shown if you are gonna cross data from 2 different graphics)
   graphic_2  =>  Name of Graphic_2 (same comment of above)

   type  =>  Type of graphic (values 1 to 6)
                1 => Vertical bars (default)
                2 => Horizontal bars
                3 => Dots
                4 => Lines
                5 => Pie
                6 => Donut

   skin   => Skin of the graphic (values 1 to 3)
                1 => Office (default)
                2 => Matrix
                3 => Spring

   credits => Only if you want to show my credits in the image. :)
                0 => doesn't show (default)
                1 => shows

   x[0]  =>  Name of the first parameter in Axis X
   x[1]  =>  Name of the second parameter in Axis X
   ... (etc)

   y[0]  =>  Value from "graphic_1" relative for "x[0]"
   y[1]  =>  Value from "graphic_1" relative for "x[1]"
   ... (etc)

   z[0]  =>  Value from "graphic_2" relative for "x[0]"
   z[1]  =>  Value from "graphic_2" relative for "x[1]"
   ... (etc)


   NOTE: You can't cross data between graphics if you use "pie" or "donut" graphic. Values for "z"
   won't be considerated.

   That's all! Hope you make a good use of it!
   It would be nice to receive feedback from others users. All comments are welcome!

   Regards,

   Carlos Reche

*/

class graph 
{

    var $x;
    var $y;
    var $z;

    var $title;
    var $axis_x;
    var $axis_y;
    var $graphic_1;
    var $graphic_2;
    var $type = 1;
    var $skin = 1;
    var $credits = 0;
    var $latin_notation;

    var $width;
    var $height;
    var $height_title;
    var $alternate_x;

	var $size = 2;
	var $tsize = 5;

    var $total_parameters;
    var $sum_total;
    var $biggest_value;
    var $biggest_parameter;
    var $available_types;
    var $dec1 = 0;
    var $dec2 = 0;
    var $h3d = 15; // 3D height
	var $built_in = false;
	var $fontfile = "";
	var $encoding;
	
    function __construct()
    {
    	global $SysPrefs;
		$this->encoding = strtoupper($_SESSION['language']->encoding);
		$path = dirname(__FILE__).'/../fonts/';

		// If you use UTF-8 encoding you have to download and install FreeSans.ttf font.
		// It is not bundled with application due to its size.
		// You can also use another UTF-8 font and put it in config.php with the name in $UTF8_fontfile 
		$this->fontfile = $this->encoding == 'UTF-8' ? (isset($SysPrefs->UTF8_fontfile) && $SysPrefs->UTF8_fontfile != "" ?
			$path.$SysPrefs->UTF8_fontfile : $path.'FreeSans.ttf') : $path.'LiberationSans-Regular.ttf';
        
        $this->x = $this->y = $this->z = array();
        $this->biggest_x        = NULL;
        $this->biggest_y        = NULL;
        $this->alternate_x      = false;
        $this->graphic_2_exists = false;
        $this->total_parameters = 0;
        $this->sum_total        = 1;
		$this->latin_notation   = false;
    }


    function display($save="", $border=false)
    {
        $this->legend_exists        = (preg_match("/(5|6)/", $this->type)) ? true : false;
        $this->biggest_graphic_name = (strlen($this->graphic_1) > strlen($this->graphic_2)) ? $this->graphic_1 : $this->graphic_2;
        $this->height_title         = (!empty($this->title)) ? ($this->string_height($this->tsize) + 15) : 0;
        $this->space_between_bars   = ($this->type == 1) ? 40 : 30;
        $this->space_between_dots   = 40;
        $this->higher_value         = 0;
        $this->higher_value_str     = 0;

        $this->width               = 0;
        $this->height              = 0;
        $this->graphic_area_width  = 0;
        $this->graphic_area_height = 0;
        $this->graphic_area_x1     = 30;
        $this->graphic_area_y1     = 20 + $this->height_title;
        $this->graphic_area_x2     = $this->graphic_area_x1 + $this->graphic_area_width;
        $this->graphic_area_y2     = $this->graphic_area_y1 + $this->graphic_area_height;

		if (count($this->z) && (preg_match("/(1|2|3|4)/", $this->type)))
            $this->graphic_2_exists = true;
        $this->total_parameters    = count($this->x);
		for ($i = 0; $i < $this->total_parameters; $i++)
		{
			if (strlen($this->x[$i]) > strlen($this->biggest_x))
                $this->biggest_x = $this->x[$i];
			if ($this->y[$i] > $this->biggest_y)
                $this->biggest_y = number_format(round($this->y[$i], 1), 1, ".", "");
            if ($this->graphic_2_exists)    
            {
            	if (isset($this->z[$i]) && $this->z[$i] > $this->biggest_y)
                	$this->biggest_y = number_format(round($this->z[$i], 1), 1, ".", "");
            }		
        }

        if (($this->graphic_2_exists == true)  &&  ((!empty($this->graphic_1)) || (!empty($this->graphic_2))))
        {
            $this->legend_exists = true;
        }

        $this->sum_total           = array_sum($this->y);
        if ($this->sum_total == 0)
        	$this->sum_total = 1;
        $this->space_between_bars += ($this->graphic_2_exists == true) ? 10 : 0;

        $this->calculate_higher_value();
        $this->calculate_width();
        $this->calculate_height();

        $this->create_graphic($save, $border);
    }

    function create_graphic($save="", $border=false)
    {
    	$size = 3;
        $this->img = imagecreatetruecolor($this->width, $this->height);

        $this->load_color_palette();

        // Fill background
        imagefill($this->img, 0, 0, $this->color['background']);

        // Draw title
        if (!empty($this->title))
        {
            $center = ($this->width / 2) - ($this->string_width($this->title, $this->tsize) / 2);
            $this->_imagestring($this->img, $this->tsize, $center, 10, $this->title, $this->color['title']);
        }


        // Draw axis and background lines for "vertical bars", "dots" and "lines"
        if (preg_match("/^(1|3|4)$/", $this->type))
        {
            if ($this->legend_exists == true)
            {
                $this->draw_legend();
            }

            $higher_value_y    = $this->graphic_area_y1 + (0.1 * $this->graphic_area_height);
            $higher_value_size = 0.9 * $this->graphic_area_height;

            $less  = 7 * strlen($this->higher_value_str);

            imageline($this->img, $this->graphic_area_x1, $higher_value_y, $this->graphic_area_x2, $higher_value_y, $this->color['bg_lines']);
            $this->_imagestring($this->img, $this->size, ($this->graphic_area_x1-$less-7), ($higher_value_y-7), $this->higher_value_str, $this->color['axis_values']);

            for ($i = 1; $i < 10; $i++)
            {
                $dec_y = $i * ($higher_value_size / 10);
                $x1 = $this->graphic_area_x1;
                $y1 = $this->graphic_area_y2 - $dec_y;
                $x2 = $this->graphic_area_x2;
                $y2 = $this->graphic_area_y2 - $dec_y;

                imageline($this->img, $x1, $y1, $x2, $y2, $this->color['bg_lines']);
                if ($i % 2 == 0) 
                {
                    $value = $this->number_formated($this->higher_value * $i / 10, $this->dec1);
                    $len1 = strlen($this->higher_value_str);
                    $len2 = strlen($value);
                    if ($len2 < $len1)
                    	$len2 += ($len1-$len2-1);
                    $less = 7 * $len2;
                    $this->_imagestring($this->img, $this->size, ($x1-$less-7), ($y2-7), $value, $this->color['axis_values']);
                }
            }

            // Axis X
            $this->_imagestring($this->img, $this->size, $this->graphic_area_x2+40, $this->graphic_area_y2+3, $this->axis_x, $this->color['title']);
            imageline($this->img, $this->graphic_area_x1, $this->graphic_area_y2, $this->graphic_area_x2, $this->graphic_area_y2, $this->color['axis_line']);
            // Axis Y
            $this->_imagestring($this->img, $this->size, 20, $this->graphic_area_y1-20, $this->axis_y, $this->color['title']);
            imageline($this->img, $this->graphic_area_x1, $this->graphic_area_y1, $this->graphic_area_x1, $this->graphic_area_y2, $this->color['axis_line']);
        }


        // Draw axis and background lines for "horizontal bars"
        else if ($this->type == 2)
        {
            if ($this->legend_exists == true)
            {
                $this->draw_legend();
            }

            $higher_value_x    = $this->graphic_area_x2 - (0.2 * $this->graphic_area_width);
            $higher_value_size = 0.8 * $this->graphic_area_width;

            imageline($this->img, ($this->graphic_area_x1+$higher_value_size), $this->graphic_area_y1, ($this->graphic_area_x1+$higher_value_size), $this->graphic_area_y2, $this->color['bg_lines']);
            $this->_imagestring($this->img, $this->size, (($this->graphic_area_x1+$higher_value_size) - ($this->string_width($this->higher_value, $this->size)/2)), ($this->graphic_area_y2+2), $this->higher_value_str, $this->color['axis_values']);

            for ($i = 1, $alt = 15; $i < 10; $i++)
            {
                $dec_x = number_format(round($i * ($higher_value_size  / 10), 1), 1, ".", "");

                imageline($this->img, ($this->graphic_area_x1+$dec_x), $this->graphic_area_y1, ($this->graphic_area_x1+$dec_x), $this->graphic_area_y2, $this->color['bg_lines']);
                if ($i % 2 == 0) 
                {
                    $alt   = (strlen($this->biggest_y) > 4 && $alt != 15) ? 15 : 2;
                    $value = $this->number_formated($this->higher_value * $i / 10, $this->dec1);
                    $this->_imagestring($this->img, $this->size, (($this->graphic_area_x1+$dec_x) - ($this->string_width($this->higher_value, $this->size)/2)), ($this->graphic_area_y2), $value, $this->color['axis_values'], $alt);
                }
            }

            // Axis X
            $this->_imagestring($this->img, $this->size, ($this->graphic_area_x2+10), ($this->graphic_area_y2+3), $this->axis_y, $this->color['title']);
            imageline($this->img, $this->graphic_area_x1, $this->graphic_area_y2, $this->graphic_area_x2, $this->graphic_area_y2, $this->color['axis_line']);
            // Axis Y
            $this->_imagestring($this->img, $this->size, 20, ($this->graphic_area_y1-20), $this->axis_x, $this->color['title']);
            imageline($this->img, $this->graphic_area_x1, $this->graphic_area_y1, $this->graphic_area_x1, $this->graphic_area_y2, $this->color['axis_line']);
        }


        // Draw legend box for "pie" or "donut"
        else if (preg_match("/^(5|6)$/", $this->type))
        {
            $this->draw_legend();
        }



        /**
        * Draw graphic: VERTICAL BARS
        */
        if ($this->type == 1)
        {
            $num = 1;
            $x   = $this->graphic_area_x1 + 20;

            foreach ($this->x as $i => $parameter)
            {
                if (isset($this->z[$i])) 
                {
                    $size = round($this->z[$i] * $higher_value_size / $this->higher_value);
                    $x1   = $x + 10;
                    $y1   = ($this->graphic_area_y2 - $size) + 1;
                    $x2   = $x1 + 20;
                    $y2   = $this->graphic_area_y2 - 1;
                    imageline($this->img, ($x1+1), ($y1-1), $x2, ($y1-1), $this->color['bars_2_shadow']);
                    imageline($this->img, ($x2+1), ($y1-1), ($x2+1), $y2, $this->color['bars_2_shadow']);
                    imageline($this->img, ($x2+2), ($y1-1), ($x2+2), $y2, $this->color['bars_2_shadow']);
                    imagefilledrectangle($this->img, $x1, $y1, $x2, $y2, $this->color['bars_2']);
                }

                $size = round($this->y[$i] * $higher_value_size / $this->higher_value);
                $alt  = (($num % 2 == 0) && (strlen($this->biggest_x) > 5)) ? 15 : 2;
                $x1   = $x;
                $y1   = ($this->graphic_area_y2 - $size) + 1;
                $x2   = $x1 + 20;
                $y2   = $this->graphic_area_y2 - 1;
                $x   += $this->space_between_bars;
                $num++;

                imageline($this->img, ($x1+1), ($y1-1), $x2, ($y1-1), $this->color['bars_shadow']);
                imageline($this->img, ($x2+1), ($y1-1), ($x2+1), $y2, $this->color['bars_shadow']);
                imageline($this->img, ($x2+2), ($y1-1), ($x2+2), $y2, $this->color['bars_shadow']);
                imagefilledrectangle($this->img, $x1, $y1, $x2, $y2, $this->color['bars']);
                $this->_imagestring($this->img, $this->size, $x1, ($y2+2), $parameter, $this->color['axis_values'], $alt);
            }
        }


        /**
        * Draw graphic: HORIZONTAL BARS
        */
        else if ($this->type == 2)
        {
            $y = 10;

            foreach ($this->x as $i => $parameter)
            {
                if (isset($this->z[$i])) 
                {
                    $size = round($this->z[$i] * $higher_value_size / $this->higher_value);
                    $x1   = $this->graphic_area_x1 + 1;
                    $y1   = $this->graphic_area_y1 + $y + 10;
                    $x2   = $x1 + $size;
                    $y2   = $y1 + 15;
                    imageline($this->img, ($x1), ($y2+1), $x2, ($y2+1), $this->color['bars_2_shadow']);
                    imageline($this->img, ($x1), ($y2+2), $x2, ($y2+2), $this->color['bars_2_shadow']);
                    imageline($this->img, ($x2+1), ($y1+1), ($x2+1), ($y2+2), $this->color['bars_2_shadow']);
                    imagefilledrectangle($this->img, $x1, $y1, $x2, $y2, $this->color['bars_2']);
                    $this->_imagestring($this->img, $this->size, ($x2+7), ($y1+7), $this->number_formated($this->z[$i], $this->dec2), $this->color['bars_2_shadow']);
                }

                $size = round(($this->y[$i] / $this->higher_value) * $higher_value_size);
                $x1   = $this->graphic_area_x1 + 1;
                $y1   = $this->graphic_area_y1 + $y;
                $x2   = $x1 + $size;
                $y2   = $y1 + 15;
                $y   += $this->space_between_bars;

                imageline($this->img, ($x1), ($y2+1), $x2, ($y2+1), $this->color['bars_shadow']);
                imageline($this->img, ($x1), ($y2+2), $x2, ($y2+2), $this->color['bars_shadow']);
                imageline($this->img, ($x2+1), ($y1+1), ($x2+1), ($y2+2), $this->color['bars_shadow']);
                imagefilledrectangle($this->img, $x1, $y1, $x2, $y2, $this->color['bars']);
                $this->_imagestring($this->img, $this->size, ($x2+7), ($y1+2), $this->number_formated($this->y[$i], $this->dec2), $this->color['bars_shadow']);

                $this->_imagestring($this->img, $this->size, 30, ($y1+2), $parameter, $this->color['axis_values']);
            }
        }


        /**
        * Draw graphic: DOTS or LINE
        */
        else if (preg_match("/^(3|4)$/", $this->type))
        {

            $x[0] = $this->graphic_area_x1+1;

            foreach ($this->x as $i => $parameter)
            {
                if ($this->graphic_2_exists == true) 
                {
                    $size  = round($this->z[$i] * $higher_value_size / $this->higher_value);
                    $z[$i] = $this->graphic_area_y2 - $size;
                }

                $alt   = (($i % 2 == 0) && (strlen($this->biggest_x) > 5)) ? 15 : 2;
                $size  = round($this->y[$i] * $higher_value_size / $this->higher_value);
                $y[$i] = $this->graphic_area_y2 - $size;

                if ($i != 0) 
                {
                    imageline($this->img, $x[$i], ($this->graphic_area_y1+10), $x[$i], ($this->graphic_area_y2-1), $this->color['bg_lines']);
                }
                $this->_imagestring($this->img, $this->size, $x[$i], ($this->graphic_area_y2+2), $parameter, $this->color['axis_values'], $alt);

                $x[$i+1] = $x[$i] + 40;
            }

            foreach ($x as $i => $value_x)
            {
                if ($this->graphic_2_exists == true)
                {
                    if (isset($z[$i+1])) 
                    {
                        // Draw lines
                        if ($this->type == 4)
                        {
                            imageline($this->img, $x[$i], $z[$i], $x[$i+1], $z[$i+1], $this->color['line_2']);
                            imageline($this->img, $x[$i], ($z[$i]+1), $x[$i+1], ($z[$i+1]+1), $this->color['line_2']);
                        }
                        imagefilledrectangle($this->img, $x[$i]-1, $z[$i]-1, $x[$i]+2, $z[$i]+2, $this->color['line_2']);
                    }
                    else 
                    { // Draw last dot
                        imagefilledrectangle($this->img, $x[$i-1]-1, $z[$i-1]-1, $x[$i-1]+2, $z[$i-1]+2, $this->color['line_2']);
                    }
                }

                if (count($y) > 1)
                {
                    if (isset($y[$i+1])) 
                    {
                        // Draw lines
                        if ($this->type == 4)
                        {
                            imageline($this->img, $x[$i], $y[$i], $x[$i+1], $y[$i+1], $this->color['line']);
                            imageline($this->img, $x[$i], ($y[$i]+1), $x[$i+1], ($y[$i+1]+1), $this->color['line']);
                        }
                        imagefilledrectangle($this->img, $x[$i]-1, $y[$i]-1, $x[$i]+2, $y[$i]+2, $this->color['line']);
                    }
                    else 
                    { // Draw last dot
                        imagefilledrectangle($this->img, $x[$i-1]-1, $y[$i-1]-1, $x[$i-1]+2, $y[$i-1]+2, $this->color['line']);
                    }
                }

            }
        }


        /**
        * Draw graphic: PIE or DONUT
        */
        else if (preg_match("/^(5|6)$/", $this->type))
        {
            $center_x = ($this->graphic_area_x1 + $this->graphic_area_x2) / 2;
            $center_y = ($this->graphic_area_y1 + $this->graphic_area_y2) / 2;
            $width    = $this->graphic_area_width;
            $height   = $this->graphic_area_height;
            $start    = 0;
            $sizes    = array();

            foreach ($this->x as $i => $parameter)
            {
                $size    = $this->y[$i] * 360 / $this->sum_total;
				$size = round2($size, 0);
                $sizes[] = $size;
                $start  += $size;
            }
            $start = 270;

            // Draw PIE
            if ($this->type == 5)
            {
                // Draw shadow
                foreach ($sizes as $i => $size)
                {
                    $num_color = $i + 1;
                    while ($num_color > 7) 
                    {
                        $num_color -= 5;
                    }
                    $color = 'arc_' . $num_color . '_shadow';

                    for ($i = $this->h3d; $i >= 0; $i--)
                    {
                        if ($size >= 1)
                        	imagefilledarc($this->img, $center_x, ($center_y+$i), $width, $height, $start, ($start+$size), $this->color[$color], IMG_ARC_NOFILL);
                    }
                    $start += $size;
                }

                $start = 270;

                // Draw pieces
                foreach ($sizes as $i => $size)
                {
                    $num_color = $i + 1;
                    while ($num_color > 7) 
                    {
                        $num_color -= 5;
                    }
                    $color = 'arc_' . $num_color;
                    if ($size >= 1)
                    	imagefilledarc($this->img, $center_x, $center_y, ($width+2), ($height+2), $start, ($start+$size), $this->color[$color], IMG_ARC_EDGED);
                    $start += $size;
                }
            }

            // Draw DONUT
            else if ($this->type == 6)
            {
                foreach ($sizes as $i => $size)
                {
                    $num_color = $i + 1;
                    while ($num_color > 7) 
                    {
                        $num_color -= 5;
                    }
                    $color        = 'arc_' . $num_color;
                    $color_shadow = 'arc_' . $num_color . '_shadow';
                    if ($size >= 1)
                    	imagefilledarc($this->img, $center_x, $center_y, $width, $height, $start, ($start+$size), $this->color[$color], IMG_ARC_PIE);
                    $start += $size;
                }
                imagefilledarc($this->img, $center_x, $center_y, 100, 100, 0, 360, $this->color['background'], IMG_ARC_PIE);
                imagearc($this->img, $center_x, $center_y, 100, 100, 0, 360, $this->color['bg_legend']);
                imagearc($this->img, $center_x, $center_y, ($width+1), ($height+1), 0, 360, $this->color['bg_legend']);
            }
        }


        if ($this->credits == true) 
        {
            $this->draw_credits();
        }
		if ($save != "")
			imagepng($this->img, $save);
		else
		{
        	header('Content-type: image/png');
        	imagepng($this->img);
        }	
        imagedestroy($this->img);
    }

    function calculate_width()
    {
        switch ($this->type)
        {
            // Vertical bars
            case 1:
                $this->legend_box_width   = ($this->legend_exists == true) ? ($this->string_width($this->biggest_graphic_name, $this->tsize) + 25) : 0;
                $this->graphic_area_width = ($this->space_between_bars * $this->total_parameters) + 30;
                $this->graphic_area_x1   += $this->string_width(($this->higher_value_str), $this->size);
                $this->width += $this->graphic_area_x1 + 20;
                $this->width += ($this->legend_exists == true) ? 50 : ((7 * strlen($this->axis_x)) + 10);
                break;

            // Horizontal bars
            case 2:
                $this->legend_box_width   = ($this->legend_exists == true) ? ($this->string_width($this->biggest_graphic_name, $this->size) + 25) : 0;
                $this->graphic_area_width = ($this->string_width($this->higher_value_str, $this->size) > 50) ? (5 * ($this->string_width($this->higher_value_str, $this->size)) * 0.85) : 200;
                $this->graphic_area_x1 += 7 * strlen($this->biggest_x);
                $this->width += ($this->legend_exists == true) ? 60 : ((7 * strlen($this->axis_y)) + 30);
                $this->width += $this->graphic_area_x1;
                break;

            // Dots
            case 3:
                $this->legend_box_width   = ($this->legend_exists == true) ? ($this->string_width($this->biggest_graphic_name, $this->size) + 25) : 0;
                $this->graphic_area_width = ($this->space_between_dots * $this->total_parameters) - 10;
                $this->graphic_area_x1   += $this->string_width(($this->higher_value_str), $this->size);
                $this->width += $this->graphic_area_x1 + 20;
                $this->width += ($this->legend_exists == true) ? 40 : ((7 * strlen($this->axis_x)) + 30);
                break;

            // Lines
            case 4:
                $this->legend_box_width   = ($this->legend_exists == true) ? ($this->string_width($this->biggest_graphic_name, $this->size) + 25) : 0;
                $this->graphic_area_width = ($this->space_between_dots * $this->total_parameters) - 10;
                $this->graphic_area_x1   += $this->string_width(($this->higher_value_str), $this->size);
                $this->width += $this->graphic_area_x1 + 20;
                $this->width += ($this->legend_exists == true) ? 40 : ((7 * strlen($this->axis_x)) + 30);
                break;

            // Pie
            case 5:
                $this->legend_box_width   = $this->string_width($this->biggest_x, $this->size) + 85;
                $this->graphic_area_width = 200;
                $this->width += 90;
                break;

            // Donut
            case 6:
                $this->legend_box_width   = $this->string_width($this->biggest_x, $this->size) + 85;
                $this->graphic_area_width = 180;
                $this->width += 90;
                break;
        }

        $this->graphic_area_width = max($this->graphic_area_width, $this->string_width($this->title, $this->size));
        $this->width += $this->graphic_area_width;
        $this->width += $this->legend_box_width;


        $this->graphic_area_x2 = $this->graphic_area_x1 + $this->graphic_area_width;
        $this->legend_box_x1   = $this->graphic_area_x2 + 40;
        $this->legend_box_x2   = $this->legend_box_x1 + $this->legend_box_width;
    }

    function calculate_height()
    {
        switch ($this->type)
        {
            // Vertical bars
            case 1:
                $this->legend_box_height   = ($this->graphic_2_exists == true) ? 40 : 0;
                $this->graphic_area_height = 150;
                $this->height += 65;
                break;

            // Horizontal bars
            case 2:
                $this->legend_box_height   = ($this->graphic_2_exists == true) ? 40 : 0;
                $this->graphic_area_height = ($this->space_between_bars * $this->total_parameters) + 10;
                $this->height += 65;
                break;

            // Dots
            case 3:
                $this->legend_box_height   = ($this->graphic_2_exists == true) ? 40 : 0;
                $this->graphic_area_height = 150;
                $this->height += 65;
                break;

            // Lines
            case 4:
                $this->legend_box_height   = ($this->graphic_2_exists == true) ? 40 : 0;
                $this->graphic_area_height = 150;
                $this->height += 65;
                break;

            // Pie
            case 5:
                $this->legend_box_height   = (!empty($this->axis_x)) ? 30 : 5;
                $this->legend_box_height  += (14 * $this->total_parameters);
                $this->graphic_area_height = 150;
                $this->height += 50;
                break;

            // Donut
            case 6:
                $this->legend_box_height   = (!empty($this->axis_x)) ? 30 : 5;
                $this->legend_box_height  += (14 * $this->total_parameters);
                $this->graphic_area_height = 180;
                $this->height += 50;
                break;
        }

        $this->height += $this->height_title;
        $this->height += ($this->legend_box_height > $this->graphic_area_height) ? ($this->legend_box_height - $this->graphic_area_height) : 0;
        $this->height += $this->graphic_area_height;

        $this->graphic_area_y2 = $this->graphic_area_y1 + $this->graphic_area_height;
        $this->legend_box_y1   = $this->graphic_area_y1 + 10;
        $this->legend_box_y2   = $this->legend_box_y1 + $this->legend_box_height;
    }

    function draw_legend()
    {
        $x1 = $this->legend_box_x1;
        $y1 = $this->legend_box_y1;
        $x2 = $this->legend_box_x2;
        $y2 = $this->legend_box_y2;

        imagefilledrectangle($this->img, $x1, $y1, $x2, $y2, $this->color['bg_legend']);

        $x = $x1 + 5;
        $y = $y1 + 5;


        // Draw legend values for VERTICAL BARS, HORIZONTAL BARS, DOTS and LINES
        if (preg_match("/^(1|2|3|4)$/", $this->type))
        {
            $color_1 = (preg_match("/^(1|2)$/", $this->type)) ? $this->color['bars']   : $this->color['line'];
            $color_2 = (preg_match("/^(1|2)$/", $this->type)) ? $this->color['bars_2'] : $this->color['line_2'];

            imagefilledrectangle($this->img, $x, $y, ($x+10), ($y+10), $color_1);
            imagerectangle($this->img, $x, $y, ($x+10), ($y+10), $this->color['title']);
            $this->_imagestring($this->img, $this->size, ($x+15), ($y-2), $this->graphic_1, $this->color['axis_values']);
            $y += 20;
            imagefilledrectangle($this->img, $x, $y, ($x+10), ($y+10), $color_2);
            imagerectangle($this->img, $x, $y, ($x+10), ($y+10), $this->color['title']);
            $this->_imagestring($this->img, $this->size, ($x+15), ($y-2), $this->graphic_2, $this->color['axis_values']);
        }

        // Draw legend values for PIE or DONUT
        else if (preg_match("/^(5|6)$/", $this->type))
        {
            if (!empty($this->axis_x))
            {
                $this->_imagestring($this->img, $this->size, ((($x1+$x2)/2) - (strlen($this->axis_x)*7/2)), $y, 
                	$this->axis_x." (".$this->graphic_1.")", $this->color['title']);
                $y += 25;
            }

            $num = 1;

            foreach ($this->x as $i => $parameter)
            {
                while ($num > 7) 
                {
                    $num -= 5;
                }
                $color = 'arc_' . $num;

                $percent = number_format(round(($this->y[$i] * 100 / $this->sum_total), 2), 2, ".", "") . ' %';
                $less    = (strlen($percent) * 7);

                if ($num != 1) 
                {
                    imageline($this->img, ($x1+15), ($y-2), ($x2-5), ($y-2), $this->color['bg_lines']);
                }
                imagefilledrectangle($this->img, $x, $y, ($x+10), ($y+10), $this->color[$color]);
                imagerectangle($this->img, $x, $y, ($x+10), ($y+10), $this->color['title']);
                $this->_imagestring($this->img, $this->size, ($x+15), ($y-2), $parameter, $this->color['axis_values']);
                $this->_imagestring($this->img, $this->size, ($x2-$less), ($y-2), $percent, $this->color['axis_values']);
                $y += 14;
                $num++;
            }
        }
    }


    function string_width($string, $size) 
    {
        $single_width = $size + 4;
        if ($this->encoding == "UTF-8")
        	$width = mb_strlen($string, "UTF-8");
        else	
        	$width = strlen($string);
        return $single_width * $width;
    }

    function string_height($size) 
    {
        if ($size <= 1) 
        {
            $height = 8;
        } 
        else if ($size <= 3) 
        {
            $height = 12;
        } 
        else if ($size >= 4) 
        {
            $height = 14;
        }
        return $height;
    }

    function calculate_higher_value() 
    {
        $digits   = strlen(round($this->biggest_y));
        $interval = pow(10, ($digits-1));
        $this->higher_value     = round(($this->biggest_y - ($this->biggest_y % $interval) + $interval), 1);
        $this->higher_value_str = $this->number_formated($this->higher_value, $this->dec1);
    }

    function number_formated($number, $dec_size = 1)
    {
        if ($this->latin_notation == true) 
            return number_format(round($number, $dec_size), $dec_size, ",", ".");
       	return number_format(round($number, $dec_size), $dec_size, ".", ",");
    }

    function number_float($number)
    {
        if ($this->latin_notation == true) 
            $number = str_replace(".", "", $number);
       	return (float)str_replace(",", "", $number);
    }


    function draw_credits() 
    {
        $this->_imagestring($this->img, $this->size - 2, ($this->width-120), ($this->height-10), "Powered by Carlos Reche", $this->color['title']);
    }


    function load_color_palette()
    {
        switch ($this->skin)
        {
            // Office
            case 1:
                $this->color['title']       = imagecolorallocate($this->img,  40,  70,  130);
                $this->color['background']  = imagecolorallocate($this->img, 255, 255, 255);
                $this->color['axis_values'] = imagecolorallocate($this->img,  50,  50,  50);
                $this->color['axis_line']   = imagecolorallocate($this->img, 100, 100, 100);
                $this->color['bg_lines']    = imagecolorallocate($this->img, 220, 220, 220);
                $this->color['bg_legend']   = imagecolorallocate($this->img, 255, 255, 255);

                if (preg_match("/^(1|2)$/", $this->type))
                {
                    $this->color['bars']          = imagecolorallocate($this->img, 100, 150, 200);
                    $this->color['bars_shadow']   = imagecolorallocate($this->img,  50, 100, 150);
                    $this->color['bars_2']        = imagecolorallocate($this->img, 200, 250, 150);
                    $this->color['bars_2_shadow'] = imagecolorallocate($this->img, 120, 170,  70);
                }
                else if (preg_match("/^(3|4)$/", $this->type))
                {
                    $this->color['line']   = imagecolorallocate($this->img, 100, 150, 200);
                    $this->color['line_2'] = imagecolorallocate($this->img, 230, 100, 100);
                }
                else if (preg_match("/^(5|6)$/", $this->type))
                {
                    $this->color['arc_1']        = imagecolorallocate($this->img, 255, 150,   0);
                    $this->color['arc_2']        = imagecolorallocate($this->img, 150,   0, 255);
                    $this->color['arc_3']        = imagecolorallocate($this->img,   0, 255, 255);
                    $this->color['arc_4']        = imagecolorallocate($this->img, 255,   0,   0);
                    $this->color['arc_5']        = imagecolorallocate($this->img,   0, 255,   0);
                    $this->color['arc_6']        = imagecolorallocate($this->img,   0,   0, 255);
                    $this->color['arc_7']        = imagecolorallocate($this->img, 255, 255,   0);
                    $this->color['arc_1_shadow'] = imagecolorallocate($this->img, 127,  75,   0);
                    $this->color['arc_2_shadow'] = imagecolorallocate($this->img,  75,   0, 127);
                    $this->color['arc_3_shadow'] = imagecolorallocate($this->img,   0, 127, 127);
                    $this->color['arc_4_shadow'] = imagecolorallocate($this->img, 127,   0,   0);
                    $this->color['arc_5_shadow'] = imagecolorallocate($this->img,   0, 127,   0);
                    $this->color['arc_6_shadow'] = imagecolorallocate($this->img,   0,   0, 127);
                    $this->color['arc_7_shadow'] = imagecolorallocate($this->img, 127, 127,   0);
                }
                break;

            // Matrix
            case 2:
                $this->color['title']       = imagecolorallocate($this->img, 255, 255, 255);
                $this->color['background']  = imagecolorallocate($this->img,   0,   0,   0);
                $this->color['axis_values'] = imagecolorallocate($this->img,   0, 230,   0);
                $this->color['axis_line']   = imagecolorallocate($this->img,   0, 200,   0);
                $this->color['bg_lines']    = imagecolorallocate($this->img, 100, 100, 100);
                $this->color['bg_legend']   = imagecolorallocate($this->img,  70,  70,  70);

                if (preg_match("/^(1|2)$/", $this->type))
                {
                    $this->color['bars']          = imagecolorallocate($this->img,  50, 200,  50);
                    $this->color['bars_shadow']   = imagecolorallocate($this->img,   0, 150,   0);
                    $this->color['bars_2']        = imagecolorallocate($this->img, 255, 255, 255);
                    $this->color['bars_2_shadow'] = imagecolorallocate($this->img, 220, 220, 220);
                }
                else if (preg_match("/^(3|4)$/", $this->type))
                {
                    $this->color['line']   = imagecolorallocate($this->img, 220, 220, 220);
                    $this->color['line_2'] = imagecolorallocate($this->img,   0, 180,   0);
                }
                else if (preg_match("/^(5|6)$/", $this->type))
                {
                    $this->color['arc_1']        = imagecolorallocate($this->img, 255, 255, 255);
                    $this->color['arc_2']        = imagecolorallocate($this->img, 200, 220, 200);
                    $this->color['arc_3']        = imagecolorallocate($this->img, 160, 200, 160);
                    $this->color['arc_4']        = imagecolorallocate($this->img, 135, 180, 135);
                    $this->color['arc_5']        = imagecolorallocate($this->img, 115, 160, 115);
                    $this->color['arc_6']        = imagecolorallocate($this->img, 100, 140, 100);
                    $this->color['arc_7']        = imagecolorallocate($this->img,  90, 120,  90);
                    $this->color['arc_1_shadow'] = imagecolorallocate($this->img, 127, 127, 127);
                    $this->color['arc_2_shadow'] = imagecolorallocate($this->img, 100, 110, 100);
                    $this->color['arc_3_shadow'] = imagecolorallocate($this->img,  80, 100,  80);
                    $this->color['arc_4_shadow'] = imagecolorallocate($this->img,  67,  90,  67);
                    $this->color['arc_5_shadow'] = imagecolorallocate($this->img,  57,  80,  57);
                    $this->color['arc_6_shadow'] = imagecolorallocate($this->img,  50,  70,  50);
                    $this->color['arc_7_shadow'] = imagecolorallocate($this->img,  45,  60,  45);
                }
                break;


            // Spring
            case 3:
                $this->color['title']       = imagecolorallocate($this->img, 250,  50,  50);
                $this->color['background']  = imagecolorallocate($this->img, 255, 255, 255);
                $this->color['axis_values'] = imagecolorallocate($this->img,  50, 150,  50);
                $this->color['axis_line']   = imagecolorallocate($this->img,  50, 100,  50);
                $this->color['bg_lines']    = imagecolorallocate($this->img, 200, 224, 180);
                //$this->color['bg_legend']   = imagecolorallocate($this->img, 230, 230, 200);
                $this->color['bg_legend']   = imagecolorallocate($this->img, 255, 255, 255);

                if (preg_match("/^(1|2)$/", $this->type))
                {
                    $this->color['bars']          = imagecolorallocate($this->img, 255, 170,  80);
                    $this->color['bars_shadow']   = imagecolorallocate($this->img, 200, 120,  30);
                    $this->color['bars_2']        = imagecolorallocate($this->img, 250, 230,  80);
                    $this->color['bars_2_shadow'] = imagecolorallocate($this->img, 180, 150,   0);
                }
                else if (preg_match("/^(3|4)$/", $this->type))
                {
                    $this->color['line']   = imagecolorallocate($this->img, 230, 100,   0);
                    $this->color['line_2'] = imagecolorallocate($this->img, 220, 200,  50);
                }
                else if (preg_match("/^(5|6)$/", $this->type))
                {
                    $this->color['arc_1']        = imagecolorallocate($this->img, 100, 150, 200);
                    $this->color['arc_2']        = imagecolorallocate($this->img, 200, 250, 150);
                    $this->color['arc_3']        = imagecolorallocate($this->img, 250, 200, 150);
                    $this->color['arc_4']        = imagecolorallocate($this->img, 250, 150, 150);
                    $this->color['arc_5']        = imagecolorallocate($this->img, 250, 250, 150);
                    $this->color['arc_6']        = imagecolorallocate($this->img, 230, 180, 250);
                    $this->color['arc_7']        = imagecolorallocate($this->img, 200, 200, 150);
                    $this->color['arc_1_shadow'] = imagecolorallocate($this->img,  50,  75, 100);
                    $this->color['arc_2_shadow'] = imagecolorallocate($this->img, 100, 125,  75);
                    $this->color['arc_3_shadow'] = imagecolorallocate($this->img, 125, 100,  75);
                    $this->color['arc_4_shadow'] = imagecolorallocate($this->img, 125,  75,  75);
                    $this->color['arc_5_shadow'] = imagecolorallocate($this->img, 125, 125,  75);
                    $this->color['arc_6_shadow'] = imagecolorallocate($this->img, 115,  90, 125);
                    $this->color['arc_7_shadow'] = imagecolorallocate($this->img, 100, 100,  75);
                }
                break;

        }
    }

    function _imagestring($img, $size, $x, $y, $string, $col, $alt=0)
    {
		if ($this->encoding != 'UTF-8') {
			if (function_exists('iconv'))
				$string = iconv($this->encoding, 'UTF-8', $string);
			else
				$string = mb_convert_encoding($string, 'UTF-8', $this->encoding);
		}
		
		// Handling ev. RTL languages
		if ($alt)
		{
			if ($this->encoding == 'UTF-8' && is_arabic($string))
				$alt_len = 18;
			else
				$alt_len = 12;
			if (strlen($string) > $alt_len)
   				$string = substr($string, 0, $alt_len);
   		}		
		if ($this->encoding == 'UTF-8')
		{
			if (is_arabic($string))
				$string = arabic($string, "we");
			elseif (is_hebrew($string))
				$string = hebrew($string);
		}

    	if ($this->built_in)
    	{
    		imagestring($img, $size, $x, $y + $alt, $string, $col);
    	}	
    	else	
    	{
    		if ($size == 1)
    			$size = 7;
    		elseif ($size == 2)
    			$size = 8;
    		elseif ($size == 3)
    			$size = 9;
    		elseif ($size == 4)
    			$size = 11;
    		else
    			$size = 12;
    		$y += $size + 3;	
			$angle = 0;

			if ($this->encoding == 'UTF-8' && is_arabic($string))
			{
				$size += 2;
				$string = str_replace(" ", "  ", $string);
			}	
   			
   			imagettftext($img, $size, $angle, $x, $y + $alt, $col, $this->fontfile, $string);
    	}
    }	
}

// The following is for handling RTL texts. GD does not handle RTL at all.
// The function, arabic, has been taken from Milad Rastian and
// modified by Bagram Siadat.
// The function has been further modified and several bugs are fixed.

function is_arabic($text)
{
	return preg_match('/\p{Arabic}/u', $text);
} 

function is_hebrew($text)
{
	return preg_match('/\p{Hebrew}/u', $text);
} 

function utf8_strlen($str) 
{
	return preg_match_all('/[\x00-\x7F\xC0-\xFD]/', $str, $dummy);
}

function utf8_chr($uni) 
{
	$r = "";
    # ASCII range (including control chars)
    if ( ($uni >= 0) && ($uni <= 0x007f) ) 
            $r .= chr($uni);
	elseif ($uni <= 0x07ff) // 2 byte sequence
 	{
		$r .= chr(0xc0 | ($uni >> 6));
		$r .= chr(0x80 | ($uni & 0x003f));
	} 
	elseif($uni == 0xFEFF) // Byte order mark (skip)
		return chr(0); // nop -- zap the BOM
	elseif ($uni >= 0xD800 && $uni <= 0xDFFF) // Test for illegal surrogates 
		return chr(0); // found a surrogate
	elseif ($uni <= 0xffff) // 3 byte sequence 
	{
		$r .= chr(0xe0 | ($uni >> 12));
		$r .= chr(0x80 | (($uni >> 6) & 0x003f));
		$r .= chr(0x80 | ($uni & 0x003f));
	} 
	elseif ($uni <= 0x10ffff) // 4 byte sequence 
	{
		$r .= chr(0xf0 | ($uni >> 18));
		$r .= chr(0x80 | (($uni >> 12) & 0x3f));
		$r .= chr(0x80 | (($uni >> 6) & 0x3f));
		$r .= chr(0x80 | ($uni & 0x3f));
	} 
	else 
		return chr(0);
	return $r;
}

///
function arabic($str,$z="",$method='normal')
{
$p_chars = array (
    utf8_chr(0x0622) => array (utf8_chr(0xfe82), utf8_chr(0xfe82), utf8_chr(0x0622)),
    utf8_chr(0x0627) => array (utf8_chr(0xfe8e), utf8_chr(0xfe8e), utf8_chr(0x0627)),
    utf8_chr(0x0628) => array (utf8_chr(0xfe90), utf8_chr(0xfe92), utf8_chr(0xfe91)),
    utf8_chr(0x067e) => array (utf8_chr(0xfb57), utf8_chr(0xfb59), utf8_chr(0xfb58)),
    utf8_chr(0x062a) => array (utf8_chr(0xfe96), utf8_chr(0xfe98), utf8_chr(0xfe97)),
    utf8_chr(0x062b) => array (utf8_chr(0xfe9a), utf8_chr(0xfe9c), utf8_chr(0xfe9b)),
    utf8_chr(0x062c) => array (utf8_chr(0xfe9e), utf8_chr(0xfea0), utf8_chr(0xfe9f)),
    utf8_chr(0x0686) => array (utf8_chr(0xfb7b), utf8_chr(0xfb7d), utf8_chr(0xfb7c)),
    utf8_chr(0x062d) => array (utf8_chr(0xfea2), utf8_chr(0xfea4), utf8_chr(0xfea3)),
    utf8_chr(0x062e) => array (utf8_chr(0xfea6), utf8_chr(0xfea8), utf8_chr(0xfea7)),
    utf8_chr(0x062f) => array (utf8_chr(0xfeaa), utf8_chr(0xfeaa), utf8_chr(0xfea9)),
    utf8_chr(0x0630) => array (utf8_chr(0xfeac), utf8_chr(0xfeac), utf8_chr(0xfeab)),
    utf8_chr(0x0631) => array (utf8_chr(0xfeae), utf8_chr(0xfeae), utf8_chr(0xfead)),
    utf8_chr(0x0632) => array (utf8_chr(0xfeb0), utf8_chr(0xfeb0), utf8_chr(0xfeaf)),
    utf8_chr(0x0698) => array (utf8_chr(0xfb8b), utf8_chr(0xfb8b), utf8_chr(0xfb8a)),
    utf8_chr(0x0633) => array (utf8_chr(0xfeb2), utf8_chr(0xfeb4), utf8_chr(0xfeb3)),
    utf8_chr(0x0634) => array (utf8_chr(0xfeb6), utf8_chr(0xfeb8), utf8_chr(0xfeb7)),
    utf8_chr(0x0635) => array (utf8_chr(0xfeba), utf8_chr(0xfebc), utf8_chr(0xfebb)),
    utf8_chr(0x0636) => array (utf8_chr(0xfebe), utf8_chr(0xfec0), utf8_chr(0xfebf)),
    utf8_chr(0x0637) => array (utf8_chr(0xfec2), utf8_chr(0xfec4), utf8_chr(0xfec3)),
    utf8_chr(0x0638) => array (utf8_chr(0xfec6), utf8_chr(0xfec8), utf8_chr(0xfec7)),
    utf8_chr(0x0639) => array (utf8_chr(0xfeca), utf8_chr(0xfecc), utf8_chr(0xfecb)),
    utf8_chr(0x063a) => array (utf8_chr(0xfece), utf8_chr(0xfed0), utf8_chr(0xfecf)),
    utf8_chr(0x0641) => array (utf8_chr(0xfed2), utf8_chr(0xfed4), utf8_chr(0xfed3)),
    utf8_chr(0x0642) => array (utf8_chr(0xfed6), utf8_chr(0xfed8), utf8_chr(0xfed7)),
    utf8_chr(0x06a9) => array (utf8_chr(0xfeda), utf8_chr(0xfedc), utf8_chr(0xfedb)),
    utf8_chr(0x06af) => array (utf8_chr(0xfb93), utf8_chr(0xfb95), utf8_chr(0xfb94)),
    utf8_chr(0x0644) => array (utf8_chr(0xfede), utf8_chr(0xfee0), utf8_chr(0xfedf)),
    utf8_chr(0x0645) => array (utf8_chr(0xfee2), utf8_chr(0xfee4), utf8_chr(0xfee3)),
    utf8_chr(0x0646) => array (utf8_chr(0xfee6), utf8_chr(0xfee8), utf8_chr(0xfee7)),
    utf8_chr(0x0648) => array (utf8_chr(0xfeee), utf8_chr(0xfeee), utf8_chr(0xfeed)),
    utf8_chr(0x06cc) => array (utf8_chr(0xfbfd), utf8_chr(0xfbff), utf8_chr(0xfbfe)),
    utf8_chr(0x0643) => array (utf8_chr(0xfeda), utf8_chr(0xfedc), utf8_chr(0xfedb)),
    utf8_chr(0x064a) => array (utf8_chr(0xfef2), utf8_chr(0xfef4), utf8_chr(0xfef3)),
    utf8_chr(0x0623) => array (utf8_chr(0xfe84), utf8_chr(0xfe84), utf8_chr(0xfe83)),
    utf8_chr(0x0624) => array (utf8_chr(0xfe86), utf8_chr(0xfe86), utf8_chr(0xfe85)),
    utf8_chr(0x0625) => array (utf8_chr(0xfe88), utf8_chr(0xfe88), utf8_chr(0xfe87)),
    utf8_chr(0x0626) => array (utf8_chr(0xfe8a), utf8_chr(0xfe8c), utf8_chr(0xfe8b)),
    utf8_chr(0x0629) => array (utf8_chr(0xfe94), utf8_chr(0xfe98), utf8_chr(0xfe97))
);

$nastaligh = array (
    utf8_chr(0x0647) => array (utf8_chr(0xfbab), utf8_chr(0xfbad), utf8_chr(0xfbac))
);

$normal = array (
    utf8_chr(0x0647) => array (utf8_chr(0xfeea), utf8_chr(0xfeec), utf8_chr(0xfeeb))
);

$mp_chars = array (utf8_chr(0x0622), utf8_chr(0x0627), utf8_chr(0x062f), utf8_chr(0x0630), utf8_chr(0x0631), utf8_chr(0x0632), 
	utf8_chr(0x0698), utf8_chr(0x0648), utf8_chr(0x0623), utf8_chr(0x0625), utf8_chr(0x0624));

$ignorelist = array (utf8_chr(0x0000), utf8_chr(0x064c), utf8_chr(0x064d), utf8_chr(0x064b), utf8_chr(0x064f), utf8_chr(0x0650), 
	utf8_chr(0x064e), utf8_chr(0x0651), utf8_chr(0x0653), utf8_chr(0x0670), utf8_chr(0x0654), utf8_chr(0xfe76), utf8_chr(0xfe7a), 
	utf8_chr(0xfe78), utf8_chr(0xfe7c), utf8_chr(0xfe7e), utf8_chr(0xfe74), utf8_chr(0xfe70), utf8_chr(0xfc5e), utf8_chr(0xfc5f), 
	utf8_chr(0xfc60), utf8_chr(0xfc61), utf8_chr(0xfc62), utf8_chr(0xfc63));

	$str_back = $output = $e_output = $str_next = $str1 = $num = "";
	if ($method == 'nastaligh')
		$p_chars = array_merge($p_chars, $nastaligh);
	else
		$p_chars = array_merge($p_chars, $normal);
	$str_len = utf8_strlen($str);
	preg_match_all("/./u", $str, $ar);
	for ($i = 0; $i < $str_len; $i++)
	{
		if (isset($ar[0][$i]))
			$str1 = $ar[0][$i];
		if(isset($ar[0][$i+1]) && in_array($ar[0][$i+1], $ignorelist))
		{
			if (isset($ar[0][$i+2]))
				$str_next = $ar[0][$i+2];
			if ($i == 2) 
				$str_back = $ar[0][$i-2];
			if ($i > 1 && $i != 2) 
				$str_back = $ar[0][$i-1];
		}
		elseif ($i > 0 && isset($ar[0][$i-1]) && !in_array($ar[0][$i-1], $ignorelist))
		{
			if (isset($ar[0][$i+1]))
				$str_next = $ar[0][$i+1];
			if ($i != 0) 
				$str_back = $ar[0][$i-1];
		}
		else
		{
			if (isset($ar[0][$i+1]) && !empty($ar[0][$i+1]))
				$str_next = $ar[0][$i+1];
			elseif ($i > 0 && isset($ar[0][$i-1])) 
				$str_next = $ar[0][$i-1];
			if ($i > 1 && isset($ar[0][$i-2])) 
				$str_back = $ar[0][$i-2];
		}
		if (!in_array($str1,$ignorelist))
		{
			if (array_key_exists($str1,$p_chars))
			{
				if (!$str_back || $str_back==" " || !array_key_exists($str_back,$p_chars))
				{
					if (!array_key_exists($str_back, $p_chars) && !array_key_exists($str_next, $p_chars)) 
						$output = $str1.$output;
					else 
						$output = $p_chars[$str1][2].$output;
					continue;
				}
				elseif (array_key_exists($str_next, $p_chars) && array_key_exists($str_back, $p_chars))
				{
					if (in_array($str_back, $mp_chars) && array_key_exists($str_next, $p_chars))
						$output = $p_chars[$str1][2].$output;
					else
						$output = $p_chars[$str1][1].$output;
					continue;
				}
				elseif (array_key_exists($str_back, $p_chars) && !array_key_exists($str_next, $p_chars))
				{
					if (in_array($str_back, $mp_chars))
						$output = $str1.$output;
					else
						$output = $p_chars[$str1][0].$output;
					continue;
				}
			}
			elseif ($z == "we")
			{
				$number = array('0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
					"\xD9\xA0", "\xD9\xA1", "\xD9\xA2", "\xD9\xA3", "\xD9\xA4", "\xD9\xA5", "\xD9\xA6", 
					"\xD9\xA7", "\xD9\xA8", "\xD9\xA9",	"\xDB\xB0", "\xDB\xB1", "\xDB\xB2", "\xDB\xB3", 
					"\xDB\xB4", "\xDB\xB5", "\xDB\xB6", "\xDB\xB7", "\xDB\xB8", "\xDB\xB9");
				if (in_array($str1, $number))
				{
					$num .= $str1;
					$str1 = "";
				}
				if (!in_array($str_next, $number))
				{
					$str1 .= $num;
					$num = "";
				}
				//$output = $str1.$output;
				$output = $output.$str1;
			}
			elseif ($z == "fa")
			{
				$number = array (utf8_chr(0x0660), utf8_chr(0x0661), utf8_chr(0x0662), 
					utf8_chr(0x0663), utf8_chr(0x0664), utf8_chr(0x0665), utf8_chr(0x0666), 
					utf8_chr(0x0667), utf8_chr(0x0668), utf8_chr(0x0669), utf8_chr(0x06f4), 
					utf8_chr(0x06f5), utf8_chr(0x06f6), utf8_chr(0x0030), utf8_chr(0x0031), 
					utf8_chr(0x0032), utf8_chr(0x0033), utf8_chr(0x0034), utf8_chr(0x0035), 
					utf8_chr(0x0036), utf8_chr(0x0037), utf8_chr(0x0038), utf8_chr(0x0039));
				switch ($str1)
				{
					case ")" : 
						$str1 = "("; 
						break;
					case "(" : 
						$str1 = ")"; 
						break;
					case "}" : 
						$str1 = "{"; 
						break;
					case "{" : 
						$str1 = "}"; 
						break;
					case "]" : 
						$str1 = "["; 
						break;
					case "[" : 
						$str1 = "]"; 
						break;
					case ">" : 
						$str1 = "<"; 
						break;
					case "<" : 
						$str1 = ">"; 
						break;
				}
				if (in_array($str1, $number))
				{
					$num .= $str1;
					$str1 = "";
				}
				if (!in_array($str_next, $number))
				{
					$str1 .= $num;
					$num = "";
				}
				$output = $str1.$output;
			}
			else
			{
				if (($str1 == utf8_chr(0x060c)) || ($str1 == utf8_chr(0x061f)) || ($str1 == utf8_chr(0x0621)) || 
					(array_key_exists($str_next, $p_chars) && array_key_exists($str_back, $p_chars)) || 
					($str1 == " " && array_key_exists($str_back, $p_chars)) || ($str1 == " " && 
					array_key_exists($str_next, $p_chars)))
				{
					if ($e_output)
					{
						$output = $e_output.$output;
						$e_output = "";
					}
					$output = $str1.$output;
				}
				else
				{
					$e_output .= $str1;
					if (array_key_exists($str_next, $p_chars) || $str_next == "")
					{
						$output = $e_output.$output;
						$e_output = "";
					}
				}
			}
		}
		else
			$output = $str1.$output;
		$str_next = null;
		$str_back = null;
	}
	return  $output;
}

