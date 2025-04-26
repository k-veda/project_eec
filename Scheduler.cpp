//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include "Scheduler.hpp"
#include <bits/stdc++.h>
#include <iostream>

static bool migrating = false;
static int numTasks;
static vector<MachineId_t> all_machines;
using namespace std;
static vector<TaskId_t> task_queue;
static map<MachineId_t, vector<VMId_t>> machine_to_vm;

struct EnergyConsumptionComparator {
   bool operator()(const MachineId_t& m1, const MachineId_t& m2) const {
        MachineInfo_t m1info = Machine_GetInfo(m1);
        MachineInfo_t m2info = Machine_GetInfo(m2);
        int energy_consumption_proxy_1 = m1info.p_states[m1info.p_state] * (m1info.num_cpus + m1info.gpus * 5) + 
            (m1info.energy_consumed / Machine_GetClusterEnergy() * 5);
        int energy_consumption_proxy_2 = m2info.p_states[m2info.p_state] * (m2info.num_cpus + m2info.gpus * 5) + 
            (m2info.energy_consumed / Machine_GetClusterEnergy() * 5);
        return energy_consumption_proxy_1 < energy_consumption_proxy_2;
   } 
};

struct UtilizationComparator {
    bool operator()(const MachineId_t& m1, const MachineId_t& m2) const {
         MachineInfo_t m1info = Machine_GetInfo(m1);
         MachineInfo_t m2info = Machine_GetInfo(m2);
         int utilization_1 = (m1info.memory_used / m1info.memory_size) * 5 + m1info.active_tasks;
         int utilization_2 = (m2info.memory_used / m2info.memory_size) * 5 + m2info.active_tasks;
         return utilization_1 < utilization_2;
    } 
 };

struct VMComparator {
    bool operator() (const VMId_t& v1, const VMId_t& v2) const {
        VMInfo_t info1 = VM_GetInfo(v1);
        VMInfo_t info2 = VM_GetInfo(v2);
        MachineInfo_t m1info = Machine_GetInfo(info1.machine_id);
        MachineInfo_t m2info = Machine_GetInfo(info2.machine_id);
        int energy_consumption_proxy_1 = m1info.p_states[m1info.p_state] * (m1info.num_cpus + m1info.gpus * 5) + 
            (m1info.energy_consumed / Machine_GetClusterEnergy() * 5);
        int energy_consumption_proxy_2 = m2info.p_states[m2info.p_state] * (m2info.num_cpus + m2info.gpus * 5) + 
            (m1info.energy_consumed / Machine_GetClusterEnergy() * 5);
        return energy_consumption_proxy_1 < energy_consumption_proxy_2;
    }
};

void Scheduler::Init() {

   SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
   SimOutput("Scheduler::Init(): Initializing scheduler", 1);
   cout << Machine_GetTotal() << endl;
	numTasks = 0;

   // adding machines to array of all machines
   for(unsigned i = 0; i < Machine_GetTotal(); i++) {
       MachineInfo_t current_info = Machine_GetInfo(MachineId_t(i));
       all_machines.push_back(MachineId_t(i));
       vector<VMId_t> temp;
       machine_to_vm.insert({MachineId_t(i), temp});
   }

   sort(all_machines.begin(), all_machines.end(), EnergyConsumptionComparator());


   bool arm_machine_found = false;
   bool x86_machine_found = false;
   bool power_machine_found = false;

   for (unsigned i = 0; i < all_machines.size(); i++) {
    MachineInfo_t machine_info_current = Machine_GetInfo(all_machines[i]);
    if (!arm_machine_found && Machine_GetCPUType(all_machines[i]) == ARM) {
        Machine_SetState(all_machines[i], S0);
        cout << all_machines[i] << endl;
        machines.push_back(all_machines[i]);
        arm_machine_found = true;
    } else if (!x86_machine_found && Machine_GetCPUType(all_machines[i]) == X86) {
        Machine_SetState(all_machines[i], S0);
        cout << all_machines[i] << endl;
        machines.push_back(all_machines[i]);
        x86_machine_found = true;
    } else if (!power_machine_found && Machine_GetCPUType(all_machines[i]) == POWER) {
        Machine_SetState(all_machines[i], S0);
        cout << all_machines[i] << endl;
        machines.push_back(all_machines[i]);
        power_machine_found = true;
    }
    else {
        Machine_SetState(all_machines[i], S5);
        cout << "setting machine state" << endl;
        cout << "Machine " << all_machines[i] << " state after setting: " << Machine_GetInfo(all_machines[i]).s_state << endl;
    }
   }

    MachineInfo_t current_machine = Machine_GetInfo(machines[0]);
    int numVMs = current_machine.num_cpus / 2;
    for (int j = 0; j < numVMs; j++) {
        add_VM_to_Machine(machines[0]);
    }

   SimOutput("Scheduler::Init(): VM ids are " + to_string(vms[0]) + " ahd " + to_string(vms[1]), 3);
}

