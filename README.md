# agent-sandbox

A patched copy of [FiveFilters Full-Text RSS](https://bitbucket.org/fivefilters/full-text-rss)
with one behaviour change:

> **Every feed item's `<link>` is rewritten to point back through this Full-Text RSS instance.**
>
> So when a user clicks an article in their RSS reader, the click goes to
> `http(s)://<this-host>/makefulltextfeed.php?url=<original-article-url>`
> and they get the full-text version of the article served by this app
> instead of the (often partial / paywalled / cluttered) original page.

This is unconditional — there is no toggle. The UI itself is unchanged from
upstream.

## Layout

| Path | What it is |
| --- | --- |
| `full-text-rss/` | Full-Text RSS source, cloned from upstream Bitbucket and patched. |
| `full-text-rss.patch` | Unified diff of every change applied on top of upstream. Currently only `makefulltextfeed.php`. |
| `docs/ui-screenshot.png` | Screenshot of the UI as served by the Docker container. |
| `Dockerfile` | Container image (modeled on [`heussd/fivefilters-full-text-rss-docker`](https://github.com/heussd/fivefilters-full-text-rss-docker)) that serves the patched FTR. |

## How the rewrite works

In `makefulltextfeed.php`:

* A new helper `proxy_item_url($item_url)` builds
  `<scheme>://<host><script-dir>/makefulltextfeed.php?url=<urlencoded original URL>`
  using `$_SERVER['HTTP_HOST']` and `$_SERVER['SCRIPT_NAME']`, the same way
  the existing `get_self_url()` does.
* The three `setLink(...)` calls in the per-item loop are wrapped in
  `proxy_item_url(...)` so the rewrite applies whether the original link
  comes from `$permalink`, the SimplePie permalink, or `$effective_url`
  (when `favour_effective_url` is enabled).
* **Recursion-safe:** if the URL already starts with this instance's
  `makefulltextfeed.php` URL it is returned unchanged, so an FTR feed
  consumed by another FTR instance won't get double-wrapped.
* **Auth-aware:** if the caller supplied the actual API key, the proxied
  links include `key=<index>&hash=<sha1(key.url)>` so they pass auth on
  this instance. If only `key-index + hash` was supplied (the secret
  isn't known to us), the proxied links omit auth.

The channel-level `<link>` (and the channel image's `<link>`) are
intentionally **not** rewritten — those describe the source feed, not
articles a user would click.

## Upstream baseline

Upstream commit: `384d52fd83361ffd6e7f28bd39b322970a015a28`
("Fix PHP 7.2/7.3 incompatibilites") from
<https://bitbucket.org/fivefilters/full-text-rss>.

To re-create / verify the patch:

```sh
git clone https://bitbucket.org/fivefilters/full-text-rss.git /tmp/ftr-upstream
diff -urN --label a/makefulltextfeed.php --label b/makefulltextfeed.php \
    /tmp/ftr-upstream/makefulltextfeed.php full-text-rss/makefulltextfeed.php
```

## Building and running

```sh
docker build -t ftr-patched .
docker run --rm -p 8080:80 ftr-patched
# then open http://localhost:8080/
```

## End-to-end verification

Against CNN's public RSS feed:

```sh
curl 'http://localhost:8080/makefulltextfeed.php?url=rss.cnn.com/rss/edition.rss&max=3' \
    | grep -oE '<link>[^<]+</link>'
```

Result: every `<item><link>` is rewritten to
`http://localhost:8080/makefulltextfeed.php?url=<encoded-original-url>`,
while the channel's own `<link>` (CNN homepage) is left untouched.

Clicking a rewritten link returns a single-item feed whose item link is the
*same* proxied URL — confirming the recursion guard works (no double
`url=...?url=...` wrapping).

## Screenshot

![FTR UI as served by the container](docs/ui-screenshot.png)
