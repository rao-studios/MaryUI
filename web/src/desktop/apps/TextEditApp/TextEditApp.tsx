/**
 * TextEditApp — the first real application: a plain-text editor with a name
 * field, a Save button, a TextArea and a status bar. Reached from Spotlight
 * only (hidden from Window › Open …). ⌘/Ctrl+S saves; the web keeps documents
 * in localStorage where the C desktop writes ~/Documents/<name>.txt.
 */

import { useEffect, useState, type KeyboardEvent, type SyntheticEvent } from 'react'
import { Button } from '@/components/Button'
import { Icon } from '@/components/Icon'
import { TextArea } from '@/components/TextArea'
import { TextField } from '@/components/TextField'
import { Toolbar, ToolbarSpacer } from '@/components/Toolbar'
import { charCount, lineCol, wordCount } from '@/lib/textStats'
import { useWMDispatch } from '@/desktop/wm/useWM'
import type { AppProps } from '@/desktop/apps/registry'
import styles from './TextEditApp.module.css'

const STORE_PREFIX = 'lp.textedit/'

/** The stored document's file name: one path component, .txt appended once. */
export function documentFileName(name: string): string {
  const base = (name.trim() || 'Untitled').replace(/\//g, '-')
  return base.endsWith('.txt') ? base : `${base}.txt`
}

function load(name: string): string | null {
  try {
    return localStorage.getItem(STORE_PREFIX + documentFileName(name))
  } catch {
    return null
  }
}

function clock(): string {
  const now = new Date()
  const hour = now.getHours() % 12 || 12
  return `${hour}:${String(now.getMinutes()).padStart(2, '0')} ${now.getHours() < 12 ? 'AM' : 'PM'}`
}

export function TextEditApp({ windowId }: AppProps) {
  const dispatch = useWMDispatch()
  const [name, setName] = useState('')
  const [text, setText] = useState(() => load('') ?? '')
  const [dirty, setDirty] = useState(false)
  const [status, setStatus] = useState(() => (load('') !== null ? `Opened ${documentFileName('')}` : ''))
  const [caret, setCaret] = useState(0)

  useEffect(() => {
    dispatch({ type: 'SET_TITLE', id: windowId, title: `${dirty ? '• ' : ''}${name.trim() || 'Untitled'}` })
  }, [dispatch, windowId, dirty, name])

  const save = () => {
    const file = documentFileName(name)
    try {
      localStorage.setItem(STORE_PREFIX + file, text)
      setDirty(false)
      setStatus(`Saved ${file} at ${clock()}`)
    } catch {
      setStatus('Could not save')
    }
  }

  const onKeyDown = (event: KeyboardEvent<HTMLDivElement>) => {
    if ((event.metaKey || event.ctrlKey) && (event.key === 's' || event.key === 'S')) {
      event.preventDefault()
      save()
    }
  }

  const trackCaret = (event: SyntheticEvent<HTMLTextAreaElement>) => setCaret(event.currentTarget.selectionStart)
  const { line, col } = lineCol(text, caret)
  const words = wordCount(text)
  const chars = charCount(text)

  return (
    <div className={styles.editor} onKeyDown={onKeyDown}>
      <Toolbar>
        <TextField
          className={styles.name}
          icon={<Icon name="document" size={14} />}
          placeholder="Untitled"
          value={name}
          onChange={(event) => {
            setName(event.target.value)
            setDirty(true)
          }}
          aria-label="Document name"
        />
        <Button size="sm" icon={<Icon name="download" size={12} />} onClick={save}>
          Save
        </Button>
        <ToolbarSpacer />
        <span className={styles.status}>{status}</span>
      </Toolbar>
      <div className={styles.body}>
        <TextArea
          className={styles.area}
          placeholder="Type something…"
          value={text}
          autoFocus
          onChange={(event) => {
            setText(event.target.value)
            setDirty(true)
            trackCaret(event)
          }}
          onSelect={trackCaret}
          onKeyUp={trackCaret}
          onClick={trackCaret}
          aria-label="Document"
        />
      </div>
      <div className={styles.statusBar}>
        {words} {words === 1 ? 'word' : 'words'} · {chars} {chars === 1 ? 'character' : 'characters'} · Ln {line}, Col {col}
        {dirty ? ' · Edited' : ''}
      </div>
    </div>
  )
}
