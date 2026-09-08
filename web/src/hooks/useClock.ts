/** The menu bar clock: "Mon 5:42 PM", refreshed on the minute. */

import { useEffect, useState } from 'react'

function format(date: Date): string {
  const day = date.toLocaleDateString(undefined, { weekday: 'short' })
  const time = date.toLocaleTimeString(undefined, { hour: 'numeric', minute: '2-digit' })
  return `${day} ${time}`
}

export function useClock(): string {
  const [now, setNow] = useState(() => format(new Date()))
  useEffect(() => {
    let timer: ReturnType<typeof setTimeout>
    const schedule = () => {
      const ms = 60_000 - (Date.now() % 60_000) + 50
      timer = setTimeout(() => {
        setNow(format(new Date()))
        schedule()
      }, ms)
    }
    schedule()
    return () => clearTimeout(timer)
  }, [])
  return now
}
