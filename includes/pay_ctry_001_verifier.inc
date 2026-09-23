<?php
/** PAY-CTRY-001 deterministic manifest parser, canonicalizer and verifier. */
include_once(dirname(__FILE__).'/pay_ctry_001_contract.inc');
include_once(dirname(__FILE__).'/pay_ctry_001_archive.inc');

function pay_ctry_001_fail(&$error, $code)
{
    $error = $code;
    return false;
}

/** Strict JSON parser used to reject duplicate keys and numeric ambiguity. */
class pay_ctry_001_json_reader
{
    public $s;
    public $i = 0;
    public $n = 0;
    public $nodes = 0;
    public $error = null;
    function __construct($s) { $this->s = $s; $this->n = strlen($s); }
    function ws() { while ($this->i < $this->n && strpos(" \t\r\n", $this->s[$this->i]) !== false) $this->i++; }
    function fail($code) { if ($this->error === null) $this->error = $code; return false; }
    function value($depth = 0) {
        if ($depth > PAY_CTRY_001_MAX_JSON_DEPTH) return $this->fail('manifest_json_depth_exceeded');
        if (++$this->nodes > PAY_CTRY_001_MAX_JSON_NODES) return $this->fail('manifest_json_node_limit_exceeded');
        $this->ws(); if ($this->i >= $this->n) return $this->fail('manifest_json_unexpected_end');
        $c = $this->s[$this->i];
        if ($c === '{') return $this->object_value($depth);
        if ($c === '[') return $this->array_value($depth);
        if ($c === '"') return $this->string_value();
        if ($c === 't' && substr($this->s,$this->i,4)==='true') { $this->i+=4; return true; }
        if ($c === 'f' && substr($this->s,$this->i,5)==='false') { $this->i+=5; return false; }
        if ($c === 'n' && substr($this->s,$this->i,4)==='null') { $this->i+=4; return null; }
        if ($c === '-' || ($c >= '0' && $c <= '9')) return $this->number_value();
        return $this->fail('manifest_json_token_invalid');
    }
    function object_value($depth) {
        $this->i++; $out=array(); $seen=array(); $this->ws();
        if ($this->i < $this->n && $this->s[$this->i] === '}') { $this->i++; return $out; }
        while ($this->i < $this->n) {
            $this->ws(); if ($this->i >= $this->n || $this->s[$this->i] !== '"') return $this->fail('manifest_json_object_key_invalid');
            $key=$this->string_value(); if ($key===false && $this->error!==null) return false;
            if (isset($seen[$key])) return $this->fail('manifest_json_duplicate_key'); $seen[$key]=true;
            $this->ws(); if ($this->i >= $this->n || $this->s[$this->i] !== ':') return $this->fail('manifest_json_colon_missing'); $this->i++;
            $value=$this->value($depth + 1); if ($value===false && $this->error!==null) return false; $out[$key]=$value;
            $this->ws(); if ($this->i >= $this->n) return $this->fail('manifest_json_object_unterminated');
            if ($this->s[$this->i] === '}') { $this->i++; return $out; }
            if ($this->s[$this->i] !== ',') return $this->fail('manifest_json_object_separator_invalid'); $this->i++;
        }
        return $this->fail('manifest_json_object_unterminated');
    }
    function array_value($depth) {
        $this->i++; $out=array(); $this->ws();
        if ($this->i < $this->n && $this->s[$this->i] === ']') { $this->i++; return $out; }
        while ($this->i < $this->n) {
            $value=$this->value($depth + 1); if ($value===false && $this->error!==null) return false; $out[]=$value;
            $this->ws(); if ($this->i >= $this->n) return $this->fail('manifest_json_array_unterminated');
            if ($this->s[$this->i] === ']') { $this->i++; return $out; }
            if ($this->s[$this->i] !== ',') return $this->fail('manifest_json_array_separator_invalid'); $this->i++;
        }
        return $this->fail('manifest_json_array_unterminated');
    }
    function string_value() {
        $start=$this->i; $this->i++; $escaped=false;
        while ($this->i < $this->n) {
            $c=$this->s[$this->i];
            if (!$escaped && $c === '"') {
                $this->i++; $token=substr($this->s,$start,$this->i-$start); $v=json_decode($token,true);
                if (!is_string($v) || preg_match('//u',$v)!==1) return $this->fail('manifest_json_string_invalid');
                return $v;
            }
            if (!$escaped && ord($c) < 0x20) return $this->fail('manifest_json_control_character');
            if (!$escaped && $c === '\\') $escaped=true; else $escaped=false;
            $this->i++;
        }
        return $this->fail('manifest_json_string_unterminated');
    }
    function number_value() {
        $rest=substr($this->s,$this->i);
        if (!preg_match('/^-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?/', $rest, $m)) return $this->fail('manifest_json_number_invalid');
        $token=$m[0]; $this->i+=strlen($token);
        if (strpos($token,'.')!==false || stripos($token,'e')!==false) return $this->fail('manifest_json_fractional_number_denied');
        $neg=$token[0]==='-'; $digits=$neg?substr($token,1):$token;
        if (strlen(ltrim($digits,'0'))>16 || (strlen(ltrim($digits,'0'))===16 && strcmp(ltrim($digits,'0'),'9007199254740991')>0)) return $this->fail('manifest_json_integer_out_of_range');
        return (int)$token;
    }
}

