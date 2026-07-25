#!/usr/bin/env python3
"""Render the user manual into a PHP data file for the animatek.net theme.

The manual in `manual/` is the single source of truth. This script turns each
chapter into a block of HTML carrying the theme's own Tailwind classes, and
writes them all into one PHP file that `inc/animatek-nme-manual-template.php`
includes. Nothing here is design: the class names are the theme's, and the
generated file is meant to be committed to the theme repo, not edited.

    python3 packaging/build-web-manual.py [--out PATH]

Locales: English from `manual/*.md`, Spanish from `manual/es/*.md`. A locale
with no files is skipped, so the Spanish side can land later without breaking
the build.
"""

from __future__ import annotations

import argparse
import html
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MANUAL = REPO / "manual"

DEFAULT_OUT = (
    REPO.parent
    / "animatek-tailpress-wp/wp/wp-content/themes/animatek-tailpress"
    / "inc/animatek-nme-manual-data.php"
)

# Chapter files in reading order. README.md is the manual's own index and is
# replaced by the page's sidebar, so it is deliberately not included.
CHAPTERS = [
    "01-getting-started.md",
    "02-interface.md",
    "03-editing-patches.md",
    "04-working-with-the-synth.md",
    "05-tools-and-floaters.md",
    "06-files-and-formats.md",
    "07-shortcuts.md",
    "08-troubleshooting.md",
    "09-mcp-bridge.md",
]

# Theme classes, kept in one place so a restyle is a single edit here.
CLS = {
    "h3": "mt-8 mb-3 text-lg font-extrabold tracking-tight text-slate-900",
    "p": "mb-4 leading-relaxed text-slate-700",
    "ul": "mb-4 flex flex-col gap-2",
    "ol": "mb-4 flex list-decimal flex-col gap-2 pl-5 text-slate-700",
    "li_ul": "relative pl-4 text-slate-700 before:absolute before:left-0 before:top-[0.68em] "
             "before:h-[0.34rem] before:w-[0.34rem] before:rounded-[1px] before:bg-[#F4D35E]",
    "li_ol": "text-slate-700",
    "code": "rounded border border-slate-200 bg-slate-50 px-1.5 py-0.5 font-mono text-[0.875em]",
    "pre": "mb-4 overflow-x-auto rounded-lg border border-slate-200 bg-slate-50 p-4 "
           "font-mono text-[0.82rem] leading-relaxed text-slate-700",
    "table_wrap": "mb-4 overflow-x-auto rounded-lg border border-slate-300 bg-white",
    "table": "w-full border-collapse text-sm",
    "th": "border-b border-slate-200 px-4 py-2.5 text-left text-[0.68rem] font-black "
          "uppercase tracking-widest text-slate-500",
    "td": "border-b border-slate-200 px-4 py-2.5 text-left align-top text-slate-700",
    "kbd": "inline-block rounded border border-b-2 border-slate-400 bg-white px-1.5 "
           "py-0.5 font-mono text-[0.78rem] font-semibold leading-normal text-slate-900",
    "strong": "font-bold text-slate-900",
    "a": "text-[#2C7FFF] hover:underline",
}

# Anything that looks like a key chord becomes a <kbd> run rather than <code>.
KEYS = (
    "Ctrl", "Cmd", "Shift", "Alt", "Enter", "Escape", "Esc", "Delete", "Backspace",
    "Space", "Tab", "Home", "End",
)
CHORD_RE = re.compile(
    r"^(?:%s|F\d{1,2}|[A-Za-z0-9,.+\-])(?:\+(?:%s|F\d{1,2}|[A-Za-z0-9,.+\-]))*$"
    % ("|".join(KEYS), "|".join(KEYS))
)


def slugify(text: str) -> str:
    """GitHub-style heading anchor, so links written in the markdown resolve."""
    text = re.sub(r"`([^`]+)`", r"\1", text)
    text = re.sub(r"\*\*?([^*]+)\*\*?", r"\1", text)
    text = text.lower()
    text = re.sub(r"[^\w\s-]", "", text, flags=re.UNICODE)
    return re.sub(r"[\s_]+", "-", text.strip())


def is_chord(text: str) -> bool:
    if "+" not in text:
        # A bare `F1` / `Z` / `S` is a key too, but a bare `.pch` is not.
        return bool(re.fullmatch(r"F\d{1,2}|[A-Z]", text))
    return bool(CHORD_RE.fullmatch(text))


