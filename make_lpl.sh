#!/usr/bin/env bash

LPL="Commodore Amiga.lpl"

# rom path
# rom name
# core path
# core name
# crc
# playlist name

rm -f "$LPL"

D=`pwd`
for i in *zip; do
  echo "$D/$i"  >> "$LPL"
  echo "${i%%.zip}"  >> "$LPL"
  echo "puae_libretro.so"  >> "$LPL"
  echo "PUAE"  >> "$LPL"
  echo  >> "$LPL"
  echo "$LPL"  >> "$LPL"
done
