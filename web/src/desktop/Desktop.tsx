/**
 * Desktop — the simulated machine. Owns the window-manager store, keeps its
 * bounds in step with the viewport, mounts the shared SVG defs, the wallpaper,
 * the windows and Spotlight, and opens the starting apps once. There is no
 * standalone menu bar: app commands live inside Spotlight (see SpotlightHost).
 */

import { useCallback, useEffect, useReducer, useRef, useState } from 'react'
import { SvgDefs } from '@/components/SvgDefs'
import { useDesktopKeys } from '@/hooks/useDesktopKeys'
import { openApp } from './apps/registry'
import { initialSpotlight, spotlightReducer } from './spotlight'
import { SpotlightHost } from './SpotlightHost'
import { Wallpaper } from './Wallpaper'
import { WindowLayer } from './WindowLayer'
import { initialState, reducer } from './wm/reducer'
import { createStore } from './wm/store'
import { WMContext, type WMStore } from './wm/useWM'
import styles from './Desktop.module.css'

function boundsOf(width: number, height: number) {
  return { x: 0, y: 0, w: width, h: height }
}

export function Desktop() {
  const rootRef = useRef<HTMLDivElement>(null)
  const [store] = useState<WMStore>(() =>
    createStore(reducer, initialState(boundsOf(window.innerWidth, window.innerHeight))),
  )

  useEffect(() => {
    const el = rootRef.current
    if (!el) return
    const observer = new ResizeObserver(() => {
      store.dispatch({ type: 'SET_BOUNDS', bounds: boundsOf(el.clientWidth, el.clientHeight) })
    })
    observer.observe(el)
    return () => observer.disconnect()
  }, [store])

  useEffect(() => {
    if (store.getState().order.length > 0) return
    openApp(store, 'finder')
    openApp(store, 'gallery')
  }, [store])

  const [spotlight, dispatchSpotlight] = useReducer(spotlightReducer, initialSpotlight)
  const onEscape = useCallback(() => dispatchSpotlight({ type: 'CLOSE' }), [])
  const onToggleSpotlight = useCallback(() => dispatchSpotlight({ type: 'TOGGLE' }), [])
  useDesktopKeys(store, { onEscape, onToggleSpotlight, suspended: spotlight.open })

  return (
    <WMContext.Provider value={store}>
      <div ref={rootRef} className={styles.desktop}>
        <SvgDefs />
        <Wallpaper />
        <div className={styles.windows}>
          <WindowLayer />
        </div>
        <SpotlightHost state={spotlight} dispatch={dispatchSpotlight} />
      </div>
    </WMContext.Provider>
  )
}
