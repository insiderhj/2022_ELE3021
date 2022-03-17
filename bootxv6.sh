XV6_DIR=/home/insiderhj/OS/xv6-public/

qemu-system-i386 -nographic -serial mon:stdio -hdb ${XV6_DIR}fs.img  ${XV6_DIR}xv6.img -smp 1 -m 512
