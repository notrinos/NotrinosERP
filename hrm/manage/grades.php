<?php
/**********************************************************************
    Legacy compatibility route for Manage Pay Grades.
***********************************************************************/
$page_security = 'SA_PAYGRADE';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');

// HRM-FND-003 closes the stale legacy writer surface. Preserve the same
// SA_PAYGRADE authority and forward users to the maintained synchronized page.
meta_forward($path_to_root.'/hrm/manage/pay_grades.php');
