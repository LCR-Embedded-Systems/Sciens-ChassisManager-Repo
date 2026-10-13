SUMMARY = "Persistent Network Configuration Tool"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://src/main.cpp \
           file://src/utils.cpp \
           file://src/lcr_network_config.cpp \
           file://src/lcr_Address.cpp \
           file://src/lcr_DHCP.cpp \
           file://src/lcr_IPv6AcceptRA.cpp \
           file://src/lcr_Link.cpp \
           file://src/lcr_Match.cpp \
           file://src/lcr_Neighbor.cpp \
           file://src/lcr_Network.cpp \
           file://src/lcr_Route.cpp \
           file://include/lcr_network_config.hpp \
           file://include/global.hpp"

S = "${WORKDIR}"

inherit pkgconfig systemd

DEPENDS += "sdbusplus phosphor-dbus-interfaces systemd phosphor-logging fmt nlohmann-json"

do_compile() {
    ${CXX} ${CXXFLAGS} -std=c++20 ${LDFLAGS} \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags fmt` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags nlohmann-json` \
    ${WORKDIR}/src/main.cpp \
    ${WORKDIR}/src/utils.cpp \
    ${WORKDIR}/src/lcr_network_config.cpp \
    ${WORKDIR}/src/lcr_Address.cpp \
    ${WORKDIR}/src/lcr_DHCP.cpp \
    ${WORKDIR}/src/lcr_IPv6AcceptRA.cpp \
    ${WORKDIR}/src/lcr_Link.cpp \
    ${WORKDIR}/src/lcr_Match.cpp \
    ${WORKDIR}/src/lcr_Neighbor.cpp \
    ${WORKDIR}/src/lcr_Network.cpp \
    ${WORKDIR}/src/lcr_Route.cpp \
    -I ${WORKDIR}/include \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs fmt` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs nlohmann-json` \
    -o lcr_network_config
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 lcr_network_config ${D}${bindir}/lcr_network_config
}

PROVIDES = "lcr_network_config"
RPROVIDES:${PN} = "lcr_network_config"

SYSTEMD_AUTO_ENABLE:${PN} = "enable"
