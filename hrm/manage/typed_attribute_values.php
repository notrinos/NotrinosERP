<?php
/** HRM-FND-007 single-subject governed Typed Attribute value production route. */
$page_security = 'SA_HRM_VIEW_TYPED_ATTRIBUTE_VALUE';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/typed_attribute_value_browser_command_db.inc');

function hrm_typed_attribute_browser_subject_type($value)
{
    return is_string($value) && in_array($value, hrm_typed_attribute_subject_types(), true) ? $value : false;
}

function hrm_typed_attribute_browser_now()
{
    return date('Y-m-d H:i:s');
}

$method = isset($_SERVER['REQUEST_METHOD']) ? strtoupper((string)$_SERVER['REQUEST_METHOD']) : 'GET';
$action = isset($_POST['route_action']) ? (string)$_POST['route_action'] : '';
$subject_type = hrm_typed_attribute_browser_subject_type(
    isset($_POST['subject_type']) ? (string)$_POST['subject_type'] : 'worker');
$subject_id_raw = isset($_POST['subject_id']) ? (string)$_POST['subject_id'] : '';
$subject_id = hrm_typed_attribute_value_positive_id($subject_id_raw, false);
$message = '';
$loaded = false;

if ($method === 'POST') {
    if (!check_csrf_token()) {
        $message = _('The governed Typed Attribute request was rejected.');
    } elseif ($subject_type === false || $subject_id === false
        || !hrm_typed_attribute_value_subject_exists($subject_type, $subject_id, false)) {
        $message = _('The selected normalized subject does not exist or is not available.');
    } elseif ($action === 'load_subject') {
        $loaded = true;
    } elseif ($action === 'append_value') {
        $loaded = true;
        $code = isset($_POST['attribute_code']) ? (string)$_POST['attribute_code'] : '';
        $value = isset($_POST['attribute_value']) ? (string)$_POST['attribute_value'] : '';
        $effective_from = isset($_POST['effective_from']) ? (string)$_POST['effective_from'] : '';
        $result = hrm_typed_attribute_value_append_current_version(array(
            'attribute_code'=>$code,
            'subject_type'=>$subject_type,
            'subject_id'=>$subject_id,
            'value'=>$value,
            'effective_from'=>$effective_from,
        ));
        if ($result === false)
            $message = _('The governed Typed Attribute value was not written. Authorization, definition, predecessor custody, or validation failed closed.');
        else
            display_notification(_('Governed Typed Attribute value version appended successfully.'));
        unset($value, $_POST['attribute_value']);
    } else {
        $message = _('Unsupported Typed Attribute route action.');
    }
}

page(_($help_context = 'Governed Typed Attribute Values'));

if (!function_exists('user_check_access') || !user_check_access('SA_EMPLOYEE')) {
    display_error(_('Employee-management authority is also required for governed Typed Attribute values.'));
    end_page();
    exit;
}
if ($message !== '') display_error($message);

display_note(_('This route operates on one exact normalized Person, Worker, Employment, or Assignment at a time. It does not enumerate subjects, map legacy custom_data, provide bulk import/export, retire definitions, or feed payroll.'), 0, 1);

start_form(false, 'typed_attribute_values.php');
hidden('_token', ensure_csrf_token());
hidden('route_action', 'load_subject');
start_table(TABLESTYLE2);
$subject_options = array('person'=>_('Person'), 'worker'=>_('Worker'), 'employment'=>_('Employment'), 'assignment'=>_('Assignment'));
label_row(_('Subject type:'), array_selector('subject_type', $subject_type === false ? 'worker' : $subject_type, $subject_options));
text_row(_('Subject ID:'), 'subject_id', $subject_id_raw, 20, 20);
end_table(1);
submit_center('load', _('Load Governed Values'));
end_form();

if ($loaded && $subject_type !== false && $subject_id !== false) {
    $as_of = hrm_typed_attribute_browser_now();
    $rows = get_hrm_typed_attribute_current_values($subject_type, $subject_id, $as_of, 50);
    if ($rows === false) {
        display_error(_('Current Typed Attribute values are unavailable because authorization, definition, value-chain, or encrypted custody is ambiguous.'));
    } else {
        echo '<h3>'._('Current Authorized Values').'</h3>';
        start_table(TABLESTYLE, "width='100%'");
        table_header(array(_('Attribute'), _('Type'), _('Sensitivity'), _('Owner Scope'), _('Value'), _('Effective From')));
        $k = 0;
        foreach ($rows as $row) {
            alt_table_row_color($k);
            label_cell(htmlspecialchars((string)$row['attribute_code'], ENT_QUOTES, 'UTF-8'));
            label_cell(htmlspecialchars((string)$row['value_type'], ENT_QUOTES, 'UTF-8'));
            label_cell(htmlspecialchars((string)$row['sensitivity'], ENT_QUOTES, 'UTF-8'));
            label_cell(htmlspecialchars((string)$row['owner_scope'], ENT_QUOTES, 'UTF-8'));
            label_cell(htmlspecialchars((string)$row['value'], ENT_QUOTES, 'UTF-8'));
            label_cell(htmlspecialchars((string)$row['effective_from'], ENT_QUOTES, 'UTF-8'));
            end_row();
        }
        if (!$rows) label_row(_('Status'), _('No authorized current values exist for this exact subject.'), "colspan=1", "colspan=5");
        end_table(1);
    }

    $choices = get_hrm_typed_attribute_current_definition_choices($subject_type, $subject_id, $as_of, 50, true);
    if (is_array($choices) && count($choices) > 0) {
        $options = array();
        foreach ($choices as $choice) {
            $options[(string)$choice['attribute_code']] = (string)$choice['attribute_code'].' — '
                .(string)$choice['value_type'].' / '.(string)$choice['sensitivity'].' / '.(string)$choice['owner_scope'];
        }
        echo '<h3>'._('Append Immutable Value Version').'</h3>';
        display_note(_('The predecessor is resolved server-side from the immutable value chain. The browser cannot submit a predecessor id. Sensitive input is never redisplayed after submission.'), 0, 1);
        start_form(false, 'typed_attribute_values.php');
        hidden('_token', ensure_csrf_token());
        hidden('route_action', 'append_value');
        hidden('subject_type', $subject_type);
        hidden('subject_id', (string)$subject_id);
        start_table(TABLESTYLE2);
        label_row(_('Attribute:'), array_selector('attribute_code', null, $options));
        echo '<tr><td class="label">'._('New value:').'</td><td><input type="password" name="attribute_value" value="" maxlength="1024" autocomplete="new-password" required></td></tr>';
        text_row(_('Effective from (YYYY-MM-DD HH:MM:SS):'), 'effective_from', $as_of, 19, 19);
        end_table(1);
        submit_center('append', _('Append Governed Value Version'));
        end_form();
    } elseif ($choices === false) {
        display_error(_('Writable Typed Attribute definitions are unavailable because authorization or definition custody is ambiguous.'));
    } else {
        display_note(_('No currently effective Typed Attribute definition is writable for this subject under the current explicit object-scope permissions.'), 0, 1);
    }
}

end_page();
