#!/bin/bash -ex

builddir=build/bios
pxedir=${builddir}/pxe

mkdir ${pxedir}
mkdir ${pxedir}/boot

cp ${builddir}/bin/pxekboot.img ${pxedir}/boot/
cp ${builddir}/test/test-ia32.elf ${builddir}/test/test-amd64.elf ${pxedir}/

cat > ${pxedir}/boot/kboot.cfg << EOF
set "timeout" 5

entry "Test (32-bit)" {
    kboot "test-ia32.elf" ["test-ia32.elf"]
}

entry "Test (64-bit)" {
    kboot "test-amd64.elf" ["test-amd64.elf"]
}
EOF

qemu-system-x86_64 -bootp tftp://10.0.2.2//boot/pxekboot.img -tftp ${pxedir} -boot n -serial stdio -vga std -m 512 -monitor vc:1024x768 -s
rm -rf ${pxedir}
