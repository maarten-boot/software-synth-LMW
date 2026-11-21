#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Spec S9.8: the documents as HTML for the downloads and the Windows installer (user 2026-11-21: Windows
# often has no program for .md files; a browser opens HTML everywhere). No dependencies: only the Markdown
# our documents use — headings, paragraphs, ordered and bullet lists (with indented continuation lines,
# code blocks and images inside an item), fenced code, tables, rulers, **bold**, *italic*, `code`,
# links, images, bare https:// links and backslash escapes.
#
# Usage: md2html.py <out folder>
# Writes README.html, Tutorial.html, THIRD_PARTY.html and images/*.png from the repository this script
# lives in. Links between these documents point to the HTML copies, images to images/, any other link
# into the repository to its page on GitHub. Black on white (Spec S6.2; links 8.6:1).
import html
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO_URL = "https://github.com/maarten-boot/software-synth-LMW/blob/main/"
DOCS = {"README.md": "README.html", "docs/tutorial/Tutorial.md": "Tutorial.html", "THIRD_PARTY.md": "THIRD_PARTY.html"}
IMAGES = "docs/images"

STYLE = """
body { background: #ffffff; color: #000000; font-family: system-ui, "Segoe UI", Arial, sans-serif;
       line-height: 1.5; max-width: 60em; margin: 0 auto; padding: 1em 16px; }
a { color: #0645ad; }
code, pre { font-family: Consolas, "DejaVu Sans Mono", monospace; background: #f3f3f3; color: #000000; }
code { padding: 0 0.2em; }
pre { padding: 0.6em; overflow-x: auto; border: 1px solid #000000; }
img { max-width: 100%; height: auto; border: 1px solid #000000; }
table { border-collapse: collapse; }
th, td { border: 1px solid #000000; padding: 0.2em 0.5em; text-align: left; vertical-align: top; }
hr { border: 0; border-top: 1px solid #000000; }
"""


def target(link, source):
    """Where a link of the document `source` (a repository path) points in the HTML folder."""
    if re.match(r"[a-z]+:", link) or link.startswith("#"):
        return link
    path, _, anchor = link.partition("#")
    repo_path = os.path.normpath(os.path.join(os.path.dirname(source), path)).replace(os.sep, "/")
    if repo_path in DOCS:
        out = DOCS[repo_path]
    elif repo_path.startswith(IMAGES + "/"):
        out = "images/" + os.path.basename(repo_path)
    else:
        out = REPO_URL + repo_path
    return out + ("#" + anchor if anchor else "")


def inline(text, source):
    """Inline Markdown to HTML: code spans first (taken out), then escapes, images, links, emphasis."""
    spans = []

    def keep(fragment):
        spans.append(fragment)
        return "\x00%d\x00" % (len(spans) - 1)

    text = re.sub(r"`([^`]+)`", lambda m: keep("<code>" + html.escape(m.group(1)) + "</code>"), text)
    text = re.sub(r"\\([\\`*_\[\]()#+\-.!|>])", lambda m: keep(html.escape(m.group(1))), text)
    text = re.sub(r"!\[([^\]]*)\]\(([^)\s]+)\)",
                  lambda m: keep('<img src="%s" alt="%s">' % (html.escape(target(m.group(2), source)),
                                                              html.escape(m.group(1)))), text)
    links = []
    text = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)",
                  lambda m: (links.append((m.group(1), m.group(2))), "\x01%d\x01" % (len(links) - 1))[1], text)
    text = re.sub(r"https?://[^\s<>()]+[^\s<>().,;:]",
                  lambda m: keep('<a href="%s">%s</a>' % (html.escape(m.group(0)), html.escape(m.group(0)))), text)
    text = html.escape(text, quote=False)
    text = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])", r"<em>\1</em>", text)
    text = re.sub(r"\x01(\d+)\x01", lambda m: '<a href="%s">%s</a>' % (
        html.escape(target(links[int(m.group(1))][1], source)), inline(links[int(m.group(1))][0], source)), text)
    while "\x00" in text:
        text = re.sub(r"\x00(\d+)\x00", lambda m: spans[int(m.group(1))], text)
    return text


def indent_of(line):
    return len(line) - len(line.lstrip(" "))


