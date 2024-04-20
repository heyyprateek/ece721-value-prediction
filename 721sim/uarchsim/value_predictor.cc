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
		vpq.h_phase=vpq.t_phase=0;
		vpq.head=vpq.tail=0;	
		VPQsize=VPQ_SIZE;//300;
	   index_bits=SVP_INDEX_BITS;//10;
		tag_bits=SVP_TAG_BITS;//10;
		confmax=SVP_CONFMAX;//1;
		confinc=SVP_CONFINC;//1;
		replace_stride=SVP_REPLACE_STRIDE;//3;
		replace=SVP_REPLACE;//1;
		predINTALU=SVP_PREDINTALU;//1;
		predFPALU=SVP_PREDFPALU;//1;
		predLOAD=SVP_PREDLOAD;//1;
		VPQ_full_policy=VPQ_FULL_POLICY;//1;
		oracleconf=SVP_ORACLECONF;
		uint64_t size =pow(2,index_bits);
		val_predictor.resize(size);
		vpq.vpq_data.resize(VPQsize);

		//printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>0000>>>>>############vpq.tail value =%ld, vpq size=%ld, svp size=%ld\n",vpq.tail,vpq.vpq_data.size(), val_predictor.size());
//		print("<<<<<<<<<< exiting is value_predictor constructor \n");
	}

	void value_predictor::index_calc(uint64_t &pc){
		uint64_t index=index_bits;
		index=1<<index;
		index=index-1;
		pc=pc&index;
	}
	
	void value_predictor::squash(){
		uint64_t ip_tag;
		uint64_t pc_index;
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
			uint64_t pc_index=vpq.vpq_data[vpq.tail].index;
			uint64_t pc_tag=vpq.vpq_data[vpq.tail].tag;
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
		//printf(">vpq_update>>>>>>vpq_tail %ld, vpq index %ld actual index %ld, vpq_tag %ld actual tag %ld \n",vpq_tail_ptr,vpq.vpq_data[vpq_tail_ptr].index,pc_index,vpq.vpq_data[vpq_tail_ptr].tag,pc_tag);
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

	void value_predictor::train_or_rep(uint64_t pc){
		uint64_t ip_tag;
		int64_t new_stride;
		uint64_t vpq_val;
		pc=pc>>2;
		ip_tag=pc>>index_bits;
		uint64_t pc_index=pc;
		index_calc(pc_index);
		//printf(">>>>>from train_or_rep>>>>> vpq_head %ld \n",vpq.head,pc,ip_tag);
		assert(vpq.vpq_data[vpq.head].tag==ip_tag);
		assert(vpq.vpq_data[vpq.head].index==pc_index);
		new_stride=vpq.vpq_data[vpq.head].value-val_predictor[pc_index].retired_value;

//SVP hit
		if(ip_tag==val_predictor[pc_index].tag){
			if(val_predictor[pc_index].stride==new_stride){
				//printf("stride match for stride=%ld , pc_index=%ld, pc_tag%ld\n",new_stride,pc_index,ip_tag);
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
			//printf("SVP miss \n");
			val_predictor[pc_index].tag=ip_tag;
			val_predictor[pc_index].conf=0;
			val_predictor[pc_index].retired_value=val_predictor[pc_index].stride=vpq.vpq_data[vpq.head].value;
//Todo
//Instance counter fix;
			uint64_t instance=0;
			uint64_t head=vpq.head;
			uint64_t tail=vpq.tail;
			bool h_phase=vpq.h_phase;
			bool t_phase=vpq.t_phase;
			if(head==(VPQsize-1)){
				head=0;
				h_phase=!h_phase;
			}
			else{
				head++;
			}
			while(!((head==tail) && (h_phase==t_phase))){
				if(vpq.vpq_data[head].index==pc_index && vpq.vpq_data[head].tag==ip_tag){
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
		
		if(vpq.head==(VPQsize-1)){
			vpq.head=0;
			vpq.h_phase=!vpq.h_phase;
		}
		else{
			vpq.head++;
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
	
	void value_predictor::predict(uint64_t pc, uint64_t &prediction, bool &confidence, bool &pred_flag, bool &stall,bool &vpq_entry_flag, uint64_t &vpq_tail_ptr){
	//	printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>1111>>>>>############vpq.tail value =%ld \n",vpq.tail);
		pc=pc>>2;
	//	uint64_t	prediction;
		uint64_t hit;
		uint64_t ip_tag;
		pred_flag=0;
		stall=0;
		confidence=0;
		vpq_entry_flag=0;
	   
		ip_tag=pc>>index_bits;
		uint64_t pc_index=pc;
		index_calc(pc_index);
		
		//printf(">>>>>>>>>>>>>>>>>>>>>>before entry>>>>>>############vpq.tail value =%ld, ip_tag%lx pc_index%lx pc-%lx\n",vpq.tail, ip_tag,pc_index,pc<<2);
		if(tag_bits>0){
			hit=(ip_tag==val_predictor[pc_index].tag);
		}
		else{
			hit=1;
		}
		if(!(vpq.head==vpq.tail && vpq.h_phase!=vpq.t_phase))
		{
			//printf(">>>>>>>>>>>>>>>>>>>>>>>>>>22222>>>>>>############vpq.tail value =%ld, ip_tag%lx pc_index%lx \n",vpq.tail, ip_tag,pc_index);
			vpq_entry_flag=1;
			vpq_tail_ptr=vpq.tail;
			vpq.vpq_data[vpq.tail].tag=ip_tag;
			vpq.vpq_data[vpq.tail].index=pc_index;

//SVP hit or miss
//fix confidence
			if(hit==1){
				pred_flag=1;
				val_predictor[pc_index].instance++;
				prediction=val_predictor[pc_index].retired_value+val_predictor[pc_index].instance*val_predictor[pc_index].stride;
			}
			else if (hit==0){
			//	vpmeas_miss++;
				pred_flag=0;
			}
//VPQ Update
			if(vpq.tail==(VPQsize-1)){
				vpq.tail=0;
				vpq.t_phase=!vpq.t_phase;
			}
			else{
				vpq.tail++;
			}
		}
//VPQ full
		else{ 
			vpq_entry_flag=0;
			pred_flag=0;
			confidence=0;
			if(VPQ_full_policy==1){
				stall=0;
		//		vpmeas_ineligible_drop++;
			}
			else if(VPQ_full_policy==0){
				stall=1;
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
	void value_predictor::debug_print(){
		//printf("debug prints >>>>>>>>>>");
		printf("svp-  tag-   conf  retired_val  stride  instance  \n");
		for(int i=0;i<val_predictor.size();i++){
			printf("\t%10d\t%8lx\t%6ld\t%13ld\t%6ld\t%8ld \n",i,val_predictor[i].tag,val_predictor[i].conf,val_predictor[i].retired_value,val_predictor[i].stride,val_predictor[i].instance);
		}
		uint64_t head=vpq.head;
		uint64_t tail=vpq.tail;
		bool h_phase=vpq.h_phase;
		bool t_phase=vpq.t_phase;
		printf("vpq entry-, pc - pc-tag  pc-index \n");
		while(!((head==tail) && (h_phase==t_phase))){
			uint64_t pc =((vpq.vpq_data[head].tag<<index_bits)||vpq.vpq_data[head].index);
			pc=pc<<2;
			printf("vpq entry-%ld, pc - %lx pc-tag %lx pc-index %lx\n",head,pc,vpq.vpq_data[head].tag,vpq.vpq_data[head].index);
			if(head==(VPQsize-1)){
				head=0;
				h_phase=!h_phase;
			}
			else{
				head++;
			}
		}
		
	}
	bool value_predictor::space_avail(unsigned int bundle_vp){
		int space;
		if(vpq.h_phase==vpq.t_phase)
			space=VPQ_SIZE-vpq.tail+vpq.head;
		else
			space=vpq.head-vpq.tail;

		if(space>=bundle_vp)
			return true;
		else return false;
		
	}
unsigned int payload::get_size() {
   return(PAYLOAD_BUFFER_SIZE);
}

