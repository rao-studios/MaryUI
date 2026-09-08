/** ScrollArea — an overflow container with thin platinum scrollbars. */

import { forwardRef, type HTMLAttributes } from 'react'
import { cx } from '@/lib/cx'
import styles from './ScrollArea.module.css'

export interface ScrollAreaProps extends HTMLAttributes<HTMLDivElement> {
  axis?: 'y' | 'x' | 'both'
}

export const ScrollArea = forwardRef<HTMLDivElement, ScrollAreaProps>(function ScrollArea(
  { axis = 'y', className, children, ...rest },
  ref,
) {
  return (
    <div ref={ref} className={cx(styles.scroll, styles[axis], className)} {...rest}>
      {children}
    </div>
  )
})
