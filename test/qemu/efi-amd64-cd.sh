#!/bin/bash -ex

builddir=build/efi-amd64
isodir=${builddir}/iso

mkdir ${isodir}
mkdir ${isodir}/boot

truncate -s 4M ${isodir}/boot/efiboot.img
mkfs.vfat -n KBOOT_EFI ${isodir}/boot/efiboot.img

export MTOOLSRC=$(pwd)/${isodir}/mtoolsrc
echo "drive c: file=\"$(pwd)/${isodir}/boot/efiboot.img\"" > ${MTOOLSRC}

mmd c:/EFI
mmd c:/EFI/boot
mcopy ${builddir}/bin/kbootx64.efi c:/EFI/boot/bootx64.efi

rm ${MTOOLSRC}

cp ${builddir}/test/test-ia32.elf ${builddir}/test/test-amd64.elf ${isodir}/

cat > ${isodir}/boot/kboot.cfg << EOF
set "timeout" 5

entry "Test (32-bit)" {
    kboot "/test-ia32.elf" ["/test-ia32.elf"]
}

entry "Test (64-bit)" {
    kboot "/test-amd64.elf" ["/test-amd64.elf"]
}
EOF

xorriso -as mkisofs -J -R -l -V "KBOOT" -e boot/efiboot.img -no-emul-boot -o ${builddir}/test.iso ${isodir}
rm -rf ${isodir}

if [ ! -e ".ovmf-amd64-cd.bin" ]; then
    cp test/qemu/efi/ovmf-amd64.bin .ovmf-amd64-cd.bin
fi

qemu-system-x86_64 -pflash .ovmf-amd64-cd.bin -cdrom ${builddir}/test.iso -serial stdio -m 512 -monitor vc:1024x768 -s
