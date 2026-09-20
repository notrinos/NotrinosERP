<?php
/** CLI-only bounded PAY-CORE-009 resume command. */
if (PHP_SAPI !== 'cli') {
    if (!headers_sent()) { http_response_code(404); header('Cache-Control: no-store'); header('X-Content-Type-Options: nosniff'); }
    exit;
}
$path_to_root=dirname(__DIR__);
function pc9_cli_exit($ok,$code,$details=array()){$out=array_merge(array('ok'=>(bool)$ok,'code'=>(string)$code),$details);fwrite($ok?STDOUT:STDERR,json_encode($out)."\n");exit($ok?0:1);}
function pc9_cli_options($args){$o=array();foreach($args as $a){if(!preg_match('/^--([a-z-]+)=(.*)$/D',(string)$a,$m)||isset($o[$m[1]]))return false;$o[$m[1]]=$m[2];}return $o;}
function pc9_cli_os_user(){foreach(array('USERNAME','USER') as $key){$u=trim((string)getenv($key));if($u!=='')return $u;}if(function_exists('posix_geteuid')&&function_exists('posix_getpwuid')){$p=@posix_getpwuid(posix_geteuid());if(is_array($p)&&!empty($p['name']))return (string)$p['name'];}return '';}
$options=pc9_cli_options(array_slice($argv,1));
if($options===false||array_diff(array_keys($options),array('company','run-id','user-id','chunk-size'))||!isset($options['company'],$options['run-id'],$options['user-id'])||preg_match('/^\d+$/D',$options['company'])!==1||preg_match('/^[1-9]\d*$/D',$options['run-id'])!==1||preg_match('/^[1-9]\d*$/D',$options['user-id'])!==1||(isset($options['chunk-size'])&&preg_match('/^[1-9]\d*$/D',$options['chunk-size'])!==1))pc9_cli_exit(false,'invalid_arguments');
$company=(int)$options['company'];$run_id=(int)$options['run-id'];$user_id=(int)$options['user-id'];$chunk=isset($options['chunk-size'])?(int)$options['chunk-size']:null;
$db_connections=array();require $path_to_root.'/config_db.php';
if(!isset($db_connections[$company])||!is_array($db_connections[$company]))pc9_cli_exit(false,'unknown_company');
$expected_os=trim((string)($db_connections[$company]['pay_core_009_cli_os_user']??(defined('PAY_CORE_009_CLI_OS_USER')?PAY_CORE_009_CLI_OS_USER:'')));
$actual_os=pc9_cli_os_user();
if($expected_os===''||$actual_os===''||strcasecmp($expected_os,$actual_os)!==0)pc9_cli_exit(false,'cli_os_identity_rejected');
if(!defined('TB_PREF'))define('TB_PREF','&TB_PREF&');if(!defined('VARLIB_PATH'))define('VARLIB_PATH',$path_to_root.'/tmp');if(!defined('VARLOG_PATH'))define('VARLOG_PATH',$path_to_root.'/tmp');
$_SESSION=array();$dflt_lang='en_US';$installed_languages=array(array('code'=>'en_US','name'=>'English','encoding'=>'UTF-8'));
$_SESSION['language']=(object)array('code'=>'en_US','encoding'=>'UTF-8','dir'=>'ltr');
include_once $path_to_root.'/version.php';
include_once $path_to_root.'/includes/errors.inc';
include_once $path_to_root.'/includes/current_user.inc';
include_once $path_to_root.'/includes/access_levels.inc';
include_once $path_to_root.'/admin/db/company_db.inc';
include_once $path_to_root.'/includes/prefs/sysprefs.inc';
$SysPrefs=new sys_prefs();$_SESSION['SysPrefs'] =& $SysPrefs;
include_once $path_to_root.'/includes/main.inc';
include_once $path_to_root.'/admin/db/security_db.inc';
include_once $path_to_root.'/hrm/includes/hrm_constants.inc';
include_once $path_to_root.'/hrm/includes/db/employees_db.inc';
include_once $path_to_root.'/hrm/includes/db/payroll_db.inc';
include_once $path_to_root.'/hrm/includes/payroll_engine.inc';
include_once $path_to_root.'/hrm/includes/payroll/pay_core_009_runner.inc';
if(!isset($db_version)||(string)$db_version!=='1.0.669')pc9_cli_exit(false,'source_version_mismatch');
if(!set_global_connection($company))pc9_cli_exit(false,'database_connection_failed');
$company_version=isset($SysPrefs->prefs['version_id'])?(string)$SysPrefs->prefs['version_id']:'';if($company_version!=='1.0.669')pc9_cli_exit(false,'company_version_mismatch',array('company_version'=>$company_version));
$user=get_user($user_id);if(!$user||!empty($user['inactive']))pc9_cli_exit(false,'user_identity_rejected');$role=get_security_role((int)$user['role_id']);if(!$role)pc9_cli_exit(false,'security_role_missing');
$cu=new current_user();$cu->company=$company;$cu->cur_con=$company;$cu->user=$user_id;$cu->access=(int)$user['role_id'];$cu->loginname=(string)$user['login_id'];$cu->username=$cu->loginname;$cu->name=(string)$user['real_name'];$cu->logged=true;$cu->role_set=array();foreach($role['areas'] as $code){$code=(int)$code;if(in_array($code&~0xff,$role['sections']))$cu->role_set[]=$code;}$_SESSION['wa_current_user']=$cu;
if(!user_check_access('SA_HRM_EXECUTE_PAY_CORE_RUN'))pc9_cli_exit(false,'execute_access_denied');
$error=null;$result=hrm_pay_core_009_run_chunk($run_id,'cli',$chunk,$error);if($result===false)pc9_cli_exit(false,'resume_failed',array('reason'=>(string)$error));
$status=hrm_pay_core_009_status($run_id,$status_error);if($status===false)pc9_cli_exit(false,'status_failed',array('reason'=>(string)$status_error));
pc9_cli_exit(true,'chunk_complete',array('run_id'=>$run_id,'state'=>$status['state'],'checkpoint_no'=>$status['checkpoint_no'],'counts'=>$status['counts'],'finalized'=>!empty($result['finalized'])));
?>
