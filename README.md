A stripped down implementation of C# suitable for native development
on embedded systems.

Currently the only target supported is PicoCalc. You should be able to target pico2 by editing the toplevel CMakeLists.txt file (search for PICO_BOARD)

To build it, clone the repo then run
```
cmake -B build
cd build
make
```

Then either run the tinysharp.elf through openocd or drag and drop the uf2 after plugging in a cable into the micro USB port while holding down BOOTSEL.
