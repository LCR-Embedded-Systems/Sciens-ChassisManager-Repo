require pseudo.inc

SRC_URI = "git://git.yoctoproject.org/pseudo;branch=master \
           file://fallback-passwd \
           file://fallback-group \
           "
SRC_URI[prebuilt.sha256sum] = "ed9f456856e9d86359f169f46a70ad7be4190d6040282b84c8d97b99072485aa"

# SRCREV = "ec6151a2b057109b3f798f151a36690af582e166"
SRCREV = "9ce8c09980af23ebd4ebf072010469882d0459a6"
S = "${WORKDIR}/git"
PV = "1.9.3+git${SRCPV}"
# PV = "1.9.0+git${SRCPV}"

# largefile and 64bit time_t support adds these macros via compiler flags globally
# remove them for pseudo since pseudo intercepts some of the functions which will be
# aliased due to this e.g. open/open64 and it will complain about duplicate definitions
# pseudo on 32bit systems is not much of use anyway and these features are not of much
# use for it.
TARGET_CC_ARCH:remove = "-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64"

# error: use of undeclared identifier '_STAT_VER'
COMPATIBLE_HOST:libc-musl = 'null'
