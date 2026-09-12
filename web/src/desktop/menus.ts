/**
 * The menu bar's models: what each menu says and what it does. Kept as data so
 * the MenuBar stays a dumb renderer and the same commands can be bound to keys.
 */

import type { MenuEntry } from '@/components/Menu'
import type { WMState } from './wm/types'
import type { WMStore } from './wm/useWM'
import { apps, openApp } from './apps/registry'
import { updateSettings, type Settings } from './settings'

export interface MenuModel {
  id: string
  label: string
  entries: MenuEntry[]
}

const SEP: MenuEntry = { separator: true }

export function buildMenus(store: WMStore, state: WMState, settings: Settings): MenuModel[] {
  const focused = state.focusedId ? state.windows[state.focusedId] : null
  const dispatch = store.dispatch

  const windowEntries: MenuEntry[] = state.order.map((id) => {
    const w = state.windows[id]
    return {
      id: `win-${id}`,
      label: `${w.state === 'shaded' ? '◇ ' : ''}${w.title}`,
      checked: state.focusedId === id,
      onSelect: () => {
        if (w.state === 'shaded') dispatch({ type: 'TOGGLE_SHADE', id })
        dispatch({ type: 'FOCUS', id })
      },
    }
  })

  return [
    {
      id: 'rao',
      label: 'Rao',
      entries: [
        { id: 'about', label: 'About Liquid Platinum', onSelect: () => openApp(store, 'about') },
        SEP,
        { id: 'gallery', label: 'Design System…', onSelect: () => openApp(store, 'gallery') },
        SEP,
        {
          id: 'reduced',
          label: 'Reduce Motion',
          checked: settings.reducedMotion,
          onSelect: () => updateSettings({ reducedMotion: !settings.reducedMotion }),
        },
      ],
    },
    {
      id: 'file',
      label: 'File',
      entries: [
        { id: 'new', label: 'New Finder Window', shortcut: '⌘N', onSelect: () => openApp(store, 'finder') },
        { id: 'open', label: 'Open…', shortcut: '⌘O', disabled: true },
        SEP,
        {
          id: 'close',
          label: 'Close Window',
          shortcut: '⌘W',
          disabled: !focused,
          onSelect: () => focused && dispatch({ type: 'CLOSE', id: focused.id }),
        },
        SEP,
        { id: 'info', label: 'Get Info', shortcut: '⌘I', disabled: true },
      ],
    },
    {
      id: 'edit',
      label: 'Edit',
      entries: [
        { id: 'undo', label: 'Undo', shortcut: '⌘Z', disabled: true },
        { id: 'redo', label: 'Redo', shortcut: '⇧⌘Z', disabled: true },
        SEP,
        { id: 'cut', label: 'Cut', shortcut: '⌘X', disabled: true },
        { id: 'copy', label: 'Copy', shortcut: '⌘C', disabled: true },
        { id: 'paste', label: 'Paste', shortcut: '⌘V', disabled: true },
        SEP,
        { id: 'all', label: 'Select All', shortcut: '⌘A', disabled: true },
      ],
    },
    {
      id: 'view',
      label: 'View',
      entries: [
        {
          id: 'goo',
          label: 'Liquid Merge',
          checked: settings.goo,
          onSelect: () => updateSettings({ goo: !settings.goo }),
        },
        SEP,
        {
          id: 'wp-molten',
          label: 'Molten Wallpaper',
          checked: settings.wallpaper === 'molten',
          onSelect: () => updateSettings({ wallpaper: 'molten' }),
        },
        {
          id: 'wp-proc',
          label: 'Procedural Wallpaper',
          checked: settings.wallpaper === 'procedural',
          onSelect: () => updateSettings({ wallpaper: 'procedural' }),
        },
        {
          id: 'wp-raster',
          label: 'Raster Wallpaper',
          checked: settings.wallpaper === 'raster',
          onSelect: () => updateSettings({ wallpaper: 'raster' }),
        },
        SEP,
        {
          id: 'wp-tone-platinum',
          label: 'Molten · Platinum',
          checked: settings.moltenTone === 'platinum',
          disabled: settings.wallpaper !== 'molten',
          onSelect: () => updateSettings({ moltenTone: 'platinum' }),
        },
        {
          id: 'wp-tone-faithful',
          label: 'Molten · Faithful',
          checked: settings.moltenTone === 'faithful',
          disabled: settings.wallpaper !== 'molten',
          onSelect: () => updateSettings({ moltenTone: 'faithful' }),
        },
        SEP,
        {
          id: 'accent-blue',
          label: 'Blue Appearance',
          checked: settings.accent === 'blue',
          onSelect: () => updateSettings({ accent: 'blue' }),
        },
        {
          id: 'accent-graphite',
          label: 'Graphite Appearance',
          checked: settings.accent === 'graphite',
          onSelect: () => updateSettings({ accent: 'graphite' }),
        },
        SEP,
        {
          id: 'folders-manila',
          label: 'Folders · Manila',
          checked: settings.folders === 'manila',
          onSelect: () => updateSettings({ folders: 'manila' }),
        },
        {
          id: 'folders-slate',
          label: 'Folders · Slate',
          checked: settings.folders === 'slate',
          onSelect: () => updateSettings({ folders: 'slate' }),
        },
      ],
    },
    {
      id: 'window',
      label: 'Window',
      entries: [
        {
          id: 'shade',
          label: focused?.state === 'shaded' ? 'Unshade' : 'Shade',
          shortcut: '⌘M',
          disabled: !focused,
          onSelect: () => focused && dispatch({ type: 'TOGGLE_SHADE', id: focused.id }),
        },
        {
          id: 'zoom',
          label: focused?.state === 'zoomed' ? 'Restore' : 'Zoom',
          disabled: !focused,
          onSelect: () => focused && dispatch({ type: 'TOGGLE_ZOOM', id: focused.id }),
        },
        { id: 'cycle', label: 'Cycle Through Windows', shortcut: '⌃`', disabled: state.order.length < 2, onSelect: () => dispatch({ type: 'FOCUS_NEXT' }) },
        SEP,
        ...Object.values(apps)
          .filter((app) => !app.hidden)
          .map((app) => ({
          id: `open-${app.id}`,
            label: `Open ${app.title}`,
            onSelect: () => openApp(store, app.id),
          })),
        ...(windowEntries.length ? [SEP, ...windowEntries] : []),
      ],
    },
    {
      id: 'help',
      label: 'Help',
      entries: [
        { id: 'help', label: 'Liquid Platinum Help', disabled: true },
        { id: 'readme', label: 'Read the README', onSelect: () => window.open('https://github.com/', '_blank', 'noopener') },
      ],
    },
  ]
}
