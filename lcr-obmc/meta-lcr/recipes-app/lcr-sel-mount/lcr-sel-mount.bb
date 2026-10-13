SUMMARY = "Mount qspi-sel, qspi-policybits, qspi-persistent, and qspi-config (mtd5, mtd6, mtd7, and mtd8) as JFFS2 for persistent storage"
LICENSE = "CLOSED"

SRC_URI = "file://var-sel.mount \
           file://var-policybits.mount \
           file://usr-share-lcr_configs.mount \
           file://var-persistent.mount"

S = "${WORKDIR}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "var-sel.mount var-policybits.mount var-persistent.mount usr-share-lcr_configs.mount"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install() {
    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${WORKDIR}/var-sel.mount ${D}${systemd_unitdir}/system/
    install -m 0644 ${WORKDIR}/var-policybits.mount ${D}${systemd_unitdir}/system/
    install -m 0644 ${WORKDIR}/var-persistent.mount ${D}${systemd_unitdir}/system/
    install -m 0644 ${WORKDIR}/usr-share-lcr_configs.mount ${D}${systemd_unitdir}/system/
}

FILES:${PN} += " \
    ${systemd_unitdir}/system/var-sel.mount \
    ${systemd_unitdir}/system/var-policybits.mount \
    ${systemd_unitdir}/system/var-persistent.mount \
    ${systemd_unitdir}/system/usr-share-lcr_configs.mount \
"