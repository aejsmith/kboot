#!/bin/bash -ex

builddir=build/bios
isodir=${builddir}/iso

mkdir ${isodir}
mkdir ${isodir}/boot

cp ${builddir}/bin/cdboot.img ${isodir}/boot/
#cp ${builddir}/test/test32.elf ${builddir}/test/test64.elf ${isodir}/

cat > ${isodir}/boot/kboot.cfg << EOF
set "timeout" 5

entry "Test (32-bit)" {
	kboot "/test32.elf" ["/test32.elf"]
}

entry "Test (64-bit)" {
	kboot "/test64.elf" ["/test64.elf"]
}

entry "Chainload (hd0)" {
	device "(hd0)"
	chainload
}
EOF

mkisofs -J -R -l -b boot/cdboot.img -V "CDROM" -boot-load-size 4 -boot-info-table -no-emul-boot -o ${builddir}/test.iso ${isodir}
rm -rf ${isodir}
qemu-system-x86_64 -cdrom ${builddir}/test.iso -serial stdio -vga std -boot d -m 512 -monitor vc:1024x768 -s
