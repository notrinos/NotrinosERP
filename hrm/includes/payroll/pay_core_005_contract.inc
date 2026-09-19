<?php
/**
 * PAY-CORE-005 pure deterministic calculation contract.
 *
 * No database, session, request, filesystem, network, GL, payment or mutable
 * runtime state is accessed from this file. All business facts must arrive as
 * explicit sealed/canonical inputs.
 */
if (!defined('HRM_PAY_CORE_005_CONTRACT_VERSION'))
    define('HRM_PAY_CORE_005_CONTRACT_VERSION', '1.0');
if (!defined('HRM_PAY_CORE_005_PRODUCTION_CALCULATION_ALLOWED'))
    define('HRM_PAY_CORE_005_PRODUCTION_CALCULATION_ALLOWED', false);
if (!defined('HRM_PAY_CORE_005_ACCOUNTING_AUTHORITY_ALLOWED'))
    define('HRM_PAY_CORE_005_ACCOUNTING_AUTHORITY_ALLOWED', false);
if (!defined('HRM_PAY_CORE_005_PAYMENT_AUTHORITY_ALLOWED'))
    define('HRM_PAY_CORE_005_PAYMENT_AUTHORITY_ALLOWED', false);
if (!defined('HRM_PAY_CORE_005_HISTORICAL_MUTATION_ALLOWED'))
    define('HRM_PAY_CORE_005_HISTORICAL_MUTATION_ALLOWED', false);

function hrm_pay_core_005_is_sha256($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
}

function hrm_pay_core_005_token($value, $max=128)
{
    return is_string($value) && strlen($value) >= 1 && strlen($value) <= $max
        && preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:-]*$/D', $value) === 1 ? $value : false;
}

function hrm_pay_core_005_canonicalize($value)
{
    if (!is_array($value))
        return $value;
    if (count($value) === 0)
        return array();
    $is_list = array_keys($value) === range(0, count($value)-1);
    if ($is_list) {
        $out = array();
        foreach ($value as $item)
            $out[] = hrm_pay_core_005_canonicalize($item);
        return $out;
    }
    ksort($value, SORT_STRING);
    foreach ($value as $key=>$item)
        $value[$key] = hrm_pay_core_005_canonicalize($item);
    return $value;
}

function hrm_pay_core_005_canonical_json($value)
{
    $json = json_encode(hrm_pay_core_005_canonicalize($value), JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE|JSON_PRESERVE_ZERO_FRACTION);
    return is_string($json) ? $json : false;
}

function hrm_pay_core_005_hash($value)
{
    $json = hrm_pay_core_005_canonical_json($value);
    return $json === false ? false : hash('sha256', $json);
}

function hrm_pay_core_005_resource_limits()
{
    return array(
        'max_nodes'=>256,
        'max_dependencies_per_node'=>32,
        'max_plan_bytes'=>1048576,
        'max_trace_rows'=>512,
        'max_solver_iterations'=>64,
        'max_decimal_integer_digits'=>24,
        'max_scenario_assignments'=>64,
    );
}

function hrm_pay_core_005_scenarios()
{
    return array(
        'regular','off_cycle','supplemental','retroactive_adjustment','correction',
        'reversal','termination_final_pay','zero_pay','negative_adjustment',
        'multi_assignment','multi_relationship'
    );
}

/** Fixed-scale decimal, six internal fractional digits, half-away-from-zero. */
class HRM_Pay_Core_005_Decimal
{
    const SCALE = 6;

    public static function normalize($value)
    {
        if (is_int($value)) $value=(string)$value;
        if (!is_string($value) || !preg_match('/^-?(?:0|[1-9][0-9]*)(?:\.[0-9]{1,6})?$/D',$value))
            throw new RuntimeException('pc5_decimal_invalid');
        $negative = $value[0] === '-';
        if ($negative) $value=substr($value,1);
        $parts=explode('.',$value,2);$whole=ltrim($parts[0],'0');if($whole==='')$whole='0';
        $limits=hrm_pay_core_005_resource_limits();
        if (strlen($whole)>$limits['max_decimal_integer_digits'])
            throw new RuntimeException('pc5_decimal_magnitude_limit');
        $frac=isset($parts[1])?$parts[1]:'';$frac=str_pad($frac,self::SCALE,'0',STR_PAD_RIGHT);
        $digits=ltrim($whole.$frac,'0');if($digits==='')$digits='0';if($digits==='0')$negative=false;
        return array('negative'=>$negative,'digits'=>$digits);
    }

    public static function string(array $d)
    {
        $digits=str_pad($d['digits'],self::SCALE+1,'0',STR_PAD_LEFT);$p=strlen($digits)-self::SCALE;
        $out=substr($digits,0,$p).'.'.substr($digits,$p);
        return (!empty($d['negative'])&&$d['digits']!=='0'?'-':'').$out;
    }

