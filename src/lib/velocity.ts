/**
 * Pointer velocity and acceleration from raw pointer samples. Velocity is the
 * displacement across the last ~80 ms of samples, smoothed with an EMA;
 * acceleration is the frame-to-frame change of that smoothed velocity. When
 * the pointer stops sending events (finger held still) the velocity decays to
 * zero instead of freezing at its last value.
 *
 * Times are milliseconds (as pointer events report them); outputs are px/s and px/s².
 */

export interface MotionSample {
  vx: number
  vy: number
  ax: number
  ay: number
}

interface Sample {
  t: number
  x: number
  y: number
}

const CAPACITY = 8
const WINDOW_MS = 80
const STALE_MS = 60
const SMOOTHING = 0.5

export class PointerTracker {
  private samples: Sample[] = []
  private svx = 0
  private svy = 0
  private lastSampleAt = -1

  push(t: number, x: number, y: number): void {
    this.samples.push({ t, x, y })
    if (this.samples.length > CAPACITY) this.samples.shift()
  }

  reset(): void {
    this.samples.length = 0
    this.svx = 0
    this.svy = 0
    this.lastSampleAt = -1
  }

  get hasSamples(): boolean {
    return this.samples.length > 0
  }

  /** Call once per frame with the frame's timestamp (ms). */
  sample(now: number): MotionSample {
    const newest = this.samples[this.samples.length - 1]
    let rawVx = 0
    let rawVy = 0

    if (newest && now - newest.t <= STALE_MS) {
      let oldest = newest
      for (let i = this.samples.length - 2; i >= 0; i--) {
        if (newest.t - this.samples[i].t > WINDOW_MS) break
        oldest = this.samples[i]
      }
      const dt = (newest.t - oldest.t) / 1000
      if (dt > 0) {
        rawVx = (newest.x - oldest.x) / dt
        rawVy = (newest.y - oldest.y) / dt
      }
    }

    const prevVx = this.svx
    const prevVy = this.svy
    this.svx += (rawVx - this.svx) * SMOOTHING
    this.svy += (rawVy - this.svy) * SMOOTHING

    const frameDt = this.lastSampleAt < 0 ? 0 : (now - this.lastSampleAt) / 1000
    this.lastSampleAt = now
    const ax = frameDt > 0 ? (this.svx - prevVx) / frameDt : 0
    const ay = frameDt > 0 ? (this.svy - prevVy) / frameDt : 0

    return { vx: this.svx, vy: this.svy, ax, ay }
  }
}
