#! /bin/sh
autoreconf --install && ./configure "$@" && exit 0
exit 1
