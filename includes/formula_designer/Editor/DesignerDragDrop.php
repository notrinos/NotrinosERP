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
 * DesignerDragDrop — server-side mirror of the Phase 3 drop rules.
 *
 * @package FormulaDesigner\Editor
 * @since   2.0.0
 */
class FormulaDesigner_Editor_DesignerDragDrop
{
    /**
     * Validate whether a payload can be inserted at a connector position.
     *
     * @param array $tokens
     * @param int   $position
     * @param array $payload
     * @return array
     */
    public function validateInsertion(array $tokens, $position, array $payload)
    {
        $result_tokens = $this->simulateInsertion($tokens, $position, $payload);

        if ($result_tokens === null) {
            return array(
                'valid' => false,
                'reason' => 'Unable to build the dragged token payload.',
            );
        }

        $valid = $this->isSequenceValid($result_tokens);

        return array(
            'valid' => $valid,
            'reason' => $valid ? '' : 'Token cannot be inserted at this position.',
        );
    }

    /**
     * Simulate the resulting token sequence for one insertion.
     *
     * @param array $tokens
     * @param int   $position
     * @param array $payload
     * @return array|null
     */
    private function simulateInsertion(array $tokens, $position, array $payload)
    {
        $position = max(0, min((int)$position, count($tokens)));
        $tokens_to_insert = $this->buildTokensFromPayload($payload);

        if (isset($payload['action']) && $payload['action'] === 'move') {
            $move = $this->extractMovedTokens($tokens, $payload);
            $tokens = $move['tokens'];
            $tokens_to_insert = $move['moved'];

            if ((int)$payload['sourcePosition'] < $position) {
                $position -= count($tokens_to_insert);
            }
        }

        if (empty($tokens_to_insert)) {
            return null;
        }

        array_splice($tokens, $position, 0, $tokens_to_insert);

        return array_values($tokens);
    }

    /**
     * Build token models from a drag payload.
     *
     * @param array $payload
     * @return array
     */
    private function buildTokensFromPayload(array $payload)
    {
        if (isset($payload['action']) && $payload['action'] === 'move') {
            return array();
        }

        $token_type = isset($payload['tokenType']) ? (string)$payload['tokenType'] : 'literal';
        $token_value = isset($payload['tokenValue']) ? (string)$payload['tokenValue'] : '';
        $token_label = isset($payload['displayLabel']) ? (string)$payload['displayLabel'] : $token_value;
        $metadata = isset($payload['metadata']) && is_array($payload['metadata']) ? $payload['metadata'] : array();
        $token_id = 'tok-' . substr(md5($token_type . '|' . $token_value . '|' . serialize($metadata)), 0, 8);

        if ($token_type === 'function') {
            return array(
                array(
                    'id' => $token_id,
                    'type' => 'function',
                    'value' => substr($token_value, -1) === '(' ? $token_value : $token_value . '(',
                    'label' => $token_label,
                    'metadata' => $metadata,
                ),
                array(
                    'id' => $token_id . '-close',
                    'type' => 'group',
                    'value' => ')',
                    'label' => '',
                    'metadata' => array('generatedBy' => $token_id, 'autoInserted' => true),
                ),
            );
        }

        return array(
            array(
                'id' => $token_id,
                'type' => $token_type,
                'value' => $token_value,
                'label' => $token_label,
                'metadata' => $metadata,
            ),
        );
    }

