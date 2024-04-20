#ifndef VALUE_PREDICTOR_H
#define VALUE_PREDICTOR_H

#include <inttypes.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <payload.h>
//#include <pipeline.h>
class value_predictor {
public:

//SVP
	typedef struct SVP{
		uint64_t tag;
		uint64_t conf;
		uint64_t retired_value;
		int64_t stride;
		uint64_t instance;
	};

	std::vector<SVP> val_predictor;

//VPQ
	struct VPQ_data{
		uint64_t tag;
		uint64_t index;
		uint64_t value;
	}vpq_data;
	struct VPQ{
		std::vector<VPQ_data> vpq_data;
		uint64_t head,tail;
		bool h_phase,t_phase;
	}vpq;



//misc functions 
	void index_calc(uint64_t &pc);
	void vpq_update(uint64_t pc, uint64_t vpq_entry_flag,int64_t value);

	bool check_prediction(uint64_t actual_val, uint64_t predicted_val, bool confidence);

	bool space_avail(unsigned int bundle_vp);
	void train_or_rep(uint64_t pc);

	void squash();
	void rollback(uint64_t tail_ptr_chkpt, uint64_t t_phase);
	bool eligible_inst(payload_t *pay,uint64_t index);
//Input args
	uint64_t VPQsize;
	bool oracleconf;
   uint64_t index_bits; // # SVP entries is 2**(# index bits)
	uint64_t tag_bits; // 0: no tag, 62-(# index bits): full tag, or somewhere between for partial tag
 
	uint64_t confmax; // confidence threshold to be confident
	uint64_t confinc; // amount by which to increment confidence(but saturate at confmax)
	uint64_t confdec; // 0: reset confidence, >0: amount by which to decrement confidence (but saturate at 0)
	uint64_t replace_stride;// If stride changes, only update the stride if conf <= replace_stride. This can be used for stride inertia.
	uint64_t replace; // Replacement confidence threshold. Only used if SVP has tags and when the tag mismatches while training: replace if conf <= replace.
	bool predINTALU; // 1: predict integer ALU instructions with dest. reg., 0: don’t predict this instruction class
	bool predFPALU; // 1: predict flt.pt. ALU instructions with dest. reg., 0: don’t predict this instruction class
	bool predLOAD; // 1: predict load instructions with dest. reg., 0: don’t predict this instruction class
	bool VPQ_full_policy;// If VPQ full and need entries for VP-eligible





//VPU MEASUREMENTS-----------------------------------
	uint64_t vpmeas_ineligible;       // Not eligible for value prediction.
   uint64_t vpmeas_ineligible_type; // Not eligible because of type.
   uint64_t vpmeas_ineligible_drop; // VPU dropped otherwise-eligible instr. (neither predict nor train)
                         			   // due to unavailable resource (e.g., VPQ_full_policy=1 and no free VPQ entry).

	uint64_t vpmeas_eligible;        // Eligible for value prediction.
   uint64_t vpmeas_miss;            // VPU was unable to generate a value prediction (e.g., SVP miss).
   uint64_t vpmeas_conf_corr;       // VPU generated a confident and correct value prediction.
   uint64_t vpmeas_conf_incorr;     // VPU generated a confident and incorrect value prediction. (MISPREDICTION)
   uint64_t vpmeas_unconf_corr;     // VPU generated an unconfident and correct value prediction. (LOST OPPORTUNITY)
   uint64_t vpmeas_unconf_incorr;   // VPU generated an unconfident and incorrect value prediction.;


	void checkpoint(uint64_t & , bool& );


	void restore(uint64_t , bool );
	////////////////////////////////////////
	// Public functions.
	////////////////////////////////////////
	value_predictor();
	~value_predictor();

	//////////////////////////////////////////
	// Functions related to Rename Stage.   //
	//////////////////////////////////////////
	void predict(uint64_t , uint64_t&, bool&, bool& , bool&, bool&, uint64_t&);
	void debug_print();
};
#endif
