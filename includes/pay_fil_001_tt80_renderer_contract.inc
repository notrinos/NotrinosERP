<?php
/** PAY-FIL-001 TT80 05/KK-TNCN renderer candidate contract. Pure helpers; no DB/network/signing authority. */
if (!defined('PAY_FIL_001_TT80_RENDERER_CONTRACT_VERSION')) define('PAY_FIL_001_TT80_RENDERER_CONTRACT_VERSION', '1.0');

/** Exact development-only renderer identity pinned to the locally accepted compatibility stack. */
function pay_fil_001_tt80_renderer_profile()
{
    return array(
        'renderer_id'=>'pay-fil-001.vn.tt80.05-kk-tncn-xml',
        'renderer_version'=>'1.0',
        'jurisdiction_code'=>'VN',
        'obligation_type'=>'personal-income-tax-withholding',
        'form'=>'05/KK-TNCN',
        'form_id'=>'05_KK_TNCN_TT80',
        'declaration_code'=>'864',
        'schema_version'=>'2.9.3',
        'namespace'=>'http://kekhaithue.gdt.gov.vn/TKhaiThue',
        'period_kind'=>'Q',
        'media_type'=>'application/xml',
        'development_gate'=>'OFFICIAL_LOCAL_VALIDATOR_COMPATIBILITY',
        'semantic_mapping_required'=>true,
        'semantic_mapping_status'=>'UNPUBLISHED',
        'official_schema_claimed'=>false,
        'production_generation_allowed'=>false,
        'production_signing_allowed'=>false,
        'production_transmission_allowed'=>false,
        'network_authority_allowed'=>false,
        'connector_secret_custody_allowed'=>false
    );
}

/**
 * Return the fixed main-table indicator order for the TT80 renderer.
 *
 * @return array Field names in declaration order.
 */
function pay_fil_001_tt80_indicator_fields()
{
    return array('mst_cu','ct15','ct16','ct17','ct18','ct19','ct20','ct21','ct22','ct23','ct24','ct25','ct25_1','ct26','ct27','ct28','ct29','ct30','ct31','ct32');
}

/**
 * Validate a UTF-8 text field without coercing numeric values or losing leading zeroes.
 *
 * @param mixed $v Candidate field value.
 * @param int $max Maximum byte length.
 * @param string|null $error Receives a field-specific error code.
 * @param string $field Stable field identifier for diagnostics.
 * @param bool $allow_empty Whether an explicit empty string is allowed.
 * @return string|false Validated text or false.
 */
function pay_fil_001_tt80_text($v, $max, &$error, $field, $allow_empty=false)
{
    if (!is_string($v) || preg_match('//u', $v)!==1) { $error='tt80_'.$field.'_invalid'; return false; }
    if ((!$allow_empty && $v==='') || strlen($v)>$max || preg_match('/[\x00-\x08\x0B\x0C\x0E-\x1F]/', $v)) { $error='tt80_'.$field.'_invalid'; return false; }
    return $v;
}
/**
 * Validate a non-negative integral indicator already normalized as decimal text.
 *
 * @param mixed $v Candidate indicator value.
 * @param string|null $error Receives a field-specific error code.
 * @param string $field Stable field identifier for diagnostics.
 * @return string|false Validated decimal text or false.
 */
function pay_fil_001_tt80_uint_string($v, &$error, $field)
{
    if (!is_string($v) || preg_match('/^(?:0|[1-9][0-9]{0,17})$/D', $v)!==1) { $error='tt80_'.$field.'_invalid'; return false; }
    return $v;
}
/**
 * Escape validated text for use in an XML text node.
 *
 * @param string $v Validated UTF-8 text.
 * @return string XML-escaped text.
 */
function pay_fil_001_tt80_escape($v){return htmlspecialchars((string)$v, ENT_QUOTES|ENT_XML1, 'UTF-8');}

