#include <inttypes.h>
#include <value_predictor.h>
#include <payload.h>
#include <pipeline.h>
#include <cassert>
#include <cmath>
//#include <pipeline.h>
//#define print printf
//#define print quiet
//#define IS_INTALU(flags) ((flags)&(F_ICOMP)) //integer ALU instruction
//#define IS_FPALU(flags) ((flags)&(F_FCOMP)) //floating-point ALU instruction
//void quiet (const char*,...){}
//public:
	////////////////////////////////////////
	// Public functions.
	////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// This is the constructor function.
	// When a value_predictor object is instantiated, the caller indicates:
	/////////////////////////////////////////////////////////////////////
	value_predictor::value_predictor(){
//		print(">>>>>>>>entering value_predictor constructor \n");
	//	RMT.resize(n_log_regs);
		vpq.vpq_data.resize(VPQsize);
		vpq.head=vpq.tail=0;	
		VPQsize=300;
	   index_bits=10;
		tag_bits=10;
		confmax=1;
		confinc=1;
		replace_stride=3;
		replace=1;
		predINTALU=1;
		predFPALU=1;
		predLOAD=1;
		VPQ_full_policy=1;
		uint64_t size =pow(2,index_bits);
		val_predictor.resize(size);

//		print("<<<<<<<<<< exiting is value_predictor constructor \n");
	}

	void value_predictor::index_calc(uint64_t &pc){
		uint64_t index=index_bits;
		index=1<<index;
		index=index-1;
		pc=pc&index;
	}
	
	void value_predictor::squash(){
		int ip_tag;
		int pc_index;
		while(!(vpq.tail==vpq.head &&vpq.t_phase==vpq.h_phase)){
			ip_tag=vpq.vpq_data[vpq.head].tag;
			pc_index=vpq.vpq_data[vpq.head].index;
			if(val_predictor[pc_index].tag==ip_tag){
				val_predictor[pc_index].instance--;
			}
			if(vpq.head==(VPQsize-1)){
				vpq.head=0;
				vpq.h_phase=!vpq.h_phase;
			}
			else{
				vpq.head++;
			}
		}
	}
	
	void value_predictor::rollback(uint64_t tail_ptr_chkpt, uint64_t t_phase){
		while(!(vpq.tail==tail_ptr_chkpt &&vpq.t_phase==t_phase)){
			if(vpq.tail==0){
				vpq.tail=VPQsize-1;
				vpq.t_phase=!vpq.t_phase;
			}
			else vpq.tail--;
		
	//		vpq.vpq_data[vpq.tail].
			int pc_index=vpq.vpq_data[vpq.tail].index;
			int pc_tag=vpq.vpq_data[vpq.tail].tag;
			//assert(val_predictor[pc_index].tag==pc_tag);
			if(val_predictor[pc_index].tag==pc_tag){
				val_predictor[pc_index].instance--;
			}
		}
	}

	void value_predictor::vpq_update(uint64_t pc, uint64_t vpq_tail_ptr,int64_t value){
		pc=pc>>2;
		uint64_t pc_index=pc;
		uint64_t pc_tag=pc>>index_bits;
		index_calc(pc_index);
		assert(vpq.vpq_data[vpq_tail_ptr].index==pc_index);
		assert(vpq.vpq_data[vpq_tail_ptr].tag==pc_tag);
		vpq.vpq_data[vpq_tail_ptr].value=value;
//		vpq.vpq_data[vpq_tail_ptr].tag=pc>>index_bits;
//		vpq.vpq_data[vpq_tail_ptr].index=(pc&&index);
	}

	bool value_predictor::check_prediction(uint64_t actual_val, uint64_t predicted_val, bool confidence){
		if(actual_val==predicted_val){
			if(confidence){
				vpmeas_conf_corr++;       // VPU generated a confident and correct value prediction.
			}
			else{
				vpmeas_unconf_corr++;     // VPU generated an unconfident and correct value prediction. (LOST OPPORTUNITY)
			}
			return true;
		}
		else{
			if (confidence){
				vpmeas_conf_incorr++;     // VPU generated a confident and incorrect value prediction. (MISPREDICTION)
			}
			else{
				vpmeas_unconf_incorr++; 				
			}
			return false;
		}
	}

	bool value_predictor::train_or_rep(uint64_t pc){
		int ip_tag;
		int new_stride;
		uint64_t vpq_val;
		pc=pc>>2;
		ip_tag=pc>>index_bits;
		uint64_t pc_index=pc;
		index_calc(pc_index);
	//to do get val from vpq
//		assert(vpq.vpq_data[vpq.head].tag==ip_tag);
//		assert(vpq.vpq_data[vpq.head].index==pc_index);
		new_stride=vpq.vpq_data[vpq.head].value-val_predictor[pc_index].retired_value;

//SVP hit
		if(ip_tag==val_predictor[pc_index].tag){
			if(val_predictor[pc_index].stride==new_stride){
				val_predictor[pc_index].conf+=confinc;
				if(val_predictor[pc_index].conf>confmax)
					val_predictor[pc_index].conf=confmax;
			}
			else{
				if(val_predictor[pc_index].conf<=replace_stride){
						val_predictor[pc_index].stride=new_stride;
					}
					val_predictor[pc_index].conf=0;
			}
			val_predictor[pc_index].retired_value=vpq.vpq_data[vpq.head].value;
			val_predictor[pc_index].instance-=1;
		}

//SVP miss
		else if(val_predictor[pc_index].conf<=replace){
			val_predictor[pc_index].tag=ip_tag;
			val_predictor[pc_index].conf=0;
			val_predictor[pc_index].retired_value=val_predictor[pc_index].stride=vpq.vpq_data[vpq.head].value;
//Todo
//Instance counter fix;
			int instance=0;
			int head=vpq.head;
			int tail=vpq.tail;
			int h_phase=vpq.h_phase;
			int t_phase=vpq.t_phase;
			if(head==(VPQsize-1)){
				head=0;
				h_phase=!h_phase;
			}
			else{
				head++;
			}
			while(!((head==tail) && (h_phase==t_phase))){
				if(vpq.vpq_data[head].tag=ip_tag){
					instance++;
				}
				if(head==(VPQsize-1)){
					head=0;
					h_phase=!h_phase;
				}
				else{
					head++;
				}
			}
			val_predictor[pc_index].instance=instance;
		}
	}

	void value_predictor::checkpoint(uint64_t & chkpt_vpq_tail, bool& chkpt_vpq_t_phase){
		chkpt_vpq_tail = vpq.tail;
		chkpt_vpq_t_phase = vpq.t_phase;
	}

	void value_predictor::restore(uint64_t vpq_tail, bool vpq_t_phase){
		vpq.tail=vpq_tail;
		vpq.t_phase=vpq_t_phase;
	}
	
	bool value_predictor::predict(uint64_t pc, uint64_t &prediction, bool &confidence, bool pred_flag, bool &stall,bool &vpq_entry_flag, uint64_t &vpq_tail_ptr){
		pc=pc>>2;
	//	uint64_t	prediction;
		uint64_t hit;
		uint64_t ip_tag;
	   
		ip_tag=pc>>index_bits;
		uint64_t pc_index=pc;
		index_calc(pc_index);
		if(tag_bits>0){
			hit=(ip_tag==val_predictor[pc].tag);
		}
		else{
			hit=1;
		}
		if(!(vpq.head==vpq.tail && vpq.h_phase!=vpq.t_phase))
		{
			vpq_entry_flag=1;
			stall=0;
			vpq_tail_ptr=vpq.tail;
			vpq.vpq_data[vpq.tail].tag=ip_tag;
			vpq.vpq_data[vpq.tail].index=pc_index;
			
			if(hit==1){
				pred_flag=1;
				val_predictor[pc_index].instance++;
				prediction=val_predictor[pc_index].retired_value+val_predictor[pc_index].instance*val_predictor[pc_index].stride;
			}
			else if (hit==0){
				vpmeas_miss++;
				pred_flag=0;
			}
//VPQ Update
			if(vpq.tail==(VPQsize-1)){
				vpq.tail=0;
				vpq.t_phase=!vpq.t_phase;
				//	return true;
			}
			else{
				vpq.tail++;
				//	return true;
			}
		}
		else{ 
			if(VPQ_full_policy==1){
				vpq_entry_flag=0;
				stall=0;
				vpmeas_ineligible_drop++;
		//		return false;
			}
			else if(VPQ_full_policy==0){
				vpq_entry_flag=0;
				stall=1;
		//		return false;
			}
		}
	}
bool value_predictor::eligible_inst(payload_t *pay, uint64_t index){
		if(IS_INTALU(pay[index].flags))
			return(predINTALU);
		else if(IS_FPALU(pay[index].flags))
			return(predFPALU);
		else if (IS_LOAD(pay[index].flags)&& !IS_AMO(pay[index].flags))
			return (predLOAD);
		else
			return (false);
	}
unsigned int payload::get_size() {
   return(PAYLOAD_BUFFER_SIZE);
}

