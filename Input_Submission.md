machine class:
{
# RISCV machines have lower power consumption, but also lower performance - only LINUX can run on this type
Number of machines: 7
CPU type: RISCV
Number of cores: 8
Memory: 16384
S-States: [90, 75, 65, 50, 30, 8, 0]
P-States: [10, 8, 5, 3]
C-States: [10, 4, 1, 0]
MIPS: [900, 700, 500, 300]
GPUs: yes
}

machine class:
{
# x86 machines - WIN can only run on this machine
Number of machines: 10
CPU type: X86
Number of cores: 8
Memory: 16384
S-States: [120, 100, 100, 80, 40, 10, 0]
P-States: [12, 8, 6, 4]
C-States: [12, 3, 1, 0]
MIPS: [1000, 800, 600, 400]
GPUs: yes
}

machine class:
{
# to test tasks that can only run with Power CPU, also tests limited cores, memory
# POWER machines - AIX can only run on this machine
Number of machines: 5
CPU type: POWER
Number of cores: 5
Memory: 10000
S-States: [120, 90, 70, 50, 30, 10, 0]
P-States: [12, 10, 7, 4]
C-States: [12, 4, 2, 0]
MIPS: [1000, 800, 600, 400]
GPUs: no
}

task class: 
{
# Spike in load intensity
# short inter arrival, long expected runtime, short start end time window, stream task type requires short busts of computing power
Start time: 10000
End time: 400000
Inter arrival: 1000
Expected runtime: 450000
Memory: 8
VM type: LINUX
GPU enabled: no
SLA type: SLA1
CPU type: X86
Task type: STREAM
Seed: 438249
}

task class: 
{
# Constraints with OS
# VM type only compatible with CPU POWER type, medium length processing time      
Start time: 20000
End time: 600000
Inter arrival: 7000
Expected runtime: 200000
Memory: 16
VM type: AIX
GPU enabled: no
SLA type: SLA2
CPU type: POWER
Task type: HPC
Seed: 328497
}

task class: 
{
# Constraints with memory 
# greater memory requested than available in any machine class, gpu enabled (hardware constraint), long runtime
Start time: 30000
End time: 600000
Inter arrival: 9000
Expected runtime: 400000
Memory: 20000
VM type: LINUX
GPU enabled: yes
SLA type: SLA0
CPU type: X86
Task type: AI
Seed: 834329
}

task class: 
{
# Low utilization - testing if cores are powered off (SLA3, so no urgency in completing tasks)
# infrequent inter arrival, very long runtime, low memory required
Start time: 40000
End time: 800000
Inter arrival: 20000
Expected runtime: 1500000
Memory: 4
VM type: WIN
GPU enabled: no
SLA type: SLA3
CPU type: X86
Task type: WEB
Seed: 346732
}

task class: 
{
# Varying types of tasks at same time
# frequent inter arrival, crypto is compute intensive
Start time: 50000
End time: 900000
Inter arrival: 5000
Expected runtime: 600000
Memory: 12
VM type: LINUX_RT
GPU enabled: yes
SLA type: SLA2
CPU type: RISCV
Task type: CRYPTO
Seed: 329473
}

task class: 
{
# Another task arriving at the same time as different type of task to test how system deals with heteregoneity
# frequent inter arrival, OS compatible with only ARM andd X86, short requests
Start time: 50000
End time: 900000
Inter arrival: 4000
Expected runtime: 550000
Memory: 10
VM type: WIN
GPU enabled: no
SLA type: SLA1
CPU type: X86
Task type: AI
Seed: 234783
}

