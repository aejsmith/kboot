#!/usr/bin/env python
#
# Copyright (C) 2009-2013 Alex Smith
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

import sys, os, glob
from markdown2 import markdown
from jinja2 import Environment, FileSystemLoader

def main():
    if len(sys.argv) != 3:
        print "Usage: %s <docs dir> <output dir>" % (sys.argv[0])
        return 1

    # Get a list of documents and parse their content.
    documents = []
    for doc in glob.glob(os.path.join(sys.argv[1], '*.txt')):
        # Assume that the first line is the title, and that there are two lines
        # after it before the actual content. Skip any document that does not
        # have a H1 at the start.
        f = open(doc, 'r')
        title = f.readline().strip()
        underline = f.readline().strip()
        if len(title) != len(underline) or underline[0] != '=':
            continue
        f.readline()

        # Parse the document.
        html = markdown(f.read()).encode(sys.stdout.encoding or "utf-8", 'xmlcharrefreplace')
        f.close()

        # Work out the output file name.
        name = os.path.splitext(os.path.basename(doc))[0] + '.html'

        # Add it to the list.
        documents.append((name, title, html))

    # Create the template loader.
    tpl = Environment(loader=FileSystemLoader(os.path.join(sys.argv[1], 'markdown'))).get_template('template.html')

    # For each document, write the template.
    for doc in documents:
        f = open(os.path.join(sys.argv[2], doc[0]), 'w')
        f.write(tpl.render(name=doc[0], title=doc[1], content=doc[2], docs=documents))
        f.close()

if __name__ == '__main__':
    sys.exit(main())
