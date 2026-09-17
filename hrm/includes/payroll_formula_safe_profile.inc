<?php
/**********************************************************************
    PAY-RULE-002 isolated payroll-safe Formula Framework profile.

    This profile is deliberately disconnected from production payroll routing.
    It reuses the shared parser/compiler AST but supplies its own immutable
    typed context, deterministic decimal/date semantics, allowlist and resource
    enforcement. It exposes no variable providers and no DB/file/network/PHP
    or global-state access to formulas.
***********************************************************************/

include_once(dirname(__FILE__).'/../../includes/formula/formula_bootstrap.inc');

if (!defined('HRM_PAYROLL_FORMULA_SAFE_PROFILE_ID'))
    define('HRM_PAYROLL_FORMULA_SAFE_PROFILE_ID', 'notrinoserp.hrm.payroll.formula_safe_profile');
if (!defined('HRM_PAYROLL_FORMULA_SAFE_PROFILE_VERSION'))
    define('HRM_PAYROLL_FORMULA_SAFE_PROFILE_VERSION', '1.0.0');
if (!defined('HRM_PAYROLL_FORMULA_SAFE_PROFILE_INTRODUCED_VERSION'))
    define('HRM_PAYROLL_FORMULA_SAFE_PROFILE_INTRODUCED_VERSION', '1.0.460');
if (!defined('HRM_PAYROLL_FORMULA_SAFE_RUNTIME_VERSION'))
    define('HRM_PAYROLL_FORMULA_SAFE_RUNTIME_VERSION', '1.0.0');

/** Exact variable/type/unit contract. */
function hrm_payroll_formula_safe_variable_contract()
{
    return array(
        'BASIC'=>array('type'=>'decimal','unit'=>'currency'),
        'GROSS'=>array('type'=>'decimal','unit'=>'currency'),
        'DAYS_WORKED'=>array('type'=>'decimal','unit'=>'day'),
        'PAYABLE_DAYS'=>array('type'=>'decimal','unit'=>'day'),
        'WORKING_DAYS'=>array('type'=>'decimal','unit'=>'day'),
        'DAYS_IN_MONTH'=>array('type'=>'decimal','unit'=>'day'),
        'LEAVE_DAYS'=>array('type'=>'decimal','unit'=>'day'),
        'PAID_LEAVE_DAYS'=>array('type'=>'decimal','unit'=>'day'),
        'ABSENT_DAYS'=>array('type'=>'decimal','unit'=>'day'),
        'OVERTIME_HOURS'=>array('type'=>'decimal','unit'=>'hour'),
        'UNPAID_LEAVE_DAYS'=>array('type'=>'decimal','unit'=>'day'),
        'HOURLY_RATE'=>array('type'=>'decimal','unit'=>'currency/hour'),
        'PERIOD_START'=>array('type'=>'date','unit'=>'date'),
        'PERIOD_END'=>array('type'=>'date','unit'=>'date')
    );
}

function hrm_payroll_formula_safe_function_allowlist()
{
    return array('ABS','ROUND','DATE','DAYS','YEAR','MONTH','DAY');
}

function hrm_payroll_formula_safe_resource_limits()
{
    return array(
        'max_source_length'=>1000,
        'max_tokens'=>256,
        'max_ast_depth'=>32,
        'max_node_evaluations'=>256,
        'max_function_calls'=>32,
        'max_execution_time_ms'=>50,
        'max_memory_bytes'=>1048576,
        'max_batch_size'=>1000
    );
}

function hrm_payroll_formula_safe_has_only_keys(array $value, array $allowed)
{
    foreach ($value as $key=>$unused)
        if (!in_array($key, $allowed, true))
            return false;
    return true;
}

