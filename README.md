# md380emu

MD380 AMBE emulation that works on x86_64 and arm64 natively. This uses dynarmic to run the MD380 firmware.


## Building
```
git clone --recursive https://github.com/rhgndf/md380emu.git
cd md380emu
mkdir build
cd build
cmake ..
make -j $(nproc)
```

## Usage
```
Usage: 
       build/md380emu <decode|encode> <infile> <outfile>
       build/md380emu <decode_bench|encode_bench
```