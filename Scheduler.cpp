//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include "Scheduler.hpp"
#include <bits/stdc++.h>

static bool migrating = false;
// static unsigned active_machines = 16;
static map<MachineId_t, vector<VMId_t>> machine_to_vm;
static map<TaskId_t, VMId_t> task_to_vm;
static vector<VMId_t> migrate_queue;
static int active_machines;
static int active_tasks;
static vector<MachineId_t> sla_machines;


struct TaskComparator_Deadline {
   bool operator()(const TaskId_t& t1, const TaskId_t& t2) const {
       if(GetTaskInfo(t1).target_completion != GetTaskInfo(t2).target_completion) {
           return GetTaskInfo(t1).target_completion < GetTaskInfo(t2).target_completion;
       }
       return 0;
   }
};

static priority_queue<TaskId_t, vector<TaskId_t>, TaskComparator_Deadline> task_queue;

void Scheduler::Init() {
    // Find the parameters of the clusters
    // Get the total number of machines
    // For each machine:
    //      Get the type of the machine
    //      Get the memory of the machine
    //      Get the number of CPUs
    //      Get if there is a GPU or not
    // 
    SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    SimOutput("Scheduler::Init(): Initializing scheduler", 1);
    
    unsigned total_machines = Machine_GetTotal();
    for (unsigned i = 0; i < total_machines; ++i) {
        //machines.push_back(MachineId_t(i));
        // MachineInfo_t current_machine = Machine_GetInfo(i);
        
        // Create and attach VMs based on CPU compatibility
        vector<VMId_t> temp;
        machine_to_vm.insert({MachineId_t(i), temp});
        HandleWakeup(MachineId_t(i));
        active_machines++;
    }
    
    SimOutput("Scheduler::Init(): VM ids are " + to_string(vms[0]) + " ahd " + to_string(vms[1]), 3);
}

int Scheduler::decide_VM(TaskId_t task_id) {
    TaskInfo_t task = GetTaskInfo(task_id);
    
    // Iterate over existing VMs to find the best fit

    sort(vms.begin(), vms.end(), [](VMId_t v1, VMId_t v2) {
        return VM_GetInfo(v1).active_tasks.size() < VM_GetInfo(v2).active_tasks.size(); 
    });

    if(vms.size() >= machines.size()) {
        for (VMId_t vm_id : vms) {
            VMInfo_t vm_info = VM_GetInfo(vm_id);
            if(vm_info.cpu == task.required_cpu) {
                VM_AddTask(vm_id, task_id, task.priority);
                task_to_vm.insert({task_id, vm_id});
                return vm_id;
            }
        }
    }

    // If no suitable VM found, create a new one
    int new_vm = VM_Create(task.required_vm, task.required_cpu);
    vms.push_back(new_vm);
    decideMachine(new_vm);
    
    // Assign task to the chosen VM
    VM_AddTask(new_vm, task_id, task.priority);
    task_to_vm.insert({task_id, new_vm});
    return new_vm;
}

int Scheduler::decideMachine(VMId_t created_vm) {
    VMInfo_t vm_info = VM_GetInfo(created_vm);
    MachineId_t chosen_machine = -1;
    unsigned max_memory = 0;

    for (MachineId_t machine_id : machines) {
        MachineInfo_t machine_info = Machine_GetInfo(machine_id);
        if(machine_info.s_state == S5) {
            continue;
        }
        // Ensure the machine is compatible and has enough resources
        if (machine_info.cpu == vm_info.cpu && machine_info.active_vms < vms.size() / machines.size()) {
            chosen_machine = machine_id;
        }
    }

    VM_Attach(created_vm, chosen_machine);
    (machine_to_vm.at(chosen_machine)).push_back(created_vm);
    return chosen_machine;
}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
    //vms.push_back(vm_id);
    (machine_to_vm.at(VM_GetInfo(vm_id).machine_id)).push_back(vm_id);
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    task_queue.push(task_id);
    TaskId_t next_task = task_queue.top(); 
    task_queue.pop();
    int vm_id = decide_VM(next_task);
    SimOutput("New task " + to_string(next_task) + " assigned to VM " + to_string(vm_id), 1);
}

void Scheduler::PeriodicCheck(Time_t now) {
    for (MachineId_t machine_id : machines) {
        MachineInfo_t machine_info = Machine_GetInfo(machine_id);
        if (machine_info.active_tasks < machine_info.num_cpus / 4) {
            Machine_SetCorePerformance(machine_id, 0, P3); 
        } else if(machine_info.active_tasks < machine_info.num_cpus / 3) {
            Machine_SetCorePerformance(machine_id, 0, P2);
        } else if(machine_info.active_tasks < machine_info.num_cpus / 2) {
            Machine_SetCorePerformance(machine_id, 0, P1);
        } else {
            Machine_SetCorePerformance(machine_id, 0, P0);
        }
    }
}

