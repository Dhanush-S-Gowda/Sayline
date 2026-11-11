# Spoken punctuation

Status: implemented on 2026-11-05 with user approval. Desktop acceptance remains pending.

## First release

Opt-in text-and-punctuation icon on the overlay; off by default, persisted via QSettings. English commands:

| Spoken phrase | Output |
|---|---|
| question mark | ? |
| full stop | . |
| comma | , |
| exclamation mark | ! |
| colon | : |
| semicolon | ; |

Match whole words, case-insensitively, longest phrase first. Do not infer aliases
such as “question marked” without explicit configuration. Keep “period” optional:
it is also an ordinary word. Paragraphs, brackets and quote commands are later work.

## Pipeline and ownership

ASR events -> raw Transcript -> pure punctuation formatter -> desired display/text
-> existing replacementEdit -> verified clipboard -> existing simulated keys.

Keep raw recognition and rendered text separate. Never feed rendered punctuation
back into the recognizer or reprocess it as raw speech. The overlay and inserted
text must derive from the SAME rendered snapshot; the compact overlay replaces
line/paragraph breaks with spaces for presentation only. Retain raw and rendered session text
plus the option/rules version in history; event logs use counts, not dictated text.

## Live behavior and correctness boundary

Version 2 renders commands in live previews and authoritative completions. Each raw
word retains the punctuation mode active when it first arrived. Switching off
preserves existing punctuation and makes newly arriving command words literal;
switching on likewise affects new words only. A multiword command crossing a toggle
boundary stays literal. A word already in progress keeps its original mode.

Rendering always starts from raw recognition, so a command split across deltas or
completions becomes one symbol. Finalization reconciles word modes against the raw
preview, ignoring casing and automatic ASR punctuation. A bounded word alignment
preserves modes through ordinary insertions, deletions and corrections. Extensive
rewrites and repeated ambiguous words can make the original speech boundary
unrecoverable; alignment is best effort in those cases. The backend's unlabelled
suffix/revision delta ambiguity remains unchanged. Explicit full snapshots and
completed text remain authoritative for recognized wording.

The overlay and insertion use the same live rendering, with line breaks flattened
only in the overlay label. Existing Unicode prefix
diff and verified clipboard publication replace changed suffixes without appending
symbols blindly or feeding formatted text back into recognition.

## Literal text and ambiguity

In command mode, “literal question mark” -> the words “question mark”; “literal
comma” -> “comma”. Consume the escape only before a recognized command. Apply the
escape and command matching in ONE pass, so escaped output cannot be converted
again. New words received with mode off remain literal; earlier word modes are retained.

No rule can determine all intent from words alone. “What does question mark mean?”
needs an escape or mode-off when literal wording is intended. Avoid heuristic NLP
that silently guesses. An explicit “insert question mark” vocabulary is an
alternative if the user prefers fewer accidental replacements.

## Spacing and recognizer punctuation

Remove preceding spaces before closing punctuation; retain one following space
before the next word. Do not rewrite casing, grammar or unrelated punctuation.
Suppress an adjacent automatic ASR punctuation mark only as part of consuming a
recognized command; do not globally collapse punctuation (ellipses etc.).

Examples with command mode enabled:
- `Hello comma world full stop` -> `Hello, world.`
- `Are you ready question mark.` -> `Are you ready?`
- `Write literal question mark` -> `Write question mark`
- `time period` -> unchanged

## Proof and rollout

1. Pure formatter tests: every command; case/word boundaries; literal escapes;
   spacing; ASR-added punctuation; ordinary words; Unicode; mode-off.
2. Transcript tests: command split across deltas and utterances; raw partial
   revisions followed by final text; punctuation correction without duplication.
3. Integration: overlay/rendered target agree; verified clipboard receives exact
   rendered text; publication failure sends no correction keys.
4. Desktop acceptance: Zed editor and embedded terminal, browser, standalone
   terminal; toggling on/off; finalization after Ctrl+Space; retained history.
5. Ship behind opt-in setting, then enable by default only if user chooses it.

Acceptance: approved commands become symbols once finalized; literal wording is
recoverable; no stale-clipboard regression; no changes to shortcut/model lifecycle.

## Implementation evidence (2026-11-05)

`Transcript::display()` formats raw committed and preview text using per-word mode
spans. Controller overlay and insertion both use `transcript()`. History retains raw
`transcript`/`committed`/`partial` plus `rendered_transcript`, current
`punctuation_enabled`, `punctuation_rules_version=2`, and `punctuation_spans`
(start/length in UTF-16 code units, plus enabled state). Toggle events log mode and
raw character count, never speech text.

Automated coverage includes all six commands, case and Unicode word boundaries,
escapes, unchanged unrelated punctuation, mode-off, commands split across deltas
and completions, live revisions, toggle history preserved through finalization,
preference persistence, raw/rendered history, and exact rendered clipboard delivery.
Failed publication still sends no correction keys. The overlay hover test leaves
the pointer over the button past the old tooltip delay and verifies both on/off
clicks. Manual desktop acceptance above requires the real KDE session.

## Expanded commands (rules version 3)

The README contains the complete supported phrase and spacing reference.
The original six commands now share a command table with hyphen, em dash,
ellipsis, apostrophe, quote/parenthesis/bracket pairs, forward slash, backslash,
underscore, at sign, hash sign, percent sign, ampersand, new/next line and new
paragraph. Line breaks are clipboard text, not simulated Enter keys. Joining
symbols remove surrounding horizontal whitespace; opening symbols join right,
closing symbols join left, and line breaks preserve preceding line breaks.
Literal escapes, per-word toggle history, raw recognition storage, and live
reconciliation continue to apply to every supported command. Matching includes
Unicode combining marks in word boundaries. Rules version 3 is stored in new
session history records.

Number words zero through nine also render as individual digits under the same
punctuation toggle and per-word mode history. Digit replacement preserves spaces
and recognition punctuation. `literal two` preserves the word. With the toggle
off, raw recognition output is preserved, including any digits supplied by ASR.