/** Validate the immutable on-disk profile contract. */
function hrm_payroll_formula_safe_validate_profile(array $profile, &$error=null)
{
    $error = null;
    $top = array('schema_version','contract','execution','decimal_policy','date_policy','context_policy','variables','governance');
    if (!hrm_payroll_formula_safe_has_only_keys($profile, $top)) {
        $error='safe_profile_unknown_field'; return false;
    }
    foreach ($top as $required) {
        if (!array_key_exists($required, $profile)) {
            $error='safe_profile_missing_field'; return false;
        }
    }
    if ($profile['schema_version'] !== 1 || !is_array($profile['contract'])
        || $profile['contract'] !== array(
            'id'=>HRM_PAYROLL_FORMULA_SAFE_PROFILE_ID,
            'version'=>HRM_PAYROLL_FORMULA_SAFE_PROFILE_VERSION,
            'introduced_source_version'=>HRM_PAYROLL_FORMULA_SAFE_PROFILE_INTRODUCED_VERSION
        )) {
        $error='safe_profile_contract_invalid'; return false;
    }
    $execution = $profile['execution'];
    if (!is_array($execution)
        || !hrm_payroll_formula_safe_has_only_keys($execution, array('engine','production_routing','compatibility_mode','namespace_allowlist','function_allowlist','operator_allowlist','resource_limits'))
        || !isset($execution['engine'],$execution['production_routing'],$execution['compatibility_mode'],$execution['namespace_allowlist'],$execution['function_allowlist'],$execution['operator_allowlist'],$execution['resource_limits'])
        || $execution['engine'] !== 'FormulaFramework-isolated-payroll-runtime'
        || $execution['production_routing'] !== false
        || $execution['compatibility_mode'] !== false
        || $execution['namespace_allowlist'] !== array()
        || $execution['function_allowlist'] !== hrm_payroll_formula_safe_function_allowlist()
        || $execution['operator_allowlist'] !== array('+','-','*','/','%','unary+','unary-','>','<','>=','<=','==','!=','AND','OR','NOT','IF')
        || $execution['resource_limits'] !== hrm_payroll_formula_safe_resource_limits()) {
        $error='safe_profile_execution_invalid'; return false;
    }
    if ($profile['decimal_policy'] !== array(
            'scale'=>6,'rounding'=>'half_away_from_zero','input'=>'canonical_decimal_string_or_integer',
            'division_by_zero'=>'error','non_finite'=>'forbidden','implicit_float_input'=>false)) {
        $error='safe_profile_decimal_policy_invalid'; return false;
    }
    if ($profile['date_policy'] !== array(
            'representation'=>'YYYY-MM-DD','calendar'=>'proleptic_gregorian','timezone'=>'UTC',
            'today_now_functions'=>false,'invalid_date'=>'error')) {
        $error='safe_profile_date_policy_invalid'; return false;
    }
    if ($profile['context_policy'] !== array(
            'immutable'=>true,'require_exact_variables'=>true,'company_context_permitted'=>false,
            'security_context_permitted'=>false,'business_data_permitted'=>false,'php_globals_permitted'=>false,
            'database_access_permitted'=>false,'file_access_permitted'=>false,'network_access_permitted'=>false)) {
        $error='safe_profile_context_policy_invalid'; return false;
    }
    if (!is_array($profile['variables']) || count($profile['variables']) !== count(hrm_payroll_formula_safe_variable_contract())) {
        $error='safe_profile_variables_invalid'; return false;
    }
    $expected = hrm_payroll_formula_safe_variable_contract();
    foreach ($profile['variables'] as $row) {
        if (!is_array($row) || !hrm_payroll_formula_safe_has_only_keys($row,array('name','type','unit'))
            || !isset($row['name'],$row['type'],$row['unit']) || !isset($expected[$row['name']])
            || $expected[$row['name']] !== array('type'=>$row['type'],'unit'=>$row['unit'])) {
            $error='safe_profile_variable_contract_invalid'; return false;
        }
        unset($expected[$row['name']]);
    }
    if (count($expected)!==0) { $error='safe_profile_variable_contract_invalid'; return false; }
    if ($profile['governance'] !== array(
            'author_reviewer_publisher_distinct'=>true,'default_deny_publication'=>true,
            'shadow_required_before_cutover'=>true,'production_cutover_permitted'=>false,
            'formula_text_in_governance_artifacts'=>false,'payroll_context_in_governance_artifacts'=>false)) {
        $error='safe_profile_governance_invalid'; return false;
    }
    return $profile;
}

/** Load + validate profile without accepting request-derived paths. */
function hrm_payroll_formula_safe_load_profile(&$error=null)
{
    global $path_to_root;
    $path = $path_to_root.'/hrm/config/payroll_formula_safe_profile.json';
    $raw = @file_get_contents($path);
    if ($raw === false) { $error='safe_profile_missing'; return false; }
    $profile = json_decode($raw, true);
    if (!is_array($profile)) { $error='safe_profile_json_invalid'; return false; }
    return hrm_payroll_formula_safe_validate_profile($profile, $error);
}