def render_chord(text: str) -> str:
    parts = [p for p in text.split("+") if p != ""]
    if not parts:
        parts = [text]
    # `Ctrl++` splits to ['Ctrl'] and loses the '+' key; put it back.
    if text.endswith("++"):
        parts.append("+")
    return "+".join(
        '<kbd class="%s">%s</kbd>' % (CLS["kbd"], html.escape(p)) for p in parts
    )


def inline(text: str) -> str:
    """Markdown inline constructs -> HTML. Code spans are protected first."""
    spans: list[str] = []

    def stash(fragment: str) -> str:
        spans.append(fragment)
        return "\x00%d\x00" % (len(spans) - 1)

    def on_code(m: re.Match) -> str:
        raw = m.group(1)
        if is_chord(raw):
            return stash(render_chord(raw))
        return stash('<code class="%s">%s</code>' % (CLS["code"], html.escape(raw)))

    text = re.sub(r"`([^`]+)`", on_code, text)

    def on_link(m: re.Match) -> str:
        label, href = m.group(1), m.group(2)
        # Cross-chapter links: `04-working-with-the-synth.md#slot-pop-out-windows`
        # keeps its anchor, since every heading below gets that same slug as an
        # id. Without an anchor it lands on the chapter itself.
        chapter = re.match(r"^(\d\d)-[a-z0-9-]+\.md(#.*)?$", href)
        if chapter:
            href = chapter.group(2) or ("#cap-" + chapter.group(1))
        return stash(
            '<a class="%s" href="%s">%s</a>' % (CLS["a"], html.escape(href, quote=True), html.escape(label))
        )

    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", on_link, text)

    # quote=False keeps apostrophes readable in the output; php_quote() is what
    # makes the string safe to embed in the PHP file.
    text = html.escape(text, quote=False)
    text = re.sub(
        r"\*\*([^*]+)\*\*", r'<strong class="%s">\1</strong>' % CLS["strong"], text
    )
    text = re.sub(r"(?<![*\w])\*([^*]+)\*(?!\w)", r"<em>\1</em>", text)

    return re.sub(r"\x00(\d+)\x00", lambda m: spans[int(m.group(1))], text)


def unwrap(lines: list[str]) -> str:
    return " ".join(line.strip() for line in lines).strip()


def render_table(rows: list[str]) -> str:
    cells = [
        [c.strip() for c in row.strip().strip("|").split("|")]
        for row in rows
        if not re.fullmatch(r"\s*\|[\s|:-]+\|\s*", row)
    ]
    if not cells:
        return ""
    head, body = cells[0], cells[1:]
    out = ['<div class="%s">' % CLS["table_wrap"], '<table class="%s">' % CLS["table"]]
    out.append("<thead><tr>")
    for c in head:
        out.append('<th scope="col" class="%s">%s</th>' % (CLS["th"], inline(c)))
    out.append("</tr></thead><tbody>")
    for row in body:
        out.append("<tr>")
        for c in row:
            out.append('<td class="%s">%s</td>' % (CLS["td"], inline(c)))
        out.append("</tr>")
    out.append("</tbody></table></div>")
    return "".join(out)


