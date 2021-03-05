*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Resource                      ${RENODEKEYWORDS}

*** Variables ***
${UART}                       sysbus.uart0
# Change to Contiki-NG directory on your machine
${CONTIKING}                  /home/robbe/contiki-ng

*** Keywords ***
Create Machine
    [Arguments]     ${elf}      ${name}     ${id}

    Execute Command             mach create ${name}
    Execute Command             using sysbus
    Execute Command             machine LoadPlatformDescription @platforms/boards/zolertia-firefly.repl
    Execute Command             connector Connect radio wireless

    Execute Command             machine PyDevFromFile @scripts/pydev/rolling-bit.py 0x400D2004 0x4 True "sysctrl"

    Execute Command             sysbus WriteDoubleWord 0x00280028 ${id}
    Execute Command             sysbus WriteDoubleWord 0x0028002C 0x00
    Execute Command             sysbus WriteDoubleWord 0x00280030 0xAB
    Execute Command             sysbus WriteDoubleWord 0x00280034 0x89
    Execute Command             sysbus WriteDoubleWord 0x00280038 0x00
    Execute Command             sysbus WriteDoubleWord 0x0028003C 0x4B
    Execute Command             sysbus WriteDoubleWord 0x00280040 0x12
    Execute Command             sysbus WriteDoubleWord 0x00280044 0x00

    Execute Command             sysbus LoadBinary @https://dl.antmicro.com/projects/renode/cc2538_rom_dump.bin-s_524288-0c196cdc21b5397f82e0ff42b206d1cc4b6d7522 0x0
    Execute Command             sysbus LoadELF ${elf}


*** Test Cases ***
Should Talk Over Wireless Network
    Set Test Variable           ${REPEATS}      3

    Execute Command             emulation CreateWirelessMedium "wireless"
    Execute Command             wireless SetRangeWirelessFunction 11

    Create Machine              @${CONTIKING}/examples/twofaced/build/zoul/firefly/twofaced-root.elf   "root"      1
    Execute Command             wireless SetPosition radio 0 0 0
    ${root-tester}=             Create Terminal Tester      ${UART}     machine=root
    Execute Command             mach clear

    Create Machine              @${CONTIKING}/examples/twofaced/build/zoul/firefly/twofaced-node.elf   "node-1"    2
    Execute Command             wireless SetPosition radio 10 0 0
    ${node1-tester}=            Create Terminal Tester      ${UART}     machine=node-1
    Execute Command             mach clear

    Create Machine              @${CONTIKING}/examples/twofaced/build/zoul/firefly/twofaced-node.elf   "node-2"    3
    Execute Command             wireless SetPosition radio 0 10 0
    ${node2-tester}=            Create Terminal Tester      ${UART}     machine=node-2
    Execute Command             mach clear


    Start Emulation

    :FOR  ${i}  IN RANGE  0  ${REPEATS}

    \   Wait For Line On Uart       Received request 'hello ${i}' from fd00::200:0:0:[2-3]  testerId=${root-tester}      treatAsRegex=true   timeout=10
    \   Wait For Line On Uart       Sending response\\.                                     testerId=${root-tester}      treatAsRegex=true
    \   Wait For Line On Uart       Received request 'hello ${i}' from fd00::200:0:0:[2-3]  testerId=${root-tester}      treatAsRegex=true
    \   Wait For Line On Uart       Sending response\\.                                     testerId=${root-tester}      treatAsRegex=true


    :FOR  ${i}  IN RANGE  0  ${REPEATS}

    \   Wait For Line On Uart       Sending request ${i} to fd00::200:0:0:1                 testerId=${node1-tester}
    \   Wait For Line On Uart       Received response 'hello ${i}' from fd00::200:0:0:1     testerId=${node1-tester}


    :FOR  ${i}  IN RANGE  0  ${REPEATS}

    \   Wait For Line On Uart       Sending request ${i} to fd00::200:0:0:1                 testerId=${node2-tester}
    \   Wait For Line On Uart       Received response 'hello ${i}' from fd00::200:0:0:1     testerId=${node2-tester}

