# Unofficial fivefilters Full-Text RSS service
# Mirrors the upstream Dockerfile from
# https://github.com/heussd/fivefilters-full-text-rss-docker
# and then applies full-text-rss.patch on top so the resulting image carries
# our opt-in "Proxy article links" feature (?proxy_links=1 / UI checkbox).

# Stage 1: clone upstream Full-Text RSS at the pinned commit and apply our patch.
FROM alpine/git AS gitsrc
WORKDIR /ftr
RUN git clone https://bitbucket.org/fivefilters/full-text-rss.git . \
    && git reset --hard 384d52fd83361ffd6e7f28bd39b322970a015a28
COPY full-text-rss.patch /tmp/full-text-rss.patch
# git apply works without a git index and gives clear errors if the patch
# doesn't cleanly match the pinned upstream commit.
RUN git apply --verbose -p1 /tmp/full-text-rss.patch \
    && rm /tmp/full-text-rss.patch

# Stage 2: pull the latest site-config patterns from the official repo.
FROM alpine/git AS gitconfig
WORKDIR /ftr-site-config
RUN git clone https://github.com/fivefilters/ftr-site-config .


# Do not upgrade. More recent versions of PHP are seg faulting.
FROM php:5-apache

# Debian Stretch is archived; point apt at the archive mirror.
# See: https://unix.stackexchange.com/a/743863
RUN echo "deb http://archive.debian.org/debian stretch main contrib non-free" > /etc/apt/sources.list \
    && echo "Acquire::Check-Valid-Until \"false\";" > /etc/apt/apt.conf.d/99no-check-valid-until

RUN apt-get update \
    && apt-get install \
        -y --allow-unauthenticated \
        --no-install-recommends \
        libtidy-dev \
    && rm -rf /var/lib/apt/lists/*

RUN docker-php-ext-install tidy

# Patched Full-Text RSS source (upstream + full-text-rss.patch applied).
COPY --from=gitsrc /ftr /var/www/html

# Overlay the latest site-config patterns.
COPY --from=gitconfig /ftr-site-config/.* /ftr-site-config/* /var/www/html/site_config/standard/

RUN mkdir -p /var/www/html/cache/rss \
    && chmod -R 777 /var/www/html/cache \
    && chmod -R 777 /var/www/html/site_config

VOLUME /var/www/html/cache

EXPOSE 80
