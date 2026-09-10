/**
 * A minimal WebGL surface for the molten wallpaper: one fullscreen triangle,
 * one program, a handful of uniforms. It owns no clock and no loop — the caller
 * decides when a frame happens and what time to draw, which is what lets the
 * wallpaper move only while the desktop is already animating.
 *
 * `create` returns null when WebGL is unavailable or the shader will not
 * compile, so the caller can fall back rather than show a blank desktop.
 */

import { MOLTEN_FRAGMENT, MOLTEN_VERTEX, type MoltenGrade } from './moltenShader'

export interface MoltenRenderer {
  /** Size the drawing buffer. Returns true if it changed. */
  resize(cssWidth: number, cssHeight: number, scale: number): boolean
  draw(time: number, zoom: number, grade: MoltenGrade): void
  readonly lost: boolean
  dispose(): void
}

function compile(gl: WebGLRenderingContext, type: number, src: string): WebGLShader | null {
  const shader = gl.createShader(type)
  if (!shader) return null
  gl.shaderSource(shader, src)
  gl.compileShader(shader)
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    console.warn('molten: shader failed to compile\n', gl.getShaderInfoLog(shader))
    gl.deleteShader(shader)
    return null
  }
  return shader
}

export function createMoltenRenderer(canvas: HTMLCanvasElement): MoltenRenderer | null {
  let gl: WebGLRenderingContext | null = null
  try {
    gl = (canvas.getContext('webgl', { alpha: false, antialias: false, depth: false, powerPreference: 'low-power' }) ??
      canvas.getContext('experimental-webgl', { alpha: false })) as WebGLRenderingContext | null
  } catch {
    return null
  }
  if (!gl) return null

  const vs = compile(gl, gl.VERTEX_SHADER, MOLTEN_VERTEX)
  const fs = compile(gl, gl.FRAGMENT_SHADER, MOLTEN_FRAGMENT)
  if (!vs || !fs) return null

  const program = gl.createProgram()
  if (!program) return null
  gl.attachShader(program, vs)
  gl.attachShader(program, fs)
  gl.linkProgram(program)
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    console.warn('molten: program failed to link\n', gl.getProgramInfoLog(program))
    return null
  }
  gl.useProgram(program)

  // One triangle large enough to cover the clip volume; no index buffer, no quad seam.
  const buffer = gl.createBuffer()
  gl.bindBuffer(gl.ARRAY_BUFFER, buffer)
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW)
  const attrib = gl.getAttribLocation(program, 'a_position')
  gl.enableVertexAttribArray(attrib)
  gl.vertexAttribPointer(attrib, 2, gl.FLOAT, false, 0, 0)

  const u = {
    resolution: gl.getUniformLocation(program, 'u_resolution'),
    time: gl.getUniformLocation(program, 'u_time'),
    zoom: gl.getUniformLocation(program, 'u_zoom'),
    base: gl.getUniformLocation(program, 'u_base'),
    lift: gl.getUniformLocation(program, 'u_lift'),
    gain: gl.getUniformLocation(program, 'u_gain'),
    saturation: gl.getUniformLocation(program, 'u_saturation'),
  }

  let lost = false
  const onLost = (event: Event) => {
    event.preventDefault()
    lost = true
  }
  canvas.addEventListener('webglcontextlost', onLost)

  return {
    get lost() {
      return lost || (gl as WebGLRenderingContext).isContextLost()
    },
    resize(cssWidth, cssHeight, scale) {
      const w = Math.max(1, Math.round(cssWidth * scale))
      const h = Math.max(1, Math.round(cssHeight * scale))
      if (canvas.width === w && canvas.height === h) return false
      canvas.width = w
      canvas.height = h
      return true
    },
    draw(time, zoom, grade) {
      const c = gl as WebGLRenderingContext
      if (this.lost) return
      c.viewport(0, 0, canvas.width, canvas.height)
      c.uniform2f(u.resolution, canvas.width, canvas.height)
      c.uniform1f(u.time, time)
      c.uniform1f(u.zoom, zoom)
      c.uniform3f(u.base, grade.base[0], grade.base[1], grade.base[2])
      c.uniform1f(u.lift, grade.lift)
      c.uniform1f(u.gain, grade.gain)
      c.uniform1f(u.saturation, grade.saturation)
      c.drawArrays(c.TRIANGLES, 0, 3)
    },
    dispose() {
      canvas.removeEventListener('webglcontextlost', onLost)
      const c = gl as WebGLRenderingContext
      c.deleteProgram(program)
      c.deleteShader(vs)
      c.deleteShader(fs)
      c.deleteBuffer(buffer)
    },
  }
}
