/**
 * Vite configuration for the Liquid Platinum desktop.
 *
 * Mirrors the house shape (relative base, `@` alias, vitest in node) and adds
 * `tokensWatcher`, which re-runs the token build whenever `tokens/tokens.json`
 * or `icons.json` changes so token edits hot-reload exactly like a CSS edit
 * would (and the C headers in ../linux stay current).
 */

import { defineConfig, type Plugin } from 'vitest/config'
import react from '@vitejs/plugin-react'
import { fileURLToPath } from 'node:url'
import { execFile } from 'node:child_process'

const root = fileURLToPath(new URL('.', import.meta.url))
const tokensSource = fileURLToPath(new URL('./tokens/tokens.json', import.meta.url))
const iconsSource = fileURLToPath(new URL('./src/components/Icon/icons.json', import.meta.url))
const tokensScript = fileURLToPath(new URL('./scripts/build-tokens.mjs', import.meta.url))

function tokensWatcher(): Plugin {
  return {
    name: 'lp-tokens-watcher',
    configureServer(server) {
      server.watcher.add([tokensSource, iconsSource])
      server.watcher.on('change', (file) => {
        if (file !== tokensSource && file !== iconsSource) return
        execFile(process.execPath, [tokensScript], (error, _stdout, stderr) => {
          if (error) server.config.logger.error(`[tokens] ${stderr || error.message}`)
          else server.config.logger.info('[tokens] rebuilt from tokens.json and icons.json (web + C headers)', { timestamp: true })
        })
      })
    },
  }
}

export default defineConfig({
  root,
  base: './',
  plugins: [react(), tokensWatcher()],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  server: {
    host: '127.0.0.1',
    port: 8140,
    strictPort: false,
  },
  build: {
    outDir: 'dist',
    emptyOutDir: true,
  },
  test: {
    environment: 'node',
    globals: true,
    include: ['src/**/*.test.{ts,tsx}', 'scripts/**/*.test.ts'],
  },
})