function pay_ctry_001_json_decode_strict($json, &$error)
{
    $error=null;
    if (!is_string($json) || $json==='' || substr($json,0,3)==="\xEF\xBB\xBF" || preg_match('//u',$json)!==1)
        return pay_ctry_001_fail($error,'manifest_json_encoding_invalid');
    $r=new pay_ctry_001_json_reader($json); $v=$r->value();
    if ($r->error!==null) return pay_ctry_001_fail($error,$r->error);
    $r->ws(); if ($r->i!==$r->n) return pay_ctry_001_fail($error,'manifest_json_trailing_data');
    if (!is_array($v)) return pay_ctry_001_fail($error,'manifest_json_root_not_object');
    return $v;
}

function pay_ctry_001_is_list_array($value)
{
    if (!is_array($value)) return false; $i=0; foreach ($value as $k=>$v) { if ($k!==$i++) return false; } return true;
}

function pay_ctry_001_canonical_value($value)
{
    if (is_array($value)) {
        if (pay_ctry_001_is_list_array($value)) { $o=array(); foreach($value as $v) $o[]=pay_ctry_001_canonical_value($v); return $o; }
        $keys=array_keys($value); sort($keys,SORT_STRING); $o=array(); foreach($keys as $k)$o[$k]=pay_ctry_001_canonical_value($value[$k]); return $o;
    }
    return $value;
}

function pay_ctry_001_canonical_json($value)
{
    $v=pay_ctry_001_canonical_value($value);
    $flags=JSON_UNESCAPED_SLASHES; if (defined('JSON_UNESCAPED_UNICODE')) $flags|=JSON_UNESCAPED_UNICODE;
    return json_encode($v,$flags);
}

function pay_ctry_001_closed_object($value, $required, $optional, &$error, $prefix)
{
    if (!is_array($value) || pay_ctry_001_is_list_array($value)) return pay_ctry_001_fail($error,$prefix.'_not_object');
    foreach($required as $k) if(!array_key_exists($k,$value)) return pay_ctry_001_fail($error,$prefix.'_field_missing_'.$k);
    $allowed=array_fill_keys(array_merge($required,$optional),true);
    foreach($value as $k=>$v) if(!isset($allowed[$k])) return pay_ctry_001_fail($error,$prefix.'_unknown_field_'.$k);
    return true;
}
function pay_ctry_001_ascii_token($v,$regex,$max)
{
    return is_string($v)&&strlen($v)>0&&strlen($v)<=$max&&preg_match($regex,$v)===1;
}
function pay_ctry_001_sha($v){return is_string($v)&&preg_match('/^[a-f0-9]{64}$/D',$v)===1;}
function pay_ctry_001_semver($v){return pay_ctry_001_ascii_token($v,'/^[0-9]+\.[0-9]+\.[0-9]+(?:[-+][A-Za-z0-9.-]+)?$/D',64);}
function pay_ctry_001_iso_utc_epoch($v)
{
    if(!is_string($v)||preg_match('/^(20[0-9]{2}|19[7-9][0-9])-(0[1-9]|1[0-2])-([0-2][0-9]|3[01])T([01][0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9])Z$/D',$v)!==1)return false;
    $t=strtotime($v); return $t===false||gmdate('Y-m-d\TH:i:s\Z',$t)!==$v?false:$t;
}

