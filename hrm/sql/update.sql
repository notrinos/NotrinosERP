-- HRM Module: Migration SQL
-- ALTER and data migration from current tables
--
-- Canonical migration scripts are maintained in:
-- 1) hrm_alter_tables.sql
-- 2) hrm_new_tables.sql
-- Run this file from hrm/sql directory using MySQL client.

SOURCE hrm_alter_tables.sql;
SOURCE hrm_new_tables.sql;

