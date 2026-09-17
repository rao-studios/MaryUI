/**
 * ObjectIcon — the lit tier: an icon drawn as an object rather than diagrammed
 * as an outline. Geometry comes from objects.json, which holds no colour; every
 * pass below is the shared recipe under `object` in tokens.json.
 *
 * Draw order, which is the doc's "two lights, one dominant" at icon scale:
 * contact shadow, body ramp, tone, brushed grain, the broad key, the 1px rim on
 * the lit edge, the 1px occlusion on the away edge, and the keyline last.
 *
 * Layered gradients, not feSpecularLighting. A filter reads alpha as a height
 * field, and at these sizes a 1px bump has no gradient to light — it collapses
 * to a flat wash or an aliased ring. Filters also allocate an offscreen surface
 * per element, which is real cost when Finder shows twenty of them, and they
 * quantise the keyline's antialiasing to the filter region's grid. The one
 * filter kept is the contact shadow, at the lit tier only.
 *
 * Nothing here animates, so an idle desktop still schedules no frames.
 *
 * Callers normally reach this through <Icon variant="object">, which handles
 * the small-size fallback; use it directly only when an object is required.
 */

import { useId, type CSSProperties } from 'react'
import { cx } from '@/lib/cx'
import { iconTier } from './tiers'
import { objectDefs, type ObjectName, type ObjectPart } from './objects'
import {
  BRUSH_PATTERN_ID,
  CONTACT_FILTER_ID,
  GLOSS_GRADIENT_ID,
  KEY_GRADIENT_ID,
  MATTE,
  MATTE_KEY_GRADIENT_ID,
  RECIPE,
  VIEWBOX,
  bodyGradientId,
  facetStops,
  formGradient,
  isDynamic,
  keylineAlpha,
  rampAt,
  rimFor,
  shapeStops,
  tintColor,
  type Finish,
  type MaterialName,
  type Shape,
} from './recipe'
import styles from './ObjectIcon.module.css'

export interface ObjectIconProps {
  name: ObjectName
  size?: number
  className?: string
  style?: CSSProperties
  title?: string
}

export function ObjectIcon({ name, size = 32, className, style, title }: ObjectIconProps) {
  const uid = useId().replace(/:/g, '')
  const def = objectDefs[name]
  const lit = iconTier(size) === 'lit'
  const silhouette = def.parts.find((p) => p.id === def.silhouette) ?? def.parts[0]
  const visible = def.parts.filter((p) => size >= (p.min ?? 0))
  const bodies = visible.filter((p) => (p.role ?? 'body') === 'body')

  /*
   * A material whose ramp is a CSS variable needs its gradient here rather than
   * in SvgDefs: a var() in a gradient stop is read against the gradient
   * element's own context, so a shared one would always report the app root's
   * appearance. Emitted per part, and only for the parts that need it.
   */
  const local = bodies.filter((p) => !p.tint && isDynamic(p.material ?? def.material))
  const localId = (partId: string) => `${uid}-g-${partId}`
  /*
   * A shape's gradient depends on the part's material AND on the finish (matte
   * slides every bright stop down the ramp), so sharing one per material in
   * SvgDefs would need shapes x materials x finishes of them. There are only
   * ever a few shaped parts in an icon, so they are emitted here instead — the
   * same trade the dynamic materials above already make.
   */
  const finish: Finish = def.finish ?? 'lit'
  const shaped = bodies.filter((p) => !p.tint && p.shape && p.shape !== 'flat')
  const shapeId = (partId: string) => `${uid}-s-${partId}`

  return (
    <svg
      className={cx(styles.icon, className)}
      width={size}
      height={size}
      viewBox={`0 0 ${VIEWBOX} ${VIEWBOX}`}
      style={style}
      role={title ? 'img' : undefined}
      aria-hidden={title ? undefined : 'true'}
    >
      {title ? <title>{title}</title> : null}

      {/*
       * Only the clip paths are per instance, and they are two to four small
       * nodes. Every expensive def — the body ramps, the grain, the key, the
       * contact filter — is shared once from SvgDefs.
       */}
      <defs>
        {bodies.map((part) => (
          <clipPath key={part.id} id={`${uid}-${part.id}`}>
            <path d={part.d} clipRule={part.fillRule} />
          </clipPath>
        ))}
        {lit ? (
          <clipPath id={`${uid}-sil`}>
            <path d={silhouette.d} />
          </clipPath>
        ) : null}
        {shaped.map((part) => {
          const g = formGradient(part.shape as Exclude<Shape, 'flat'>)
          const stops = shapeStops(part.shape as Exclude<Shape, 'flat'>, finish).map((st) => (
            <stop key={st.offset} offset={st.offset} stopColor={rampAt(part.material ?? def.material, st.t)} />
          ))
          return g.kind === 'radial' ? (
            <radialGradient key={part.id} id={shapeId(part.id)} cx={g.cx} cy={g.cy} r={g.r}>
              {stops}
            </radialGradient>
          ) : (
            <linearGradient key={part.id} id={shapeId(part.id)} x1={g.x1} y1={g.y1} x2={g.x2} y2={g.y2}>
              {stops}
            </linearGradient>
          )
        })}
        {local.map((part) => {
          const [from, to] = facetStops(part.material ?? def.material, part.facet ?? 'flat')
          return (
            <linearGradient key={part.id} id={localId(part.id)} x1="0" y1="0" x2="0.85" y2="1">
              <stop offset="0" stopColor={from} />
              <stop offset="1" stopColor={to} />
            </linearGradient>
          )
        })}
      </defs>

      <g filter={lit ? `url(#${CONTACT_FILTER_ID})` : undefined}>
        {visible.map((part) =>
          (part.role ?? 'body') === 'body' ? (
            <Body
              key={part.id}
              part={part}
              material={part.material ?? def.material}
              clip={`${uid}-${part.id}`}
              gradient={
                shaped.includes(part) ? shapeId(part.id) : local.includes(part) ? localId(part.id) : undefined
              }
              lit={lit}
              finish={finish}
            />
          ) : (
            <Line key={part.id} part={part} />
          ),
        )}

        {/* The broad key: one wash over the whole object, scaled by how specular the material is. */}
        {lit ? (
          <g
            clipPath={`url(#${uid}-sil)`}
            opacity={
              finish === 'matte'
                ? Math.min(RECIPE.gloss(def.material), MATTE.glossMax)
                : RECIPE.gloss(def.material)
            }
          >
            <path
              d={silhouette.d}
              fill={`url(#${finish === 'matte' ? MATTE_KEY_GRADIENT_ID : KEY_GRADIENT_ID})`}
              style={{ mixBlendMode: 'screen' }}
            />
          </g>
        ) : null}

        {/*
         * The glossy finish, over everything the lit recipe did. Application
         * icons only — a glossy folder would be a category error.
         */}
        {lit && finish === 'glossy' ? (
          <g clipPath={`url(#${uid}-sil)`}>
            <path d={silhouette.d} fill={`url(#${GLOSS_GRADIENT_ID})`} style={{ mixBlendMode: 'screen' }} />
          </g>
        ) : null}

        {/* Last, always: this is what keeps an object legible when it is small. */}
        <path
          d={silhouette.d}
          fill="none"
          stroke={RECIPE.keyline}
          strokeOpacity={keylineAlpha(size) / 0.32}
          strokeWidth={VIEWBOX / size}
        />
      </g>
    </svg>
  )
}