/** Normalize list ordering for signature canonicalization. */
function pay_ctry_001_manifest_signature_payload($manifest)
{
    $m=$manifest; unset($m['signing']['signature_base64']);
    if(isset($m['artifacts'])&&is_array($m['artifacts'])) usort($m['artifacts'],function($a,$b){return strcmp((string)$a['path'],(string)$b['path']);});
    if(isset($m['capabilities'])&&is_array($m['capabilities'])) usort($m['capabilities'],function($a,$b){return strcmp((string)$a['id'],(string)$b['id']);});
    if(isset($m['dependencies'])&&is_array($m['dependencies'])) usort($m['dependencies'],function($a,$b){$c=strcmp((string)$a['pack_id'],(string)$b['pack_id']);return $c!==0?$c:strcmp((string)$a['version'],(string)$b['version']);});
    return pay_ctry_001_canonical_json($m);
}

function pay_ctry_001_validate_manifest($m, &$error)
{
    $error=null;
    if(!pay_ctry_001_closed_object($m,pay_ctry_001_manifest_top_level_keys(),array(),$error,'manifest'))return false;
    if($m['manifest_version']!==PAY_CTRY_001_MANIFEST_VERSION)return pay_ctry_001_fail($error,'manifest_version_unsupported');
    if(!pay_ctry_001_closed_object($m['pack'],array('id','version','publisher_id'),array('name'),$error,'pack'))return false;
    if(!pay_ctry_001_ascii_token($m['pack']['id'],'/^[a-z0-9][a-z0-9._-]{2,63}$/D',64))return pay_ctry_001_fail($error,'pack_id_invalid');
    if(!pay_ctry_001_semver($m['pack']['version']))return pay_ctry_001_fail($error,'pack_version_invalid');
    if(!pay_ctry_001_ascii_token($m['pack']['publisher_id'],'/^[a-z0-9][a-z0-9._-]{2,63}$/D',64))return pay_ctry_001_fail($error,'publisher_id_invalid');
    if(isset($m['pack']['name'])&&(!is_string($m['pack']['name'])||strlen($m['pack']['name'])>160||preg_match('//u',$m['pack']['name'])!==1))return pay_ctry_001_fail($error,'pack_name_invalid');

    if(!pay_ctry_001_closed_object($m['host'],array('min_db_version'),array('max_db_version'),$error,'host'))return false;
    if(!pay_ctry_001_semver($m['host']['min_db_version'])||(isset($m['host']['max_db_version'])&&!pay_ctry_001_semver($m['host']['max_db_version'])))return pay_ctry_001_fail($error,'host_version_invalid');
    if(isset($m['host']['max_db_version'])&&version_compare($m['host']['min_db_version'],$m['host']['max_db_version'],'>'))return pay_ctry_001_fail($error,'host_version_range_invalid');

    if(!pay_ctry_001_ascii_token($m['capability_registry_version'],'/^[A-Za-z0-9._-]{1,64}$/D',64))return pay_ctry_001_fail($error,'capability_registry_version_invalid');
    if(!is_array($m['capabilities'])||!pay_ctry_001_is_list_array($m['capabilities'])||count($m['capabilities'])>PAY_CTRY_001_MAX_CAPABILITIES)return pay_ctry_001_fail($error,'capabilities_not_list');
    $cap_seen=array(); foreach($m['capabilities'] as $cap){if(!pay_ctry_001_closed_object($cap,array('id','constraints'),array(),$error,'capability'))return false;if(!pay_ctry_001_ascii_token($cap['id'],'/^[a-z][a-z0-9]*(?:\.[a-z0-9_-]+)+$/D',96))return pay_ctry_001_fail($error,'capability_id_invalid');if(isset($cap_seen[$cap['id']]))return pay_ctry_001_fail($error,'capability_duplicate');$cap_seen[$cap['id']]=true;if(!is_array($cap['constraints'])||(count($cap['constraints'])>0&&pay_ctry_001_is_list_array($cap['constraints'])))return pay_ctry_001_fail($error,'capability_constraints_not_object');foreach($cap['constraints'] as $k=>$v){if(!pay_ctry_001_ascii_token($k,'/^[a-z][a-z0-9_]{0,31}$/D',32)||!(is_string($v)||is_int($v)||is_bool($v)||$v===null))return pay_ctry_001_fail($error,'capability_constraint_invalid');}}

    if(!is_array($m['dependencies'])||!pay_ctry_001_is_list_array($m['dependencies'])||count($m['dependencies'])>PAY_CTRY_001_MAX_DEPENDENCIES)return pay_ctry_001_fail($error,'dependencies_not_list');
    $dep_seen=array(); foreach($m['dependencies'] as $dep){if(!pay_ctry_001_closed_object($dep,array('pack_id','version'),array(),$error,'dependency'))return false;if(!pay_ctry_001_ascii_token($dep['pack_id'],'/^[a-z0-9][a-z0-9._-]{2,63}$/D',64)||!pay_ctry_001_semver($dep['version']))return pay_ctry_001_fail($error,'dependency_invalid');if($dep['pack_id']===$m['pack']['id'])return pay_ctry_001_fail($error,'dependency_self_reference');if(isset($dep_seen[$dep['pack_id']]))return pay_ctry_001_fail($error,'dependency_duplicate');$dep_seen[$dep['pack_id']]=true;}

    if(!is_array($m['artifacts'])||!pay_ctry_001_is_list_array($m['artifacts'])||count($m['artifacts'])<1||count($m['artifacts'])>PAY_CTRY_001_MAX_MEMBERS-1)return pay_ctry_001_fail($error,'artifact_list_invalid');
    $types=pay_ctry_001_allowed_artifact_types();$path_seen=array(); foreach($m['artifacts'] as $a){if(!pay_ctry_001_closed_object($a,array('path','type','media_type','sha256','bytes'),array(),$error,'artifact'))return false;$pe=null;$p=pay_ctry_001_normalize_member_path($a['path'],$pe);if($p===false||substr($p,-1)==='/')return pay_ctry_001_fail($error,'artifact_path_invalid');if(strpos($p,'artifacts/')!==0&&strpos($p,'evidence/')!==0)return pay_ctry_001_fail($error,'artifact_path_prefix_denied');if(isset($path_seen[strtolower($p)]))return pay_ctry_001_fail($error,'artifact_duplicate_path');$path_seen[strtolower($p)]=true;if(!isset($types[$a['type']])||!in_array($a['media_type'],$types[$a['type']],true))return pay_ctry_001_fail($error,'artifact_type_or_media_denied');if(!pay_ctry_001_sha($a['sha256'])||!is_int($a['bytes'])||$a['bytes']<0||$a['bytes']>PAY_CTRY_001_MAX_MEMBER_BYTES)return pay_ctry_001_fail($error,'artifact_digest_or_size_invalid');$suffix=strtolower(pathinfo($p,PATHINFO_EXTENSION));if(!in_array($suffix,pay_ctry_001_allowed_suffixes(),true))return pay_ctry_001_fail($error,'artifact_suffix_denied');}
    if(!pay_ctry_001_sha($m['semantic_root'])||!pay_ctry_001_sha($m['evidence_root']))return pay_ctry_001_fail($error,'root_digest_invalid');

    if(!pay_ctry_001_closed_object($m['signing'],array('signer_id','key_id','algorithm','signed_at','signature_base64'),array(),$error,'signing'))return false;
    if(!pay_ctry_001_ascii_token($m['signing']['signer_id'],'/^[a-z0-9][a-z0-9._-]{2,63}$/D',64)||!pay_ctry_001_ascii_token($m['signing']['key_id'],'/^[A-Za-z0-9._:-]{3,96}$/D',96))return pay_ctry_001_fail($error,'signer_or_key_id_invalid');
    if($m['signing']['algorithm']!=='rsa-sha256')return pay_ctry_001_fail($error,'signature_algorithm_unsupported');
    if(pay_ctry_001_iso_utc_epoch($m['signing']['signed_at'])===false)return pay_ctry_001_fail($error,'signed_at_invalid');
    $sig=base64_decode($m['signing']['signature_base64'],true);if(!is_string($sig)||strlen($sig)<128||strlen($sig)>1024)return pay_ctry_001_fail($error,'signature_encoding_invalid');

    $encoded=pay_ctry_001_canonical_json($m);foreach(pay_ctry_001_forbidden_manifest_activation_keys() as $forbidden){if(preg_match('/"'.preg_quote($forbidden,'/').'"\s*:/i',$encoded))return pay_ctry_001_fail($error,'activation_scope_key_denied');}
    return true;
}

