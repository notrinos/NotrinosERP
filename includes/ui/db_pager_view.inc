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
//--------------------------------------------------------------------------------------------------
function pager_link($link_text, $url, $icon=false)
{
	global $path_to_root;
	
	if (user_graphic_links() && $icon)
		$link_text = set_icon($icon, $link_text);

	$href = $path_to_root . $url;
	return "<a href='$href'>" . $link_text . "</a>";
}

function navi_button($name, $value, $enabled=true, $icon = false) {
	global $path_to_root;
  	return "<button ". ($enabled ? '':'disabled')
  		." class=\"navibutton\" type=\"submit\""
	    ." name=\"$name\"  id=\"$name\" value=\"$value\">"
	  	.($icon ? "<img src='$path_to_root/themes/".user_theme()."/images/".$icon."'>":'')
		."<span>$value</span></button>\n";
}

function navi_button_cell($name, $value, $enabled=true, $align='left') {
	label_cell(navi_button($name, $value, $enabled), "align='$align'");
}


//-----------------------------------------------------------------------------
//
//    Sql paged table view. Call this function inside form.
//
function display_db_pager(&$pager) {
    global	$path_to_root;

	$pager->select_records();

	div_start("_{$pager->name}_span");
    $headers = array();

	foreach($pager->columns as $num_col=>$col) {
		// record status control column is displayed only when control checkbox is on
    	if (isset($col['head']) && ($col['type']!='inactive' || get_post('show_inactive'))) {
			if (!isset($col['ord']))
				$headers[] = $col['head'];
			else {
	  			$icon = (($col['ord'] == 'desc') ? 'sort_desc.gif' : 
					($col['ord'] == 'asc' ? 'sort_asc.gif' : 'sort_none.gif'));
				$headers[] = navi_button($pager->name.'_sort_'.$num_col, 
					$col['head'], true, $icon);
			}
 		}
	}
    /* show a table of records returned by the sql */
    start_table(TABLESTYLE, "width='$pager->width'");
    table_header($headers);

	if($pager->header_fun) {	// if set header handler
		start_row("class='{$pager->header_class}'");
		$fun = $pager->header_fun;
		if (method_exists($pager, $fun)) { 
			$h = $pager->$fun($pager);
  		} elseif (function_exists($fun)) {
			$h = $fun($pager);
		}
		
		foreach($h as $c) {	 // draw header columns
			$pars = isset($c[1]) ? $c[1] : '';
			label_cell($c[0], $pars);
		}
		end_row();
	}

	$cc = 0; //row colour counter
   	foreach($pager->data as $line_no => $row) {	

        if ($pager->row_fun) {
            if (function_exists($pager->row_fun)) {
                $fn = $pager->row_fun;
                $row = $fn($row);
            }
        }

		$marker = $pager->marker;
	    if ($marker && $marker($row)) 
        	start_row("class='$pager->marker_class'");
	    else	
			alt_table_row_color($cc);
	    foreach ($pager->columns as $k=>$col) {
		   $coltype = $col['type'];
		   $cell = isset($col['name']) ? $row[$col['name']] : '';

		   if (isset($col['fun'])) { // use data input function if defined
		    $fun = $col['fun']; 
		    if (method_exists($pager, $fun)) { 
				$cell = $pager->$fun($row, $cell);
  			} elseif (function_exists($fun)) {
				$cell = $fun($row, $cell);
			} else
				$cell = '';
		   }
		   switch($coltype) { // format column
		    case 'time':
		  	  label_cell($cell, "width='40'"); break;
		    case 'date':
		  	  label_cell(sql2date($cell), "align='center' nowrap"); break;
		    case 'dstamp':	// time stamp displayed as date
		  	  label_cell(sql2date(substr($cell, 0, 10)), "align='center' nowrap"); break;
		    case 'tstamp':	// time stamp - FIX user format
		  	  label_cell(sql2date(substr($cell, 0, 10)).
			  ' '. substr($cell, 10), "align='center'"); break;
		    case 'percent':
		  	  percent_cell($cell); break;
		    case 'amount':
			  if ($cell=='')
			  	label_cell('');
			  else
		  	  	amount_cell($cell, false); break;
		    case 'qty':
			  if ($cell=='')
			  	label_cell('');
			  else
		  	  	qty_cell($cell, false, isset($col['dec']) ? $col['dec'] : null); break;
			case 'email':
				email_cell($cell,isset( $col['align']) ? "align='" . $col['align'] . "'" : null);
				break;
		    case 'rate':
				label_cell(number_format2($cell, user_exrate_dec()), "align=center"); break;
			case 'inactive':
				if(get_post('show_inactive'))
					$pager->inactive_control_cell($row); break;
		    default:
			  if (isset( $col['align']))
			  	  label_cell($cell, "align='" . $col['align'] . "'");
			  else
			  	  label_cell($cell);
		    case 'skip':	// column not displayed
	  	  }
	  }
	    end_row();
	}
	//end of while loop

	if($pager->footer_fun) {	// if set footer handler
		start_row("class='{$pager->footer_class}'");
		$fun = $pager->footer_fun;
		if (method_exists($pager, $fun)) { 
			$h = $pager->$fun($pager);
  		} elseif (function_exists($fun)) {
			$h = $fun($pager);
		}
		
		foreach($h as $c) {	 // draw footer columns
			$pars = isset($c[1]) ? $c[1] : '';
			label_cell($c[0], $pars);
		}
		end_row();
	}

	start_row("class='navibar'");
	$colspan = count($pager->columns);
		$inact = @$pager->inactive_ctrl==true 
			? ' '.checkbox(null, 'show_inactive', null, true). _("Show also Inactive") : '';
	 if($pager->rec_count) {
		echo "<td colspan=$colspan class='navibar' style='border:none;padding:3px;'>";
		echo "<div style='float:right;'>";
		$but_pref = $pager->name.'_page_';
	    start_table();
		start_row();
		if (@$pager->inactive_ctrl) 
				submit('Update', _('Update'), true, '', null); // inactive update
		echo navi_button_cell($but_pref.'first', _('First'), $pager->first_page, 'right');
		echo navi_button_cell($but_pref.'prev', _('Prev'), $pager->prev_page,'right');
		echo navi_button_cell($but_pref.'next', _('Next'), $pager->next_page,'right');
		echo navi_button_cell($but_pref.'last', _('Last'), $pager->last_page, 'right');
		end_row(); 
		end_table();
		echo "</div>";
		$from = ($pager->curr_page-1)*$pager->page_len+1;
		$to = $from + $pager->page_len - 1;
		if ($to > $pager->rec_count)
		  $to = $pager->rec_count;
		$all = $pager->rec_count;
		echo sprintf( _('Records %d-%d of %d'), $from, $to, $all);
		echo $inact;
		echo "</td>";
	} else {
	  	label_cell( _('No records') . $inact, "colspan=$colspan class='navibar'");
	}

	end_row();

	end_table();

   if (isset($pager->marker_txt))
   		display_note($pager->marker_txt, 0, 1, "class='$pager->notice_class'");

  div_end();
  return true;
}


