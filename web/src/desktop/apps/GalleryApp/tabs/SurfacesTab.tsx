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
        <p className={styles.note}>
          Adjacent controls share one filtered layer. The blobs carry no lighting of their own — the filter thresholds
          them into a silhouette and lights <em>that</em>, which is what gives a merged bead volume. Hover to loosen the
          tension and watch them neck; drag the window to see them trail and bridge.
        </p>
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
        <h2 className={styles.heading}>Merge tension</h2>
        <p className={styles.note}>
          The same three drops at rest and held open. `rest` thresholds steeply so beads stay distinct; `flow` blurs
          further so they bridge. Both cut at the same alpha, so the silhouette keeps its size when tension changes.
        </p>
        <div className={styles.row}>
          {([false, true] as const).map((flowing) => (
            <div key={String(flowing)} style={{ display: 'grid', justifyItems: 'center', gap: 6 }}>
              <GooGroup size="sm" blobs={{ count: 3, size: 26, shape: 'circle' }} gap={10} flowing={flowing}>
                {[0, 1, 2].map((i) => (
                  <span key={i} data-goo-index={i} style={{ width: 26, height: 26 }} />
                ))}
              </GooGroup>
              <span className={styles.mono} style={{ color: 'var(--lp-ink-tertiary)' }}>{flowing ? 'flow' : 'rest'}</span>
            </div>
          ))}
        </div>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>Liquid corners</h2>
        <p className={styles.note}>
          A window's four corners each run their own spring, detuned against each other, between{' '}
          <code className={styles.mono}>radius.window-min</code> and <code className={styles.mono}>radius.window-max</code>.
          Fling this window sideways: the corners it leads with flatten, the ones it trails with round, and they arrive
          home one after another rather than together.
        </p>
        <div className={styles.row}>
          {([['8px', 'min'], ['12px', 'rest'], ['16px', 'max']] as const).map(([r, label]) => (
            <div key={label} style={{ display: 'grid', justifyItems: 'center', gap: 6 }}>
              <Surface variant="raised" style={{ width: 76, height: 56, borderRadius: r, boxShadow: 'var(--lp-shadow-emboss-raised)' }} />
              <span className={styles.mono} style={{ color: 'var(--lp-ink-tertiary)' }}>{`${label} ${r}`}</span>
            </div>
          ))}
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