function Body({
  part,
  material,
  clip,
  gradient,
  lit,
  finish,
}: {
  part: ObjectPart
  material: MaterialName
  clip: string
  /** Set when the ramp is per instance: a CSS-variable material, or a shape. */
  gradient?: string
  lit: boolean
  finish: Finish
}) {
  const ramp = gradient ?? bodyGradientId(material, part.facet ?? 'flat')
  const fill = part.tint ? tintColor(part.tint) : `url(#${ramp})`
  const grain = RECIPE.grain(material)
  const bevel = part.bevel ?? 'none'
  /* A well is lit from the opposite side: its rim sits along the bottom. */
  const flip = bevel === 'well' ? -1 : 1
  const d = RECIPE.bevelOffset * flip

  return (
    <g clipPath={`url(#${clip})`}>
      <path d={part.d} fill={fill} fillRule={part.fillRule} />

      {/* Tone is a lighting offset, not a colour: white screened or black multiplied. */}
      {part.tone ? (
        <path
          d={part.d}
          fill={part.tone > 0 ? '#fff' : '#000'}
          opacity={Math.abs(part.tone) * RECIPE.toneStep}
        />
      ) : null}

      {lit && grain > 0 && !part.tint ? (
        <path
          d={part.d}
          fill={`url(#${BRUSH_PATTERN_ID})`}
          opacity={RECIPE.grainOpacity * grain}
          style={{ mixBlendMode: 'overlay' }}
        />
      ) : null}

      {bevel !== 'none' ? (
        <>
          <path d={part.d} fill="none" stroke={rimFor(finish)} strokeWidth={2} transform={`translate(${d} ${d})`} />
          <path d={part.d} fill="none" stroke={RECIPE.occlusion} strokeWidth={2} transform={`translate(${-d} ${-d})`} />
        </>
      ) : null}
    </g>
  )
}

/**
 * A crease is the 1px emboss pair the rest of the system already uses, at icon
 * scale: a light line just below a dark one. A tinted crease is a mark rather
 * than a fold, so it drops the letterpress and just draws.
 */
function Line({ part }: { part: ObjectPart }) {
  if (part.tint)
    return (
      <path
        d={part.d}
        fill="none"
        stroke={tintColor(part.tint)}
        strokeWidth={1.4}
        strokeLinecap="round"
        strokeLinejoin="round"
      />
    )
  return (
    <>
      <path d={part.d} fill="none" stroke={RECIPE.rim} strokeWidth={1} strokeLinecap="round" transform="translate(0 0.85)" />
      <path d={part.d} fill="none" stroke={RECIPE.keyline} strokeWidth={1} strokeLinecap="round" />
    </>
  )
}
