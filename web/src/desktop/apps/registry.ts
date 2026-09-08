/**
 * The desktop's applications. Each entry says how a window for it opens and
 * which component fills it. Windows render `component` with their own id.
 */

import type { ComponentType } from 'react'
import type { Rect, Size } from '@/lib/geometry'
import type { WMStore } from '@/desktop/wm/useWM'
import { FinderApp } from './FinderApp'
import { GalleryApp } from './GalleryApp'
import { AboutApp } from './AboutApp'

export interface AppProps {
  windowId: string
}

export interface AppDefinition {
  id: string
  title: string
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
    component: FinderApp,
    defaultRect: { x: 72, y: 72, w: 720, h: 460 },
    minSize: { w: 420, h: 240 },
  },
  gallery: {
    id: 'gallery',
    title: 'Liquid Platinum',
    component: GalleryApp,
    defaultRect: { x: 520, y: 140, w: 760, h: 560 },
    minSize: { w: 520, h: 320 },
    singleton: true,
  },
  about: {
    id: 'about',
    title: 'About Liquid Platinum',
    component: AboutApp,
    defaultRect: { x: 360, y: 180, w: 380, h: 300 },
    minSize: { w: 380, h: 300 },
    singleton: true,
    resizable: false,
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
