/** Mock file system for the Finder window. */

import type { IconName } from '@/components/Icon'

export type FileKind = 'folder' | 'document' | 'image' | 'music' | 'code'

export interface FileEntry {
  name: string
  kind: FileKind
  size: string
  modified: string
}

export interface Location {
  id: string
  name: string
  icon: IconName
  files: FileEntry[]
}

export const kindIcon: Record<FileKind, IconName> = {
  folder: 'folder',
  document: 'document',
  image: 'image',
  music: 'music',
  code: 'code',
}

export const kindLabel: Record<FileKind, string> = {
  folder: 'Folder',
  document: 'Document',
  image: 'Image',
  music: 'Audio',
  code: 'Source',
}

export const favorites: Location[] = [
  {
    id: 'desktop',
    name: 'Desktop',
    icon: 'desktop',
    files: [
      { name: 'Liquid Platinum.sketch', kind: 'document', size: '18.4 MB', modified: 'Today, 4:12 PM' },
      { name: 'monogram.svg', kind: 'image', size: '6 KB', modified: 'Today, 3:48 PM' },
      { name: 'wallpaper-ref.jpg', kind: 'image', size: '1.1 MB', modified: 'Yesterday' },
      { name: 'Notes', kind: 'folder', size: '--', modified: 'Sep 5, 2026' },
    ],
  },
  {
    id: 'repositories',
    name: 'Repositories',
    icon: 'folder',
    files: [
      { name: 'Mary', kind: 'folder', size: '--', modified: 'Today, 2:57 PM' },
      { name: 'MaryUI', kind: 'folder', size: '--', modified: 'Today, 4:41 PM' },
      { name: 'Bonnie', kind: 'folder', size: '--', modified: 'Today, 3:00 PM' },
      { name: 'Conduit', kind: 'folder', size: '--', modified: 'Today, 2:47 PM' },
      { name: 'Fleet', kind: 'folder', size: '--', modified: 'Today, 2:57 PM' },
      { name: 'Frigate', kind: 'folder', size: '--', modified: 'Sep 4, 2026' },
      { name: 'Sewn', kind: 'folder', size: '--', modified: 'Today, 2:51 PM' },
      { name: 'Thread', kind: 'folder', size: '--', modified: 'Today, 3:55 PM' },
    ],
  },
  {
    id: 'documents',
    name: 'Documents',
    icon: 'document',
    files: [
      { name: 'Design principles.md', kind: 'document', size: '12 KB', modified: 'Aug 30, 2026' },
      { name: 'Platinum ramp.numbers', kind: 'document', size: '210 KB', modified: 'Aug 28, 2026' },
      { name: 'Motion notes.md', kind: 'document', size: '4 KB', modified: 'Aug 27, 2026' },
    ],
  },
  {
    id: 'downloads',
    name: 'Downloads',
    icon: 'download',
    files: [
      { name: 'aqua-reference.png', kind: 'image', size: '2.4 MB', modified: 'Aug 22, 2026' },
      { name: 'platinum-theme-1999.zip', kind: 'document', size: '840 KB', modified: 'Aug 22, 2026' },
    ],
  },
]

export const locations: Location[] = [
  {
    id: 'maryui',
    name: 'MaryUI',
    icon: 'drive',
    files: [
      { name: 'src', kind: 'folder', size: '--', modified: 'Today' },
      { name: 'tokens', kind: 'folder', size: '--', modified: 'Today' },
      { name: 'scripts', kind: 'folder', size: '--', modified: 'Today' },
      { name: 'public', kind: 'folder', size: '--', modified: 'Today' },
      { name: 'package.json', kind: 'code', size: '1 KB', modified: 'Today' },
      { name: 'vite.config.ts', kind: 'code', size: '2 KB', modified: 'Today' },
      { name: 'tsconfig.json', kind: 'code', size: '1 KB', modified: 'Today' },
      { name: 'README.md', kind: 'document', size: '9 KB', modified: 'Today' },
      { name: 'LICENSE', kind: 'document', size: '11 KB', modified: 'Today' },
    ],
  },
  {
    id: 'cloud',
    name: 'Rao Cloud',
    icon: 'cloud',
    files: [
      { name: 'Ambient captures', kind: 'folder', size: '--', modified: 'Today' },
      { name: 'Voice notes', kind: 'folder', size: '--', modified: 'Yesterday' },
      { name: 'mary-intro.m4a', kind: 'music', size: '3.2 MB', modified: 'Sep 2, 2026' },
    ],
  },
]

export const allLocations = [...favorites, ...locations]
