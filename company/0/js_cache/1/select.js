

var loadSelect2={'select':function(e){if((e.hasAttribute('multiple')===false)&&$(e).hasClass('nosearch')===false){$(e).select2({dropdownAutoWidth:true,
templateResult:function(item){var selectionText=item.text.replace(/</g,'&lt;').split('\n');var returnString=$('<span></span>');$.each(selectionText,function(index,value){line=value===undefined ? '':value;returnString.append(line+'</br>');})
return returnString;}
});$(e).on('select2:close',function(){$(this).focus();});$(e).on('select2:open',function(e2){$('.dynamic_combo_btn').remove();var target_id=$(e2.target).attr('id');var search_btn=$('#_'+target_id+'_search').clone();var add_btn=$('#_'+target_id+'_add').clone();$(search_btn).addClass('dynamic_combo_btn');$(add_btn).addClass('dynamic_combo_btn');$(search_btn).removeAttr('id hidden');$(add_btn).removeAttr('id hidden');$('.select2-dropdown').append(search_btn);if($(add_btn)!=='undefined')
$('.select2-dropdown').append(add_btn);$(search_btn).add(add_btn).click(function(){$('select').select2('close');});});}
}
}
Behaviour.register(loadSelect2);