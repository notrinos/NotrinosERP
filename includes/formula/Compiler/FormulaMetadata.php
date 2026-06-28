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
 * FormulaMetadata — Pre-computed formula metadata.
 *
 * Carries compile-time information about a formula: the referenced variables,
 * called functions, estimated complexity, cacheability, and version tracking.
 * This metadata is computed once during compilation and stored alongside
 * the AST. It enables introspection without re-parsing the formula.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 */
class Formula_Compiler_FormulaMetadata
{
    /** @var string SHA-256 checksum of the original formula source */
    public $sourceChecksum;

    /** @var string[] Referenced variable names (case-insensitive, uppercase) */
    public $referencedVariables;

    /** @var string[] Referenced function names */
    public $referencedFunctions;

    /** @var string[] Namespace prefixes referenced (e.g., 'Employee', 'Company') */
    public $referencedNamespaces;

    /** @var int Number of AST nodes in the compiled formula */
    public $astNodeCount;

    /** @var int Maximum nesting depth of the AST */
    public $astDepth;

    /** @var float Estimated complexity score (1-100) */
    public $estimatedComplexity;

    /** @var bool Whether this formula is safe to cache */
    public $isCacheable;

    /** @var string|null Required security permission (SA_*), or null if public */
    public $requiredPermission;

    /** @var string Language specification version used to parse this formula */
    public $languageVersion;

    /** @var string Compiler version that produced this output */
    public $compilerVersion;

    /** @var float Compile time in milliseconds */
    public $compileTimeMs;

    /**
     * Construct formula metadata.
     *
     * @param array $data Associative array of property values
     */
    public function __construct(array $data = array())
    {
        $this->sourceChecksum       = isset($data['sourceChecksum']) ? (string)$data['sourceChecksum'] : '';
        $this->referencedVariables  = isset($data['referencedVariables']) ? (array)$data['referencedVariables'] : array();
        $this->referencedFunctions  = isset($data['referencedFunctions']) ? (array)$data['referencedFunctions'] : array();
        $this->referencedNamespaces = isset($data['referencedNamespaces']) ? (array)$data['referencedNamespaces'] : array();
        $this->astNodeCount         = isset($data['astNodeCount']) ? (int)$data['astNodeCount'] : 0;
        $this->astDepth             = isset($data['astDepth']) ? (int)$data['astDepth'] : 0;
        $this->estimatedComplexity  = isset($data['estimatedComplexity']) ? (float)$data['estimatedComplexity'] : 0.0;
        $this->isCacheable          = isset($data['isCacheable']) ? (bool)$data['isCacheable'] : true;
        $this->requiredPermission   = isset($data['requiredPermission']) ? $data['requiredPermission'] : null;
        $this->languageVersion      = isset($data['languageVersion']) ? (string)$data['languageVersion'] : FORMULA_LANGUAGE_VERSION;
        $this->compilerVersion      = isset($data['compilerVersion']) ? (string)$data['compilerVersion'] : FORMULA_COMPILER_VERSION;
        $this->compileTimeMs        = isset($data['compileTimeMs']) ? (float)$data['compileTimeMs'] : 0.0;
    }

    /**
     * Convert to array for serialization and caching.
     *
     * @return array
     */
    public function toArray()
    {
        return array(
            'sourceChecksum'       => $this->sourceChecksum,
            'referencedVariables'  => $this->referencedVariables,
            'referencedFunctions'  => $this->referencedFunctions,
            'referencedNamespaces' => $this->referencedNamespaces,
            'astNodeCount'         => $this->astNodeCount,
            'astDepth'             => $this->astDepth,
            'estimatedComplexity'  => $this->estimatedComplexity,
            'isCacheable'          => $this->isCacheable,
            'requiredPermission'   => $this->requiredPermission,
            'languageVersion'      => $this->languageVersion,
            'compilerVersion'      => $this->compilerVersion,
            'compileTimeMs'        => $this->compileTimeMs,
        );
    }

    /**
     * Create from a serialized array.
     *
     * @param array $data
     * @return Formula_Compiler_FormulaMetadata
     */
    public static function fromArray(array $data)
    {
        return new self($data);
    }
}