    /**
     * Extract the moved token or token pair from the current sequence.
     *
     * @param array $tokens
     * @param array $payload
     * @return array
     */
    private function extractMovedTokens(array $tokens, array $payload)
    {
        $source = isset($payload['sourcePosition']) ? (int)$payload['sourcePosition'] : -1;
        $moved = array();

        if ($source < 0 || !isset($tokens[$source])) {
            return array('tokens' => $tokens, 'moved' => $moved);
        }

        $moved[] = $tokens[$source];
        array_splice($tokens, $source, 1);

        if (
            isset($moved[0]['type'])
            && $moved[0]['type'] === 'function'
            && isset($tokens[$source]['type'])
            && $tokens[$source]['type'] === 'group'
            && isset($tokens[$source]['metadata']['generatedBy'])
            && $tokens[$source]['metadata']['generatedBy'] === $moved[0]['id']
        ) {
            $moved[] = $tokens[$source];
            array_splice($tokens, $source, 1);
        }

        return array('tokens' => array_values($tokens), 'moved' => $moved);
    }

    /**
     * Validate the resulting token sequence.
     *
     * @param array $tokens
     * @return bool
     */
    private function isSequenceValid(array $tokens)
    {
        $count = count($tokens);
        $depth = 0;
        $index = 0;

        if ($count === 0) {
            return true;
        }

        if (!$this->canStart($tokens[0]) || !$this->canEnd($tokens[$count - 1])) {
            return false;
        }

        while ($index < $count) {
            $token = $tokens[$index];

            if ($this->isFunction($token) || $this->isOpenGroup($token)) {
                $depth += 1;
            }

            if ($this->isCloseGroup($token)) {
                $depth -= 1;
                if ($depth < 0) {
                    return false;
                }
            }

            if ($index < $count - 1 && !$this->isTransitionValid($token, $tokens[$index + 1])) {
                return false;
            }

            $index += 1;
        }

        return $depth === 0;
    }

    /**
     * @param array $token
     * @return bool
     */
    private function canStart(array $token)
    {
        return $this->isValueStarter($token) || $this->isUnaryMinus($token);
    }

    /**
     * @param array $token
     * @return bool
     */
    private function canEnd(array $token)
    {
        return $this->isOperand($token) || $this->isCloseGroup($token);
    }

    /**
     * @param array $left
     * @param array $right
     * @return bool
     */
    private function isTransitionValid(array $left, array $right)
    {
        if ($this->isFunction($left) && $this->isCloseGroup($right)) {
            return true;
        }

        if ($this->isFunction($left) || $this->isOpenGroup($left) || $this->isBinaryOperator($left) || $this->isUnaryMinus($left)) {
            return $this->isValueStarter($right) || $this->isUnaryMinus($right);
        }

        if ($this->isOperand($left) || $this->isCloseGroup($left)) {
            return $this->isBinaryOperator($right) || $this->isCloseGroup($right);
        }

        return false;
    }

    /**
     * @param array $token
     * @return bool
     */
    private function isValueStarter(array $token)
    {
        return $this->isOperand($token) || $this->isFunction($token) || $this->isOpenGroup($token);
    }

    /**
     * @param array $token
     * @return bool
     */
    private function isOperand(array $token)
    {
        return isset($token['type']) && in_array($token['type'], array('variable', 'literal'), true);
    }

    /**
     * @param array $token
     * @return bool
     */
    private function isFunction(array $token)
    {
        return isset($token['type']) && $token['type'] === 'function';
    }

    /**
     * @param array $token
     * @return bool
     */
    private function isOpenGroup(array $token)
    {
        return isset($token['type'], $token['value']) && $token['type'] === 'group' && $token['value'] === '(';
    }

    /**
     * @param array $token
     * @return bool
     */
    private function isCloseGroup(array $token)
    {
        return isset($token['type'], $token['value']) && $token['type'] === 'group' && $token['value'] === ')';
    }

    /**
     * @param array $token
     * @return bool
     */
    private function isBinaryOperator(array $token)
    {
        return isset($token['type'], $token['value'])
            && $token['type'] === 'operator'
            && !$this->isUnaryMinus($token);
    }

    /**
     * @param array $token
     * @return bool
     */
    private function isUnaryMinus(array $token)
    {
        return isset($token['type'], $token['value'])
            && $token['type'] === 'operator'
            && $token['value'] === '-';
    }
}