void Scheduler::add_VM_to_Machine(MachineId_t machine_id) {
    MachineInfo_t current_machine = Machine_GetInfo(machine_id);
    
    if(current_machine.cpu == POWER) {
        VMId_t created_vm = VM_Create(AIX, POWER);
        vms.push_back(created_vm);
        sort(vms.begin(), vms.end(), VMComparator());
        //cout << "vm attach 1" << endl;
        VM_Attach(created_vm, machine_id);
        (machine_to_vm.at(machine_id)).push_back(created_vm);

    } else {
        VMId_t created_vm = VM_Create(LINUX, current_machine.cpu);
        sort(vms.begin(), vms.end(), VMComparator());
        vms.push_back(created_vm);
        //cout << "vm attach 2" << endl;
        VM_Attach(created_vm, machine_id);
        (machine_to_vm.at(machine_id)).push_back(created_vm);
    }
}


void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
   // Update your data structure. The VM now can receive new tasks
   (machine_to_vm.at(VM_GetInfo(vm_id).machine_id)).push_back(vm_id);
   vms.push_back(vm_id);
}


void Scheduler::NewTask(Time_t now, TaskId_t task_id) {

   int chosen_vm = decide_VM(task_id);
	//cout << "active machines: " << machines.size() << ", num tasks: " << numTasks << ", num vms: " << vms.size() << ", task queue size: " << task_queue.size() << endl;
   Priority_t priority;
   if (RequiredSLA(task_id) == SLA0) {
       priority = HIGH_PRIORITY;
   } else if (RequiredSLA(task_id) == SLA1) {
       priority = MID_PRIORITY;
   } else {
       priority = LOW_PRIORITY;
   }

	if (chosen_vm == -1) {
		task_queue.push_back(task_id);
	} else {
		if (migrating) {
			task_queue.push_back(task_id);
		} else {
			VM_AddTask(chosen_vm, task_id, priority);
			numTasks++;
		}
	}
}

int Scheduler::decide_VM (TaskId_t task_id) {
	TaskInfo_t task = GetTaskInfo(task_id);
    
	// Iterate over existing VMs to find the best fit
	for (VMId_t vm_id : vms) {
		 VMInfo_t vm_info = VM_GetInfo(vm_id);
		 
		 // Check if VM has enough memory
		 if ((IsTaskGPUCapable(task_id) && (Machine_GetInfo(vm_info.machine_id).gpus)) || !IsTaskGPUCapable(task_id)) {
			  float memory_left = Machine_GetInfo(vm_info.machine_id).memory_size - Machine_GetInfo(vm_info.machine_id).memory_used;
			  if (memory_left >= (int)task.required_memory) {
				  if (RequiredVMType(task_id) == AIX && (Machine_GetInfo(vm_info.machine_id).cpu == POWER)) {
						return vm_id;
				  } else if (RequiredVMType(task_id) == WIN && ((Machine_GetInfo(vm_info.machine_id).cpu == ARM) || (Machine_GetInfo(vm_info.machine_id).cpu == X86))) {
						return vm_id;
				  } else if (RequiredVMType(task_id) == AIX || RequiredVMType(task_id) == WIN || RequiredCPUType(task_id) != Machine_GetInfo(vm_info.machine_id).cpu){
						//cout << "wrong type" << endl;
						continue; // machine doesn't have correct cpu type for either win or aix
				  } else {
						return vm_id;
				  }

		 }
	}
}
  // cout << "creating new vm" << endl;
   VMId_t created_vm = VM_Create(RequiredVMType(task_id),RequiredCPUType(task_id));
   int result = decideMachine(created_vm, IsTaskGPUCapable(task_id));
   if (result == -1) {
       // ERROR no machines left
       return -1;
   }
   vms.push_back(created_vm);
   sort(vms.begin(), vms.end(), VMComparator());
   return created_vm;
}


