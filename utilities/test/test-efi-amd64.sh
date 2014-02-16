#!/bin/bash -ex

builddir=build/efi-amd64
fsdir=${builddir}/testfs

mkdir ${fsdir}
mkdir -p ${fsdir}/efi/boot

cp ${builddir}/bin/bootx64.efi ${fsdir}/efi/boot/
#cp ${builddir}/test/test32.elf ${builddir}/test/test64.elf ${fsdir}/

cat > ${fsdir}/loader.cfg << EOF
set "timeout" 5

entry "Test (32-bit)" {
	kboot "/test32.elf" ["/test32.elf"]
}

entry "Test (64-bit)" {
	kboot "/test64.elf" ["/test64.elf"]
}
EOF

if [ ! -e ".ovmf-x86_64.bin" ]; then
    cp utilities/test/efi/ovmf-x86_64.bin .ovmf-x86_64.bin
fi

qemu-system-x86_64 -pflash .ovmf-x86_64.bin -hda fat:${fsdir} -serial stdio \
    -m 512 -monitor vc:1024x768

rm -rf ${fsdir}