/** Decimal helper using canonical fixed-scale strings, independent of locale. */
class HRM_Payroll_Formula_Decimal
{
    const SCALE = 6;

    public static function normalize($value)
    {
        if (is_int($value))
            $value = (string)$value;
        if (!is_string($value) || !preg_match('/^-?(?:0|[1-9][0-9]*)(?:\.[0-9]{1,6})?$/D', $value))
            throw new RuntimeException('safe_decimal_invalid');
        $negative = strlen($value)>0 && $value[0]==='-';
        if ($negative) $value=substr($value,1);
        $parts=explode('.', $value, 2);
        $whole=ltrim($parts[0],'0'); if ($whole==='') $whole='0';
        $frac=isset($parts[1])?$parts[1]:'';
        $frac=str_pad($frac,self::SCALE,'0',STR_PAD_RIGHT);
        $digits=ltrim($whole.$frac,'0'); if($digits==='')$digits='0';
        if ($digits==='0') $negative=false;
        return array('negative'=>$negative,'digits'=>$digits);
    }

    public static function fromFloatLiteral($value)
    {
        if (!is_int($value) && !is_float($value)) throw new RuntimeException('safe_decimal_literal_invalid');
        if (is_float($value) && (is_nan($value)||is_infinite($value))) throw new RuntimeException('safe_decimal_literal_invalid');
        return self::normalize(number_format((float)$value, self::SCALE, '.', ''));
    }

    public static function toString(array $d)
    {
        $digits=str_pad($d['digits'], self::SCALE+1, '0', STR_PAD_LEFT);
        $p=strlen($digits)-self::SCALE;
        $out=substr($digits,0,$p).'.'.substr($digits,$p);
        return (!empty($d['negative']) && $d['digits']!=='0' ? '-' : '').$out;
    }