/** Validate a normalized source-bound render model. This validates shape only; it does not approve the semantic mapping. */
function pay_fil_001_validate_tt80_render_model(array $m, &$error=null)
{
    $error=null;$profile=pay_fil_001_tt80_renderer_profile();
    foreach(array('source_binding','header','taxpayer','tax_office','tax_agent','extension','indicators') as $k) if(!isset($m[$k])||!is_array($m[$k])) { $error='tt80_model_section_missing'; return false; }
    $b=$m['source_binding'];
    foreach(array('company_id','legal_entity_id','obligation_sha256','source_set_sha256','semantic_mapping_sha256','currency_code') as $k) if(!array_key_exists($k,$b)) { $error='tt80_source_binding_field_missing'; return false; }
    if (pay_fil_001_uint($b['company_id'])===false || pay_fil_001_posint($b['legal_entity_id'])===false || !pay_fil_001_sha($b['obligation_sha256']) || !pay_fil_001_sha($b['source_set_sha256']) || !pay_fil_001_sha($b['semantic_mapping_sha256']) || $b['currency_code']!=='VND') { $error='tt80_source_binding_invalid'; return false; }
    $h=$m['header'];
    foreach(array('record_id','service_name','service_version','provider_info','declaration_kind','sequence_no','period','period_start','period_end','prepared_on','signer_name','signed_on') as $k) if(!array_key_exists($k,$h)) { $error='tt80_header_field_missing'; return false; }
    if(pay_fil_001_tt80_text($h['record_id'],64,$error,'record_id')===false || pay_fil_001_tt80_text($h['service_name'],128,$error,'service_name')===false || pay_fil_001_tt80_text($h['service_version'],32,$error,'service_version')===false || pay_fil_001_tt80_text($h['provider_info'],160,$error,'provider_info')===false || !in_array($h['declaration_kind'],array('C','B'),true) || pay_fil_001_posint($h['sequence_no'])===false || preg_match('#^[1-4]/20[0-9]{2}$#D',(string)$h['period'])!==1 || pay_fil_001_date($h['period_start'])===false || pay_fil_001_date($h['period_end'])===false || pay_fil_001_date($h['prepared_on'])===false || pay_fil_001_tt80_text($h['signer_name'],160,$error,'signer_name')===false || pay_fil_001_date($h['signed_on'])===false) { if(!$error)$error='tt80_header_invalid'; return false; }
    $q=(int)substr((string)$h['period'],0,1);$y=(int)substr((string)$h['period'],2);$expectedStart=sprintf('%04d-%02d-01',$y,(($q-1)*3)+1);$endMonth=$q*3;$expectedEnd=date('Y-m-t',strtotime(sprintf('%04d-%02d-01',$y,$endMonth)));
    if($h['period_start']!==$expectedStart||$h['period_end']!==$expectedEnd){$error='tt80_quarter_dates_invalid';return false;}
    $t=$m['taxpayer'];foreach(array('tax_code','name','address','ward_name','ward_code','district_code','district_name','province_code','province_name','phone','fax','email','business_activity') as $k) if(!array_key_exists($k,$t)){ $error='tt80_taxpayer_field_missing'; return false; }
    if(pay_fil_001_tt80_text($t['tax_code'],32,$error,'tax_code')===false||pay_fil_001_tt80_text($t['name'],200,$error,'taxpayer_name')===false||pay_fil_001_tt80_text($t['address'],240,$error,'taxpayer_address')===false||pay_fil_001_tt80_text($t['ward_name'],160,$error,'ward_name')===false||pay_fil_001_tt80_text($t['ward_code'],64,$error,'ward_code')===false||pay_fil_001_tt80_text($t['district_code'],64,$error,'district_code',true)===false||pay_fil_001_tt80_text($t['district_name'],160,$error,'district_name',true)===false||pay_fil_001_tt80_text($t['province_code'],64,$error,'province_code')===false||pay_fil_001_tt80_text($t['province_name'],160,$error,'province_name')===false||pay_fil_001_tt80_text($t['phone'],64,$error,'phone',true)===false||pay_fil_001_tt80_text($t['fax'],64,$error,'fax',true)===false||pay_fil_001_tt80_text($t['email'],160,$error,'email',true)===false)return false;
    if(pay_fil_001_tt80_text($t['business_activity'],240,$error,'business_activity',true)===false)return false;
    $o=$m['tax_office'];foreach(array('code','name') as $k)if(!array_key_exists($k,$o)){$error='tt80_tax_office_field_missing';return false;}
    if(pay_fil_001_tt80_text($o['code'],64,$error,'tax_office_code',true)===false||pay_fil_001_tt80_text($o['name'],160,$error,'tax_office_name',true)===false)return false;
    $agent=$m['tax_agent'];$agentFields=array('tax_code'=>32,'name'=>200,'address'=>240,'district_code'=>64,'district_name'=>160,'province_code'=>64,'province_name'=>160,'phone'=>64,'fax'=>64,'email'=>160,'contract_number'=>80,'contract_signed_on'=>10,'employee_name'=>160,'professional_certificate'=>80);
    foreach($agentFields as $k=>$max){if(!array_key_exists($k,$agent)){$error='tt80_tax_agent_field_missing';return false;}if(pay_fil_001_tt80_text($agent[$k],$max,$error,'tax_agent_'.$k,true)===false)return false;}
    if($agent['contract_signed_on']!==''&&pay_fil_001_date($agent['contract_signed_on'])===false){$error='tt80_tax_agent_contract_date_invalid';return false;}
    $extension=$m['extension'];foreach(array('reason_code','reason') as $k)if(!array_key_exists($k,$extension)){$error='tt80_extension_field_missing';return false;}
    if(pay_fil_001_tt80_text($extension['reason_code'],64,$error,'extension_reason_code',true)===false||pay_fil_001_tt80_text($extension['reason'],200,$error,'extension_reason',true)===false)return false;
    $i=$m['indicators'];foreach(pay_fil_001_tt80_indicator_fields() as $k)if(!array_key_exists($k,$i)){ $error='tt80_indicator_field_missing'; return false; }
    if(pay_fil_001_tt80_text($i['mst_cu'],32,$error,'mst_cu')===false || !is_bool($i['ct15'])) { $error=$error?:'tt80_indicator_invalid'; return false; }
    foreach(array('ct16','ct17','ct18','ct19','ct20','ct21','ct22','ct23','ct24','ct25','ct25_1','ct26','ct27','ct28','ct29','ct30','ct31','ct32') as $k) if(pay_fil_001_tt80_uint_string($i[$k],$error,$k)===false)return false;
    $out=$m;$out['source_binding']['company_id']=pay_fil_001_uint($b['company_id']);$out['source_binding']['legal_entity_id']=pay_fil_001_posint($b['legal_entity_id']);$out['header']['sequence_no']=pay_fil_001_posint($h['sequence_no']);return $out;
}

