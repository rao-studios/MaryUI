# TextArea

A multi-line inset well for editing text. Focus lights the accent ring; the well itself never changes colour.

## Anatomy
`--lp-surface-well` at `--lp-radius-sm` with `--lp-shadow-emboss-well`; padding `--lp-space-2`; text `--lp-text-md`
in `--lp-font-ui` (`mono`: `--lp-font-mono`); selection `--lp-accent-soft`; placeholder `--lp-ink-tertiary`.
A native `<textarea>` with `resize: none` and `user-select: text` (the body turns selection off).

## States
focus (accent ring `--lp-accent-focus-ring`) · disabled `--lp-ink-disabled`.

## Behavior
The browser's: caret, selection, ↑/↓ by visual line, clipboard, key repeat. The C port implements the same
editing set itself (see `linux/src/components/TextArea`, PARITY.md D10).

## Props
`extends TextareaHTMLAttributes` + `mono?: boolean`. `spellCheck` defaults to `false`.

## Tokens
`--lp-surface-well`, `--lp-shadow-emboss-well`, `--lp-radius-sm`, `--lp-space-2`, `--lp-text-md`,
`--lp-text-leading-normal`, `--lp-font-ui/-mono`, `--lp-accent-soft/-focus-ring`, `--lp-ink-*`.
