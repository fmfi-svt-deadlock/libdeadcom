FROM alpine:3.7

RUN set -x \
    && apk add --update --no-cache \
         findutils \
         gcc \
         make \
         musl-dev \
         ruby \
         valgrind \
    && apk add --no-cache --virtual .build-deps \
         ruby-dev \
    && gem install --no-ri --no-rdoc cddl \
    && apk del .build-deps \
    && rm -rf /root/.cache /var/cache/apk/*
