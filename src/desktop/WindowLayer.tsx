/** Renders every window in insertion order; z comes from the store, never from DOM order. */

import { Window } from '@/components/Window'
import { selectOrder, selectWindow } from '@/desktop/wm/selectors'
import { useWM } from '@/desktop/wm/useWM'
import { apps } from '@/desktop/apps/registry'

function AppWindow({ id }: { id: string }) {
  const record = useWM(selectWindow(id))
  if (!record) return null
  const app = apps[record.appId]
  const Component = app?.component
  return <Window id={id}>{Component ? <Component windowId={id} /> : null}</Window>
}

export function WindowLayer() {
  const order = useWM(selectOrder)
  return (
    <>
      {order.map((id) => (
        <AppWindow key={id} id={id} />
      ))}
    </>
  )
}
