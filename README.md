# agent-sandbox

A patched copy of [FiveFilters Full-Text RSS](https://bitbucket.org/fivefilters/full-text-rss)
with one added feature:

> **Optional rewrite of every feed item's `<link>` to point back through this
> Full-Text RSS instance.** When enabled, clicking an article in a reader goes
> to `http(s)://<this-host>/makefulltextfeed.php?url=<original-article-url>`
> and serves the full-text version via this app instead of the original
> (often partial) page.

The feature is **opt-in** and works two ways:

* **Web UI:** tick the new **"Proxy article links"** checkbox under *Options*.
* **API:** add `&proxy_links=1` to any `makefulltextfeed.php` request.

When the flag is absent (or `0` / empty), behaviour is identical to upstream.

## Layout

| Path | What it is |
| --- | --- |
| `Dockerfile` | `FROM heussd/fivefilters-full-text-rss:3.8.1` (the published upstream image) with `full-text-rss.patch` applied on top in a small builder stage. No FTR source is vendored in this repo, no apt/git work is repeated. |
| `full-text-rss.patch` | Unified diff (`index.php` + `makefulltextfeed.php`) of every change applied on top of the published image — the single source of truth for the feature. |
| `docs/ui-screenshot.png` | Screenshot of the UI as served by the Docker container, showing the new checkbox. |

## How the rewrite works

In `makefulltextfeed.php`:

* New helper `proxy_item_url($item_url)` builds
  `<scheme>://<host><script-dir>/makefulltextfeed.php?url=<urlencoded original URL>`
  using `$_SERVER['HTTP_HOST']` and `$_SERVER['SCRIPT_NAME']`, the same way
  the existing `get_self_url()` does.
* It is a no-op unless `$_GET['proxy_links']` is non-empty (so the default
  upstream behaviour is preserved when the flag isn't set).
* The three per-item `setLink(...)` call sites (`$permalink`, the SimplePie
  permalink fallback, and `$effective_url` when `favour_effective_url` is
  enabled) are wrapped in `proxy_item_url(...)`.
* **Recursion-safe:** if the URL already starts with this instance's
  `makefulltextfeed.php` URL it is returned unchanged, so an FTR feed
  consumed by another FTR instance won't get double-wrapped.
* **Auth-aware:** if the caller supplied the actual API key, the proxied
  links include `key=<index>&hash=<sha1(key.url)>` so they pass auth on
  this instance. If only `key-index + hash` was supplied (the secret
  isn't recoverable), the proxied links omit auth.
* `get_self_url()` also emits `proxy_links=…` so the canonical
  `<atom:link rel="self">` of the generated feed reflects the chosen mode.
* The cache key (`$cache_id` in `makefulltextfeed.php`) incorporates the
  `proxy_links` flag so a default request and a `proxy_links=1` request for
  the same feed don't collide in the on-disk cache.

The channel-level `<link>` (and the channel image's `<link>`) are
intentionally **not** rewritten — those describe the source feed, not
articles a user would click.

In `index.php`: a single new `control-group` adds a checkbox named
`proxy_links` between the *Links* select and *If extraction fails* — no
JavaScript needed; the form just submits `proxy_links=1` when ticked.

## Upstream baseline

Built on top of the published image
[`heussd/fivefilters-full-text-rss:3.8.1`](https://hub.docker.com/r/heussd/fivefilters-full-text-rss),
which itself packages [fivefilters Full-Text RSS](https://bitbucket.org/fivefilters/full-text-rss).

To verify the patch applies cleanly to that image:

```sh
docker create --name ftr-extract heussd/fivefilters-full-text-rss:3.8.1 true
mkdir /tmp/ftr && cd /tmp/ftr
docker cp ftr-extract:/var/www/html/index.php           .
docker cp ftr-extract:/var/www/html/makefulltextfeed.php .
docker rm ftr-extract
patch -p1 --dry-run < /path/to/this/repo/full-text-rss.patch
```

The Docker build performs the same `patch` step inside its `patcher`
stage, so a successful `docker build` is itself proof that the patch still
matches the pinned upstream image.

## Building and running

```sh
docker build -t ftr-patched .
docker run --rm -p 8080:80 ftr-patched
# then open http://localhost:8080/
```

## End-to-end verification

Against CNN's public RSS feed:

```sh
# Default (off): item links unchanged
curl 'http://localhost:8080/makefulltextfeed.php?url=rss.cnn.com/rss/edition.rss&max=2' \
    | grep -oE '<link>[^<]+</link>'

# Opt-in: each <item><link> rewritten to go through this instance
curl 'http://localhost:8080/makefulltextfeed.php?url=rss.cnn.com/rss/edition.rss&max=2&proxy_links=1' \
    | grep -oE '<link>[^<]+</link>'
```

When `proxy_links=1` is set, every `<item><link>` becomes
`http://localhost:8080/makefulltextfeed.php?url=<encoded-original-url>`,
while the channel's own `<link>` (CNN homepage) and the channel image's
`<link>` are left untouched. The feed's `<atom:link rel="self">` carries
`proxy_links=1` so the URL remains canonical for any reader that re-shares
it.

Clicking a rewritten link returns a single-item feed whose item link is the
*same* proxied URL — confirming the recursion guard works (no double
`url=...?url=...` wrapping).

## Screenshot

![FTR UI as served by the container](docs/ui-screenshot.png)