    private static function cmpAbs($a,$b)
    {
        $a=ltrim($a,'0');$b=ltrim($b,'0'); if($a==='')$a='0'; if($b==='')$b='0';
        if(strlen($a)!==strlen($b)) return strlen($a)<strlen($b)?-1:1;
        if($a===$b)return 0; return strcmp($a,$b)<0?-1:1;
    }
    private static function addAbs($a,$b)
    {
        $i=strlen($a)-1;$j=strlen($b)-1;$carry=0;$out='';
        while($i>=0||$j>=0||$carry){$n=($i>=0?(ord($a[$i--])-48):0)+($j>=0?(ord($b[$j--])-48):0)+$carry;$out=chr(48+($n%10)).$out;$carry=(int)floor($n/10);} return ltrim($out,'0')===''?'0':ltrim($out,'0');
    }
    private static function subAbs($a,$b)
    {
        $i=strlen($a)-1;$j=strlen($b)-1;$borrow=0;$out='';
        while($i>=0){$n=(ord($a[$i--])-48)-$borrow-($j>=0?(ord($b[$j--])-48):0);if($n<0){$n+=10;$borrow=1;}else$borrow=0;$out=chr(48+$n).$out;} $out=ltrim($out,'0'); return $out===''?'0':$out;
    }
    private static function mulAbs($a,$b)
    {
        if($a==='0'||$b==='0')return '0'; $la=strlen($a);$lb=strlen($b);$r=array_fill(0,$la+$lb,0);
        for($i=$la-1;$i>=0;$i--)for($j=$lb-1;$j>=0;$j--){$p=(ord($a[$i])-48)*(ord($b[$j])-48)+$r[$i+$j+1];$r[$i+$j+1]=$p%10;$r[$i+$j]+=(int)floor($p/10);} $s=implode('',$r);$s=ltrim($s,'0');return $s===''?'0':$s;
    }
    private static function divmodAbs($numerator,$denominator)
    {
        if($denominator==='0') throw new RuntimeException('safe_decimal_divide_by_zero');
        $q='';$rem='0';$len=strlen($numerator);
        for($i=0;$i<$len;$i++){$rem=ltrim(($rem==='0'?'':$rem).$numerator[$i],'0');if($rem==='')$rem='0';$digit=0;while(self::cmpAbs($rem,$denominator)>=0){$rem=self::subAbs($rem,$denominator);$digit++;} $q.=chr(48+$digit);} $q=ltrim($q,'0');if($q==='')$q='0';return array($q,$rem);
    }
    private static function roundedQuotient($numerator,$denominator)
    {
        list($q,$r)=self::divmodAbs($numerator,$denominator);
        if(self::cmpAbs(self::mulAbs($r,'2'),$denominator)>=0)$q=self::addAbs($q,'1');
        return $q;
    }
    public static function compare(array $a,array $b)
    {
        if($a['negative']!==$b['negative'])return $a['negative']?-1:1; $c=self::cmpAbs($a['digits'],$b['digits']);return $a['negative']?-$c:$c;
    }
    public static function negate(array $a){if($a['digits']!=='0')$a['negative']=!$a['negative'];return $a;}
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
        $raw=self::mulAbs($a['digits'],$b['digits']);$scale=str_repeat('0',self::SCALE);$divisor='1'.$scale;
        $digits=self::roundedQuotient($raw,$divisor);return array('negative'=>($digits!=='0'&&$a['negative']!==$b['negative']),'digits'=>$digits);
    }
    public static function div(array $a,array $b)
    {
        if($b['digits']==='0')throw new RuntimeException('safe_decimal_divide_by_zero');
        $numerator=$a['digits'].str_repeat('0',self::SCALE);$digits=self::roundedQuotient($numerator,$b['digits']);
        return array('negative'=>($digits!=='0'&&$a['negative']!==$b['negative']),'digits'=>$digits);
    }
    public static function mod(array $a,array $b)
    {
        if($b['digits']==='0')throw new RuntimeException('safe_decimal_divide_by_zero'); list($q,$r)=self::divmodAbs($a['digits'],$b['digits']); unset($q);
        return array('negative'=>($r!=='0'&&$a['negative']),'digits'=>$r);
    }
    public static function abs(array $a){$a['negative']=false;return $a;}
    public static function round(array $a,$places)
    {
        $places=(int)$places;if($places<0||$places>self::SCALE)throw new RuntimeException('safe_round_places_invalid');
        $drop=self::SCALE-$places;if($drop===0)return $a;$divisor='1'.str_repeat('0',$drop);$digits=self::roundedQuotient($a['digits'],$divisor).str_repeat('0',$drop);
        $digits=ltrim($digits,'0');if($digits==='')$digits='0';return array('negative'=>($digits!=='0'&&$a['negative']),'digits'=>$digits);
    }
}

/** Immutable typed runtime value. */
class HRM_Payroll_Formula_Value
{
    private $type; private $unit; private $value;
    function __construct($type,$unit,$value){$this->type=$type;$this->unit=$unit;$this->value=$value;}
    function type(){return $this->type;} function unit(){return $this->unit;} function raw(){return $this->value;}
    function decimalString(){if($this->type!=='decimal')throw new RuntimeException('safe_value_not_decimal');return HRM_Payroll_Formula_Decimal::toString($this->value);}
    function scalarInt(){if($this->type!=='decimal'||$this->unit!=='scalar')throw new RuntimeException('safe_value_not_scalar');$s=$this->decimalString();if(!preg_match('/^-?[0-9]+\.000000$/D',$s))throw new RuntimeException('safe_value_not_integer');return (int)substr($s,0,-7);}
    static function decimal($value,$unit='scalar'){return new self('decimal',$unit,is_array($value)?$value:HRM_Payroll_Formula_Decimal::normalize($value));}
    static function date($value){return new self('date','date',hrm_payroll_formula_safe_normalize_date($value));}
    static function boolean($value){return new self('boolean','boolean',(bool)$value);}
}

