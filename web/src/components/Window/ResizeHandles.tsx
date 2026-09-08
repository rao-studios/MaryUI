/** The eight invisible grab zones around a window's edge. */

import { RESIZE_HANDLES } from '@/lib/geometry'
import { cx } from '@/lib/cx'
import type { ResizeStarter } from '@/hooks/useResize'
import styles from './Window.module.css'

export function ResizeHandles({ onStart }: { onStart: ResizeStarter }) {
  return (
    <>
      {RESIZE_HANDLES.map((handle) => (
        <div key={handle} className={cx(styles.handle, styles[`h_${handle}`])} data-no-drag="" onPointerDown={onStart(handle)} />
      ))}
    </>
  )
}
