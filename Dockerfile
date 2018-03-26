FROM alpine:3.7

RUN apk add --update --no-cache \
        findutils \
        gcc \
        make \
        musl-dev \
        ruby \
        valgrind
