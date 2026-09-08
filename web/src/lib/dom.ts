/** Small DOM helpers shared by the motion engine and the hooks. */

export function setVars(el: HTMLElement, vars: Record<string, string | number>): void {
  for (const [name, value] of Object.entries(vars)) el.style.setProperty(name, String(value))
}

/** True when the OS asks for reduced motion or the View menu forces it. */
export function prefersReducedMotion(): boolean {
  if (typeof window === 'undefined') return false
  if (document.documentElement.dataset.reducedMotion === 'on') return true
  return window.matchMedia?.('(prefers-reduced-motion: reduce)').matches ?? false
}

let svgFilterSupport: boolean | null = null

/** Whether `filter: url(#id)` is usable on HTML elements; the goo layers turn off otherwise. */
export function supportsSvgFilter(): boolean {
  if (svgFilterSupport !== null) return svgFilterSupport
  svgFilterSupport =
    typeof CSS !== 'undefined' && typeof CSS.supports === 'function' ? CSS.supports('filter', 'url(#lp-probe)') : false
  return svgFilterSupport
}

export function isMac(): boolean {
  return typeof navigator !== 'undefined' && /Mac|iPhone|iPad/.test(navigator.platform)
}
