/* HRM Attendance JS */

var activeAttCell = null;

function openAttModal(cell) {
    activeAttCell = cell;

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
    document.addEventListener("keydown", function(e) {
        if (e.key === "Escape")
            closeAttModal();
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