function pay_ctry_001_forbidden_content($path,$content,&$error)
{
    $error=null;
    if(strpos($path,'artifacts/')!==0&&strpos($path,'evidence/')!==0)return pay_ctry_001_fail($error,'member_core_target_denied');
    $suffix=strtolower(pathinfo($path,PATHINFO_EXTENSION)); if(!in_array($suffix,pay_ctry_001_allowed_suffixes(),true))return pay_ctry_001_fail($error,'member_executable_suffix_denied');
    $probe=$content;
    $patterns=array('/<\?(?:php|=)?/i','/^\s*#!\s*\//m','/<script\b/i','/\b(?:eval|shell_exec|passthru|proc_open|popen|system|exec)\s*\(/i','/\b(?:CREATE|ALTER|DROP|TRUNCATE)\s+TABLE\b/i','/\b(?:INSERT\s+INTO|UPDATE\s+[A-Za-z0-9_`]+\s+SET|DELETE\s+FROM)\b/i');
    foreach($patterns as $re)if(preg_match($re,$probe))return pay_ctry_001_fail($error,'member_forbidden_code_or_sql');
    return true;
}

function pay_ctry_001_compute_roots($manifest,$members)
{
    $semantic=array();$evidence=array();
    foreach($manifest['artifacts'] as $a){$row=array('path'=>$a['path'],'type'=>$a['type'],'media_type'=>$a['media_type'],'sha256'=>$a['sha256'],'bytes'=>$a['bytes']);if(pay_ctry_001_is_evidence_type($a['type'])||strpos($a['path'],'evidence/')===0)$evidence[]=$row;else$semantic[]=$row;}
    usort($semantic,function($a,$b){return strcmp($a['path'],$b['path']);});usort($evidence,function($a,$b){return strcmp($a['path'],$b['path']);});
    return array('semantic_root'=>hash('sha256',pay_ctry_001_canonical_json(array('root_version'=>'semantic-root-v1','artifacts'=>$semantic))),'evidence_root'=>hash('sha256',pay_ctry_001_canonical_json(array('root_version'=>'evidence-root-v1','artifacts'=>$evidence))));
}

