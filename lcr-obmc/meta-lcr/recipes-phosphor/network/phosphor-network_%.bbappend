FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://60-phosphor-networkd-end0.network"
SRC_URI += "file://60-phosphor-networkd-end1.network"

PERSISTENT_NETWORK_DIR ?= "/var/persistent"

do_install:append() {
    install -d ${D}${systemd_unitdir}/network
    install -m 0644 ${WORKDIR}/60-phosphor-networkd-end0.network ${D}${systemd_unitdir}/network/60-phosphor-networkd-end0.network
    install -m 0644 ${WORKDIR}/60-phosphor-networkd-end1.network ${D}${systemd_unitdir}/network/60-phosphor-networkd-end1.network

    install -d ${D}${PERSISTENT_NETWORK_DIR}
    install -m 0644 ${WORKDIR}/60-phosphor-networkd-end0.network ${D}/var/persistent/60-phosphor-networkd-end0.network
    install -m 0644 ${WORKDIR}/60-phosphor-networkd-end1.network ${D}/var/persistent/60-phosphor-networkd-end1.network
}

FILES:${PN} += "${systemd_unitdir}/network/60-phosphor-networkd-end0.network"
FILES:${PN} += "${systemd_unitdir}/network/60-phosphor-networkd-end1.network"
FILES:${PN} += "${PERSISTENT_NETWORK_DIR}/60-phosphor-networkd-end0.network"
FILES:${PN} += "${PERSISTENT_NETWORK_DIR}/60-phosphor-networkd-end1.network"
