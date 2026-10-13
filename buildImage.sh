#!/bin/bash

BOLD='\e[1m'
BOLD_GREEN='\e[1;32m'
BOLD_RED='\e[1;31m'
BOLD_YELLOW='\e[1;33m'
BOLD_BLUE='\e[1;34m'
RESET='\e[0m'

# ====================== LOGGING SETUP ======================
BUILD_LOG=~/projects/last_build.log
FAILURE_LOG=~/projects/last_build_failure.log
mkdir -p ~/projects

# Tee all output (stdout + stderr) to both terminal and log file
exec 1> >(tee -a "$BUILD_LOG") 2>&1

CURRENT_XSA="BCM_top_wrapper_BASELINE"

printf "${BOLD_BLUE}Building on branch: ${RESET}\n" && git branch --show-current

cp ~/projects/LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/$CURRENT_XSA.xsa ~/projects/peta-bsp
cp -r ~/projects/LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/final_xsas/. ~/projects/peta-bsp
cp -a ~/projects/LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/project-spec ~/projects/peta-bsp
cp ~/projects/LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/utils/import-obmc-rootfs.sh ~/projects/peta-bsp/images/linux
cp ~/projects/LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/utils/lcr-image.its ~/projects/peta-bsp/images/linux

BRANCH=$(git branch --show-current)

cd ~/projects/LCR-ChassisManager-Repo/lcr-obmc
source oe-init-build-env .

cd ~/projects/LCR-ChassisManager-Repo/lcr-obmc
. ./setup zc702-zynq7

# ------------------ FAILURE HANDLER ------------------
fail() {
    local stage="$1"
    echo ""
    echo "=================================================================="
    echo "BUILD FAILED"
    echo "Stage     : $stage"
    echo "Time      : $(date)"
    echo "Full log  : $BUILD_LOG"
    echo "Failure log: $FAILURE_LOG"
    echo "=================================================================="

    {
        echo "BUILD FAILED: $(date)"
        echo "Stage: $stage"
        echo ""
        echo "===== Last 100 lines of build output ====="
        tail -n 100 "$BUILD_LOG"
    } > "$FAILURE_LOG"

    exit 1
}

if [ "${SIMULATION}" = "1" ]; then
    printf "${BOLD_BLUE}LCR INFO: SIMULATION=1 - binaries will be built with -DSIMULATION${RESET}\n"
else
    printf "${BOLD_BLUE}LCR INFO: normal (non-simulation) build${RESET}\n"
fi

# Bitbake clean
bitbake -c clean lcr-ipmitool
if [ $? -ne 0 ]; then
    fail "bitbake -c clean lcr-ipmitool"
fi

# Main OpenBMC build
bitbake obmc-phosphor-image
if [ $? -ne 0 ]; then
    fail "bitbake obmc-phosphor-image"
fi

# ------------------ PetaLinux section ------------------
cd ~/projects/petalinux-2024.2
. ./settings.sh
cd ..

petalinux-create project -n peta-bsp --template zynq --tmpdir ~/tmp-peta

cd peta-bsp
printf "${BOLD_BLUE}LCR INFO running petalinux config now${RESET}\n"
petalinux-config --get-hw-description=./$CURRENT_XSA.xsa --silentconfig
if [ $? -ne 0 ]; then
    fail "petalinux-config"
fi

printf "${BOLD_BLUE}LCR INFO running grep and move now${RESET}\n"
grep -rl @@@HOME@@@ ./project-spec/ | xargs sed -i "s|@@@HOME@@@|$HOME|g"

printf "${BOLD_BLUE}LCR INFO running petalinux build now${RESET}\n"
petalinux-build
if [ $? -ne 0 ]; then
    fail "petalinux-build"
fi

cd ./images/linux
printf "${BOLD_BLUE}LCR INFO running import obmc rootfs now${RESET}\n"
./import-obmc-rootfs.sh
mkimage -f lcr-image.its image.ub
petalinux-package boot --fsbl ./zynq_fsbl.elf --u-boot ./u-boot.elf --fpga ./system.bit --force

