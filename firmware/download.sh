#!/bin/bash

if [ -f D002.032.bin ]; then
  echo "File already exists, exiting..."
  exit 1
fi

if [ -x "$(command -v curl)" ]; then
  curl -L -o D002.032.zip https://md380.org/firmware/orig/TYT-Tytera-MD-380-FW-v232.zip
else
  wget -O D002.032.zip https://md380.org/firmware/orig/TYT-Tytera-MD-380-FW-v232.zip
fi

if [ ! -f D002.032.zip ]; then
  echo "File not downloaded, exiting..."
  exit 1
fi

# Unzip the file "Firmware 2.32/MD-380-D2.32(AD).bin", overwrite if exists
unzip -j -o D002.032.zip "Firmware 2.32/MD-380-D2.32(AD).bin" -d ./

if [ "$(shasum -a 256 MD-380-D2.32\(AD\).bin | cut -d ' ' -f 1)" != "e1a1a11a84d9afd2d2ceed61be8395e32a8034785abd0be0236478f7d8609f43" ]; then
  echo "Checksum mismatch, exiting..."
  exit 1
fi

# Unwrap the file to "firmware.img"
python3 md380-fw.py --unwrap MD-380-D2.32\(AD\).bin D002.032.bin

# Cleanup
rm MD-380-D2.32\(AD\).bin D002.032.zip
