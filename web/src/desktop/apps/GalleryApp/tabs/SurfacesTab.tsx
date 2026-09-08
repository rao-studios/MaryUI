/** The metal itself: every Surface variant, plus a goo-grouped toolbar. */

import { Button } from '@/components/Button'
import { GooGroup } from '@/components/GooGroup'
import { Icon } from '@/components/Icon'
import { Surface, type SurfaceVariant } from '@/components/Surface'
import styles from '../GalleryApp.module.css'

const variants: SurfaceVariant[] = ['raised', 'flat', 'titlebar', 'bar', 'well', 'body']

export function SurfacesTab() {
  return (
    <>
      <section className={styles.section}>
        <h2 className={styles.heading}>Surface variants</h2>
        <p className={styles.note}>Base gradient, brushed grain, sheen. Only the two gradient stops change between variants.</p>
        <div className={styles.tiles}>
          {variants.map((variant) => (
            <Surface key={variant} variant={variant} sheen={variant !== 'well' && variant !== 'body'} className={styles.tile}>
              {variant}
            </Surface>
          ))}
        </div>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>Liquid merge</h2>
        <p className={styles.note}>Adjacent controls share a goo-filtered metal layer; hover to see them flow together.</p>
        <div className={styles.row}>
          <GooGroup size="sm" blobs={{ count: 4, size: 30, shape: 'circle' }} gap={6}>
            {(['home', 'folder', 'star', 'gear'] as const).map((name, i) => (
              <button key={name} type="button" data-goo-index={i} aria-label={name} style={{ width: 30, height: 30, display: 'grid', placeItems: 'center', color: 'var(--lp-ink-secondary)' }}>
                <Icon name={name} size={16} />
              </button>
            ))}
          </GooGroup>
          <GooGroup size="sm" blobs={{ count: 3, size: 22, shape: 'pill' }} gap={4} style={{ height: 22, width: 260 }}>
            {['Cut', 'Copy', 'Paste'].map((label, i) => (
              <button key={label} type="button" data-goo-index={i} style={{ flex: 1, height: 22, fontSize: 12, color: 'var(--lp-ink-primary)', textShadow: '0 1px 0 var(--lp-ink-emboss)' }}>
                {label}
              </button>
            ))}
          </GooGroup>
        </div>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>Emboss</h2>
        <div className={styles.row}>
          <Surface variant="raised" className={styles.tile} style={{ width: 180, boxShadow: 'var(--lp-shadow-emboss-raised)' }}>
            raised
          </Surface>
          <Surface variant="well" className={styles.tile} style={{ width: 180 }}>
            well
          </Surface>
          <Surface variant="raised" className={styles.tile} style={{ width: 180, boxShadow: 'var(--lp-shadow-emboss-pressed)' }}>
            pressed
          </Surface>
          <Button variant="primary">accent</Button>
        </div>
      </section>
    </>
  )
}
