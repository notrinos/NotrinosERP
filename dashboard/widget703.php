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

$today = Today();

$result = gl_top($today);

$title = _('Class Balances');
$calculated = _('Calculated Return');

$assets = $liabilities = $income = $cost = $total = 0;
while ($myrow = db_fetch($result)) {
	if ($myrow['ctype'] > 3) {
		$total += $myrow['total'];
		if ($myrow['ctype'] == 4)
			$income += abs(-$myrow['total']);
		else
			$cost += abs(-$myrow['total']);
	} elseif ($myrow['ctype'] == 1)
		$assets += $myrow['total'];
	elseif ($myrow['ctype'] == 2)
		$liabilities += $myrow['total'];
}
$return = abs(-$total);
$class_total = $income + $cost + $return;

$chart_id = 'class_balance_' . substr(md5(uniqid('', true)), 0, 10);
$chart_data = array(
	'income' => $income,
	'cost' => $cost,
	'calculated' => $return,
	'total' => $class_total,
	'assets' => abs($assets),
	'liabilities' => abs($liabilities),
	'labels' => array(
		'calculated' => $calculated,
		'income' => _('Income'),
		'cost' => _('Cost'),
		'assets' => _('Assets'),
		'liabilities' => _('Liabilities'),
	),
	'values' => array(
		'calculated' => number_format2($return, user_price_dec()),
		'income' => number_format2($income, user_price_dec()),
		'cost' => number_format2($cost, user_price_dec()),
		'assets' => number_format2(abs($assets), 2),
		'liabilities' => number_format2(abs($liabilities), 2),
	),
);

$json_flags = 0;
if (defined('JSON_UNESCAPED_UNICODE')) $json_flags |= JSON_UNESCAPED_UNICODE;
if (defined('JSON_UNESCAPED_SLASHES')) $json_flags |= JSON_UNESCAPED_SLASHES;
if (defined('JSON_HEX_TAG')) $json_flags |= JSON_HEX_TAG;
if (defined('JSON_HEX_AMP')) $json_flags |= JSON_HEX_AMP;
if (defined('JSON_HEX_APOS')) $json_flags |= JSON_HEX_APOS;
if (defined('JSON_HEX_QUOT')) $json_flags |= JSON_HEX_QUOT;
$chart_json = json_encode($chart_data, $json_flags);

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

if($widget->checkSecurity('SA_GLANALYTIC')) {
	echo "<div class='class-balance-ring-chart' style='position:relative;width:100%;height:100%;min-height:0;'>";
	echo "<canvas id='$chart_id'></canvas>";
	echo "</div>";
	echo "<script>(function(){";
	echo "var c=document.getElementById('$chart_id');if(!c||typeof Chart==='undefined')return;";
	echo "var d=$chart_json;";
	echo "var plugin={id:'classBalanceRing',afterDraw:function(chart){var ctx=chart.ctx,a=chart.chartArea,w=a.right-a.left,h=a.bottom-a.top,total=Math.max(d.total,1),cx=a.left+w*0.29,cy=a.top+h*0.60,r=Math.min(w*0.22,h*0.35),outer=Math.max(16,r*0.22),inner=Math.max(18,r*0.32),icx=cx-1,blue='rgba(54,162,235,0.55)',dark='rgba(10,118,191,1)',track='rgba(54,162,235,0.10)',line='rgba(10,118,191,0.45)',text='#111827';";
	echo "function arc(x,y,rad,lw,pct,col){var s=-Math.PI/2,e=s+Math.max(0,Math.min(1,pct))*Math.PI*2;ctx.beginPath();ctx.arc(x,y,rad,s,e);ctx.lineWidth=lw;ctx.lineCap='butt';ctx.strokeStyle=col;ctx.stroke();}";
	echo "function full(x,y,rad,lw,col){ctx.beginPath();ctx.arc(x,y,rad,0,Math.PI*2);ctx.lineWidth=lw;ctx.strokeStyle=col;ctx.stroke();}";
	echo "function textAt(txt,x,y,sz,bold,align){ctx.fillStyle=text;ctx.font=(bold?'700 ':'400 ')+sz+'px Arial,sans-serif';ctx.textAlign=align||'center';ctx.textBaseline='middle';ctx.fillText(txt,x,y);}";
	echo "ctx.save();ctx.fillStyle=track;ctx.beginPath();ctx.arc(cx,cy,r+outer*0.5,0,Math.PI*2);ctx.fill();full(cx,cy,r,outer,track);arc(cx,cy,r,outer,d.income/total,blue);arc(icx,cy,r*0.58,inner,d.calculated/total,dark);";
	echo "textAt(d.labels.cost,cx,cy-12,12,0);textAt(d.values.cost,cx,cy+18,20,1);";
	echo "var topY=cy-r-52;ctx.strokeStyle=dark;ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(cx,cy-r*0.58);ctx.lineTo(cx,topY+16);ctx.stroke();ctx.fillStyle=dark;ctx.beginPath();ctx.arc(cx,topY+16,4,0,Math.PI*2);ctx.fill();textAt(d.labels.calculated,cx,topY-22,12,0);textAt(d.values.calculated,cx,topY,18,1);";
	echo "var ix=cx+r*0.68,iy=cy-r*0.74,ex=ix+30,ey=iy-26,dx=ex+52;ctx.strokeStyle=line;ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(ix,iy);ctx.lineTo(ex,ey);ctx.lineTo(dx,ey);ctx.stroke();ctx.fillStyle=blue;ctx.beginPath();ctx.arc(dx,ey,4,0,Math.PI*2);ctx.fill();textAt(d.labels.income,dx+42,ey-18,12,0,'center');textAt(d.values.income,dx+42,ey+12,19,1,'center');";
	echo "var sx=a.left+w*0.66,sy=cy+2,gap=w*0.22;textAt(d.values.assets,sx,sy,17,1);textAt(d.labels.assets,sx,sy+30,12,0);textAt(d.values.liabilities,sx+gap,sy,17,1);textAt(d.labels.liabilities,sx+gap,sy+30,12,0);ctx.restore();}};";
	echo "new Chart(c,{type:'doughnut',data:{datasets:[{data:[1],backgroundColor:['rgba(0,0,0,0)'],borderWidth:0}]},options:{responsive:true,maintainAspectRatio:false,animation:false,events:[],plugins:{legend:{display:false},tooltip:{enabled:false}}},plugins:[plugin]});";
	echo "})();</script>";
}

$widget->End();
