/** Entry: bake the brushed texture, then mount the desktop. */

import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { installBrushTexture } from '@/lib/textures'
import { Desktop } from '@/desktop/Desktop'
import '@/styles/base.css'

installBrushTexture()

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <Desktop />
  </StrictMode>,
)