/** Deterministically serialize one validated model. Runtime publication remains blocked until semantic mapping is published. */
function pay_fil_001_render_tt80_xml_candidate(array $model, &$error=null)
{
    $m=pay_fil_001_validate_tt80_render_model($model,$error);if($m===false)return false;$p=pay_fil_001_tt80_renderer_profile();$h=$m['header'];$t=$m['taxpayer'];$o=$m['tax_office'];$agent=$m['tax_agent'];$extension=$m['extension'];$i=$m['indicators'];
    $e='pay_fil_001_tt80_escape';$x=array();$x[]='<?xml version="1.0" encoding="UTF-8"?>';$x[]='<HSoThueDTu xmlns="'.$p['namespace'].'">';$x[]='  <HSoKhaiThue id="'.$e($h['record_id']).'">';$x[]='    <TTinChung>';$x[]='      <TTinDVu>';$x[]='        <maDVu>NotrinosERP</maDVu>';$x[]='        <tenDVu>'.$e($h['service_name']).'</tenDVu>';$x[]='        <pbanDVu>'.$e($h['service_version']).'</pbanDVu>';$x[]='        <ttinNhaCCapDVu>'.$e($h['provider_info']).'</ttinNhaCCapDVu>';$x[]='      </TTinDVu>';$x[]='      <TTinTKhaiThue>';$x[]='        <TKhaiThue>';$x[]='          <maTKhai>864</maTKhai>';$x[]='          <tenTKhai>TK khấu trừ thuế thu nhập cá nhân Mẫu 05/KK-TNCN (TT80/2021)</tenTKhai>';$x[]='          <moTaBMau/>';$x[]='          <pbanTKhaiXML>2.9.3</pbanTKhaiXML>';$x[]='          <loaiTKhai>'.$e($h['declaration_kind']).'</loaiTKhai>';$x[]='          <soLan>'.(int)$h['sequence_no'].'</soLan>';$x[]='          <KyKKhaiThue>';$x[]='            <kieuKy>Q</kieuKy>';$x[]='            <kyKKhai>'.$e($h['period']).'</kyKKhai>';$x[]='            <kyKKhaiTuNgay>'.$e($h['period_start']).'</kyKKhaiTuNgay>';$x[]='            <kyKKhaiDenNgay>'.$e($h['period_end']).'</kyKKhaiDenNgay>';$x[]='            <kyKKhaiTuThang/>';$x[]='            <kyKKhaiDenThang/>';$x[]='          </KyKKhaiThue>';$x[]='          <maCQTNoiNop/>';$x[]='          <tenCQTNoiNop/>';$x[]='          <ngayLapTKhai>'.$e($h['prepared_on']).'</ngayLapTKhai>';$x[]='          <GiaHan>';$x[]='            <maLyDoGiaHan/>';$x[]='            <lyDoGiaHan/>';$x[]='          </GiaHan>';$x[]='          <nguoiKy>'.$e($h['signer_name']).'</nguoiKy>';$x[]='          <ngayKy>'.$e($h['signed_on']).'</ngayKy>';$x[]='          <nganhNgheKD/>';$x[]='        </TKhaiThue>';$x[]='        <NNT>';$x[]='          <mst>'.$e($t['tax_code']).'</mst>';$x[]='          <tenNNT>'.$e($t['name']).'</tenNNT>';$x[]='          <dchiNNT>'.$e($t['address']).'</dchiNNT>';$x[]='          <tenXaNNT>'.$e($t['ward_name']).'</tenXaNNT>';$x[]='          <maXaNNT>'.$e($t['ward_code']).'</maXaNNT>';$x[]='          <maHuyenNNT>'.$e($t['district_code']).'</maHuyenNNT>';$x[]='          <tenHuyenNNT>'.$e($t['district_name']).'</tenHuyenNNT>';$x[]='          <maTinhNNT>'.$e($t['province_code']).'</maTinhNNT>';$x[]='          <tenTinhNNT>'.$e($t['province_name']).'</tenTinhNNT>';$x[]='          <dthoaiNNT>'.$e($t['phone']).'</dthoaiNNT>';$x[]='          <faxNNT>'.$e($t['fax']).'</faxNNT>';$x[]='          <emailNNT>'.$e($t['email']).'</emailNNT>';$x[]='        </NNT>';$x[]='        <DLyThue>';$x[]='          <mstDLyThue>'.$e($t['tax_code']).'</mstDLyThue>';$x[]='          <tenDLyThue/>';$x[]='          <dchiDLyThue/>';$x[]='          <maHuyenDLyThue/>';$x[]='          <tenHuyenDLyThue/>';$x[]='          <maTinhDLyThue/>';$x[]='          <tenTinhDLyThue/>';$x[]='          <dthoaiDLyThue/>';$x[]='          <faxDLyThue/>';$x[]='          <emailDLyThue/>';$x[]='          <soHDongDLyThue/>';$x[]='          <ngayKyHDDLyThue>'.$e($h['signed_on']).'</ngayKyHDDLyThue>';$x[]='          <NVienDLy>';$x[]='            <tenNVienDLyThue/>';$x[]='            <cchiHNghe/>';$x[]='          </NVienDLy>';$x[]='        </DLyThue>';$x[]='      </TTinTKhaiThue>';$x[]='    </TTinChung>';$x[]='    <CTieuTKhaiChinh>';
    foreach(pay_fil_001_tt80_indicator_fields() as $k)$x[]='      <'.$k.'>'.($k==='ct15'?($i[$k]?'true':'false'):$e($i[$k])).'</'.$k.'>';
    $x[]='    </CTieuTKhaiChinh>';$x[]='    <PLuc/>';$x[]='  </HSoKhaiThue>';$x[]='</HSoThueDTu>';
    $xml=implode("\n",$x)."\n";
    $xml=str_replace(
        array('<maCQTNoiNop/>','<tenCQTNoiNop/>','<maLyDoGiaHan/>','<lyDoGiaHan/>','<nganhNgheKD/>','<mstDLyThue>'.$e($t['tax_code']).'</mstDLyThue>','<tenDLyThue/>','<dchiDLyThue/>','<maHuyenDLyThue/>','<tenHuyenDLyThue/>','<maTinhDLyThue/>','<tenTinhDLyThue/>','<dthoaiDLyThue/>','<faxDLyThue/>','<emailDLyThue/>','<soHDongDLyThue/>','<ngayKyHDDLyThue>'.$e($h['signed_on']).'</ngayKyHDDLyThue>','<tenNVienDLyThue/>','<cchiHNghe/>'),
        array('<maCQTNoiNop>'.$e($o['code']).'</maCQTNoiNop>','<tenCQTNoiNop>'.$e($o['name']).'</tenCQTNoiNop>','<maLyDoGiaHan>'.$e($extension['reason_code']).'</maLyDoGiaHan>','<lyDoGiaHan>'.$e($extension['reason']).'</lyDoGiaHan>','<nganhNgheKD>'.$e($t['business_activity']).'</nganhNgheKD>','<mstDLyThue>'.$e($agent['tax_code']).'</mstDLyThue>','<tenDLyThue>'.$e($agent['name']).'</tenDLyThue>','<dchiDLyThue>'.$e($agent['address']).'</dchiDLyThue>','<maHuyenDLyThue>'.$e($agent['district_code']).'</maHuyenDLyThue>','<tenHuyenDLyThue>'.$e($agent['district_name']).'</tenHuyenDLyThue>','<maTinhDLyThue>'.$e($agent['province_code']).'</maTinhDLyThue>','<tenTinhDLyThue>'.$e($agent['province_name']).'</tenTinhDLyThue>','<dthoaiDLyThue>'.$e($agent['phone']).'</dthoaiDLyThue>','<faxDLyThue>'.$e($agent['fax']).'</faxDLyThue>','<emailDLyThue>'.$e($agent['email']).'</emailDLyThue>','<soHDongDLyThue>'.$e($agent['contract_number']).'</soHDongDLyThue>','<ngayKyHDDLyThue>'.$e($agent['contract_signed_on']).'</ngayKyHDDLyThue>','<tenNVienDLyThue>'.$e($agent['employee_name']).'</tenNVienDLyThue>','<cchiHNghe>'.$e($agent['professional_certificate']).'</cchiHNghe>'),
        $xml
    );
    return array('payload'=>$xml,'payload_sha256'=>hash('sha256',$xml),'byte_length'=>strlen($xml),'renderer_id'=>$p['renderer_id'],'renderer_version'=>$p['renderer_version'],'schema_id'=>$p['form_id'],'schema_version'=>$p['schema_version'],'media_type'=>$p['media_type'],'official_schema_claimed'=>false,'production_generation_allowed'=>false,'submission_authorized'=>false);
}
?>
