# Unofficial fivefilters Full-Text RSS service
# Patched to add a "Convert links to footnotes" checkbox to the UI.
# Modeled on https://github.com/heussd/fivefilters-full-text-rss-docker

# Pull the latest site-config patterns from the official repo.
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

# Copy our patched Full-Text RSS source (with the new "Convert links" checkbox)
# instead of cloning fresh from Bitbucket.
COPY full-text-rss/ /var/www/html/

# Overlay the latest site-config patterns.
COPY --from=gitconfig /ftr-site-config/.* /ftr-site-config/* /var/www/html/site_config/standard/

RUN mkdir -p /var/www/html/cache/rss \
    && chmod -R 777 /var/www/html/cache \
    && chmod -R 777 /var/www/html/site_config

VOLUME /var/www/html/cache

EXPOSE 80
