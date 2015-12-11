#!/bin/bash -ex

builddir=build/efi-ia32
fsdir=${builddir}/testfs

mkdir ${fsdir}
mkdir -p ${fsdir}/efi/boot

cp ${builddir}/bin/kbootia32.efi ${fsdir}/efi/boot/bootia32.efi
cp ${builddir}/test/test-ia32.elf ${builddir}/test/test-amd64.elf ${fsdir}/

cat > ${fsdir}/efi/boot/kboot.cfg << EOF
set "timeout" 5

entry "Test (32-bit)" {
    kboot "/test-ia32.elf" ["/test-ia32.elf"]
}

entry "Test (64-bit)" {
    kboot "/test-amd64.elf" ["/test-amd64.elf"]
}
EOF

if [ ! -e ".ovmf-ia32.bin" ]; then
    cp test/qemu/efi/ovmf-ia32.bin .ovmf-ia32.bin
fi

qemu-system-x86_64 -pflash .ovmf-ia32.bin -hda fat:${fsdir} -serial stdio -m 512 -monitor vc:1024x768 -s

rm -rf ${fsdir}
