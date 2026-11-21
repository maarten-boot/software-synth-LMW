#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# T-S9.8-html: scripts/md2html.py writes README.html, Tutorial.html, THIRD_PARTY.html and the images;
# every page is well-formed (tags close in order), black on white, has no Markdown marks left (**, ```,
# "## "), its images exist, links between the pages point to the HTML copies, and the tutorial keeps its
# 11 lessons as numbered lists that continue across images and code blocks (Lesson 6 has 6 steps).
# Usage: test_md2html.py <scratch folder>
import os
import re
import subprocess
import sys
from html.parser import HTMLParser

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
failures = 0


def check(ok, what):
    global failures
    if not ok:
        failures += 1
        print("FAIL:", what)


class Nesting(HTMLParser):
    VOID = {"img", "br", "hr", "meta"}

    def __init__(self):
        super().__init__()
        self.stack, self.errors, self.images, self.links = [], 0, [], []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "img":
            self.images.append(a.get("src", ""))
        if tag == "a":
            self.links.append(a.get("href", ""))
        if tag not in self.VOID:
            self.stack.append(tag)

    def handle_endtag(self, tag):
        if self.stack and self.stack[-1] == tag:
            self.stack.pop()
        else:
            self.errors += 1


out = sys.argv[1]
subprocess.run([sys.executable, "-I", os.path.join(ROOT, "scripts", "md2html.py"), out], check=True)
for name in ("README.html", "Tutorial.html", "THIRD_PARTY.html"):
    path = os.path.join(out, name)
    check(os.path.isfile(path), name + " written")
    if not os.path.isfile(path):
        continue
    text = open(path, encoding="utf-8").read()
    p = Nesting()
    p.feed(text)
    check(p.errors == 0 and not p.stack, name + ": tags close in order")
    check("background: #ffffff; color: #000000" in text, name + ": black on white")
    body = re.sub(r"<pre>.*?</pre>|<code>.*?</code>", "", text, flags=re.S)
    check("**" not in body and "```" not in body and "\n## " not in body, name + ": no Markdown marks left")
    for src in p.images:
        check(os.path.isfile(os.path.join(out, src)), name + ": image " + src)
    for href in p.links:
        check(not href.endswith(".md"), name + ": link to the HTML copy, not " + href)
readme = open(os.path.join(out, "README.html"), encoding="utf-8").read()
check('href="Tutorial.html"' in readme and 'src="images/synth.png"' in readme, "README links the tutorial and shows the Synth tab")
tutorial = open(os.path.join(out, "Tutorial.html"), encoding="utf-8").read()
check(len(re.findall(r"<h2>Lesson \d+ ", tutorial)) == 11, "tutorial: 11 lessons")
lesson6 = tutorial[tutorial.index("<h2>Lesson 6 "):tutorial.index("<h2>Lesson 7 ")]
check(lesson6.count("<ol") == 1 and lesson6.count("<li>") == 6, "Lesson 6: one list with 6 steps (the image inside step 2)")
check("<pre><code>lmw9p unix!" in tutorial, "Lesson 11: the commands as a code block")
print("%d FAILED" % failures if failures else "T-S9.8-html: all passed")
sys.exit(1 if failures else 0)
