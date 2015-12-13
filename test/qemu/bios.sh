#!/bin/bash -ex

builddir=build/bios
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

build/host/bin/kboot-mkiso --targets=bios ${builddir}/test.iso ${isodir}
rm -rf ${isodir}
qemu-system-x86_64 -cdrom ${builddir}/test.iso -serial stdio -vga std -boot d -m 512 -monitor vc:1024x768 -s