void Scheduler::Shutdown(Time_t time) {
    // Do your final reporting and bookkeeping here.
    // Report about the total energy consumed
    // Report about the SLA compliance
    // Shutdown everything to be tidy :-)
    for(auto & vm: vms) {
        VM_Shutdown(vm);
    }
    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    // Do any bookkeeping necessary for the data structures
    // Decide if a machine is to be turned off, slowed down, or VMs to be migrated according to your policy
    // This is an opportunity to make any adjustments to optimize performance/energy
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);
}

void Scheduler::SLAHandle(TaskId_t task_id) {
   // TaskInfo_t violated = GetTaskInfo(task_id);
        // Check if we need to redistribute tasks
        // If redistribution is not enough, turn on more machines

    // cout << "violation on machine " << VM_GetInfo(task_to_vm.at(task_id)).machine_id << endl;
    // cout << "number of vms " << Machine_GetInfo(VM_GetInfo(task_to_vm.at(task_id)).machine_id).active_vms << endl;
    // cout << "number of tasks " << Machine_GetInfo(VM_GetInfo(task_to_vm.at(task_id)).machine_id).active_tasks << endl;

    SetTaskPriority(task_id, HIGH_PRIORITY);
    bool found_machine = false;
    MachineId_t violated_machine = VM_GetInfo(task_to_vm.at(task_id)).machine_id;
    sla_machines.push_back(violated_machine);

    for(MachineId_t machine: machines) {
        auto it = find(sla_machines.begin(), sla_machines.end(), machine);
        if(it == sla_machines.end() && machine != violated_machine && Machine_GetInfo(machine).cpu == Machine_GetInfo(violated_machine).cpu && Machine_GetInfo(machine).s_state == S0) {
            //SimOutput("vm to be migrated: " + to_string(task_to_vm.at(task_id)) + " new machine: " + to_string(machine), 1);
                it = find(vms.begin(), vms.end(), task_to_vm.at(task_id));
                if(it == vms.end()) {
                    it = remove(vms.begin(), vms.end(), task_to_vm.at(task_id));
                    vms.erase(it, vms.end());
                    VM_Migrate(task_to_vm.at(task_id), machine);
                    found_machine = true;
                    break;
                }
            }
     }

    if(!found_machine) {
        for (MachineId_t machine_id : machines) {
            if (Machine_GetInfo(machine_id).s_state == S5 && VM_GetInfo(task_to_vm.at(task_id)).cpu == Machine_GetInfo(machine_id).cpu) {
                Machine_SetState(machine_id, S0);
                //SimOutput("Turning on machine " + to_string(machine_id) + " to handle SLA violations", 1);
                migrate_queue.push_back(task_to_vm.at(task_id));
                break;
            }
        }
    }
}

void Scheduler::HandleWakeup(MachineId_t machineid) {
    MachineInfo_t current_machine = Machine_GetInfo(machineid);

    if(migrate_queue.size() > 0) {
        for(auto vm: migrate_queue) {
            auto it = remove(vms.begin(), vms.end(), vm);
            vms.erase(it, vms.end());
            VM_Migrate(vm, machineid);
        }
    } else {
        if (current_machine.cpu == POWER) {
            VMId_t aix_vm = VM_Create(AIX, POWER);
            vms.push_back(aix_vm);
            VM_Attach(aix_vm, machineid);
            (machine_to_vm.at(machineid)).push_back(aix_vm);
        } else if (current_machine.cpu == ARM || current_machine.cpu == X86) {
            VMId_t win_vm = VM_Create(WIN, current_machine.cpu);
            vms.push_back(win_vm);
            VM_Attach(win_vm, machineid);
            (machine_to_vm.at(machineid)).push_back(win_vm);
        }
        
        VMId_t linux_vm = VM_Create(LINUX, current_machine.cpu);
        vms.push_back(linux_vm);
        VM_Attach(linux_vm, machineid);
        (machine_to_vm.at(machineid)).push_back(linux_vm);
        
        VMId_t linux_rt_vm = VM_Create(LINUX_RT, current_machine.cpu);
        vms.push_back(linux_rt_vm);
        VM_Attach(linux_rt_vm, machineid);
        (machine_to_vm.at(machineid)).push_back(linux_rt_vm);
    }
    machines.push_back(machineid);
}


// Public interface below

static Scheduler Scheduler;

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 4);
    Scheduler.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 4);
    Scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 4);
    Scheduler.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
    Scheduler.MemoryHandle(machine_id);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
    Scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 4);
    Scheduler.PeriodicCheck(time);
}

void SimulationComplete(Time_t time) {
    // This function is called before the simulation terminates Add whatever you feel like.
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;     // SLA3 do not have SLA violation issues
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
    SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 4);
    
    Scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {
    SimOutput("SLA violation: for task " + to_string(task_id) + " was detected at time " + to_string(time), 0);
    Scheduler.SLAHandle(task_id);
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
    if(Machine_GetInfo(machine_id).s_state == S0) {
        Scheduler.HandleWakeup(machine_id);
        active_machines++;
    } else {
        active_machines--;
    }
}