def blocks(lines, source):
    """Block-level Markdown to HTML."""
    out = []
    i = 0
    para = []

    def flush():
        if para:
            out.append("<p>" + "\n".join(inline(p, source) for p in para) + "</p>")
            para.clear()

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        if not stripped:
            flush(); i += 1; continue
        if stripped.startswith("```"):
            flush()
            ind = indent_of(line)
            code = []
            i += 1
            while i < len(lines) and not lines[i].strip().startswith("```"):
                code.append(lines[i][ind:] if lines[i][:ind].strip() == "" else lines[i])
                i += 1
            out.append("<pre><code>" + html.escape("\n".join(code)) + "</code></pre>")
            i += 1
            continue
        m = re.match(r"(#{1,6})\s+(.*)", stripped)
        if m:
            flush()
            n = len(m.group(1))
            out.append("<h%d>%s</h%d>" % (n, inline(m.group(2), source), n))
            i += 1; continue
        if re.fullmatch(r"-{3,}|\*{3,}", stripped):
            flush(); out.append("<hr>"); i += 1; continue
        if stripped.startswith("|") and i + 1 < len(lines) and re.fullmatch(r"\|?\s*:?-+:?\s*(\|\s*:?-+:?\s*)*\|?", lines[i + 1].strip()):
            flush()
            def cells(row):
                return [c.strip() for c in row.strip().strip("|").split("|")]
            rows = ["<tr>" + "".join("<th>%s</th>" % inline(c, source) for c in cells(stripped)) + "</tr>"]
            i += 2
            while i < len(lines) and lines[i].strip().startswith("|"):
                rows.append("<tr>" + "".join("<td>%s</td>" % inline(c, source) for c in cells(lines[i])) + "</tr>")
                i += 1
            out.append("<table>\n" + "\n".join(rows) + "\n</table>")
            continue
        m = re.match(r"(\s*)([-*]|\d+\.)\s+(.*)", line)
        if m:
            flush()
            ordered = m.group(2)[0].isdigit()
            base = len(m.group(1))
            start = int(m.group(2)[:-1]) if ordered else 1
            items = []

            def same_list(k):
                mm = re.match(r"(\s*)([-*]|\d+\.)\s+(.*)", lines[k]) if k < len(lines) else None
                return mm is not None and len(mm.group(1)) == base and mm.group(2)[0].isdigit() == ordered

            while i < len(lines):
                j = i
                while j < len(lines) and lines[j].strip() == "":
                    j += 1                       # blank lines between items keep the list going
                if not same_list(j):
                    break
                i = j
                m = re.match(r"(\s*)([-*]|\d+\.)\s+(.*)", lines[i])
                content_indent = len(m.group(1)) + len(m.group(2)) + 1
                body = [m.group(3)]
                i += 1
                while i < len(lines):            # continuation: indented lines and blank lines inside
                    nxt = lines[i]
                    if nxt.strip() == "":
                        if i + 1 < len(lines) and lines[i + 1].strip() and indent_of(lines[i + 1]) > base:
                            body.append(""); i += 1; continue
                        break
                    if indent_of(nxt) <= base:
                        break
                    body.append(nxt[min(content_indent, indent_of(nxt)):])
                    i += 1
                inner = blocks(body, source)
                if inner.startswith("<p>") and inner.count("<p>") == 1 and inner.endswith("</p>"):
                    inner = inner[3:-4]          # a one-paragraph item: no <p>
                items.append("<li>" + inner + "</li>")
            tag = "ol" if ordered else "ul"
            attr = ' start="%d"' % start if ordered and start != 1 else ""
            out.append("<%s%s>\n%s\n</%s>" % (tag, attr, "\n".join(items), tag))
            continue
        para.append(stripped)
        i += 1
    flush()
    return "\n".join(out)


def convert(source):
    with open(os.path.join(ROOT, source), encoding="utf-8") as f:
        text = f.read()
    lines = text.split("\n")
    title = next((l[2:].strip() for l in lines if l.startswith("# ")), os.path.basename(source))
    return ("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
            "<title>%s</title>\n<style>%s</style>\n</head>\n<body>\n%s\n</body>\n</html>\n"
            % (html.escape(title), STYLE, blocks(lines, source)))


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: md2html.py <out folder>")
    out = sys.argv[1]
    os.makedirs(os.path.join(out, "images"), exist_ok=True)
    for source, name in DOCS.items():
        with open(os.path.join(out, name), "w", encoding="utf-8", newline="\n") as f:
            f.write(convert(source))
    for name in sorted(os.listdir(os.path.join(ROOT, IMAGES))):
        if name.lower().endswith(".png"):
            shutil.copyfile(os.path.join(ROOT, IMAGES, name), os.path.join(out, "images", name))


if __name__ == "__main__":
    main()
