import { useMemo } from 'react'

/**
 * A tokeniser for pi's dialect, small enough to read.
 *
 * Colour carries the one distinction that matters when scanning a clause:
 * variables (uppercase or leading underscore) against everything else. Numbers,
 * quoted strings and the `:-` neck get their own so a rule's shape is visible
 * without reading it.
 */

type Kind = 'var' | 'atom' | 'num' | 'str' | 'op' | 'punct' | 'neck' | 'space'

interface Token {
  kind: Kind
  text: string
}

const VAR_START = /[A-Z_]/
const ATOM_START = /[a-z]/
const WORD = /[A-Za-z0-9_]/
const DIGIT = /[0-9]/
const OPERATOR = /[=<>!+\-*/;\\]/

export function tokenise(source: string): Token[] {
  const tokens: Token[] = []
  let i = 0

  const push = (kind: Kind, text: string) => {
    const last = tokens[tokens.length - 1]
    if (last && last.kind === kind) last.text += text
    else tokens.push({ kind, text })
  }

  while (i < source.length) {
    const c = source[i]

    if (c === "'") {
      let text = c
      i++
      while (i < source.length) {
        text += source[i]
        if (source[i] === "'") {
          i++
          break
        }
        i++
      }
      push('str', text)
      continue
    }

    if (c === ':' && source[i + 1] === '-') {
      push('neck', ':-')
      i += 2
      continue
    }

    if (c === '?' && source[i + 1] === '-') {
      push('neck', '?-')
      i += 2
      continue
    }

    if (DIGIT.test(c)) {
      let text = ''
      while (i < source.length && /[0-9.]/.test(source[i])) text += source[i++]
      push('num', text)
      continue
    }

    if (VAR_START.test(c)) {
      let text = ''
      while (i < source.length && WORD.test(source[i])) text += source[i++]
      push('var', text)
      continue
    }

    if (ATOM_START.test(c)) {
      let text = ''
      while (i < source.length && WORD.test(source[i])) text += source[i++]
      push('atom', text)
      continue
    }

    if (OPERATOR.test(c)) {
      let text = ''
      while (i < source.length && OPERATOR.test(source[i])) text += source[i++]
      push('op', text)
      continue
    }

    if (/\s/.test(c)) {
      push('space', c)
      i++
      continue
    }

    push('punct', c)
    i++
  }

  return tokens
}

export function PrologText({ source }: { source: string }) {
  const tokens = useMemo(() => tokenise(source), [source])
  return (
    <>
      {tokens.map((token, index) =>
        token.kind === 'space' ? (
          token.text
        ) : (
          <span key={index} className={`tok-${token.kind}`}>
            {token.text}
          </span>
        ),
      )}
    </>
  )
}
