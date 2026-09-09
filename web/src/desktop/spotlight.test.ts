import { describe, expect, it } from 'vitest'
import { initialState, reducer } from './wm/reducer'
import type { WMState } from './wm/types'
import { apps } from './apps/registry'
import { buildSpotlightItems, initialSpotlight, spotlightReducer, spotlightResults, SPOTLIGHT_MAX_RESULTS } from './spotlight'

const bounds = { x: 0, y: 24, w: 1280, h: 776 }
const registry = Object.values(apps)

function open(state: WMState, appId = 'finder') {
  const app = apps[appId]
  return reducer(state, { type: 'OPEN', spec: { appId, title: app.title, singleton: app.singleton } })
}

function results(query: string, wm: WMState = initialState(bounds)) {
  return spotlightResults(buildSpotlightItems(registry, wm), query)
}

describe('spotlight', () => {
  it('empty query shows dock', () => {
    const wm = open(initialState(bounds))
    const dock = results('', wm)
    expect(dock.map((i) => i.title)).toEqual(['Finder', 'Gallery', 'About', 'TextEdit', 'Terminal'])
    expect(dock[4].kind).toBe('command')
    expect(dock[0].running).toBe(true)
    expect(dock[1].running).toBe(false)
    expect(dock.every((i) => i.kind !== 'window')).toBe(true)
    expect(results('   ', wm)).toHaveLength(5)
  })

  it('filters apps by title prefix and substring', () => {
    expect(results('te').map((i) => i.title)).toEqual(['TextEdit', 'Terminal'])
    const edit = results('edit')
    expect(edit.map((i) => i.title)).toEqual(['TextEdit'])
    expect(edit[0].subtitle).toBe('Application')
  })

  it('ranks prefix matches first', () => {
    expect(results('a').map((i) => i.title)).toEqual(['About', 'Gallery', 'Terminal'])
  })

  it('is case-insensitive', () => {
    expect(results('GAL').map((i) => i.title)).toEqual(['Gallery'])
  })

  it('includes open windows', () => {
    const wm = open(initialState(bounds))
    const rao = results('rao', wm)
    expect(rao).toHaveLength(1)
    expect(rao[0]).toMatchObject({ kind: 'window', id: 'w1', title: 'Rao', subtitle: 'Window · Finder' })
  })

  it('wraps selection', () => {
    let s = { ...initialSpotlight, open: true, selection: 4 }
    s = spotlightReducer(s, { type: 'MOVE', delta: 1, count: 5 })
    expect(s.selection).toBe(0)
    s = spotlightReducer(s, { type: 'MOVE', delta: -1, count: 5 })
    expect(s.selection).toBe(4)
    s = spotlightReducer(s, { type: 'MOVE', delta: 1, count: 0 })
    expect(s.selection).toBe(0)
  })

  it('resets the selection when the query changes', () => {
    let s = spotlightReducer(initialSpotlight, { type: 'OPEN' })
    s = spotlightReducer(s, { type: 'SELECT', index: 3 })
    s = spotlightReducer(s, { type: 'SET_QUERY', query: 'te' })
    expect(s.selection).toBe(0)
    expect(s.query).toBe('te')
  })

  it('toggle opens with an empty query and closes', () => {
    let s = spotlightReducer(initialSpotlight, { type: 'SET_QUERY', query: 'stale' })
    s = spotlightReducer(s, { type: 'SELECT', index: 2 })
    s = spotlightReducer(s, { type: 'TOGGLE' })
    expect(s).toEqual({ open: true, query: '', selection: 0 })
    s = spotlightReducer(s, { type: 'TOGGLE' })
    expect(s.open).toBe(false)
    expect(spotlightReducer(s, { type: 'CLOSE' })).toBe(s)
  })

  it('caps results at eight', () => {
    let wm = initialState(bounds)
    for (let i = 0; i < 10; i++) wm = open(wm)
    expect(wm.order).toHaveLength(10)
    expect(results('rao', wm)).toHaveLength(SPOTLIGHT_MAX_RESULTS)
  })

  it('hides Spotlight-only apps from the registry listing but not from the dock', () => {
    expect(apps.textedit.hidden).toBe(true)
    expect(results('').some((i) => i.id === 'textedit')).toBe(true)
  })
})
