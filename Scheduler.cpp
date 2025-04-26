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
static vector<TaskId_t> task_queue;
static map<MachineId_t, vector<VMId_t>> machine_to_vm;
static map<VMId_t, vector<TaskId_t>> migrate_queue;
static map<TaskId_t, VMId_t> task_to_vm;
static vector<int> num_machines_per_type(4);
static int completed_tasks = 0;

struct UtilizationComparator {
    bool operator()(const MachineId_t& m1, const MachineId_t& m2) const {
         MachineInfo_t m1info = Machine_GetInfo(m1);
         MachineInfo_t m2info = Machine_GetInfo(m2);
         int utilization_1 = (m1info.memory_used / m1info.memory_size) * 5 + m1info.active_tasks;
         int utilization_2 = (m2info.memory_used / m2info.memory_size) * 5 + m2info.active_tasks;
         return utilization_1 < utilization_2;
    } 
 };


void Scheduler::Init() {
    // SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    // SimOutput("Scheduler::Init(): Initializing scheduler", 1);
    

    for(unsigned i = 0; i < Machine_GetTotal(); i++) {
        // creating new machine
        MachineId_t machine_id = MachineId_t(i);
        Machine_SetState(machine_id, S0);
        machines.push_back(machine_id);
        MachineInfo_t current_machine = Machine_GetInfo(machine_id);
        vector<VMId_t> temp;
        machine_to_vm.insert({MachineId_t(i), temp});
        num_machines_per_type[current_machine.cpu]++;
    }

    // SimOutput("Scheduler::Init(): VM ids are " + to_string(vms[0]) + " ahd " + to_string(vms[1]), 3);
}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
    vms.push_back(vm_id);
    migrating = false;
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    // task_queue.push_back(task_id);
    //cout << task_id << endl;
    decide_VM(task_id);
    //tasks++;
}

void Scheduler::decide_VM(TaskId_t task_id) {
    VMId_t chosen_vm = -1;
    //find vm to mount on

    if(vms.size() >= machines.size()) {
        for(VMId_t vm : vms) {
            VMInfo_t vm_info = VM_GetInfo(vm);
            
            if (vm_info.cpu == RequiredCPUType(task_id)) {
                chosen_vm = vm;
                break;
            }
        }

        if(chosen_vm != -1) {
            VM_AddTask(chosen_vm, task_id, GetTaskInfo(task_id).priority);
            task_to_vm.insert({task_id, chosen_vm});
        }
    }
    bool found_machine = false;
    if(vms.size() == 0 || chosen_vm == -1) {

        //choose machine
        for(MachineId_t machine: machines) {
            MachineInfo_t machine_info = Machine_GetInfo(machine);

            if(machine_info.s_state == S0 && machine_info.cpu == GetTaskInfo(task_id).required_cpu &&
                machine_info.memory_size - machine_info.memory_used >= GetTaskInfo(task_id).required_memory) {
                    VMId_t new_vm = VM_Create(GetTaskInfo(task_id).required_vm, GetTaskInfo(task_id).required_cpu);
                    chosen_vm = new_vm;
                    vms.push_back(new_vm);
                    found_machine = true;
                    VM_Attach(chosen_vm, machine);
                    machine_to_vm.at(machine).push_back(chosen_vm);
                    break;
            }
        }
        // need to make a new machine
        if(!found_machine) {
            for(MachineId_t machine: machines) {
                if(Machine_GetInfo(machine).s_state == S5 && Machine_GetInfo(machine).cpu == GetTaskInfo(task_id).required_cpu) {
                    Machine_SetState(machine, S0);
                    num_machines_per_type[Machine_GetCPUType(machine)]++;
                    task_queue.push_back(task_id);
                    break;
                }
            }
         } else { // machine was found and vm was attached
            if(!migrating) {
                VM_AddTask(chosen_vm, task_id, GetTaskInfo(task_id).priority);
                task_to_vm.insert({task_id, chosen_vm});
            } else {
                task_queue.push_back(task_id);
            }
        }
    }
}

