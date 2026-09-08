/** Every control, every state. */

import { useState } from 'react'
import { Button } from '@/components/Button'
import { Checkbox } from '@/components/Checkbox'
import { Icon } from '@/components/Icon'
import { ListHeader, ListRow } from '@/components/ListRow'
import { ProgressBar } from '@/components/ProgressBar'
import { SegmentedControl } from '@/components/SegmentedControl'
import { Slider } from '@/components/Slider'
import { TextField } from '@/components/TextField'
import { Toggle } from '@/components/Toggle'
import styles from '../GalleryApp.module.css'

type Range = 'day' | 'week' | 'month'

export function ControlsTab() {
  const [segment, setSegment] = useState<Range>('week')
  const [on, setOn] = useState(true)
  const [off, setOff] = useState(false)
  const [checked, setChecked] = useState(true)
  const [slider, setSlider] = useState(42)
  const [text, setText] = useState('')
  const [row, setRow] = useState('Liquid Platinum.sketch')

  return (
    <>
      <section className={styles.section}>
        <h2 className={styles.heading}>Buttons</h2>
        <div className={styles.row}>
          <Button>Default</Button>
          <Button variant="primary">Primary</Button>
          <Button variant="quiet">Quiet</Button>
          <Button icon={<Icon name="star" size={14} />}>With icon</Button>
          <Button iconOnly icon={<Icon name="gear" />}>
            Settings
          </Button>
          <Button disabled>Disabled</Button>
        </div>
        <div className={styles.row}>
          <Button size="sm">Small</Button>
          <Button size="sm" variant="primary">
            Small primary
          </Button>
          <Button size="sm" variant="quiet" icon={<Icon name="plus" size={12} />}>
            Add
          </Button>
        </div>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>Segmented control</h2>
        <p className={styles.note}>Hover the segment beside the selection and watch the metal reach for it.</p>
        <div className={styles.row}>
          <SegmentedControl<Range>
            aria-label="Range"
            value={segment}
            onChange={setSegment}
            options={[
              { value: 'day', label: 'Day' },
              { value: 'week', label: 'Week' },
              { value: 'month', label: 'Month' },
            ]}
          />
          <SegmentedControl<Range>
            size="sm"
            aria-label="View"
            value={segment}
            onChange={setSegment}
            options={[
              { value: 'day', icon: <Icon name="grid" size={13} />, ariaLabel: 'Icons' },
              { value: 'week', icon: <Icon name="list" size={13} />, ariaLabel: 'List' },
              { value: 'month', icon: <Icon name="image" size={13} />, ariaLabel: 'Gallery' },
            ]}
          />
        </div>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>Toggles and checkboxes</h2>
        <div className={styles.row}>
          <Toggle checked={on} onChange={setOn} label="Ambient perception" />
          <Toggle checked={off} onChange={setOff} label="Speak replies" />
          <Toggle checked disabled onChange={() => {}} label="Disabled on" />
          <Checkbox checked={checked} onChange={setChecked}>
            Remember through Thread
          </Checkbox>
          <Checkbox checked={false} onChange={() => {}} disabled>
            Disabled
          </Checkbox>
        </div>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>Fields and sliders</h2>
        <div className={styles.stack}>
          <TextField placeholder="Search the design system" icon={<Icon name="search" size={14} />} value={text} onChange={(e) => setText(e.target.value)} />
          <TextField round placeholder="Capsule field" />
          <TextField placeholder="Disabled" disabled />
          <Slider label="Brush opacity" value={slider} onChange={setSlider} showValue format={(v) => `${v}%`} />
          <Slider label="Disabled" value={30} onChange={() => {}} disabled />
        </div>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>Progress</h2>
        <div className={styles.stack}>
          <ProgressBar value={slider / 100} label="Determinate" />
          <ProgressBar label="Indeterminate" />
        </div>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>List rows</h2>
        <div role="grid" style={{ borderRadius: 'var(--lp-radius-sm)', overflow: 'hidden', boxShadow: 'var(--lp-shadow-emboss-well)' }}>
          <ListHeader columns={['Name', 'Modified', 'Size']} />
          {[
            ['Liquid Platinum.sketch', 'Today, 4:12 PM', '18.4 MB', 'document'],
            ['monogram.svg', 'Today, 3:48 PM', '6 KB', 'image'],
            ['tokens.json', 'Today, 5:02 PM', '9 KB', 'code'],
            ['Notes', 'Sep 5, 2026', '--', 'folder'],
          ].map(([name, modified, size, icon]) => (
            <ListRow
              key={name}
              icon={<Icon name={icon as 'document'} size={14} />}
              name={name}
              columns={[modified, size]}
              selected={row === name}
              onSelect={() => setRow(name)}
            />
          ))}
        </div>
      </section>
    </>
  )
}
