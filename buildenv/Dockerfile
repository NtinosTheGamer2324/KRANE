FROM randomdude/gcc-cross-x86_64-elf

RUN apt-get update 
RUN apt-get upgrade -y
RUN apt-get install -y nasm
RUN apt-get install -y xorriso
RUN apt-get install -y grub-pc-bin
RUN apt-get install -y grub-common
RUN apt-get install -y grub-efi-amd64-bin
RUN apt-get install -y mtools
RUN apt-get install -y dosfstools
RUN apt-get install -y parted
RUN apt-get install -y gcc

# Native 16-bit C compiler and binary tools from APT
RUN apt-get install -y bcc bin86

RUN apt-get update --fix-missing && apt-get upgrade -y
RUN apt-get install -y udev

VOLUME /root/env
WORKDIR /root/env