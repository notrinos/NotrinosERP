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

// Security gate — deny direct access.
if (!defined('FORMULA_DESIGNER_BOOTSTRAP_LOADED')) {
    header('HTTP/1.0 403 Forbidden');
    exit('Direct access not permitted.');
}

/**
 * AIResult — Value object returned by AI provider operations.
 *
 * Carries the generated result along with metadata about the AI
 * provider, confidence, duration, and processing details.
 *
 * @package FormulaDesigner\AI
 * @since   2.0.0
 */
class FormulaDesigner_AI_AIResult
{
    /** @var bool Whether the operation succeeded */
    private $success;

    /** @var string|null The primary result value (NFX formula, explanation text, etc.) */
    private $result;

    /** @var array Additional candidate results (for multi-candidate responses) */
    private $candidates;

    /** @var string Human-readable explanation of the result */
    private $explanation;

    /** @var float Confidence score (0.0–1.0) */
    private $confidence;

    /** @var string Complexity level: 'beginner', 'intermediate', 'advanced' */
    private $complexity;

    /** @var int Estimated number of node evaluations for this formula */
    private $estimatedEvaluations;

    /** @var string Provider that generated the result */
    private $provider;

    /** @var float Duration in milliseconds */
    private $durationMs;

    /** @var string|null Error message if the operation failed */
    private $errorMessage;

    /** @var array Additional metadata */
    private $metadata;

    /**
     * Construct an AIResult.
     *
     * @param bool        $success
     * @param string|null $result
     * @param array       $candidates
     * @param string      $explanation
     * @param float       $confidence
     * @param string      $complexity
     * @param int         $estimatedEvaluations
     * @param string      $provider
     * @param float       $durationMs
     * @param string|null $errorMessage
     * @param array       $metadata
     */
    public function __construct(
        $success = true,
        $result = null,
        array $candidates = array(),
        $explanation = '',
        $confidence = 0.0,
        $complexity = 'beginner',
        $estimatedEvaluations = 0,
        $provider = 'RuleBased',
        $durationMs = 0.0,
        $errorMessage = null,
        array $metadata = array()
    ) {
        $this->success              = (bool)$success;
        $this->result               = $result;
        $this->candidates           = $candidates;
        $this->explanation          = (string)$explanation;
        $this->confidence           = (float)$confidence;
        $this->complexity           = (string)$complexity;
        $this->estimatedEvaluations = (int)$estimatedEvaluations;
        $this->provider             = (string)$provider;
        $this->durationMs           = (float)$durationMs;
        $this->errorMessage         = $errorMessage;
        $this->metadata             = $metadata;
    }

    /**
     * Create a success result with a single NFX formula.
     *
     * @param string $formula     The generated NFX formula
     * @param string $explanation Plain-language explanation
     * @param float  $confidence  Confidence score
     * @param string $provider    AI provider name
     * @param float  $durationMs  Processing time in ms
     * @return self
     */
    public static function success($formula, $explanation = '', $confidence = 0.8, $provider = 'RuleBased', $durationMs = 0.0)
    {
        return new self(true, $formula, array(), $explanation, $confidence, 'beginner', 0, $provider, $durationMs);
    }

    /**
     * Create a failure result with an error message.
     *
     * @param string $errorMessage
     * @param string $provider
     * @return self
     */
    public static function failure($errorMessage, $provider = 'RuleBased')
    {
        return new self(false, null, array(), '', 0.0, '', 0, $provider, 0.0, $errorMessage);
    }

    /**
     * Whether the operation succeeded.
     *
     * @return bool
     */
    public function isSuccess()
    {
        return $this->success;
    }

    /**
     * Get the primary result value.
     *
     * @return string|null
     */
    public function getResult()
    {
        return $this->result;
    }

    /**
     * Get all candidate results.
     *
     * @return array
     */
    public function getCandidates()
    {
        return $this->candidates;
    }

    /**
     * Get the human-readable explanation.
     *
     * @return string
     */
    public function getExplanation()
    {
        return $this->explanation;
    }

    /**
     * Get the confidence score.
     *
     * @return float
     */
    public function getConfidence()
    {
        return $this->confidence;
    }

    /**
     * Get the complexity level.
     *
     * @return string
     */
    public function getComplexity()
    {
        return $this->complexity;
    }

    /**
     * Get the estimated evaluation count.
     *
     * @return int
     */
    public function getEstimatedEvaluations()
    {
        return $this->estimatedEvaluations;
    }

    /**
     * Get the provider name.
     *
     * @return string
     */
    public function getProvider()
    {
        return $this->provider;
    }

    /**
     * Get the processing duration in milliseconds.
     *
     * @return float
     */
    public function getDurationMs()
    {
        return $this->durationMs;
    }

    /**
     * Get the error message if available.
     *
     * @return string|null
     */
    public function getErrorMessage()
    {
        return $this->errorMessage;
    }

    /**
     * Get additional metadata.
     *
     * @return array
     */
    public function getMetadata()
    {
        return $this->metadata;
    }

    /**
     * Convert the result to an array for JSON serialization.
     *
     * @return array
     */
    public function toArray()
    {
        return array(
            'success'               => $this->success,
            'result'                => $this->result,
            'candidates'            => $this->candidates,
            'explanation'           => $this->explanation,
            'confidence'            => $this->confidence,
            'complexity'            => $this->complexity,
            'estimatedEvaluations'  => $this->estimatedEvaluations,
            'provider'              => $this->provider,
            'durationMs'            => $this->durationMs,
            'errorMessage'          => $this->errorMessage,
            'metadata'              => $this->metadata,
        );
    }
}
