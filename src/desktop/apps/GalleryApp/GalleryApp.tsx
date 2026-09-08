/**
 * GalleryApp — the design system showing itself: every control in every
 * state, the surfaces, the bubbles, the tokens, and a Motion tab whose sliders
 * retune the physics live (copy the JSON patch back into tokens.json to keep).
 */

import { useState } from 'react'
import { SegmentedControl } from '@/components/SegmentedControl'
import { ScrollArea } from '@/components/ScrollArea'
import type { AppProps } from '@/desktop/apps/registry'
import { ControlsTab } from './tabs/ControlsTab'
import { SurfacesTab } from './tabs/SurfacesTab'
import { BubblesTab } from './tabs/BubblesTab'
import { TokensTab } from './tabs/TokensTab'
import { MotionTab } from './tabs/MotionTab'
import styles from './GalleryApp.module.css'

type Tab = 'controls' | 'surfaces' | 'bubbles' | 'tokens' | 'motion'

const tabs: Record<Tab, () => JSX.Element> = {
  controls: ControlsTab,
  surfaces: SurfacesTab,
  bubbles: BubblesTab,
  tokens: TokensTab,
  motion: MotionTab,
}

export function GalleryApp(_props: AppProps) {
  const [tab, setTab] = useState<Tab>('controls')
  const Body = tabs[tab]
  return (
    <div className={styles.gallery}>
      <div className={styles.tabs}>
        <SegmentedControl<Tab>
          aria-label="Gallery section"
          value={tab}
          onChange={setTab}
          options={[
            { value: 'controls', label: 'Controls' },
            { value: 'surfaces', label: 'Surfaces' },
            { value: 'bubbles', label: 'Bubbles' },
            { value: 'tokens', label: 'Tokens' },
            { value: 'motion', label: 'Motion' },
          ]}
        />
      </div>
      <ScrollArea className={styles.body}>
        <Body />
      </ScrollArea>
    </div>
  )
}
