# Build the tfpcmcia firmware using the WCH MRS toolchain in Docker.
#
# Build image (one time):
#   docker build -t tfpcmcia . --platform linux/amd64
#
# Compile firmware:
#   docker run --rm -v $PWD:/host tfpcmcia make -j -C /host/firmware
#
# Flash (run outside docker — minichlink runs on the host):
#   minichlink -w firmware/tfpcmcia.bin flash -b

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y xz-utils make

# Copy local MRS archive and extract only the RISC-V toolchain
COPY MounRiverStudio_Linux_X64_V240.tar.xz /tmp/mrs.tar.xz
RUN mkdir -p /opt/RISC-V_Embedded_GCC \
 && tar -xJf /tmp/mrs.tar.xz \
      --strip-components=9 \
      -C /opt/RISC-V_Embedded_GCC \
      "MRS-linux-x64/resources/app/resources/linux/components/WCH/Toolchain/RISC-V Embedded GCC" \
 && rm /tmp/mrs.tar.xz

ENV PATH="/opt/RISC-V_Embedded_GCC/bin:$PATH"