void Scheduler::PeriodicCheck(Time_t now) {
    // This method should be called from SchedulerCheck()
    // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
    // Unlike the other invocations of the scheduler, this one doesn't report any specific event
    // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary

    // scheduling tasks from task queue
    while (!task_queue.empty()) {
        TaskId_t temp = task_queue.front();
        task_queue.erase(task_queue.begin());
        Scheduler::NewTask(now, temp);
    }
    
    // turning off machines with no active vms
    for(unsigned i = 0; i < machines.size(); i++) {
        if (Machine_GetInfo(machines[i]).active_vms == 0 && machines.size() > 2) {
            if(num_machines_per_type[Machine_GetInfo(machines[i]).cpu] != 1) {
                MachineId_t temp = machines[i];
                num_machines_per_type[Machine_GetInfo(machines[i]).cpu]--;
                machines.erase(machines.begin() + i);
                Machine_SetState(temp, S5);
                i--;
            }
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
    sort(machines.begin(), machines.end(), UtilizationComparator());

    MachineId_t completed_machine = task_to_vm.at(task_id);
    task_to_vm.erase(task_id);

    if (num_machines_per_type[Machine_GetInfo(completed_machine).cpu] <= 1) {
        return;
    }
    // looking for machine to migrate to starting with highest utilization machine
    bool found = false;
    int to_migrate_index = -1;
    for (int j = machines.size() - 1; j >= 0 && !found; j--) {
        // cpu type matches
        if (Machine_GetCPUType(machines[j]) == Machine_GetCPUType(completed_machine)) {
            // checking if there is space to migrate to
            unsigned current_memory_usage = Machine_GetInfo(completed_machine).memory_used;
            unsigned new_memory_usage = Machine_GetInfo(machines[j]).memory_used;
            if ((current_memory_usage + new_memory_usage) < Machine_GetInfo(machines[j]).memory_size) {
                to_migrate_index = j;
                found = true;
                break;
            }
        }
    }

    if (to_migrate_index != -1)  {
        //cout << "found machine to migrate to" << endl;
        // migrate all vms for the machine
        vector<VMId_t>& current_vms = machine_to_vm[completed_machine];
        vector<VMId_t> temp_vms = current_vms; 
        
        for (unsigned j = 0; j < temp_vms.size(); j++) {
            VMId_t temp = temp_vms[j];
            auto it = find(vms.begin(), vms.end(), temp);
            if(it != vms.end()) {
                current_vms.erase(remove(current_vms.begin(), current_vms.end(), temp), current_vms.end());
                vms.erase(remove(vms.begin(), vms.end(), temp), vms.end());
                migrating = true;
                //cout << "migrating a vm right now" << endl;
                VM_Migrate(temp, machines[to_migrate_index]);
            }
        }
    }
}

void Scheduler:: SLAHandle(TaskId_t task_id) {

    auto vm_it = find(vms.begin(), vms.end(), task_to_vm.at(task_id));
        if(vm_it != vms.end()) {
            sort(machines.begin(), machines.end(), UtilizationComparator());
            MachineId_t violated_machine = VM_GetInfo(task_to_vm.at(task_id)).machine_id;
            for(MachineId_t machine: machines) {
                if(machine != violated_machine && Machine_GetInfo(machine).cpu == Machine_GetInfo(violated_machine).cpu && Machine_GetInfo(machine).s_state == S0 && Machine_GetInfo(machine).active_tasks < GetNumTasks()/Machine_GetTotal()) {
                    SimOutput("vm to be migrated: " + to_string(task_to_vm.at(task_id)) + " new machine: " + to_string(machine), 1);
                    auto it = remove(vms.begin(), vms.end(), task_to_vm.at(task_id));
                    vms.erase(it, vms.end());
                    VM_Migrate(task_to_vm.at(task_id), machine);
                    break;
                }
            }
        }
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
    //SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
    Scheduler.MigrationComplete(time, vm_id);
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 4);
    Scheduler.PeriodicCheck(time);
    // static unsigned counts = 0;
    // counts++;
    // if(counts == 10) {
    //     migrating = true;
    //     VM_Migrate(1, 9);
    // }
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
    Scheduler.SLAHandle(task_id);
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
}


