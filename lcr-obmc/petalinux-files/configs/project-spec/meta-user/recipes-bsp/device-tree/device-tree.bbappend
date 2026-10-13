FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

LCR_CURRENT_DT ?= "system-user.dtsi" 

SRC_URI:append = " file://${LCR_CURRENT_DT}"

require ${@'device-tree-sdt.inc' if d.getVar('SYSTEM_DTFILE') != '' else ''}