function hrm_payroll_formula_safe_normalize_date($value)
{
    if(!is_string($value)||!preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D',$value))throw new RuntimeException('safe_date_invalid');
    $p=explode('-',$value);if(!checkdate((int)$p[1],(int)$p[2],(int)$p[0]))throw new RuntimeException('safe_date_invalid');return $value;
}

/** Canonicalize unit algebra (currency/day, currency/hour, scalar, etc.). */
function hrm_payroll_formula_safe_unit_map($unit)
{
    if($unit==='scalar') return array();
    if(!is_string($unit) || $unit==='') throw new RuntimeException('safe_unit_invalid');
    $parts=explode('/', $unit, 2); $map=array();
    foreach(explode('*', $parts[0]) as $atom){
        if($atom==='1'||$atom==='') continue;
        if(!in_array($atom,array('currency','day','hour'),true)) throw new RuntimeException('safe_unit_invalid');
        $map[$atom]=isset($map[$atom])?$map[$atom]+1:1;
    }
    if(isset($parts[1])) foreach(explode('*',$parts[1]) as $atom){
        if($atom==='1'||$atom==='') continue;
        if(!in_array($atom,array('currency','day','hour'),true)) throw new RuntimeException('safe_unit_invalid');
        $map[$atom]=isset($map[$atom])?$map[$atom]-1:-1;
    }
    return $map;
}
function hrm_payroll_formula_safe_unit_name(array $map)
{
    foreach($map as $k=>$v) if($v===0) unset($map[$k]);
    if(!$map) return 'scalar';
    ksort($map); $num=array(); $den=array();
    foreach($map as $k=>$v){
        for($i=0;$i<abs($v);$i++){
            if($v>0) $num[]=$k; else $den[]=$k;
        }
    }
    $n=$num?implode('*',$num):'1'; return $den?$n.'/'.implode('*',$den):$n;
}
function hrm_payroll_formula_safe_unit_combine($left,$right,$sign)
{
    $a=hrm_payroll_formula_safe_unit_map($left);$b=hrm_payroll_formula_safe_unit_map($right);foreach($b as $k=>$v)$a[$k]=(isset($a[$k])?$a[$k]:0)+$sign*$v;return hrm_payroll_formula_safe_unit_name($a);
}

/** Build an immutable exact typed context. No floats, providers or globals. */
function hrm_payroll_formula_safe_context(array $values, &$error=null)
{
    $error=null;$contract=hrm_payroll_formula_safe_variable_contract();$normalized=array();
    if(count($values)!==count($contract)){ $error='safe_context_variable_set_mismatch'; return false; }
    foreach($values as $name=>$value){$key=strtoupper((string)$name);if(!isset($contract[$key])||isset($normalized[$key])){$error='safe_context_variable_set_mismatch';return false;}
        try{$spec=$contract[$key];$normalized[$key]=$spec['type']==='date'?HRM_Payroll_Formula_Value::date($value):HRM_Payroll_Formula_Value::decimal($value,$spec['unit']);}
        catch(Exception $e){$error=$e->getMessage();return false;}}
    foreach($contract as $name=>$unused)if(!isset($normalized[$name])){$error='safe_context_variable_set_mismatch';return false;}
    return $normalized;
}

/** Compiler-only function metadata; execution is owned by the isolated evaluator. */
class HRM_Payroll_Formula_Profile_Function implements Formula_Contracts_FormulaFunctionInterface
{
    private $name,$min,$max,$returnType;
    function __construct($name,$min,$max,$returnType){$this->name=$name;$this->min=$min;$this->max=$max;$this->returnType=$returnType;}
    function getName(){return $this->name;}
    function execute(Formula_Context_FormulaContext $context,array $arguments){throw new RuntimeException('safe_profile_function_direct_execution_denied');}
    function getMetadata(){return new Formula_Registry_FunctionMetadata(array('name'=>$this->name,'description'=>'PAY-RULE-002 isolated function metadata','category'=>'PayrollSafe','version'=>'1.0','minArgs'=>$this->min,'maxArgs'=>$this->max,'returnType'=>$this->returnType,'isDeterministic'=>true,'isCacheable'=>false));}
}

function hrm_payroll_formula_safe_compiler(&$error=null)
{
    global $path_to_root;$error=null;require_once $path_to_root.'/includes/formula/formula_bootstrap.inc';
    $f=new Formula_Registry_FunctionRegistry();
    foreach(array(array('ABS',1,1,'decimal'),array('ROUND',2,2,'decimal'),array('DATE',3,3,'date'),array('DAYS',2,2,'decimal'),array('YEAR',1,1,'decimal'),array('MONTH',1,1,'decimal'),array('DAY',1,1,'decimal')) as $m)$f->register(new HRM_Payroll_Formula_Profile_Function($m[0],$m[1],$m[2],$m[3]));
    $f->freeze();$v=new Formula_Registry_VariableRegistry();$v->freeze();$limits=hrm_payroll_formula_safe_resource_limits();
    return new Formula_Compiler_FormulaCompiler($f,$v,array(),array(),$limits['max_ast_depth'],$limits['max_source_length'],array_keys(hrm_payroll_formula_safe_variable_contract()));
}

/** Validate source tokens before compiler invocation. */
function hrm_payroll_formula_safe_preflight_source($formula,&$error=null)
{
    global $path_to_root;$error=null;if(!is_string($formula)||trim($formula)===''){$error='safe_formula_empty';return false;}
    $limits=hrm_payroll_formula_safe_resource_limits();if(strlen($formula)>$limits['max_source_length']){$error='safe_formula_source_limit';return false;}
    if(preg_match('/(?:^|[^0-9])(?:[0-9]+\.[0-9]{7,})(?:[^0-9]|$)/',$formula)){$error='safe_formula_decimal_scale_exceeded';return false;}
    require_once $path_to_root.'/includes/formula/formula_bootstrap.inc';
    try{$tokens=(new Formula_Compiler_Lexer($formula))->tokenize();}catch(Exception $e){$error='safe_formula_lex_failed';return false;}
    if(count($tokens)>$limits['max_tokens']){$error='safe_formula_token_limit';return false;}return true;
}

/** Isolated AST evaluator with no provider callback surface. */
class HRM_Payroll_Formula_Safe_Evaluator implements Formula_Compiler_AST_NodeVisitor
{
    private $context,$limits,$nodes=0,$functions=0,$started,$memory;
    function __construct(array $context,array $limits){$this->context=$context;$this->limits=$limits;$this->started=microtime(true);$this->memory=memory_get_usage(true);}
    private function tick($function=false){$this->nodes++;if($function)$this->functions++;if($this->nodes>$this->limits['max_node_evaluations'])throw new RuntimeException('safe_resource_node_limit');if($this->functions>$this->limits['max_function_calls'])throw new RuntimeException('safe_resource_function_limit');if((microtime(true)-$this->started)*1000>$this->limits['max_execution_time_ms'])throw new RuntimeException('safe_resource_time_limit');if(memory_get_usage(true)-$this->memory>$this->limits['max_memory_bytes'])throw new RuntimeException('safe_resource_memory_limit');}
    private function decimal(HRM_Payroll_Formula_Value $v){if($v->type()!=='decimal')throw new RuntimeException('safe_type_decimal_required');return $v;}
    private function sameUnit(HRM_Payroll_Formula_Value $a,HRM_Payroll_Formula_Value $b){if($a->unit()!==$b->unit())throw new RuntimeException('safe_unit_mismatch');}
    private function truth(HRM_Payroll_Formula_Value $v){if($v->type()!=='boolean')throw new RuntimeException('safe_type_boolean_required');return (bool)$v->raw();}
    function evaluate(Formula_Compiler_AST_Node $ast){return $ast->accept($this);}
    function visitLiteral(Formula_Compiler_AST_LiteralNode $n){$this->tick();if($n->dataType==='integer'||$n->dataType==='decimal')return HRM_Payroll_Formula_Value::decimal(HRM_Payroll_Formula_Decimal::fromFloatLiteral($n->value),'scalar');if($n->dataType==='boolean')return HRM_Payroll_Formula_Value::boolean($n->value);throw new RuntimeException('safe_literal_type_denied');}
    function visitVariable(Formula_Compiler_AST_VariableNode $n){$this->tick();if($n->namespace!=='')throw new RuntimeException('safe_namespace_denied');$name=strtoupper($n->identifier);if(!isset($this->context[$name]))throw new RuntimeException('safe_variable_denied');return $this->context[$name];}
    function visitFunction(Formula_Compiler_AST_FunctionNode $n){$this->tick(true);$name=strtoupper($n->functionName);if(!in_array($name,hrm_payroll_formula_safe_function_allowlist(),true))throw new RuntimeException('safe_function_denied');$args=array();foreach($n->arguments as $a)$args[]=$a->accept($this);
        if($name==='ABS'){ $v=$this->decimal($args[0]); return HRM_Payroll_Formula_Value::decimal(HRM_Payroll_Formula_Decimal::abs($v->raw()),$v->unit()); }
        if($name==='ROUND'){ $v=$this->decimal($args[0]);$p=$this->decimal($args[1])->scalarInt();return HRM_Payroll_Formula_Value::decimal(HRM_Payroll_Formula_Decimal::round($v->raw(),$p),$v->unit()); }
        if($name==='DATE'){ $y=$this->decimal($args[0])->scalarInt();$m=$this->decimal($args[1])->scalarInt();$d=$this->decimal($args[2])->scalarInt();if(!checkdate($m,$d,$y))throw new RuntimeException('safe_date_invalid');return HRM_Payroll_Formula_Value::date(sprintf('%04d-%02d-%02d',$y,$m,$d)); }
        if($name==='DAYS'){if($args[0]->type()!=='date'||$args[1]->type()!=='date')throw new RuntimeException('safe_type_date_required');$a=new DateTime($args[0]->raw(),new DateTimeZone('UTC'));$b=new DateTime($args[1]->raw(),new DateTimeZone('UTC'));$days=(int)$b->diff($a)->format('%r%a');return HRM_Payroll_Formula_Value::decimal((string)$days,'day');}
        if($name==='YEAR'||$name==='MONTH'||$name==='DAY'){if($args[0]->type()!=='date')throw new RuntimeException('safe_type_date_required');$parts=explode('-',$args[0]->raw());$v=$name==='YEAR'?$parts[0]:($name==='MONTH'?$parts[1]:$parts[2]);return HRM_Payroll_Formula_Value::decimal((string)((int)$v),'scalar');}
        throw new RuntimeException('safe_function_denied');}
    function visitBinary(Formula_Compiler_AST_BinaryOperatorNode $n){$this->tick();if(!in_array($n->operator,array('+','-','*','/','%'),true))throw new RuntimeException('safe_operator_denied');$a=$this->decimal($n->left->accept($this));$b=$this->decimal($n->right->accept($this));
        if($n->operator==='+'||$n->operator==='-'||$n->operator==='%')$this->sameUnit($a,$b);
        $unit=$a->unit();if($n->operator==='*')$unit=hrm_payroll_formula_safe_unit_combine($a->unit(),$b->unit(),1);if($n->operator==='/')$unit=hrm_payroll_formula_safe_unit_combine($a->unit(),$b->unit(),-1);
        $fn=$n->operator==='+'?'add':($n->operator==='-'?'sub':($n->operator==='*'?'mul':($n->operator==='/'?'div':'mod')));return HRM_Payroll_Formula_Value::decimal(call_user_func(array('HRM_Payroll_Formula_Decimal',$fn),$a->raw(),$b->raw()),$unit);}
    function visitUnary(Formula_Compiler_AST_UnaryOperatorNode $n){$this->tick();if($n->operator==='NOT')return HRM_Payroll_Formula_Value::boolean(!$this->truth($n->operand->accept($this)));if($n->operator!=='+'&&$n->operator!=='-')throw new RuntimeException('safe_operator_denied');$v=$this->decimal($n->operand->accept($this));return $n->operator==='-'?HRM_Payroll_Formula_Value::decimal(HRM_Payroll_Formula_Decimal::negate($v->raw()),$v->unit()):$v;}
    function visitConditional(Formula_Compiler_AST_ConditionalNode $n){$this->tick();return $this->truth($n->condition->accept($this))?$n->trueBranch->accept($this):$n->falseBranch->accept($this);}
    function visitComparison(Formula_Compiler_AST_ComparisonNode $n){$this->tick();$a=$n->left->accept($this);$b=$n->right->accept($this);if($a->type()!==$b->type()||$a->unit()!==$b->unit())throw new RuntimeException('safe_comparison_type_mismatch');if($a->type()==='decimal')$c=HRM_Payroll_Formula_Decimal::compare($a->raw(),$b->raw());else if($a->type()==='date')$c=strcmp($a->raw(),$b->raw());else {$av=(int)$a->raw();$bv=(int)$b->raw();$c=$av===$bv?0:($av<$bv?-1:1);}$op=$n->operator;return HRM_Payroll_Formula_Value::boolean($op==='>'?$c>0:($op==='<'?$c<0:($op==='>='?$c>=0:($op==='<='?$c<=0:($op==='=='?$c===0:$c!==0)))));}
    function visitLogical(Formula_Compiler_AST_LogicalNode $n){$this->tick();$left=$this->truth($n->left->accept($this));if($n->operator==='AND'&&!$left)return HRM_Payroll_Formula_Value::boolean(false);if($n->operator==='OR'&&$left)return HRM_Payroll_Formula_Value::boolean(true);$right=$this->truth($n->right->accept($this));if($n->operator==='AND')return HRM_Payroll_Formula_Value::boolean($left&&$right);if($n->operator==='OR')return HRM_Payroll_Formula_Value::boolean($left||$right);throw new RuntimeException('safe_operator_denied');}
    function visitRange(Formula_Compiler_AST_RangeNode $n){$this->tick();throw new RuntimeException('safe_range_denied');}
    function visitNull(Formula_Compiler_AST_NullNode $n){$this->tick();throw new RuntimeException('safe_null_denied');}
}

/** Compile through shared parser/compiler and enforce payroll profile metadata. */
function hrm_payroll_formula_safe_compile($formula,&$error=null)
{
    $error=null;if(!hrm_payroll_formula_safe_preflight_source($formula,$error))return false;
    try{$compiler=hrm_payroll_formula_safe_compiler($error);$compiled=$compiler->compile($formula);$m=$compiled->metadata;
        if($m->astDepth>hrm_payroll_formula_safe_resource_limits()['max_ast_depth']||$m->astNodeCount>hrm_payroll_formula_safe_resource_limits()['max_node_evaluations']){$error='safe_formula_ast_limit';return false;}
        foreach($m->referencedNamespaces as $ns){$error='safe_namespace_denied';return false;}foreach($m->referencedFunctions as $fn)if(!in_array(strtoupper($fn),hrm_payroll_formula_safe_function_allowlist(),true)){$error='safe_function_denied';return false;}
        $known=hrm_payroll_formula_safe_variable_contract();foreach($m->referencedVariables as $variable){$name=strtoupper((string)$variable);if(strpos($name,'.')!==false||!isset($known[$name])){$error='safe_variable_denied';return false;}}return $compiled;
    }catch(Exception $e){$error='safe_compile_failed:'.$e->getMessage();return false;}
}

/** Execute one formula in the isolated typed runtime. */
function hrm_payroll_formula_safe_evaluate($formula,array $values,&$receipt=null,&$error=null)
{
    $receipt=null;$error=null;$profile=hrm_payroll_formula_safe_load_profile($error);if($profile===false)return false;$context=hrm_payroll_formula_safe_context($values,$error);if($context===false)return false;$compiled=hrm_payroll_formula_safe_compile($formula,$error);if($compiled===false)return false;
    try{$evaluator=new HRM_Payroll_Formula_Safe_Evaluator($context,hrm_payroll_formula_safe_resource_limits());$value=$evaluator->evaluate($compiled->ast);if($value->type()!=='decimal'){$error='safe_result_not_decimal';return false;}$receipt=array('profile_id'=>HRM_PAYROLL_FORMULA_SAFE_PROFILE_ID,'profile_version'=>HRM_PAYROLL_FORMULA_SAFE_PROFILE_VERSION,'runtime_version'=>HRM_PAYROLL_FORMULA_SAFE_RUNTIME_VERSION,'formula_sha256'=>hash('sha256',trim((string)$formula)),'result_type'=>'decimal','result_unit'=>$value->unit(),'profile_sha256'=>hash_file('sha256',dirname(__FILE__).'/../config/payroll_formula_safe_profile.json'));return $value;
    }catch(Exception $e){$error=$e->getMessage();return false;}
}

/** Deterministic batch evaluation; no context is mutated or retained. */
function hrm_payroll_formula_safe_evaluate_batch($formula,array $contexts,&$receipts=null,&$error=null)
{
    $receipts=array();$error=null;$limit=hrm_payroll_formula_safe_resource_limits()['max_batch_size'];if(count($contexts)>$limit){$error='safe_batch_limit';return false;}$out=array();foreach($contexts as $context){$receipt=null;$value=hrm_payroll_formula_safe_evaluate($formula,$context,$receipt,$error);if($value===false)return false;$out[]=$value;$receipts[]=$receipt;}return $out;
}
