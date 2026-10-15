/* HRM Attendance JS */

var activeAttCell = null;
var attendanceSheetPending = false;
var attendanceSheetRestoringFilters = false;

/** Return the current editor's form after ordinary or Ajax rendering. */
function attendanceSheetForm() {
    var field = document.querySelector('[name=attendance_context]');
    return field ? field.form : null;
}

/** An open editor must be resolved explicitly before other form actions. */
function attendanceSheetEditing() {
    var field = document.querySelector('[name=att_editing]');
    return field && field.value === '1';
}

/** Keep values and focus in the editor when navigation would discard them. */
function attendanceSheetBlockNavigation() {
    var form = attendanceSheetForm();
    if (form && !attendanceSheetRestoringFilters) {
        attendanceSheetRestoringFilters = true;
        try {
            var filters = JSON.parse(form.querySelector('[name=att_filter_state]').value);
            Object.keys(filters).forEach(function(name) {
                var select = form.querySelector('[name="' + name + '"]');
                if (select) {
                    select.value = String(filters[name]);
                    if (window.jQuery) jQuery(select).trigger('change.select2');
                }
            });
        } catch (error) {
            // Keep the edit blocked even if a stale filter widget cannot reset.
        } finally { attendanceSheetRestoringFilters = false; }
    }
    var message = document.getElementById('att-edit-message');
    if (message) message.textContent = message.getAttribute('data-block-message')
        || 'Save or discard the attendance edit before changing page or filters.';
    var input = document.getElementById('cell_regular');
    if (input) input.focus();
    return false;
}

/** Guard Ajax and ordinary submits; one pending command cannot be clicked twice. */
function attendanceSheetAllowAction(name) {
    if (attendanceSheetPending) return false;
    if (attendanceSheetEditing() && !/^(save_cell|delete_cell|discard_cell)$/.test(name || ''))
        return attendanceSheetBlockNavigation();
    return true;
}

function openAttModal(cell) {
    if (attendanceSheetPending) return;
    if (attendanceSheetEditing()) { attendanceSheetBlockNavigation(); return; }
    activeAttCell = cell;
    document.querySelector('[name=att_editing]').value = '1';

    var setSelectValue = function(select, value, def) {
        if (!select)
            return;

        var resolved = (value !== null && value !== "" && value !== "0.00") ? value : def;
        select.value = resolved;

        if (window.jQuery)
            jQuery(select).trigger("change");
    };

    var emp  = cell.getAttribute("data-emp");
    var date = cell.getAttribute("data-date");
    var name = cell.getAttribute("data-name");

    document.getElementById("cell_employee_id").value = emp;
    document.getElementById("cell_date").value = date;
    document.getElementById("modal_emp_label").textContent = emp + " \u2014 " + name;

    var parts = date.split("-");
    document.getElementById("modal_date_label").textContent = parts[2] + "/" + parts[1] + "/" + parts[0];

    // Pre-fill from data attributes or reset
    var f = function(id, attr, def) {
        var v = cell.getAttribute(attr);
        document.getElementById(id).value = (v !== null && v !== "" && v !== "0" && v !== "0.00") ? v : def;
    };
    f("cell_regular",   "data-regular",   "");
    f("cell_ot_hours",  "data-ot-hours",  "");
    f("cell_clock_in",  "data-clock-in",  "");
    f("cell_clock_out", "data-clock-out", "");
    f("cell_notes",     "data-notes",     "");

    // Status selector
    var statusVal = cell.getAttribute("data-status");
    var statusSel = document.querySelector("[name=cell_status]");
    setSelectValue(statusSel, statusVal, "0");

    // OT type selector
    var otVal = cell.getAttribute("data-ot-type");
    var otSel = document.querySelector("[name=cell_ot_type]");
    setSelectValue(otSel, otVal, "0");

    // Shift selector
    var shiftVal = cell.getAttribute("data-shift");
    var shiftSel = document.querySelector("[name=cell_shift]");
    setSelectValue(shiftSel, shiftVal, "");

    // Leave selector
    var leaveVal = cell.getAttribute("data-leave");
    var lvSel = document.querySelector("[name=cell_leave]");
    setSelectValue(lvSel, leaveVal, "0");

    document.getElementById("att-modal-overlay").style.display = "block";
    document.getElementById("att-modal").style.display = "block";

    // Fix Select2 widths - they are measured when modal is hidden so re-apply after visible
    if (window.jQuery) {
        setTimeout(function() {
            jQuery("#att-modal .select2-container").css("width", "100%");
        }, 20);
    }

    document.getElementById("cell_regular").focus();
}

