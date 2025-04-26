//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include "Scheduler.hpp"
#include <bits/stdc++.h>

//static bool migrating = false;
//static unsigned active_machines = 16;
static map<MachineId_t, vector<VMId_t>> machine_to_vm;
static map<TaskId_t, VMId_t> task_to_vm;
static map<MachineId_t, vector<VMId_t>> migrate_queue;
static vector<MachineId_t> sla_machines;
static vector<int> num_machines_per_type(4);
int completed_num = 0;

struct TaskComparator_Instructions {
   bool operator()(const TaskId_t& t1, const TaskId_t& t2) const {
       if(GetTaskInfo(t1).total_instructions != GetTaskInfo(t2).total_instructions) {
           return GetTaskInfo(t1).total_instructions > GetTaskInfo(t2).total_instructions;
       } 
       return 0;
   }
};

struct MachineComparator {
   bool operator()(const MachineId_t& m1, const MachineId_t& m2) const {
       MachineInfo_t info1 = Machine_GetInfo(m1);
       MachineInfo_t info2 = Machine_GetInfo(m2);

    //    if(info1.s_state != info2.s_state) {
    //         return info1.s_state < info2.s_state;
    //    }

    //    // cpu to vm ratio
    //    if(info1.active_vms != 0 && info2.active_vms != 0) {
    //       return info1.num_cpus/info1.active_vms > info2.num_cpus/info2.active_vms; 
    //    } 

       return info1.active_tasks < info2.active_tasks;
   }
};

struct VMComparator {
   bool operator()(const VMId_t& v1, const VMId_t& v2) const {
       VMInfo_t info1 = VM_GetInfo(v1);
       VMInfo_t info2 = VM_GetInfo(v2);

    //    int counter1 = 0;
    //    int counter2 = 0;
    //    for(TaskId_t task: info1.active_tasks) {
    //     counter1 += GetTaskInfo(task).remaining_instructions;
    //    }

    //    for(TaskId_t task: info2.active_tasks) {
    //     counter2 += GetTaskInfo(task).remaining_instructions;
    //    }

       return info1.active_tasks.size() < info2.active_tasks.size();
    //    return info1.active_tasks.size() < info2.active_tasks.size();
   }
};

static priority_queue<TaskId_t, vector<TaskId_t>, TaskComparator_Instructions> task_queue;

void Scheduler::Init() {
    // Find the parameters of the clusters
    // Get the total number of machines
    // For each machine:
    //      Get the type of the machine
    //      Get the memory of the machine
    //      Get the number of CPUs
    //      Get if there is a GPU or not
    // 
    // SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    // SimOutput("Scheduler::Init(): Initializing scheduler", 1);
    
    unsigned total_machines = Machine_GetTotal();
    
    for (unsigned i = 0; i < total_machines; ++i) {
        vector<VMId_t> temp;
        machine_to_vm.insert({MachineId_t(i), temp});
        machines.push_back(MachineId_t(i));
        num_machines_per_type[Machine_GetInfo(MachineId_t(i)).cpu]++;
    }

    sort(machines.begin(), machines.end(), MachineComparator());

    // SimOutput("Scheduler::Init(): VM ids are " + to_string(vms[0]) + " and " + to_string(vms[1]), 3);
}
    
void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
    vms.push_back(vm_id);
    (machine_to_vm.at(VM_GetInfo(vm_id).machine_id)).push_back(vm_id);
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    task_queue.push(task_id);

    sort(machines.begin(), machines.end(), MachineComparator());
    // for(int i = 0; i < machines.size(); i++) {
    //     cout << "machine id " << machines[i] << " with " << Machine_GetInfo(machines[i]).active_vms << " vms" << endl;
    // }

    while(task_queue.size() > 0) {
        TaskId_t next_task = task_queue.top(); 
        task_queue.pop();
        VMId_t chosen_vm = -1;
        MachineId_t chosen_machine = -1;
        int min_load = INT_MAX;

        //find machine to mount on

        sort(machines.begin(), machines.end(), MachineComparator());

        for(MachineId_t machine: machines) {
            MachineInfo_t machine_info = Machine_GetInfo(machine);
            if(machine_info.s_state == S0 && machine_info.cpu == GetTaskInfo(next_task).required_cpu &&
            machine_info.memory_size - machine_info.memory_used >= GetTaskInfo(next_task).required_memory && (machine_info.gpus == GetTaskInfo(next_task).gpu_capable || !GetTaskInfo(next_task).gpu_capable)) {
                if(min_load > machine_info.active_vms) {
                    min_load = machine_info.active_vms;
                    chosen_machine = machine;
                }
            }
        }

        if(Machine_GetInfo(chosen_machine).active_vms > Machine_GetInfo(chosen_machine).num_cpus) {
            sort(vms.begin(), vms.end(), VMComparator());
            for(VMId_t vm : vms) {
                VMInfo_t vm_info = VM_GetInfo(vm);
                if (vm_info.cpu == RequiredCPUType(next_task)) {
                    chosen_vm = vm;
                    break;
                }
            }
        }

        bool newly_created = false;
        
        if(chosen_vm == -1) {
            VMId_t new_vm = VM_Create(GetTaskInfo(next_task).required_vm, GetTaskInfo(next_task).required_cpu);
            chosen_vm = new_vm;
            vms.push_back(new_vm);
            newly_created = true;
        }
        

        //cout << "chosen vm is " << chosen_vm << endl;

        //cout << "adding task " << next_task << " to vm " << chosen_vm << endl;
        //cout << "vm " << chosen_vm <<  " is on machine " << VM_GetInfo(chosen_vm).machine_id << endl;

        if(newly_created) {
            //cout << "machine s state is " << Machine_GetInfo(chosen_machine).s_state << endl;
            VM_Attach(chosen_vm, chosen_machine);
            machine_to_vm.at(chosen_machine).push_back(chosen_vm);
        } 
        VM_AddTask(chosen_vm, next_task, GetTaskInfo(next_task).priority);
        task_to_vm.insert({next_task, chosen_vm});

    }
    //SimOutput("New task " + to_string(task_id) + " assigned to VM " + to_string(vm_id), 1);
}