int Scheduler::decideMachine(VMId_t created_vm, bool gpu_required) {
   for (unsigned i = 0; i < machines.size(); i++) {
       //cout << "getting cpu type" << endl;
       if (VM_GetInfo(created_vm).cpu == Machine_GetCPUType(machines[i]) ) {
        // to do need to take into account gpu + maybe iterate through all machines which is sorted??
        cout << "vm attach 3" << endl;
        if (Machine_GetInfo(machines[i]).s_state == S0) {
            cout << "going to attach" << endl;
            VM_Attach(created_vm, machines[i]);
            (machine_to_vm.at(machines[i])).push_back(created_vm);
            return i;
        } else {
            return -1;
        }
       }
   }
   // need to turn on new machine if none found
   //cout << "need to turn on new machine" << endl;
   for (unsigned i = 0; i < all_machines.size(); i++) {
		MachineId_t temp = all_machines[i];
		MachineInfo_t current_machine_info = Machine_GetInfo(temp);
    if (current_machine_info.s_state != S0 && (VM_GetInfo(created_vm).cpu == Machine_GetCPUType(all_machines[i])) ) {
            cout << "found possible machine" << endl;
            Machine_SetState(temp, S0);
            machines.push_back(temp);
            sort(machines.begin(), machines.end(), UtilizationComparator());
             if (current_machine_info.s_state == S0) {
                VM_Attach(created_vm, temp);
                (machine_to_vm.at(temp)).push_back(created_vm);
                return i;
            } 
        } 
    }
    //cout << "couldn't make new machine" << endl;
    return -1;
}

void Scheduler::PeriodicCheck(Time_t now) {
   // This method should be called from SchedulerCheck()
   // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
   // Unlike the other invocations of the scheduler, this one doesn't report any specific event
   // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary

   //going through task queue and assigning tasks
   //cout << "inside periodic check" << endl;
   while (!task_queue.empty()) {
    TaskId_t temp = task_queue.front();
    task_queue.erase(task_queue.begin());
   // cout << "Scheduled task from task queue" << endl;
    Scheduler::NewTask(now, temp);
    }

   for (unsigned i = 0; i < vms.size(); i++) {
        if(VM_GetInfo(vms[i]).active_tasks.size() == 0) {
            if (vms.size() > 3) {
                cout << "turning off vm" << endl;
                VMId_t temp = vms[i];
                vms.erase(vms.begin() + i);
                VM_Shutdown(temp);
                i--;
            }
        }
	}
    for(unsigned i = 0; i < machines.size(); i++) {
        if (Machine_GetInfo(machines[i]).active_vms == 0 && machines.size() > 3) {
            cout << "turning off machine" << endl;
            MachineId_t temp = machines[i];
            machines.erase(machines.begin() + i);
            Machine_SetState(temp, S5);
            i--;
        }
    }
}

void Scheduler::SLAHandle(TaskId_t task_id) {
   
}

void Scheduler::MemoryHandle(MachineId_t machine_id) {
    
}


void Scheduler::Shutdown(Time_t time) {
   // Do your final reporting and bookkeeping here.
   // Report about the total energy consumed
   // Report about the SLA compliance
   // Shutdown everything to be tidy :-)
   for(auto & vm: vms) {
       // VMInfo_t shutdown_vm = VM_GetInfo(vm);
       // cout << shutdown_vm.active_tasks.size() << endl;
       // auto temp = vm_to_machine.find((VMId_t)vm);
       // cout << Machine_GetInfo(temp->second).active_tasks << " numtasks" << endl;
       // cout << Machine_GetInfo(temp->second).active_vms << " numvms" << endl;
       // cout << Machine_GetInfo(temp->second).p_state << " pSTATE" << endl;
       // cout << Machine_GetInfo(temp->second).s_state << " sSTATE" << endl;
       // cout << endl;
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
   numTasks--;   

   for (unsigned i = 0; i < machines.size() / 2; i++) {
    if (machine_to_vm.find(machines[i]) == machine_to_vm.end()) continue;

    vector<VMId_t>& current_vms = machine_to_vm[machines[i]];
    vector<VMId_t> temp_vms = current_vms; 
    
    bool found = false;
    int index_machine = -1;
    for (unsigned j = machines.size() - 1; j >= 0 && !found; j--) {
        if (Machine_GetCPUType(machines[j]) == Machine_GetCPUType(machines[i]) ) {
            found = true;
            index_machine = j;
        }
    }

    if (index_machine == -1) {
        cout << "no machine found error" << endl;
    }

    for (unsigned j = 0; j < temp_vms.size(); j++) {
        VMId_t temp = temp_vms[j];
        auto it = find(vms.begin(), vms.end(), temp);
        if(it != vms.end()) {
        
        current_vms.erase(remove(current_vms.begin(), current_vms.end(), temp), current_vms.end());
        cout << "migrating happening on vm " << temp << endl;
        cout << VM_GetInfo(temp).cpu << " " << Machine_GetCPUType(machines[index_machine]) << endl;
        cout << "state of machine migrating to: " << Machine_GetInfo(machines[index_machine]).s_state << endl;
        vms.erase(remove(vms.begin(), vms.end(), temp), vms.end());
        VM_Migrate(temp, machines[index_machine]);
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
   cout << "finished state change: " << machine_id << endl;
}