function closeAttModal() {
    if (attendanceSheetEditing()) return attendanceSheetBlockNavigation();
    document.getElementById("att-modal-overlay").style.display = "none";
    document.getElementById("att-modal").style.display = "none";
}

function clearAttModalInputs() {
    var setSelectValue = function(selector, value) {
        var select = document.querySelector(selector);
        if (!select)
            return;

        // Some selects may not have an empty option, so fall back to the first option.
        var hasOption = false;
        for (var i = 0; i < select.options.length; i++) {
            if (select.options[i].value === value) {
                hasOption = true;
                break;
            }
        }

        select.value = hasOption ? value : (select.options.length ? select.options[0].value : value);
        if (window.jQuery)
            jQuery(select).trigger("change");
    };

    document.getElementById("cell_regular").value = "";
    document.getElementById("cell_ot_hours").value = "";
    document.getElementById("cell_clock_in").value = "";
    document.getElementById("cell_clock_out").value = "";
    document.getElementById("cell_notes").value = "";

    setSelectValue("[name=cell_status]", "0");
    setSelectValue("[name=cell_ot_type]", "0");
    setSelectValue("[name=cell_shift]", "");
    setSelectValue("[name=cell_leave]", "0");
}

function toggleAllEmpChecks(checked) {
    var boxes = document.querySelectorAll("input.emp-check");
    for (var i = 0; i < boxes.length; i++) {
        boxes[i].checked = checked;
    }
    var hdr = document.getElementById("check_all_emp_header");
    if (hdr) hdr.checked = checked;
    var flt = document.getElementsByName("check_all_emp");
    if (flt.length) flt[0].checked = checked;
}

