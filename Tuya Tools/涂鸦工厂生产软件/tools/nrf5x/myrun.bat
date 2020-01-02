tools\nrf5x\nrfjprog.exe -f nrf52 --reset
tools\nrf5x\nrfjprog.exe -f nrf52 --erasepage 0-0x60000
tools\nrf5x\nrfjprog.exe -f nrf52 --sectorerase --program tools\nrf5x\s132_nrf52_3.0.0_softdevice.hex
tools\nrf5x\nrfjprog.exe -f nrf52 --sectorerase --program %1
tools\nrf5x\nrfjprog.exe -f nrf52 --reset