    private static function cmpAbs($a,$b)
    {
        $a=ltrim($a,'0');$b=ltrim($b,'0');if($a==='')$a='0';if($b==='')$b='0';
        if(strlen($a)!==strlen($b))return strlen($a)<strlen($b)?-1:1;
        if($a===$b)return 0;return strcmp($a,$b)<0?-1:1;
    }
    private static function addAbs($a,$b)
    {
        $i=strlen($a)-1;$j=strlen($b)-1;$carry=0;$out='';
        while($i>=0||$j>=0||$carry){$n=($i>=0?(ord($a[$i--])-48):0)+($j>=0?(ord($b[$j--])-48):0)+$carry;$out=chr(48+($n%10)).$out;$carry=(int)floor($n/10);} $out=ltrim($out,'0');return $out===''?'0':$out;
    }
    private static function subAbs($a,$b)
    {
        $i=strlen($a)-1;$j=strlen($b)-1;$borrow=0;$out='';
        while($i>=0){$n=(ord($a[$i--])-48)-$borrow-($j>=0?(ord($b[$j--])-48):0);if($n<0){$n+=10;$borrow=1;}else$borrow=0;$out=chr(48+$n).$out;} $out=ltrim($out,'0');return $out===''?'0':$out;
    }
    private static function mulAbs($a,$b)
    {
        if($a==='0'||$b==='0')return '0';$la=strlen($a);$lb=strlen($b);$r=array_fill(0,$la+$lb,0);
        for($i=$la-1;$i>=0;$i--)for($j=$lb-1;$j>=0;$j--){$p=(ord($a[$i])-48)*(ord($b[$j])-48)+$r[$i+$j+1];$r[$i+$j+1]=$p%10;$r[$i+$j]+=(int)floor($p/10);} $s=ltrim(implode('',$r),'0');return $s===''?'0':$s;
    }
    private static function divmodAbs($n,$d)
    {
        if($d==='0')throw new RuntimeException('pc5_decimal_divide_by_zero');$q='';$r='0';
        for($i=0,$l=strlen($n);$i<$l;$i++){$r=ltrim(($r==='0'?'':$r).$n[$i],'0');if($r==='')$r='0';$digit=0;while(self::cmpAbs($r,$d)>=0){$r=self::subAbs($r,$d);$digit++;}$q.=chr(48+$digit);} $q=ltrim($q,'0');return array($q===''?'0':$q,$r);
    }
    private static function roundedQuotient($n,$d)
    {
        list($q,$r)=self::divmodAbs($n,$d);if(self::cmpAbs(self::mulAbs($r,'2'),$d)>=0)$q=self::addAbs($q,'1');return $q;
    }
    public static function compare(array $a,array $b)
    {
        if($a['negative']!==$b['negative'])return $a['negative']?-1:1;$c=self::cmpAbs($a['digits'],$b['digits']);return $a['negative']?-$c:$c;
    }
    public static function negate(array $a){if($a['digits']!=='0')$a['negative']=!$a['negative'];return $a;}
    public static function abs(array $a){$a['negative']=false;return $a;}
    public static function add(array $a,array $b)
    {
        if($a['negative']===$b['negative'])return array('negative'=>$a['negative'],'digits'=>self::addAbs($a['digits'],$b['digits']));
        $c=self::cmpAbs($a['digits'],$b['digits']);if($c===0)return array('negative'=>false,'digits'=>'0');
        if($c>0)return array('negative'=>$a['negative'],'digits'=>self::subAbs($a['digits'],$b['digits']));
        return array('negative'=>$b['negative'],'digits'=>self::subAbs($b['digits'],$a['digits']));
    }
    public static function sub(array $a,array $b){return self::add($a,self::negate($b));}
    public static function mul(array $a,array $b)
    {
        $raw=self::mulAbs($a['digits'],$b['digits']);$div='1'.str_repeat('0',self::SCALE);$digits=self::roundedQuotient($raw,$div);
        return array('negative'=>$digits!=='0'&&$a['negative']!==$b['negative'],'digits'=>$digits);
    }
    public static function div(array $a,array $b)
    {
        if($b['digits']==='0')throw new RuntimeException('pc5_decimal_divide_by_zero');$n=$a['digits'].str_repeat('0',self::SCALE);$digits=self::roundedQuotient($n,$b['digits']);
        return array('negative'=>$digits!=='0'&&$a['negative']!==$b['negative'],'digits'=>$digits);
    }
    public static function round(array $a,$places)
    {
        $places=(int)$places;if($places<0||$places>self::SCALE)throw new RuntimeException('pc5_round_places_invalid');$drop=self::SCALE-$places;if($drop===0)return $a;
        $div='1'.str_repeat('0',$drop);$digits=self::roundedQuotient($a['digits'],$div).str_repeat('0',$drop);$digits=ltrim($digits,'0');if($digits==='')$digits='0';
        return array('negative'=>$digits!=='0'&&$a['negative'],'digits'=>$digits);
    }
}