function pay_ctry_001_validate_members_against_manifest($manifest,$members,&$error)
{
    $error=null;if(!isset($members['manifest.json']))return pay_ctry_001_fail($error,'manifest_member_missing');
    $expected=array('manifest.json'=>true);foreach($manifest['artifacts'] as $a)$expected[$a['path']]=true;
    if(count($members)!==count($expected))return pay_ctry_001_fail($error,'archive_member_set_mismatch');
    foreach($members as $path=>$meta)if(!isset($expected[$path]))return pay_ctry_001_fail($error,'archive_undeclared_member');
    foreach($manifest['artifacts'] as $a){if(!isset($members[$a['path']]))return pay_ctry_001_fail($error,'artifact_member_missing');$m=$members[$a['path']];if($m['sha256']!==$a['sha256']||(int)$m['bytes']!==(int)$a['bytes'])return pay_ctry_001_fail($error,'artifact_digest_mismatch');$ce=null;if(!pay_ctry_001_forbidden_content($a['path'],$m['content'],$ce))return pay_ctry_001_fail($error,$ce);if(substr($a['media_type'],-4)==='json'||$a['media_type']==='application/json'||$a['media_type']==='text/json'){$je=null;if(pay_ctry_001_json_decode_strict($m['content'],$je)===false)return pay_ctry_001_fail($error,'artifact_json_invalid');}}
    $roots=pay_ctry_001_compute_roots($manifest,$members);if($roots['semantic_root']!==$manifest['semantic_root'])return pay_ctry_001_fail($error,'semantic_root_mismatch');if($roots['evidence_root']!==$manifest['evidence_root'])return pay_ctry_001_fail($error,'evidence_root_mismatch');return $roots;
}