# Generate unique names and copy files
unique_suffix="${BRANCH}_$(date +%Y%m%d_%H%M%S)"
if [ "${SIMULATION}" = "1" ]; then

    echo "-----------------------------------------------------------------------------------------------------------------------------"
    printf "${BOLD_GREEN}Finished building on branch: $BRANCH ${RESET}\n"
    echo "-----------------------------------------------------------------------------------------------------------------------------"
    echo "Build completed successfully on: $(date)"
    echo "Branch: $BRANCH"
    
    printf "${BOLD_BLUE}LCR INFO: SIMULATION=1 - RUN ./sim_image.sh NEXT TO BOOT THE SIMULATION\n"

else


    cp BOOT.BIN "BOOT_${unique_suffix}.BIN"
    cp boot.scr "boot_${unique_suffix}.scr"
    cp image.ub "image_${unique_suffix}.ub"

    find . -type f \( -name "image_*" -o -name "boot_*" -o -name "BOOT_*" \) -mtime +7 -delete

    echo "-----------------------------------------------------------------------------------------------------------------------------"
    printf "${BOLD_GREEN}Finished building on branch: $BRANCH ${RESET}\n"
    echo "-----------------------------------------------------------------------------------------------------------------------------"

    # ====================== SUCCESS INSTRUCTIONS FILE ======================
    CURRENT_ELF="zynq_fsbl"
    INSTRUCTIONS_FILE=~/projects/build_instructions.txt

cat > "$INSTRUCTIONS_FILE" << EOF
Build completed successfully on: $(date)
Branch: $BRANCH
Unique suffix: $unique_suffix
Build log: $BUILD_LOG

-----------------------------------------------------------------------------------------------------------------------------
IF DOING A FIRST TIME FLASH OR REBUILDING THE XSA, DO THE FOLLOWING.

To copy the files to your local machine, ensure it is on the same LAN network as the build machine and run the following command from the computer you intend to flash on:
scp lcr@192.168.0.35:./projects/peta-bsp/images/linux/${CURRENT_ELF}.elf lcr@192.168.0.35:./projects/peta-bsp/images/linux/BOOT_${unique_suffix}.BIN lcr@192.168.0.35:./projects/peta-bsp/images/linux/boot_${unique_suffix}.scr lcr@192.168.0.35:./projects/peta-bsp/images/linux/image_${unique_suffix}.ub .

-----------------------------------------------------------------------------------------------------------------------------
Run this command to flash:
connect; targets; target 2; program_flash -f c:/users/tc/lcr/BOOT_${unique_suffix}.BIN -fsbl c:/users/tc/lcr/${CURRENT_ELF}.elf -offset 0x0 -flash_type qspi-x2-single; program_flash -f c:/users/tc/lcr/boot_${unique_suffix}.scr -fsbl c:/users/tc/lcr/${CURRENT_ELF}.elf -offset 0x800000 -flash_type qspi-x2-single; program_flash -f c:/users/tc/lcr/image_${unique_suffix}.ub -fsbl c:/users/tc/lcr/${CURRENT_ELF}.elf -offset 0x840000 -flash_type qspi-x2-single

-----------------------------------------------------------------------------------------------------------------------------
IF YOU ARE FLASHING NEW FIRMWARE TO A UNIT THAT ALREADY HAS A WORKING KERNEL FLASH, DO THE FOLLOWING.

To copy the files to your unit, ensure it is on the same LAN network as the build machine and run the following command from the unit:
scp lcr@192.168.0.35:./projects/peta-bsp/images/linux/BOOT_${unique_suffix}.BIN lcr@192.168.0.35:./projects/peta-bsp/images/linux/boot_${unique_suffix}.scr lcr@192.168.0.35:./projects/peta-bsp/images/linux/image_${unique_suffix}.ub .

-----------------------------------------------------------------------------------------------------------------------------
Run this command to flash:
flashcp -v BOOT_${unique_suffix}.BIN /dev/mtd0 && flashcp -v boot_${unique_suffix}.scr /dev/mtd1 && flashcp -v image_${unique_suffix}.ub /dev/mtd2

-----------------------------------------------------------------------------------------------------------------------------
Once flashed, reboot for the new firmware to take effect.
EOF

    printf "${BOLD_GREEN}Build completed successfully.${RESET}\n"
    printf "${BOLD_GREEN}Instructions saved to: ${INSTRUCTIONS_FILE}${RESET}\n"
    echo "-----------------------------------------------------------------------------------------------------------------------------"

    cat ~/projects/build_instructions.txt


fi