function hrm_pay_core_005_decimal($value)
{
    return HRM_Pay_Core_005_Decimal::normalize($value);
}
function hrm_pay_core_005_decimal_string($value)
{
    return HRM_Pay_Core_005_Decimal::string(is_array($value)?$value:hrm_pay_core_005_decimal($value));
}
function hrm_pay_core_005_decimal_add($a,$b)
{
    return HRM_Pay_Core_005_Decimal::string(HRM_Pay_Core_005_Decimal::add(hrm_pay_core_005_decimal($a),hrm_pay_core_005_decimal($b)));
}
function hrm_pay_core_005_decimal_sub($a,$b)
{
    return HRM_Pay_Core_005_Decimal::string(HRM_Pay_Core_005_Decimal::sub(hrm_pay_core_005_decimal($a),hrm_pay_core_005_decimal($b)));
}
function hrm_pay_core_005_decimal_mul($a,$b)
{
    return HRM_Pay_Core_005_Decimal::string(HRM_Pay_Core_005_Decimal::mul(hrm_pay_core_005_decimal($a),hrm_pay_core_005_decimal($b)));
}
function hrm_pay_core_005_decimal_div($a,$b)
{
    return HRM_Pay_Core_005_Decimal::string(HRM_Pay_Core_005_Decimal::div(hrm_pay_core_005_decimal($a),hrm_pay_core_005_decimal($b)));
}
function hrm_pay_core_005_decimal_compare($a,$b)
{
    return HRM_Pay_Core_005_Decimal::compare(hrm_pay_core_005_decimal($a),hrm_pay_core_005_decimal($b));
}
function hrm_pay_core_005_decimal_abs($a)
{
    return HRM_Pay_Core_005_Decimal::string(HRM_Pay_Core_005_Decimal::abs(hrm_pay_core_005_decimal($a)));
}
function hrm_pay_core_005_round_currency($amount,$minor_units)
{
    $minor_units=(int)$minor_units;if($minor_units<0||$minor_units>6)throw new RuntimeException('pc5_currency_minor_units_invalid');
    return HRM_Pay_Core_005_Decimal::string(HRM_Pay_Core_005_Decimal::round(hrm_pay_core_005_decimal($amount),$minor_units));
}

/** Explicit ratio proration. Numerator/denominator are governed inputs, never inferred. */
function hrm_pay_core_005_prorate($amount,$numerator,$denominator,$minor_units=null)
{
    if(hrm_pay_core_005_decimal_compare($denominator,'0')<=0)throw new RuntimeException('pc5_proration_denominator_invalid');
    if(hrm_pay_core_005_decimal_compare($numerator,'0')<0||hrm_pay_core_005_decimal_compare($numerator,$denominator)>0)throw new RuntimeException('pc5_proration_ratio_invalid');
    $out=hrm_pay_core_005_decimal_mul($amount,hrm_pay_core_005_decimal_div($numerator,$denominator));
    return $minor_units===null?$out:hrm_pay_core_005_round_currency($out,$minor_units);
}

/** Validate and deterministically topologically order a dependency graph. */
function hrm_pay_core_005_topological_order(array $nodes,&$error=null)
{
    $error=null;$limits=hrm_pay_core_005_resource_limits();if(count($nodes)<1||count($nodes)>$limits['max_nodes']){$error='pc5_graph_size_invalid';return false;}
    $by=array();
    foreach($nodes as $node){
        if(!is_array($node)||!isset($node['id'])||hrm_pay_core_005_token($node['id'])===false||!isset($node['dependencies'])||!is_array($node['dependencies'])){$error='pc5_graph_node_invalid';return false;}
        $id=$node['id'];if(isset($by[$id])){$error='pc5_graph_duplicate_node';return false;}if(count($node['dependencies'])>$limits['max_dependencies_per_node']){$error='pc5_graph_dependency_limit';return false;}
        $seen=array();foreach($node['dependencies'] as $dep){if(hrm_pay_core_005_token($dep)===false){$error='pc5_graph_dependency_invalid';return false;}if($dep===$id){$error='pc5_graph_self_dependency';return false;}if(isset($seen[$dep])){$error='pc5_graph_duplicate_dependency';return false;}$seen[$dep]=true;}
        $by[$id]=$node;
    }
    foreach($by as $node)foreach($node['dependencies'] as $dep)if(!isset($by[$dep])){$error='pc5_graph_unknown_dependency';return false;}
    $indegree=array();$children=array();foreach($by as $id=>$node){$indegree[$id]=count($node['dependencies']);$children[$id]=array();}
    foreach($by as $id=>$node)foreach($node['dependencies'] as $dep)$children[$dep][]=$id;
    foreach($children as &$list)sort($list,SORT_STRING);unset($list);
    $ready=array();foreach($indegree as $id=>$n)if($n===0)$ready[]=$id;sort($ready,SORT_STRING);$order=array();
    while($ready){$id=array_shift($ready);$order[]=$id;foreach($children[$id] as $child){$indegree[$child]--;if($indegree[$child]===0){$ready[]=$child;sort($ready,SORT_STRING);}}}
    if(count($order)!==count($by)){$error='pc5_graph_cycle';return false;}return $order;
}
