#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Publish the site (build/site, from tools/make_pages_site.py) to the `gh-pages` branch of `origin`, as one fresh commit (the branch holds
# only the built site; the history of the sources is on main). In the repository's Settings > Pages: Source = "Deploy from a branch",
# branch gh-pages, folder / (root). You must be signed in to GitHub for git (Git Credential Manager asks the first time).
#   tools/publish_pages.sh VERSION
set -e
cd "$(dirname "$0")/.."
V="${1:?usage: tools/publish_pages.sh VERSION}"
[ -f "build/site/index.html" ] || { echo "no build/site: run tools/release.sh $V, then python3 tools/make_pages_site.py $V build/site"; exit 1; }
REMOTE="$(git remote get-url origin)"
TMP="$(mktemp -d)"
cp -R build/site/. "$TMP"/
cd "$TMP"
git init -q -b gh-pages
git add -A
git -c user.name="${GIT_AUTHOR_NAME:-PERF-1 site}" -c user.email="${GIT_AUTHOR_EMAIL:-noreply@users.noreply.github.com}" commit -q -m "Site for PERF-1 $V"
git push -f "$REMOTE" gh-pages
cd /
rm -rf "$TMP"
echo "published: gh-pages of $REMOTE"