function pay_ctry_001_validate_capabilities($manifest,$context,&$error)
{
    $error=null;$registry=isset($context['capability_registry'])?$context['capability_registry']:false;if(!is_array($registry)||!isset($registry['version'],$registry['definitions'])||$registry['version']!==$manifest['capability_registry_version']||!is_array($registry['definitions']))return pay_ctry_001_fail($error,'capability_registry_incompatible');
    $expected=pay_ctry_001_expected_capability_registry();
    if(!is_array($registry)||!isset($registry['version'],$registry['registry_sha256'],$registry['definitions'])||$registry['version']!==$manifest['capability_registry_version']||$registry['version']!==$expected['version']||$registry['registry_sha256']!==$expected['registry_sha256']||!is_array($registry['definitions'])||count($registry['definitions'])!==count($expected['definitions']))return pay_ctry_001_fail($error,'capability_registry_tampered');
    foreach($expected['definitions'] as $code=>$expected_def){if(!isset($registry['definitions'][$code])||!is_array($registry['definitions'][$code]))return pay_ctry_001_fail($error,'capability_registry_tampered');$actual=$registry['definitions'][$code];$stored_sha=isset($actual['definition_sha256'])?(string)$actual['definition_sha256']:'';unset($actual['definition_sha256']);if(pay_ctry_001_canonical_json($actual)!==pay_ctry_001_canonical_json($expected_def)||$stored_sha!==$expected['definition_sha256'][$code]||hash('sha256',pay_ctry_001_canonical_json($actual))!==hash('sha256',pay_ctry_001_canonical_json($expected_def)))return pay_ctry_001_fail($error,'capability_registry_tampered');}
    $groups=array();foreach($manifest['capabilities'] as $cap){if(!isset($registry['definitions'][$cap['id']])||!is_array($registry['definitions'][$cap['id']]))return pay_ctry_001_fail($error,'capability_unknown');$def=$registry['definitions'][$cap['id']];$required=isset($def['required_constraints'])&&is_array($def['required_constraints'])?$def['required_constraints']:array();foreach($required as $k)if(!array_key_exists($k,$cap['constraints']))return pay_ctry_001_fail($error,'capability_required_constraint_missing');if(isset($def['allowed_constraints'])&&is_array($def['allowed_constraints']))foreach($cap['constraints'] as $k=>$v)if(!in_array($k,$def['allowed_constraints'],true))return pay_ctry_001_fail($error,'capability_constraint_not_allowed');if(isset($def['allowed_artifact_types'])&&is_array($def['allowed_artifact_types'])&&count($def['allowed_artifact_types'])){$found=false;foreach($manifest['artifacts'] as $artifact)if(in_array($artifact['type'],$def['allowed_artifact_types'],true)){$found=true;break;}if(!$found)return pay_ctry_001_fail($error,'capability_artifact_contract_unsatisfied');}if(!empty($def['exclusive_group'])){if(isset($groups[$def['exclusive_group']]))return pay_ctry_001_fail($error,'capability_conflict');$groups[$def['exclusive_group']]=$cap['id'];}}
    return true;
}

function pay_ctry_001_dependency_cycle_visit($node,$graph,&$visiting,&$done)
{
    if(isset($visiting[$node]))return true;if(isset($done[$node]))return false;$visiting[$node]=true;$next=isset($graph[$node])&&is_array($graph[$node])?$graph[$node]:array();foreach($next as $n)if(pay_ctry_001_dependency_cycle_visit($n,$graph,$visiting,$done))return true;unset($visiting[$node]);$done[$node]=true;return false;
}
function pay_ctry_001_validate_dependencies($manifest,$context,&$error)
{
    $error=null;$installed=isset($context['installed_dependencies'])&&is_array($context['installed_dependencies'])?$context['installed_dependencies']:array();foreach($manifest['dependencies'] as $dep){if(!isset($installed[$dep['pack_id']]))return pay_ctry_001_fail($error,'dependency_missing_or_conflicting');$have=$installed[$dep['pack_id']];$ok=is_array($have)?isset($have[$dep['version']]):$have===$dep['version'];if(!$ok)return pay_ctry_001_fail($error,'dependency_missing_or_conflicting');}
    $graph=isset($context['dependency_graph'])&&is_array($context['dependency_graph'])?$context['dependency_graph']:array();$candidate=$manifest['pack']['id'].'@'.$manifest['pack']['version'];$graph[$candidate]=array();foreach($manifest['dependencies'] as $dep)$graph[$candidate][]=$dep['pack_id'].'@'.$dep['version'];$visiting=array();$done=array();if(pay_ctry_001_dependency_cycle_visit($candidate,$graph,$visiting,$done))return pay_ctry_001_fail($error,'dependency_cycle');return true;
}

