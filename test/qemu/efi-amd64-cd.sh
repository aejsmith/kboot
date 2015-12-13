#!/bin/bash -ex

builddir=build/efi-amd64
isodir=${builddir}/iso

mkdir ${isodir}
mkdir ${isodir}/boot

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

build/host/bin/kboot-mkiso --targets=efi-amd64 ${builddir}/test.iso ${isodir}
rm -rf ${isodir}

if [ ! -e ".ovmf-amd64-cd.bin" ]; then
    cp test/qemu/efi/ovmf-amd64.bin .ovmf-amd64-cd.bin
fi

qemu-system-x86_64 -pflash .ovmf-amd64-cd.bin -cdrom ${builddir}/test.iso -serial stdio -m 512 -monitor vc:1024x768 -s
