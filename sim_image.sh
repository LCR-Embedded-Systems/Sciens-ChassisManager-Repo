#!/bin/bash
IMAGES=~/projects/images
QSPI=$IMAGES/qemu_qspi.bin

rm -f $QSPI
cp ~/projects/peta-bsp/images/linux/BOOT.BIN $IMAGES/
cp ~/projects/peta-bsp/images/linux/boot.scr $IMAGES/
cp ~/projects/peta-bsp/images/linux/image.ub $IMAGES/

echo "Creating empty image"
dd if=/dev/zero of=$QSPI bs=1M count=256 status=progress
echo "Uploading BOOT.BIN"
dd if=$IMAGES/BOOT.BIN   of=$QSPI bs=1 seek=0          conv=notrunc status=progress
echo "Uploading boot.scr"
dd if=$IMAGES/boot.scr   of=$QSPI bs=1 seek=$((0x800000)) conv=notrunc status=progress
echo "Uploading image.ub"
dd if=$IMAGES/image.ub   of=$QSPI bs=1 seek=$((0x840000)) conv=notrunc status=progress
echo "finished creating qemu_qspi.bin"

ls -lh $QSPI
echo "Flash image ready"

cd ~/projects/petalinux-2024.2
. ./settings.sh
cd ../peta-bsp
cp $QSPI .

petalinux-boot --qemu --kernel \
  --qemu-args="-boot mode=1 \
               -drive file=$(pwd)/qemu_qspi.bin,if=mtd,format=raw,index=0"