void Scheduler::PeriodicCheck(Time_t now) {

    if(completed_num >= GetNumTasks()/2) {
        for(int i = vms.size()-1; i >=0; i--) {
            if(VM_GetInfo(vms[i]).active_tasks.size() == 0) {
                VM_Shutdown(vms[i]);
                auto vm_it = remove(vms.begin(), vms.end(), vms[i]);
                vms.erase(vm_it, vms.end());
            }
        }
    }

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
    completed_num++;
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);
}

void Scheduler::SLAHandle(TaskId_t task_id) {

    SetTaskPriority(task_id, HIGH_PRIORITY);

    // << "violation on machine " << VM_GetInfo(task_to_vm.at(task_id)).machine_id << endl;
    //cout << "task cpu type is " << GetTaskInfo(task_id).required_cpu << endl;

    // for(MachineId_t machine: machines) {
    //     cout << "machine " << machine << " has " << Machine_GetInfo(machine).active_vms << " vms and " << Machine_GetInfo(machine).active_tasks << " tasks" << endl;
    //     cout << "machine cpu type is " << Machine_GetInfo(machine).cpu << endl;
    //     cout << "machine s state is " << Machine_GetInfo(machine).s_state << endl << endl;
    // }

    MachineId_t violated_machine = VM_GetInfo(task_to_vm.at(task_id)).machine_id; 
    sla_machines.push_back(violated_machine);
    MachineInfo_t this_machine_info = Machine_GetInfo(violated_machine);
    int numVMs = this_machine_info.active_vms;

    vector<MachineId_t> candidates;

    for(MachineId_t machine: machines) {
        if(machine == violated_machine) continue;
        auto sla_it = find(sla_machines.begin(), sla_machines.end(), machine);
        if(sla_it == sla_machines.end()) {
            MachineInfo_t other_machine_info = Machine_GetInfo(machine);
            if(other_machine_info.cpu == Machine_GetInfo(violated_machine).cpu && other_machine_info.s_state == S0) {
                candidates.push_back(machine);
            }
        }
    }

    if(candidates.size() > 0) {
        MachineId_t chosen_machine = candidates[rand() % candidates.size()];
        vector<VMId_t> violated_vms = machine_to_vm.at(violated_machine);

        for(int i = violated_vms.size()-1; i >= violated_vms.size()/2; i--) {
            auto it = find(vms.begin(), vms.end(), violated_vms[i]);
            if(it != vms.end()) {
                auto it = remove(vms.begin(), vms.end(), violated_vms[i]);
                vms.erase(it, vms.end());
                //cout << "here1" << endl;
                VM_Migrate(violated_vms[i], chosen_machine);
            }
        }
    }
}

void Scheduler::MemoryHandle(MachineId_t machine_id) {

}

void Scheduler::HandleWakeup(MachineId_t machineid) {
    if(migrate_queue.at(machineid).size() > 0) {
        for(auto vm: migrate_queue.at(machineid)) {
            auto it = remove(vms.begin(), vms.end(), vm);
            vms.erase(it, vms.end());
            //cout << "here2" << endl;
            VM_Migrate(vm, machineid);
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
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
    Scheduler.MemoryHandle(machine_id);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
    Scheduler.MigrationComplete(time, vm_id);
    //migrating = false;
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
    SimOutput("on machine " + to_string(VM_GetInfo(task_to_vm.at(task_id)).machine_id) + " and vm " + to_string(task_to_vm.at(task_id)), 1);
    Scheduler.SLAHandle(task_id);
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
    if(Machine_GetInfo(machine_id).s_state == S0) {
        Scheduler.HandleWakeup(machine_id);
    } 
}


