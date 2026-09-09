/** Text statistics for editors' status bars; the C side counts the same way (lp_text_doc_*). */

/** Whitespace-separated words. */
export function wordCount(text: string): number {
  const trimmed = text.trim()
  return trimmed ? trimmed.split(/\s+/).length : 0
}

/** Code points, not UTF-16 units. */
export function charCount(text: string): number {
  return [...text].length
}

/** The 1-based line and column of a caret at `index` (a UTF-16 offset, as the DOM reports it). */
export function lineCol(text: string, index: number): { line: number; col: number } {
  const head = text.slice(0, Math.max(0, Math.min(index, text.length)))
  const lastBreak = head.lastIndexOf('\n')
  const line = (head.match(/\n/g)?.length ?? 0) + 1
  const col = [...head.slice(lastBreak + 1)].length + 1
  return { line, col }
}
