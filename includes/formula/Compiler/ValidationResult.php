<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

/**
 * ValidationResult — Aggregate result of the AST validation pipeline.
 *
 * Collects all errors and warnings from the multi-stage validation process
 * (syntax, semantic, type, dependency checks). Validators add findings
 * during AST traversal; this object aggregates and reports them.
 *
 * A formula is valid when $isValid is true (zero errors). Warnings
 * are non-fatal and do not prevent execution.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 */
class Formula_Compiler_ValidationResult
{
    /** @var bool Whether the formula passed all validation stages */
    public $isValid;

    /** @var array Array of error messages with source location details */
    public $errors;

    /** @var array Array of non-fatal warning messages */
    public $warnings;

    /**
     * Construct a validation result.
     *
     * @param bool  $isValid  True if no errors found
     * @param array $errors   Array of error arrays, each with keys: message, line, column
     * @param array $warnings Array of warning strings
     */
    public function __construct($isValid = true, array $errors = array(), array $warnings = array())
    {
        $this->isValid  = (bool)$isValid;
        $this->errors   = $errors;
        $this->warnings = $warnings;
    }

    /**
     * Add an error to the result.
     *
     * Automatically sets isValid to false.
     *
     * @param string $message Error description
     * @param int    $line    Source line number (1-based, 0 if unknown)
     * @param int    $column  Source column number (1-based, 0 if unknown)
     * @return void
     */
    public function addError($message, $line = 0, $column = 0)
    {
        $this->isValid = false;
        $this->errors[] = array(
            'message' => (string)$message,
            'line'    => (int)$line,
            'column'  => (int)$column,
        );
    }

    /**
     * Add a warning to the result.
     *
     * Warnings do NOT invalidate the formula.
     *
     * @param string $message Warning description
     * @param int    $line    Source line number (1-based, 0 if unknown)
     * @param int    $column  Source column number (1-based, 0 if unknown)
     * @return void
     */
    public function addWarning($message, $line = 0, $column = 0)
    {
        $this->warnings[] = array(
            'message' => (string)$message,
            'line'    => (int)$line,
            'column'  => (int)$column,
        );
    }

    /**
     * Merge another validation result into this one.
     *
     * Combines errors and warnings. If the other result has errors,
     * this result becomes invalid.
     *
     * @param Formula_Compiler_ValidationResult $other
     * @return void
     */
    public function merge(Formula_Compiler_ValidationResult $other)
    {
        if (!$other->isValid) {
            $this->isValid = false;
        }
        $this->errors   = array_merge($this->errors, $other->errors);
        $this->warnings = array_merge($this->warnings, $other->warnings);
    }

    /**
     * Get the count of errors.
     *
     * @return int
     */
    public function errorCount()
    {
        return count($this->errors);
    }

    /**
     * Get the count of warnings.
     *
     * @return int
     */
    public function warningCount()
    {
        return count($this->warnings);
    }

    /**
     * Convert to array for serialization and API responses.
     *
     * @return array
     */
    public function toArray()
    {
        return array(
            'isValid'  => $this->isValid,
            'errors'   => $this->errors,
            'warnings' => $this->warnings,
        );
    }
}
