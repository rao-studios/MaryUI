/**
 * The desktop's applications. Each entry says how a window for it opens and
 * which component fills it. Windows render `component` with their own id.
 */

import type { ComponentType } from 'react'
import type { IconName } from '@/components/Icon'
import type { Rect, Size } from '@/lib/geometry'
import type { WMStore } from '@/desktop/wm/useWM'
import { FinderApp } from './FinderApp'
import { GalleryApp } from './GalleryApp'
import { AboutApp } from './AboutApp'
import { TextEditApp } from './TextEditApp'

export interface AppProps {
  windowId: string
}

export interface AppDefinition {
  id: string
  /** The window title. */
  title: string
  /** The display name in Spotlight (defaults to the title). */
  name?: string
  /** The Spotlight tile. */
  icon?: IconName
  /** Reachable from Spotlight only: not listed under Window › Open …. */
  hidden?: boolean
  component: ComponentType<AppProps>
  defaultRect?: Partial<Rect>
  minSize?: Size
  singleton?: boolean
  resizable?: boolean
}

export const apps: Record<string, AppDefinition> = {
  finder: {
    id: 'finder',
    title: 'Rao',
    name: 'Finder',
    icon: 'appFinder',
    component: FinderApp,
    defaultRect: { x: 72, y: 72, w: 720, h: 460 },
    minSize: { w: 420, h: 240 },
  },
  gallery: {
    id: 'gallery',
    title: 'Liquid Platinum',
    name: 'Gallery',
    icon: 'drop',
    component: GalleryApp,
    defaultRect: { x: 520, y: 140, w: 760, h: 560 },
    /* Wide enough for the seven tabs at full segmented metrics. */
    minSize: { w: 640, h: 320 },
    singleton: true,
  },
  about: {
    id: 'about',
    title: 'About Liquid Platinum',
    name: 'About',
    icon: 'info',
    component: AboutApp,
    defaultRect: { x: 360, y: 180, w: 380, h: 300 },
    minSize: { w: 380, h: 300 },
    singleton: true,
    resizable: false,
  },
  textedit: {
    id: 'textedit',
    title: 'Untitled',
    name: 'TextEdit',
    icon: 'pencil',
    hidden: true,
    component: TextEditApp,
    defaultRect: { x: 200, y: 120, w: 560, h: 420 },
    minSize: { w: 320, h: 220 },
  },
}

export function openApp(store: WMStore, appId: string): void {
  const app = apps[appId]
  if (!app) return
  store.dispatch({
    type: 'OPEN',
    spec: {
      appId: app.id,
      title: app.title,
      rect: app.defaultRect,
      minSize: app.minSize,
      singleton: app.singleton,
      resizable: app.resizable,
    },
  })
}