function pay_ctry_001_validate_trust_and_signature($manifest,$context,&$error)
{
    $error=null;$publisher=isset($context['publisher'])?$context['publisher']:false;$signer=isset($context['signer'])?$context['signer']:false;$key=isset($context['key'])?$context['key']:false;$now=isset($context['trusted_now'])?(int)$context['trusted_now']:time();
    if(!is_array($publisher)||!is_array($signer)||!is_array($key))return pay_ctry_001_fail($error,'publisher_signer_or_key_unknown');
    if((string)$publisher['publisher_code']!==$manifest['pack']['publisher_id'])return pay_ctry_001_fail($error,'publisher_mismatch');if((string)$signer['signer_code']!==$manifest['signing']['signer_id'])return pay_ctry_001_fail($error,'signer_mismatch');if(isset($signer['publisher_id'],$publisher['publisher_id'])&&(int)$signer['publisher_id']!==(int)$publisher['publisher_id'])return pay_ctry_001_fail($error,'signer_publisher_mismatch');if((int)$key['signer_id']!==(int)$signer['signer_id'])return pay_ctry_001_fail($error,'key_signer_mismatch');if((string)$key['key_id']!==$manifest['signing']['key_id']||(string)$key['algorithm']!=='rsa-sha256')return pay_ctry_001_fail($error,'key_identity_or_algorithm_mismatch');
    $signed=pay_ctry_001_iso_utc_epoch($manifest['signing']['signed_at']);$pub_from=pay_ctry_001_iso_utc_epoch((string)$publisher['valid_from']);$pub_to=!empty($publisher['valid_to'])?pay_ctry_001_iso_utc_epoch((string)$publisher['valid_to']):null;$key_from=pay_ctry_001_iso_utc_epoch((string)$key['valid_from']);$key_to=!empty($key['valid_to'])?pay_ctry_001_iso_utc_epoch((string)$key['valid_to']):null;$trusted_at=pay_ctry_001_iso_utc_epoch((string)$key['trusted_at']);
    if($signed===false||$pub_from===false||$key_from===false||$trusted_at===false)return pay_ctry_001_fail($error,'trusted_time_state_invalid');if($signed>$now+PAY_CTRY_001_CLOCK_SKEW_SECONDS)return pay_ctry_001_fail($error,'signature_time_future');if($signed<$pub_from||($pub_to!==null&&$signed>$pub_to))return pay_ctry_001_fail($error,'publisher_not_valid_at_signature_time');if($signed<$key_from||($key_to!==null&&$signed>$key_to))return pay_ctry_001_fail($error,'key_not_valid_at_signature_time');if($signed<$trusted_at-PAY_CTRY_001_CLOCK_SKEW_SECONDS)return pay_ctry_001_fail($error,'signature_backdated_before_trust');if($pub_to!==null&&$now>$pub_to)return pay_ctry_001_fail($error,'publisher_expired_for_new_trust');if($key_to!==null&&$now>$key_to)return pay_ctry_001_fail($error,'key_expired_for_new_trust');if(!empty($context['publisher_revoked']))return pay_ctry_001_fail($error,'publisher_revoked');if(!empty($context['key_revoked']))return pay_ctry_001_fail($error,'key_revoked');
    $pem=isset($key['public_key_pem'])?(string)$key['public_key_pem']:'';if($pem===''||strpos($pem,'PRIVATE KEY')!==false)return pay_ctry_001_fail($error,'public_key_material_invalid');$pk=@openssl_pkey_get_public($pem);if($pk===false)return pay_ctry_001_fail($error,'public_key_material_invalid');$details=@openssl_pkey_get_details($pk);if(!is_array($details)||!isset($details['type'],$details['bits'])||(int)$details['type']!==OPENSSL_KEYTYPE_RSA)return pay_ctry_001_fail($error,'signature_key_type_unsupported');if((int)$details['bits']<2048)return pay_ctry_001_fail($error,'public_key_too_weak');$canonical_pem=str_replace("\r\n","\n",trim((string)$details['key']))."\n";if(!isset($key['public_key_sha256'],$key['key_sha256'])||!hash_equals((string)$key['public_key_sha256'],hash('sha256',$canonical_pem)))return pay_ctry_001_fail($error,'trust_key_public_hash_mismatch');$key_payload=array('signer_id'=>(int)$key['signer_id'],'key_id'=>(string)$key['key_id'],'algorithm'=>(string)$key['algorithm'],'public_key_sha256'=>(string)$key['public_key_sha256'],'valid_from'=>(string)$key['valid_from'],'valid_to'=>empty($key['valid_to'])?null:(string)$key['valid_to'],'trusted_at'=>(string)$key['trusted_at']);if(!hash_equals((string)$key['key_sha256'],hash('sha256',pay_ctry_001_canonical_json($key_payload))))return pay_ctry_001_fail($error,'trust_key_record_hash_mismatch');
    $sup=isset($key['supersession'])?$key['supersession']:null;if($sup!==null){if(!is_array($sup)||!isset($sup['predecessor_key_id'],$sup['successor_key_id'],$sup['effective_at'],$sup['supersession_sha256'])||(int)$sup['predecessor_key_id']!==(int)$key['trust_key_id']||(int)$sup['successor_key_id']<=0)return pay_ctry_001_fail($error,'key_supersession_state_invalid');$sup_at=pay_ctry_001_iso_utc_epoch((string)$sup['effective_at']);$sup_payload=array('predecessor_key_id'=>(int)$sup['predecessor_key_id'],'successor_key_id'=>(int)$sup['successor_key_id'],'effective_at'=>(string)$sup['effective_at']);if($sup_at===false||!hash_equals((string)$sup['supersession_sha256'],hash('sha256',pay_ctry_001_canonical_json($sup_payload))))return pay_ctry_001_fail($error,'key_supersession_state_invalid');if($now>=$sup_at)return pay_ctry_001_fail($error,'key_superseded');}
    $payload=pay_ctry_001_manifest_signature_payload($manifest);$sig=base64_decode($manifest['signing']['signature_base64'],true);$ok=@openssl_verify($payload,$sig,$pk,OPENSSL_ALGO_SHA256);if($ok!==1)return pay_ctry_001_fail($error,'signature_invalid');return true;
}

