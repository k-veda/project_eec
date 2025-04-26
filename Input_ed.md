machine class:
{
    Number of machines: 16
    CPU type: X86
    Number of cores: 8
    Memory: 16384
    S-States: [120, 100, 100, 80, 40, 10, 0]
    P-States: [12, 8, 6, 4]
    C-States: [12, 3, 1, 0]
    MIPS: [1000, 800, 600, 400]
    GPUs: yes
}

task class:
{
    Start time: 60000
    End time: 800000
    Inter arrival: 6000
    Expected runtime: 2000000
    Memory: 8
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA0
    CPU type: X86
    Task type: WEB
    Seed: 520230
}

machine class:
{
    Number of machines: 24
    CPU type: ARM
    Number of cores: 16
    Memory: 16384
    S-States: [120, 100, 100, 80, 40, 10, 0]
    P-States: [12, 8, 6, 4]
    C-States: [12, 3, 1, 0]
    MIPS: [1000, 800, 600, 400]
    GPUs: yes
}

task class:
{
    Start time: 70000
    End time: 900000
    Inter arrival: 5000
    Expected runtime: 1800000
    Memory: 16
    VM type: LINUX
    GPU enabled: yes
    SLA type: SLA1
    CPU type: ARM
    Task type: CRYPTO
    Seed: 752390
}