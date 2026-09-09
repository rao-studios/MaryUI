# TextArea

A multi-line inset well for editing text. Focus lights the accent ring; the well itself never changes colour.
Mirrors `web/src/components/TextArea` — a native `<textarea>` there; here the document, the caret, the
selection and the clipboard are the library's own (PARITY.md D10).

## Anatomy
`surface.well` at `radius.sm` with `emboss-well`; padding `space.2`; text `text.md` in `font.ui` (`mono`: `font.mono`),
wrapped at the well's width minus the 10px scrollbar lane (word, then character); selection `accent.soft`
(`platinum.4` when the window is inactive); a 1px `ink.primary` caret; placeholder `ink.tertiary`.

## States
focus: `0 0 0 3px accent.focus-ring`, the caret, typing edits the document · disabled `ink.disabled`.

## Behavior
Click places the caret, drag selects, Shift+click extends. ←/→ (Shift extends), ↑/↓ by visual line keeping the
column, Home/End on the visual line, ⌘/Ctrl+Home/End to the document's ends, Return, Tab, Backspace/Delete
(selection-aware), ⌘/Ctrl+A/C/X/V through the process-wide `lp_text_clipboard_shared()`. The wheel scrolls;
the caret is kept in view after every edit.

## C
`lp_text_area(ctx, id, rect, &state, (lp_text_area_opts){ .placeholder = "Type something…", .mono = 0 })`
returns 1 when the text changed. `state.doc` (`lp_text_doc`) holds the text, `cursor` and `anchor`;
`lp_text_doc_word_count`, `lp_text_doc_char_count` and `lp_text_doc_line_col` feed status bars.