/** Parse archive and manifest without trusting it; useful for resolver lookup. */
function pay_ctry_001_parse_candidate($raw,&$error)
{
    $error=null;$ae=null;$members=pay_ctry_001_read_zip_members($raw,$ae);if($members===false)return pay_ctry_001_fail($error,$ae);if(!isset($members['manifest.json']))return pay_ctry_001_fail($error,'manifest_member_missing');$je=null;$manifest=pay_ctry_001_json_decode_strict($members['manifest.json']['content'],$je);if($manifest===false)return pay_ctry_001_fail($error,$je);$me=null;if(!pay_ctry_001_validate_manifest($manifest,$me))return pay_ctry_001_fail($error,$me);return array('manifest'=>$manifest,'members'=>$members,'raw_sha256'=>hash('sha256',$raw),'raw_bytes'=>strlen($raw),'manifest_sha256'=>hash('sha256',pay_ctry_001_canonical_json($manifest)));
}

/** Complete verification. Context may be produced after parsing publisher/signer/key references. */
function pay_ctry_001_verify_parsed_candidate($parsed,$context,&$error)
{
    $error=null;if(!is_array($parsed)||!isset($parsed['manifest'],$parsed['members']))return pay_ctry_001_fail($error,'candidate_parse_state_invalid');$m=$parsed['manifest'];$host=isset($context['host_db_version'])?(string)$context['host_db_version']:PAY_CTRY_001_FINAL_DB_VERSION;if(version_compare($host,$m['host']['min_db_version'],'<')||(isset($m['host']['max_db_version'])&&version_compare($host,$m['host']['max_db_version'],'>')))return pay_ctry_001_fail($error,'host_version_incompatible');$re=null;$roots=pay_ctry_001_validate_members_against_manifest($m,$parsed['members'],$re);if($roots===false)return pay_ctry_001_fail($error,$re);$ce=null;if(!pay_ctry_001_validate_capabilities($m,$context,$ce))return pay_ctry_001_fail($error,$ce);$de=null;if(!pay_ctry_001_validate_dependencies($m,$context,$de))return pay_ctry_001_fail($error,$de);$te=null;if(!pay_ctry_001_validate_trust_and_signature($m,$context,$te))return pay_ctry_001_fail($error,$te);$out=$parsed;$out['roots']=$roots;$out['signature_payload_sha256']=hash('sha256',pay_ctry_001_manifest_signature_payload($m));$out['verification_status']='accepted';return $out;
}

function pay_ctry_001_verify_candidate($raw,$context,&$error)
{
    $parsed=pay_ctry_001_parse_candidate($raw,$error);if($parsed===false)return false;return pay_ctry_001_verify_parsed_candidate($parsed,$context,$error);
}
?>
