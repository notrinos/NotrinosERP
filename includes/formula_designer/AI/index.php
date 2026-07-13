<?php
// Security gate — deny direct access.
if (!defined('FORMULA_DESIGNER_BOOTSTRAP_LOADED')) {
    header('HTTP/1.0 403 Forbidden');
    exit('Direct access not permitted.');
}
