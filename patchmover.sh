#!/bin/bash

cp -r ./devtool-mods/host-mods/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/phosphor-ipmi-host/ 2>/dev/null
cp -r ./devtool-mods/ipmb_directory-mods/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/phosphor-ipmi-ipmb/ 2>/dev/null
cp -r ./devtool-mods/phosphor-ipmi-net-mods/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/phosphor-ipmi-net/ 2>/dev/null
cp -r ./devtool-mods/bmcweb-mods/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/bmcweb/ 2>/dev/null
cp -r ./devtool-mods/entity-manager-mods/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/entity-manager/ 2>/dev/null
cp -r ./devtool-mods/sel-logger-mods/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/phosphor-sel-logger/ 2>/dev/null
