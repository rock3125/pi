#!/bin/sh
#
#     PI Prolog Interpreter
#     Copyright (C) 2004  Rock de Vocht
#
#     Licensed under the Apache License, Version 2.0 (the "License");
#     you may not use this file except in compliance with the License.
#     You may obtain a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#     Unless required by applicable law or agreed to in writing, software
#     distributed under the License is distributed on an "AS IS" BASIS,
#     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#     See the License for the specific language governing permissions and
#     limitations under the License.
#
# Print docs/iso-prolog.pdf from docs/iso-prolog.html with headless chrome,
# then stamp the document information with ghostscript, which chrome does not
# carry over from the html.  Needs a chrome build and gs; nothing else.
set -e

here=$(dirname "$0")

for candidate in google-chrome google-chrome-stable chromium chromium-browser; do
	if command -v "$candidate" >/dev/null 2>&1; then
		chrome=$candidate
		break
	fi
done
[ -n "$chrome" ] || { echo "build-iso-pdf: no chrome found" >&2; exit 1; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

"$chrome" --headless --disable-gpu --no-sandbox --no-pdf-header-footer \
	--virtual-time-budget=8000 \
	--print-to-pdf="$tmp/raw.pdf" "$here/iso-prolog.html" 2>/dev/null

if command -v gs >/dev/null 2>&1; then
	cat > "$tmp/docinfo.ps" <<'PS'
[ /Title (ISO Prolog - a working primer)
  /Author (Rock de Vocht)
  /Subject (How ISO Prolog works, with examples, and the pi interpreter's subset of it)
  /Creator (pi prolog interpreter)
  /DOCINFO pdfmark
PS
	gs -q -dBATCH -dNOPAUSE -sDEVICE=pdfwrite \
		-o "$here/iso-prolog.pdf" "$tmp/raw.pdf" "$tmp/docinfo.ps"
else
	echo "build-iso-pdf: ghostscript not found - the pdf has no title/author" >&2
	cp "$tmp/raw.pdf" "$here/iso-prolog.pdf"
fi

echo "docs/iso-prolog.pdf"
