#!/bin/sh

central_bin="$(pwd)/central/.pio/build/nrf52840_dk/firmware.elf"
peripheral_bin="$(pwd)/peripheral/.pio/build/nrf52840_dk/firmware.elf"

renode -e \
"using sysbus

# Lines below declare binary files for Zephyr BLE devices. One for central device and one for peripheral device.
# Binaries used here are defaults and can be replaced by changing variables below before running this script.

emulation CreateBLEMedium \"wireless\"

mach create \"central\"
machine LoadPlatformDescription @platforms/cpus/nrf52840.repl
connector Connect sysbus.radio wireless
showAnalyzer uart0 

mach create \"peripheral\"
machine LoadPlatformDescription @platforms/cpus/nrf52840.repl
connector Connect sysbus.radio wireless
showAnalyzer uart0 

# Set Quantum value for CPUs. This is required by BLE stack.
# Moreover, it allows better synchronisation between machines.
emulation SetGlobalQuantum \"0.00001\"

# The following series of commands is executed everytime the machine is reset.
macro reset
\"\"\"
    mach set \"central\"
    sysbus LoadELF @$central_bin

    mach set \"peripheral\"
    sysbus LoadELF @$peripheral_bin 
\"\"\"

runMacro \$reset
start
"