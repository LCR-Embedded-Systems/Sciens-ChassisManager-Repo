SUMMARY = "Chassis Control DBus Exposure Driver"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://src/main.cpp \
           file://src/utils.cpp \
           file://src/chassis_control.cpp \
           file://include/chassis_control.hpp \
           file://include/global.hpp \
           file://LCR_chassis_control.service"

S = "${WORKDIR}"

inherit pkgconfig systemd

DEPENDS += "sdbusplus phosphor-dbus-interfaces systemd phosphor-logging fmt nlohmann-json boost"

do_compile() {
    ${CXX} ${CXXFLAGS} -std=c++20 ${LDFLAGS} \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags fmt` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags nlohmann-json` \
    ${WORKDIR}/src/main.cpp \
    ${WORKDIR}/src/utils.cpp \
    ${WORKDIR}/src/chassis_control.cpp \
    -I ${WORKDIR}/include \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs fmt` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs nlohmann-json` \
    -o chcontrol
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 chcontrol ${D}${bindir}/chcontrol

    # Install the systemd service file
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/LCR_chassis_control.service ${D}${systemd_system_unitdir}/LCR_chassis_control.service
}

PROVIDES = "lcr_chassis_control"
RPROVIDES:${PN} = "lcr_chassis_control"

SYSTEMD_SERVICE:${PN} = "LCR_chassis_control.service"

SYSTEMD_AUTO_ENABLE:${PN} = "enable"