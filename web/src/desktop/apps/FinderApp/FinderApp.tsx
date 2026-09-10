/**
 * FinderApp — a file browser in the Rao chrome: toolbar, source-list sidebar,
 * icon or list view, status bar. A stress test for every primitive inside a
 * brushed window.
 */

import { useMemo, useState } from 'react'
import { Button } from '@/components/Button'
import { Icon } from '@/components/Icon'
import { ListHeader, ListRow } from '@/components/ListRow'
import { ScrollArea } from '@/components/ScrollArea'
import { SegmentedControl } from '@/components/SegmentedControl'
import { Sidebar, SidebarItem, SidebarSection } from '@/components/Sidebar'
import { TextField } from '@/components/TextField'
import { Toolbar, ToolbarGroup, ToolbarSpacer } from '@/components/Toolbar'
import { useWMDispatch } from '@/desktop/wm/useWM'
import type { AppProps } from '@/desktop/apps/registry'
import { allLocations, favorites, kindIcon, kindLabel, locations } from './files'
import styles from './FinderApp.module.css'

type ViewMode = 'icons' | 'list'

export function FinderApp({ windowId }: AppProps) {
  const dispatch = useWMDispatch()
  const [locationId, setLocationId] = useState('repositories')
  const [view, setView] = useState<ViewMode>('icons')
  const [query, setQuery] = useState('')
  const [selected, setSelected] = useState<string | null>(null)

  const location = allLocations.find((l) => l.id === locationId) ?? allLocations[0]
  const files = useMemo(() => {
    const q = query.trim().toLowerCase()
    return q ? location.files.filter((f) => f.name.toLowerCase().includes(q)) : location.files
  }, [location, query])

  const choose = (id: string) => {
    setLocationId(id)
    setSelected(null)
    dispatch({ type: 'SET_TITLE', id: windowId, title: allLocations.find((l) => l.id === id)?.name ?? 'Rao' })
  }

  return (
    <div className={styles.finder}>
      <Toolbar>
        <ToolbarGroup>
          <Button variant="quiet" size="sm" iconOnly icon={<Icon name="chevronLeft" />} disabled>
            Back
          </Button>
          <Button variant="quiet" size="sm" iconOnly icon={<Icon name="chevronRight" />} disabled>
            Forward
          </Button>
        </ToolbarGroup>
        <SegmentedControl<ViewMode>
          size="sm"
          aria-label="View"
          value={view}
          onChange={setView}
          options={[
            { value: 'icons', icon: <Icon name="grid" size={14} />, ariaLabel: 'Icons' },
            { value: 'list', icon: <Icon name="list" size={14} />, ariaLabel: 'List' },
          ]}
        />
        <ToolbarSpacer />
        <TextField
          round
          icon={<Icon name="search" size={14} />}
          placeholder="Search"
          value={query}
          onChange={(event) => setQuery(event.target.value)}
          className={styles.search}
        />
      </Toolbar>

      <div className={styles.split}>
        <Sidebar>
          <SidebarSection title="Favorites">
            {favorites.map((l) => (
              <SidebarItem key={l.id} icon={<Icon name={l.icon} size={15} />} selected={l.id === locationId} onSelect={() => choose(l.id)}>
                {l.name}
              </SidebarItem>
            ))}
          </SidebarSection>
          <SidebarSection title="Locations">
            {locations.map((l) => (
              <SidebarItem key={l.id} icon={<Icon name={l.icon} size={15} />} selected={l.id === locationId} onSelect={() => choose(l.id)}>
                {l.name}
              </SidebarItem>
            ))}
          </SidebarSection>
        </Sidebar>

        <ScrollArea className={styles.content} onClick={(e) => e.target === e.currentTarget && setSelected(null)}>
          {view === 'icons' ? (
            <div className={styles.grid} role="grid" aria-label={location.name}>
              {files.map((file) => (
                <button
                  key={file.name}
                  type="button"
                  className={styles.tile}
                  data-selected={selected === file.name ? '' : undefined}
                  onClick={() => setSelected(file.name)}
                  onDoubleClick={() => file.kind === 'folder' && choose(locationId)}
                >
                  <span className={styles.tileIcon} data-kind={file.kind}>
                    <Icon name={kindIcon[file.kind]} size={48} strokeWidth={1.6} />
                  </span>
                  <span className={styles.tileName}>{file.name}</span>
                </button>
              ))}
            </div>
          ) : (
            <div role="grid" aria-label={location.name}>
              <ListHeader columns={['Name', 'Date Modified', 'Size', 'Kind']} />
              {files.map((file) => (
                <ListRow
                  key={file.name}
                  icon={<Icon name={kindIcon[file.kind]} size={14} />}
                  name={file.name}
                  columns={[file.modified, file.size, kindLabel[file.kind]]}
                  selected={selected === file.name}
                  onSelect={() => setSelected(file.name)}
                />
              ))}
            </div>
          )}
          {files.length === 0 ? <p className={styles.empty}>No items match “{query}”.</p> : null}
        </ScrollArea>
      </div>

      <div className={styles.status}>
        {selected ? `“${selected}” selected · ` : ''}
        {files.length} item{files.length === 1 ? '' : 's'}, 412.8 GB available
      </div>
    </div>
  )
}
