#include <inttypes.h>
#include <renamer.h>
#include <cassert>
#define print printf
#define print quiet
void quiet (const char*,...){}
//public:
	////////////////////////////////////////
	// Public functions.
	////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// This is the constructor function.
	// When a renamer object is instantiated, the caller indicates:
	// 1. The number of logical registers (e.g., 32).
	// 2. The number of physical registers (e.g., 128).
	// 3. The maximum number of unresolved branches.
	//    Requirement: 1 <= n_branches <= 64.
	// 4. The maximum number of active instructions (Active List size).
	//
	// Tips:
	//
	// Assert the number of physical registers > number logical registers.
	// Assert 1 <= n_branches <= 64.
	// Assert n_active > 0.
	// Then, allocate space for the primary data structures.
	// Then, initialize the data structures based on the knowledge
	// that the pipeline is intially empty (no in-flight instructions yet).
	/////////////////////////////////////////////////////////////////////
	renamer::renamer(uint64_t n_log_regs,
		uint64_t n_phys_regs,
		uint64_t n_branches,
		uint64_t n_active){
		print(">>>>>>>>entering renamer constructor \n");
		RMT.resize(n_log_regs);
		AMT.resize(n_log_regs);
		PRF.resize(n_phys_regs);
		prf_rdy_bit.resize(n_phys_regs);
		assert(n_active>0);

		active_list.head=0;
		active_list.tail=0;
		active_list.h_phase=0;
		active_list.t_phase=0;
		active_list.active_list_vec.resize(n_active);

		free_list.head=0;
		free_list.tail=0;
		free_list.h_phase=0;
		free_list.t_phase=1;
		print("############# free_list size- %0d ",n_phys_regs-n_log_regs);
		free_list.f_list.resize(n_phys_regs-n_log_regs);
		free_list.f_list_size=(n_phys_regs-n_log_regs);
		for(int i=0;i<free_list.f_list_size;i++){
			free_list.f_list[i]=n_log_regs +i;
		}
		for(int i=0;i<n_log_regs;i++){
			RMT[i]=i;
		}
		for(int i=0;i<n_log_regs;i++){
			AMT[i]=i;
		}
		for(int i=0;i<n_phys_regs;i++){
			PRF[i]=0;
			if(i<n_log_regs)
				prf_rdy_bit[i]=1;
			else 
				prf_rdy_bit[i]=0;
			
		}
		//GBM=0xFFFFFFFFFFFFFFFF;
		GBM=0;
		print("value of n_branches %0d \n",n_branches);
		br_chkpt.resize(n_branches);
		print("<<<<<<<<<< exiting is renamer constructor \n");
	}

	/////////////////////////////////////////////////////////////////////
	// This is the destructor, used to clean up memory space and
	// other things when simulation is done.
	// I typically don't use a destructor; you have the option to keep
	// this function empty.
	/////////////////////////////////////////////////////////////////////
	//~rename::renamer();


	//////////////////////////////////////////
	// Functions related to Rename Stage.   //
	//////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// The Rename Stage must stall if there aren't enough free physical
	// registers available for renaming all logical destination registers
	// in the current rename bundle.
	//
	// Inputs:
	// 1. bundle_dst: number of logical destination registers in
	//    current rename bundle
	//
	// Return value:
	// Return "true" (stall) if there aren't enough free physical
	// registers to allocate to all of the logical destination registers
	// in the current rename bundle.
	/////////////////////////////////////////////////////////////////////
	bool renamer::stall_reg(uint64_t bundle_dst){
	print(">>>>>>>>entering stall reg \n");
	print("stall reg function call, free list head- %d, free_list tail %d, free_list h_phase %0d , tphase %0d, bundle_dst-%0d \n",free_list.head, free_list.tail,free_list.h_phase,free_list.t_phase, bundle_dst);
		if(free_list.h_phase==free_list.t_phase){
			if(bundle_dst>(free_list.tail-free_list.head)){
		//	if(bundle_dst>(free_list.f_list.size()-(free_list.tail-free_list.head))){
			//if(bundle_dst>(free_list.f_list_size-free_list.f_list.size()))
				return true;
			}
			else 
				return false;
		}
		else{
			//if(bundle_dst>(free_list.head-free_list.tail)){
			if(bundle_dst>(free_list.f_list.size()-(free_list.head-free_list.tail))){
			//if(bundle_dst>(free_list.f_list_size-free_list.f_list.size()))
				return true;
			}
			else 
				return false;
		}
	}
	/////////////////////////////////////////////////////////////////////
	// The Rename Stage must stall if there aren't enough free
	// checkpoints for all branches in the current rename bundle.
	//
	// Inputs:
	// 1. bundle_branch: number of branches in current rename bundle
	//
	// Return value:
	// Return "true" (stall) if there aren't enough free checkpoints
	// for all branches in the current rename bundle.
	/////////////////////////////////////////////////////////////////////
	bool renamer::stall_branch(uint64_t bundle_branch){
	//print(">>>>>>>>entering stall branch \n");
		ct++;
		uint64_t gbm_c;
		int count=0;
		gbm_c=GBM;
		for(int i=0;i<br_chkpt.size();i++){
			if((gbm_c%2)==0){
				count++;
			}
			gbm_c=gbm_c>>1;
		}
		if(count<bundle_branch){
			if(ct<250000==0){
			//print(">>>>>>>>>>>exiting with true stall branch, GBM=%0x, bundle=%0d\n", GBM, bundle_branch);
			}
			return true;
		}
		else{
	//		if(ct%250000==0){
				print(">>>>>>>>>>>exiting with false stall branch\n");
	//		}
			return false;
		}
	}

	/////////////////////////////////////////////////////////////////////
	// This function is used to get the branch mask for an instruction.
	/////////////////////////////////////////////////////////////////////
	uint64_t renamer::get_branch_mask(){
	print(">>>>>>>>entering get_branch_mask \n");
		return GBM;
	}

	/////////////////////////////////////////////////////////////////////
	// This function is used to rename a single source register.
	//
	// Inputs:
	// 1. log_reg: the logical register to rename
	//
	// Return value: physical register name
	/////////////////////////////////////////////////////////////////////
	uint64_t renamer::rename_rsrc(uint64_t log_reg){
	print(">>>>>>>>entering rename_rsrc \n");
		return RMT[log_reg];
	}

	/////////////////////////////////////////////////////////////////////
	// This function is used to rename a single destination register.
	//
	// Inputs:
	// 1. log_reg: the logical register to rename
	//
	// Return value: physical register name
	/////////////////////////////////////////////////////////////////////
	uint64_t renamer::rename_rdst(uint64_t log_reg){
	print(">>>>>>>>entering rename_RDST \n");
		uint64_t val;
		val=free_list.f_list[free_list.head];
		RMT[log_reg]=val;
		if(free_list.head==(free_list.f_list.size()-1)){
			free_list.head=0;
			free_list.h_phase=!free_list.h_phase;
		}
		else
			free_list.head++;
		//prf_rdy_bit[val]=0;

		return val;
	}

	/////////////////////////////////////////////////////////////////////
	// This function creates a new branch checkpoint.
	//
	// Inputs: none.
	//
	// Output:
	// 1. The function returns the branch's ID. When the branch resolves,
	//    its ID is passed back to the renamer via "resolve()" below.
	//
	// Tips:
	//
	// Allocating resources for the branch (a GBM bit and a checkpoint):
	// * Find a free bit -- i.e., a '0' bit -- in the GBM. Assert that
	//   a free bit exists: it is the user's responsibility to avoid
	//   a structural hazard by calling stall_branch() in advance.
	// * Set the bit to '1' since it is now in use by the new branch.
	// * The position of this bit in the GBM is the branch's ID.
	// * Use the branch checkpoint that corresponds to this bit.
	// 
	// The branch checkpoint should contain the following:
	// 1. Shadow Map Table (checkpointed Rename Map Table)
	// 2. checkpointed Free List head pointer and its phase bit
	// 3. checkpointed GBM
	/////////////////////////////////////////////////////////////////////
	uint64_t renamer::checkpoint(){
		print(">>>>>>>>entering checkpoint \n");
		print("GBM before checkpointing %0x\n",GBM);
		uint64_t temp=GBM;
		for(uint64_t i=0;i<br_chkpt.size();i++){
			if(temp%2==0){
				GBM|=1ULL<<i;
	//			GBM=GBM|temp;	
				br_chkpt[i].Checkpointed_GBM=GBM;
				br_chkpt[i].Checkpointed_RMT=RMT;
				br_chkpt[i].Checkpointed_f_list.head=free_list.head;
				br_chkpt[i].Checkpointed_f_list.h_phase=free_list.h_phase;
//				br_chkpt[i].Checkpointed_f_list.f_list=free_list.f_list;

				print("GBM after checkpoint and allocating resources %0x \n",GBM);
				return i;
			}
			else{
				temp=temp>>1;
			}
		}
	}

	//////////////////////////////////////////
	// Functions related to Dispatch Stage. //
	//////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// The Dispatch Stage must stall if there are not enough free
	// entries in the Active List for all instructions in the current
	// dispatch bundle.
	//
	// Inputs:
	// 1. bundle_inst: number of instructions in current dispatch bundle
	//
	// Return value:
	// Return "true" (stall) if the Active List does not have enough
	// space for all instructions in the dispatch bundle.
	/////////////////////////////////////////////////////////////////////
	bool renamer::stall_dispatch(uint64_t bundle_inst){
	print(">>>>>>>>entering stall dispatch \n");
// To do fix this condition
		print("active_list size %d, head %d, tphase-%d, h-phase%d, tail %d bundle %d \n",active_list.active_list_vec.size(),active_list.head,active_list.h_phase,active_list.t_phase,active_list.tail, bundle_inst);
		if(active_list.t_phase==active_list.h_phase){
			if((active_list.active_list_vec.size()-(active_list.tail-active_list.head)) < bundle_inst)
				return true;
			else
				return false;
		}
		else{
			if((active_list.head-active_list.tail) < bundle_inst)
				return true;
			else
				return false;
		}
	}
	/////////////////////////////////////////////////////////////////////
	// This function dispatches a single instruction into the Active
	// List.
	//
	// Inputs:
	// 1. dest_valid: If 'true', the instr. has a destination register,
	//    otherwise it does not. If it does not, then the log_reg and
	//    phys_reg inputs should be ignored.
	// 2. log_reg: Logical register number of the instruction's
	//    destination.
	// 3. phys_reg: Physical register number of the instruction's
	//    destination.
	// 4. load: If 'true', the instr. is a load, otherwise it isn't.
	// 5. store: If 'true', the instr. is a store, otherwise it isn't.
	// 6. branch: If 'true', the instr. is a branch, otherwise it isn't.
	// 7. amo: If 'true', this is an atomic memory operation.
	// 8. csr: If 'true', this is a system instruction.
	// 9. PC: Program counter of the instruction.
	//
	// Return value:
	// Return the instruction's index in the Active List.
	//
	// Tips:
	//
	// Before dispatching the instruction into the Active List, assert
	// that the Active List isn't full: it is the user's responsibility
	// to avoid a structural hazard by calling stall_dispatch()
	// in advance.
	/////////////////////////////////////////////////////////////////////
	uint64_t renamer::dispatch_inst(bool dest_valid,
	                       uint64_t log_reg,
	                       uint64_t phys_reg,
	                       bool load,
	                       bool store,
	                       bool branch,
	                       bool amo,
	                       bool csr,
	                       uint64_t PC){
		print(">>>>>>>>entering dispatch_inst \n");
		assert(!(active_list.tail==active_list.head && active_list.t_phase!=active_list.h_phase));
		uint64_t instr_index=active_list.tail;
		active_list.active_list_vec[active_list.tail].d_flag = dest_valid;
		if(dest_valid==true){
			active_list.active_list_vec[active_list.tail].l_reg_num=log_reg;
			active_list.active_list_vec[active_list.tail].p_reg_num=phys_reg;
		}
		active_list.active_list_vec[active_list.tail].cp_bit=false;
		active_list.active_list_vec[active_list.tail].ex_bit=false;
		active_list.active_list_vec[active_list.tail].ld_viol_bit=false;
		active_list.active_list_vec[active_list.tail].br_mis_bit=false;
		active_list.active_list_vec[active_list.tail].val_mis_bit=false;

		active_list.active_list_vec[active_list.tail].ld_flag=load;
		active_list.active_list_vec[active_list.tail].str_flag=store;
		active_list.active_list_vec[active_list.tail].br_flag=branch;
		active_list.active_list_vec[active_list.tail].amo_flag=amo;
		active_list.active_list_vec[active_list.tail].csr_flag=csr;
		active_list.active_list_vec[active_list.tail].pc=PC;
		if(active_list.tail==(active_list.active_list_vec.size()-1)){
			active_list.tail=0;
			active_list.t_phase=!active_list.t_phase;
		}
		else
			active_list.tail++;
		
		print("<<<<<<<<<exiting dispatch_inst \n");
		return instr_index;
	}

	/////////////////////////////////////////////////////////////////////
	// Test the ready bit of the indicated physical register.
	// Returns 'true' if ready.
	/////////////////////////////////////////////////////////////////////
	bool renamer::is_ready(uint64_t phys_reg){
		print(">>>>>>>>entering is_ready \n");
		if(prf_rdy_bit[phys_reg])
			return true;
		else
			return false;
	}

	/////////////////////////////////////////////////////////////////////
	// Clear the ready bit of the indicated physical register.
	/////////////////////////////////////////////////////////////////////
	void renamer::clear_ready(uint64_t phys_reg){
		print(">>>>>>>>entering clear_ready \n");
		prf_rdy_bit[phys_reg]=0;
		print("<<<<<<<<exiting clear_ready \n");
	}
	//////////////////////////////////////////
	// Functions related to the Reg. Read   //
	// and Execute Stages.                  //
	//////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// Return the contents (value) of the indicated physical register.
	/////////////////////////////////////////////////////////////////////
	uint64_t renamer::read(uint64_t phys_reg){
		print(">>>>>>>>entering read \n");
		return PRF[phys_reg];
	}

	/////////////////////////////////////////////////////////////////////
	// Set the ready bit of the indicated physical register.
	/////////////////////////////////////////////////////////////////////
	void renamer::set_ready(uint64_t phys_reg){
		print(">>>>>>>>entering set ready \n");
		prf_rdy_bit[phys_reg]=1;
	}
	//////////////////////////////////////////
	// Functions related to Writeback Stage.//
	//////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// Write a value into the indicated physical register.
	/////////////////////////////////////////////////////////////////////
	void renamer::write(uint64_t phys_reg, uint64_t value){
		PRF[phys_reg]=value;
	}
	/////////////////////////////////////////////////////////////////////
	// Set the completed bit of the indicated entry in the Active List.
	/////////////////////////////////////////////////////////////////////
	void renamer::set_complete(uint64_t AL_index){
		active_list.active_list_vec[AL_index].cp_bit=1;
	}

	/////////////////////////////////////////////////////////////////////
	// This function is for handling branch resolution.
	//
	// Inputs:
	// 1. AL_index: Index of the branch in the Active List.
	// 2. branch_ID: This uniquely identifies the branch and the
	//    checkpoint in question.  It was originally provided
	//    by the checkpoint function.
	// 3. correct: 'true' indicates the branch was correctly
	//    predicted, 'false' indicates it was mispredicted
	//    and recovery is required.
	//
	// Outputs: none.
	//
	// Tips:
	//
	// While recovery is not needed in the case of a correct branch,
	// some actions are still required with respect to the GBM and
	// all checkpointed GBMs:
	// * Remember to clear the branch's bit in the GBM.
	// * Remember to clear the branch's bit in all checkpointed GBMs.
	//
	// In the case of a misprediction:
	// * Restore the GBM from the branch's checkpoint. Also make sure the
	//   mispredicted branch's bit is cleared in the restored GBM,
	//   since it is now resolved and its bit and checkpoint are freed.
	// * You don't have to worry about explicitly freeing the GBM bits
	//   and checkpoints of branches that are after the mispredicted
	//   branch in program order. The mere act of restoring the GBM
	//   from the checkpoint achieves this feat.
	// * Restore the RMT using the branch's checkpoint.
	// * Restore the Free List head pointer and its phase bit,
	//   using the branch's checkpoint.
	// * Restore the Active List tail pointer and its phase bit
	//   corresponding to the entry after the branch's entry.
	//   Hints:
	//   You can infer the restored tail pointer from the branch's
	//   AL_index. You can infer the restored phase bit, using
	//   the phase bit of the Active List head pointer, where
	//   the restored Active List tail pointer is with respect to
	//   the Active List head pointer, and the knowledge that the
	//   Active List can't be empty at this moment (because the
	//   mispredicted branch is still in the Active List).
	// * Do NOT set the branch misprediction bit in the Active List.
	//   (Doing so would cause a second, full squash when the branch
	//   reaches the head of the Active List. We don’t want or need
	//   that because we immediately recover within this function.)
	/////////////////////////////////////////////////////////////////////
	void renamer::resolve(uint64_t AL_index,
		     uint64_t branch_ID,
		     bool correct){
		print(">>>>>>>>entering resolve, GBM=%0x \n",GBM);
		if(correct==true){
		print("correctly predicted branch");
			GBM=GBM&~(1ULL<<branch_ID);
			for(int i=0;i<br_chkpt.size();i++){
				br_chkpt[i].Checkpointed_GBM&=~(1ULL<<branch_ID);
			}
		
		}
		else{
		print("mispredicted branch  %0d ",br_chkpt.size());
			GBM=br_chkpt[branch_ID].Checkpointed_GBM&(~(1ULL<<branch_ID));
			RMT=br_chkpt[branch_ID].Checkpointed_RMT;
			free_list.head=br_chkpt[branch_ID].Checkpointed_f_list.head;
	//		free_list.head=br_chkpt[branch_ID].checkpointed_f_list.head;
			free_list.h_phase=br_chkpt[branch_ID].Checkpointed_f_list.h_phase;
			if(((AL_index+1)% active_list.active_list_vec.size())> active_list.tail) {
				active_list.t_phase=!active_list.t_phase;
			}
			active_list.tail=(AL_index+1)%(active_list.active_list_vec.size());
//			if (active_list.head<active_list.tail) active_list.t_phase=active_list.h_phase;
//			if (active_list.head>active_list.tail) active_list.t_phase=!active_list.h_phase;
//			if (active_list.head==active_list.tail) active_list.t_phase=!active_list.h_phase;
		}			
		//printf("<<<<<<<<<<exiting resolve GBM=%0x\n",GBM);
	}

	//////////////////////////////////////////
	// Functions related to Retire Stage.   //
	//////////////////////////////////////////

	///////////////////////////////////////////////////////////////////
	// This function allows the caller to examine the instruction at the head
	// of the Active List.
	//
	// Input arguments: none.
	//
	// Return value:
	// * Return "true" if the Active List is NOT empty, i.e., there
	//   is an instruction at the head of the Active List.
	// * Return "false" if the Active List is empty, i.e., there is
	//   no instruction at the head of the Active List.
	//
	// Output arguments:
	// Simply return the following contents of the head entry of
	// the Active List.  These are don't-cares if the Active List
	// is empty (you may either return the contents of the head
	// entry anyway, or not set these at all).
	// * completed bit
	// * exception bit
	// * load violation bit
	// * branch misprediction bit
	// * value misprediction bit
	// * load flag (indicates whether or not the instr. is a load)
	// * store flag (indicates whether or not the instr. is a store)
	// * branch flag (indicates whether or not the instr. is a branch)
	// * amo flag (whether or not instr. is an atomic memory operation)
	// * csr flag (whether or not instr. is a system instruction)
	// * program counter of the instruction
	/////////////////////////////////////////////////////////////////////
	bool renamer::precommit(bool &completed,
                       bool &exception, bool &load_viol, bool &br_misp, bool &val_misp,
	               bool &load, bool &store, bool &branch, bool &amo, bool &csr,
		       uint64_t &PC){
		//if(ct%250000==0)
		//print(">>>>>>>>entering precommit \n");
		if((active_list.head==active_list.tail) && (active_list.h_phase==active_list.t_phase)){
		print(" active list is empty \n");
			return false;
		}
		else {
			completed = active_list.active_list_vec[active_list.head].cp_bit;
         exception = active_list.active_list_vec[active_list.head].ex_bit;
			load_viol = active_list.active_list_vec[active_list.head].ld_viol_bit;
			br_misp = active_list.active_list_vec[active_list.head].br_mis_bit;
			val_misp = active_list.active_list_vec[active_list.head].val_mis_bit;
			load = active_list.active_list_vec[active_list.head].ld_flag;
			store = active_list.active_list_vec[active_list.head].str_flag;
			branch = active_list.active_list_vec[active_list.head].br_flag;
			amo = active_list.active_list_vec[active_list.head].amo_flag;
			csr = active_list.active_list_vec[active_list.head].csr_flag;
			PC = active_list.active_list_vec[active_list.head].pc;
	//	if(ct<0)//==0)
		print("<<<<<<<<<<exiting precommiting		true Load =%0d, store =0%d, branch=%0d, amo=%0d, csr=%0d br_misp=%0d val_misp=%0d cp-%0d\n",load, store, branch, amo, csr,br_misp,val_misp,completed);
			return true;
		}
	}

	/////////////////////////////////////////////////////////////////////
	// This function commits the instruction at the head of the Active List.
	//
	// Tip (optional but helps catch bugs):
	// Before committing the head instruction, assert that it is valid to
	// do so (use assert() from standard library). Specifically, assert
	// that all of the following are true:
	// - there is a head instruction (the active list isn't empty)
	// - the head instruction is completed
	// - the head instruction is not marked as an exception
	// - the head instruction is not marked as a load violation
	// It is the caller's (pipeline's) duty to ensure that it is valid
	// to commit the head instruction BEFORE calling this function
	// (by examining the flags returned by "precommit()" above).
	// This is why you should assert() that it is valid to commit the
	// head instruction and otherwise cause the simulator to exit.
	/////////////////////////////////////////////////////////////////////
	void renamer::commit(){
		print(">>>>>>>>> Entering commit stage\n");
		assert(!((active_list.head==active_list.tail) && (active_list.h_phase==active_list.t_phase)));
		assert(active_list.active_list_vec[active_list.head].cp_bit==1);
		assert(active_list.active_list_vec[active_list.head].ex_bit==0);
		assert(active_list.active_list_vec[active_list.head].ld_viol_bit==0);
		//fix this, active list will update irrespective of d_flag
		if(active_list.active_list_vec[active_list.head].d_flag==true){
			free_list.f_list[free_list.tail]=AMT[active_list.active_list_vec[active_list.head].l_reg_num];
			if(free_list.tail==(free_list.f_list.size()-1)){
				free_list.tail=0;
				free_list.t_phase=!free_list.t_phase;
			}
			else
				free_list.tail++;
			AMT[active_list.active_list_vec[active_list.head].l_reg_num]=active_list.active_list_vec[active_list.head].p_reg_num;
		}
			if(active_list.head==(active_list.active_list_vec.size()-1)){
				active_list.head=0;
				active_list.h_phase=!active_list.h_phase;
			}
			else active_list.head++;
		
		print("<<<<<<<<< Exiting commit stage \n");

	}

	//////////////////////////////////////////////////////////////////////
	// Squash the renamer class.
	//
	// Squash all instructions in the Active List and think about which
	// sructures in your renamer class need to be restored, and how.
	//
	// After this function is called, the renamer should be rolled-back
	// to the committed state of the machine and all renamer state
	// should be consistent with an empty pipeline.
	/////////////////////////////////////////////////////////////////////
	void renamer::squash(){
		print(">>>>>>>>> Entering SQUASH stage\n");
		active_list.tail=active_list.head;
		active_list.t_phase=active_list.h_phase=0;
		RMT=AMT;
		GBM=0;
		free_list.head=free_list.tail;
		free_list.t_phase=1;
		free_list.h_phase=!free_list.t_phase;
		print("<<<<<<<< Exiting SQUASH stage\n");
	}

	//////////////////////////////////////////
	// Functions not tied to specific stage.//
	//////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// Functions for individually setting the exception bit,
	// load violation bit, branch misprediction bit, and
	// value misprediction bit, of the indicated entry in the Active List.
	/////////////////////////////////////////////////////////////////////
	void renamer::set_exception(uint64_t AL_index){
		active_list.active_list_vec[AL_index].ex_bit=1;
	}
	void renamer::set_load_violation(uint64_t AL_index){
		active_list.active_list_vec[AL_index].ld_viol_bit=1;
	}
	void renamer::set_branch_misprediction(uint64_t AL_index){
		active_list.active_list_vec[AL_index].br_mis_bit=1;
	}
	void renamer::set_value_misprediction(uint64_t AL_index){
		active_list.active_list_vec[AL_index].val_mis_bit=1;
	}

	/////////////////////////////////////////////////////////////////////
	// Query the exception bit of the indicated entry in the Active List.
	/////////////////////////////////////////////////////////////////////
	bool renamer::get_exception(uint64_t AL_index){
		return active_list.active_list_vec[AL_index].ex_bit;
	}