def render_body(md: str) -> tuple[str, str]:
    """Return (chapter title, chapter body HTML)."""
    lines = md.replace("\r\n", "\n").split("\n")
    title = ""
    out: list[str] = []
    buf: list[str] = []
    i = 0

    def flush_paragraph() -> None:
        nonlocal buf
        if buf:
            out.append('<p class="%s">%s</p>' % (CLS["p"], inline(unwrap(buf))))
            buf = []

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        if stripped.startswith("# "):
            flush_paragraph()
            title = re.sub(r"^\d+\.\s*", "", stripped[2:].strip())
            i += 1
            continue

        if stripped.startswith("## ") or stripped.startswith("### "):
            flush_paragraph()
            heading = stripped.lstrip("#").strip()
            out.append(
                '<h3 id="%s" class="%s">%s</h3>'
                % (slugify(heading), CLS["h3"], inline(heading))
            )
            i += 1
            continue

        if stripped.startswith("```"):
            flush_paragraph()
            i += 1
            code: list[str] = []
            while i < len(lines) and not lines[i].strip().startswith("```"):
                code.append(lines[i])
                i += 1
            i += 1
            out.append(
                '<pre class="%s"><code>%s</code></pre>'
                % (CLS["pre"], html.escape("\n".join(code)))
            )
            continue

        if stripped.startswith("|"):
            flush_paragraph()
            rows: list[str] = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                rows.append(lines[i])
                i += 1
            out.append(render_table(rows))
            continue

        bullet = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", line)
        if bullet:
            flush_paragraph()
            ordered = not bullet.group(2) in ("-", "*")
            items: list[list[str]] = []
            while i < len(lines):
                m = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", lines[i])
                if m:
                    items.append([m.group(3)])
                    i += 1
                elif lines[i].startswith(("  ", "\t")) and lines[i].strip() and items:
                    items[-1].append(lines[i].strip())
                    i += 1
                elif not lines[i].strip():
                    # A blank line ends the list unless another item follows.
                    j = i + 1
                    if j < len(lines) and re.match(r"^(\s*)([-*]|\d+\.)\s+", lines[j]):
                        i += 1
                        continue
                    break
                else:
                    break
            tag, cls, li_cls = (
                ("ol", CLS["ol"], CLS["li_ol"]) if ordered else ("ul", CLS["ul"], CLS["li_ul"])
            )
            out.append('<%s class="%s">' % (tag, cls))
            for item in items:
                out.append('<li class="%s">%s</li>' % (li_cls, inline(unwrap(item))))
            out.append("</%s>" % tag)
            continue

        if not stripped:
            flush_paragraph()
            i += 1
            continue

        buf.append(line)
        i += 1

    flush_paragraph()
    return title, "".join(out)


def glosses(directory: Path) -> dict[str, str]:
    """One-line summaries from the manual's own index in README.md."""
    readme = directory / "README.md"
    if not readme.exists():
        return {}
    found = {}
    for line in readme.read_text(encoding="utf-8").split("\n"):
        m = re.match(r"^\d+\.\s*\[[^\]]+\]\((\d\d)-[a-z0-9-]+\.md\)\s*[:—-]\s*(.+)$", line.strip())
        if m:
            found[m.group(1)] = m.group(2).strip()
    return found


def collect(directory: Path) -> list[dict]:
    chapters = []
    gloss = glosses(directory)
    for index, name in enumerate(CHAPTERS, start=1):
        path = directory / name
        if not path.exists():
            return []
        title, body = render_body(path.read_text(encoding="utf-8"))
        num = "%02d" % index
        chapters.append(
            {
                "id": "cap-" + num,
                "num": num,
                "title": title,
                "desc": inline(gloss.get(num, "")),
                "html": body,
            }
        )
    return chapters


def php_quote(value: str) -> str:
    return "'" + value.replace("\\", "\\\\").replace("'", "\\'") + "'"


def project_version() -> str:
    """Read the version straight from CMakeLists.txt so a release bumps it."""
    text = (REPO / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\s*\(\s*AnimatekNME\s+VERSION\s+([0-9.]+)", text)
    return match.group(1) if match else ""


def emit(locales: dict[str, list[dict]], version: str) -> str:
    out = [
        "<?php",
        "/**",
        " * Animatek NME user manual, rendered for the web.",
        " *",
        " * GENERATED FILE - do not edit by hand.",
        " * Source: manual/*.md in the Animatek-NME repository.",
        " * Regenerate with: python3 packaging/build-web-manual.py",
        " */",
        "",
        "return [",
        "    %s => %s," % (php_quote("version"), php_quote(version)),
        "    %s => [" % php_quote("chapters"),
    ]
    for locale, chapters in locales.items():
        out.append("    %s => [" % php_quote(locale))
        for ch in chapters:
            out.append("        [")
            for key in ("id", "num", "title", "desc"):
                out.append("            %s => %s," % (php_quote(key), php_quote(ch[key])))
            out.append("            %s => %s," % (php_quote("html"), php_quote(ch["html"])))
            out.append("        ],")
        out.append("    ],")
    out.append("    ],")
    out.append("];")
    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()

    locales: dict[str, list[dict]] = {}

    english = collect(MANUAL)
    if not english:
        print("error: no English chapters found in %s" % MANUAL, file=sys.stderr)
        return 1
    locales["en"] = english

    spanish = collect(MANUAL / "es")
    if spanish:
        locales["es"] = spanish
    else:
        print("note: manual/es/ incomplete, skipping Spanish", file=sys.stderr)

    version = project_version()

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(emit(locales, version), encoding="utf-8")

    print("version %s" % (version or "(unknown)"))
    for locale, chapters in locales.items():
        print("%s: %d chapters" % (locale, len(chapters)))
    print("wrote %s" % args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