// Use delegated handlers so controls keep working after AJAX replaces #_page_body.
if (!window.__attendanceSheetEventsBound) {
    // Shared onchange/select2 adapters call this directly, without a DOM submit.
    var attendanceSheetOriginalRequest = JsHttpRequest.request;
    JsHttpRequest.request = function(trigger, form, timeout) {
        // jQuery's namespaced change also invokes the installed DOM onchange.
        if (attendanceSheetRestoringFilters) return false;
        var name = typeof trigger === 'string' ? trigger : (trigger.name || trigger.id || '');
        var owner = form || (typeof trigger !== 'string' && trigger.form) || attendanceSheetForm();
        if (owner && owner.querySelector('[name=attendance_context]')) {
            if (!attendanceSheetAllowAction(name)) return false;
            attendanceSheetPending = true;
        }
        return attendanceSheetOriginalRequest.call(this, trigger, form, timeout);
    };
    // A replacement form has a fresh single-use context. Transport failures leave
    // the current inputs intact and permit retry once the request indicator ends.
    var attendanceSheetObservedToken = attendanceSheetForm()
        ? attendanceSheetForm().querySelector('[name=attendance_context]').value : '';
    new MutationObserver(function() {
        var form = attendanceSheetForm();
        if (!form) return;
        var token = form.querySelector('[name=attendance_context]').value;
        var status = document.getElementById('modern-app-shell');
        if (token !== attendanceSheetObservedToken || (status && !status.classList.contains('is-ajax-busy')))
            attendanceSheetPending = false;
        if (token !== attendanceSheetObservedToken) {
            attendanceSheetObservedToken = token;
            activeAttCell = null;
            if (attendanceSheetEditing()) document.getElementById('cell_regular').focus();
        }
    }).observe(document.documentElement, {childList:true, subtree:true, attributes:true, attributeFilter:['class']});
    document.addEventListener('submit', function(e) {
        if (!e.target.querySelector('[name=attendance_context]')) return;
        var name = e.submitter ? e.submitter.name : '';
        if (!attendanceSheetAllowAction(name)) { e.preventDefault(); e.stopImmediatePropagation(); return; }
        if (/^(save_cell|delete_cell|bulk_fill|bulk_delete)$/.test(name)) attendanceSheetPending = true;
    }, true);
    // Block click handlers before the shared Behaviour Ajax layer can execute.
    document.addEventListener('click', function(e) {
        var target = e.target.closest ? e.target.closest('button, input[type=submit], a') : null;
        if (!target || !attendanceSheetForm()) return;
        if (target.id === 'att_modal_clear_btn') return;
        if (target.id === 'att_modal_discard_btn') {
            e.preventDefault(); e.stopImmediatePropagation();
            if (attendanceSheetPending) return;
            document.querySelector('[name=att_editing]').value = '0';
            document.getElementById('cell_employee_id').value = '';
            document.getElementById('cell_date').value = '';
            closeAttModal();
            var message = document.getElementById('att-edit-message');
            if (message) message.textContent = '';
            var focusTarget = activeAttCell && activeAttCell.isConnected ? activeAttCell
                : document.querySelector('.att-clickable, [name=employee_id]');
            if (focusTarget && focusTarget.querySelector('.att-open-cell')) focusTarget = focusTarget.querySelector('.att-open-cell');
            if (focusTarget) focusTarget.focus();
            return;
        }
        if (!attendanceSheetAllowAction(target.name)) { e.preventDefault(); e.stopImmediatePropagation(); }
    }, true);
    // Keyboard entry is explicit; Enter inside an editor never chooses a bulk button.
    document.addEventListener("keydown", function(e) {
        if (e.key === 'Tab' && attendanceSheetEditing()) {
            var fields = Array.prototype.filter.call(document.querySelectorAll('#att-modal input, #att-modal select, #att-modal textarea, #att-modal button, #att-modal [tabindex]'),
                function(field) { return field.type !== 'hidden' && field.tabIndex >= 0 && !field.disabled && field.offsetParent !== null; });
            if (fields.length && ((e.shiftKey && e.target === fields[0]) || (!e.shiftKey && e.target === fields[fields.length - 1]))) {
                e.preventDefault(); fields[e.shiftKey ? fields.length - 1 : 0].focus();
                return;
            }
        }
        if ((e.key === 'Enter' || e.key === ' ') && e.target.classList.contains('att-clickable')) {
            e.preventDefault(); openAttModal(e.target); return;
        }
        if (e.key === 'Enter' && attendanceSheetEditing() && !e.defaultPrevented
            && e.target.tagName === 'INPUT' && !e.target.classList.contains('select2-search__field')) {
            e.preventDefault(); document.querySelector('[name=save_cell]').click(); return;
        }
        if (e.key === "Escape" && document.getElementById('att-modal'))
            closeAttModal();
    });
    window.addEventListener('beforeunload', function(e) {
        if (attendanceSheetEditing() && !attendanceSheetPending) { e.preventDefault(); e.returnValue = ''; }
    });

    document.addEventListener("click", function(e) {
        var target = e.target || e.srcElement;
        if (target && target.nodeType === 3)
            target = target.parentNode;
        if (!target)
            return;

        if (target.id === "att_modal_clear_btn") {
            if (e.preventDefault)
                e.preventDefault();
            clearAttModalInputs();
            return false;
        }

        if (target.id === "check_all_emp_header") {
            toggleAllEmpChecks(target.checked);
            return;
        }

        if (target.name === "check_all_emp") {
            toggleAllEmpChecks(target.checked);
        }
    });

    window.__attendanceSheetEventsBound = true;
}
