/**
 * TextArea — a multi-line inset well for editing text. Focus lights the
 * accent ring; the well itself never changes color. The browser supplies the
 * caret, the selection and the clipboard; the C desktop implements its own
 * (lp_text_area).
 */

import { forwardRef, type TextareaHTMLAttributes } from 'react'
import { cx } from '@/lib/cx'
import styles from './TextArea.module.css'

export interface TextAreaProps extends TextareaHTMLAttributes<HTMLTextAreaElement> {
  /** Monospace, for code and logs. */
  mono?: boolean
}

export const TextArea = forwardRef<HTMLTextAreaElement, TextAreaProps>(function TextArea(
  { mono = false, className, spellCheck = false, ...rest },
  ref,
) {
  return <textarea ref={ref} className={cx(styles.area, mono && styles.mono, className)} spellCheck={spellCheck} {...rest} />
})
