# Unofficial fivefilters Full-Text RSS service
# Based directly on the published upstream image
# (https://hub.docker.com/r/heussd/fivefilters-full-text-rss),
# with full-text-rss.patch applied on top to add the opt-in
# "Proxy article links" feature (?proxy_links=1 / UI checkbox).

ARG FTR_IMAGE=heussd/fivefilters-full-text-rss:3.8.1

# Apply full-text-rss.patch to the two PHP files shipped in the published
# image. Done in a tiny alpine stage so we don't need apt inside the
# Debian-Stretch-archived final image.
FROM ${FTR_IMAGE} AS upstream

FROM alpine:3.20 AS patcher
RUN apk add --no-cache patch
WORKDIR /ftr
COPY --from=upstream /var/www/html/index.php           ./index.php
COPY --from=upstream /var/www/html/makefulltextfeed.php ./makefulltextfeed.php
COPY full-text-rss.patch /tmp/full-text-rss.patch
RUN patch -p1 --no-backup-if-mismatch < /tmp/full-text-rss.patch \
    && rm /tmp/full-text-rss.patch

# Final image: the published upstream + our two patched files.
FROM ${FTR_IMAGE}
COPY --from=patcher /ftr/index.php           /var/www/html/index.php
COPY --from=patcher /ftr/makefulltextfeed.php /var/www/html/makefulltextfeed.php
