#ifndef _EMU2413_H_
#define _EMU2413_H_

#include <stdint.h>

#define PI 3.14159265358979323846

/* voice data */
typedef struct __OPLL_PATCH {
	uint32_t TL, FB, EG, ML, AR, DR, SL, RR, KR, KL, AM, PM, WF;
} OPLL_PATCH;

/* slot */
typedef struct __OPLL_SLOT {

	OPLL_PATCH patch;

	int32_t type;		/* 0 : modulator 1 : carrier */

	/* OUTPUT */
	int32_t feedback;
	int32_t output[2];	/* Output value of slot */

	/* for Phase Generator (PG) */
	uint16_t *sintbl;	/* Wavetable */
	uint32_t phase;		/* Phase */
	uint32_t dphase;	/* Phase increment amount */
	uint32_t pgout;		/* output */

	/* for Envelope Generator (EG) */
	int32_t fnum;		/* F-Number */
	int32_t block;		/* Block */
	int32_t volume;		/* Current volume */
	int32_t sustine;	/* Sustine 1 = ON, 0 = OFF */
	uint32_t tll;		/* Total Level + Key scale level */
	uint32_t rks;		/* Key scale offset (Rks) */
	int32_t eg_mode;	/* Current state */
	uint32_t eg_phase;	/* Phase */
	uint32_t eg_dphase;	/* Phase increment amount */
	uint32_t egout;		/* output */
} OPLL_SLOT;

/* Mask */
#define OPLL_MASK_CH(x) (1<<(x))

/* opll */
typedef struct __OPLL {
	uint32_t adr;
	int32_t out;

	uint32_t realstep;
	uint32_t oplltime;
	uint32_t opllstep;
	int32_t prev, next;

	/* Register */
	uint8_t LowFreq[6];
	uint8_t HiFreq[6];
	uint8_t InstVol[6];
	uint8_t CustInst[8];
	int32_t slot_on_flag[6 * 2];

	/* Pitch Modulator */
	uint32_t pm_phase;
	int32_t lfo_pm;

	/* Amp Modulator */
	int32_t am_phase;
	int32_t lfo_am;

	uint32_t quality;

	/* Noise Generator */
	uint32_t noise_seed;

	/* Channel Data */
	int32_t patch_number[6];
	int32_t key_status[6];

	/* Slot */
	OPLL_SLOT slot[6 * 2];

	/* Voice Data */
	OPLL_PATCH patch[19 * 2];

	uint32_t mask;
} OPLL;

/* Create Object */
extern OPLL *OPLL_new(uint32_t clk, uint32_t rate);
extern void OPLL_delete(OPLL *);

/* Setup */
extern void OPLL_reset(OPLL *);
extern void OPLL_set_rate(OPLL * opll, uint32_t r);
extern void OPLL_set_quality(OPLL * opll, uint32_t q);

/* Port/Register access */
extern void OPLL_writeIO(OPLL *, uint32_t reg, uint32_t val);
extern void OPLL_writeReg(OPLL *, uint32_t reg, uint32_t val);

/* Synthsize */
extern int16_t OPLL_calc(OPLL *);

/* Misc */
extern void OPLL_forceRefresh(OPLL *);

/* Channel Mask */
extern uint32_t OPLL_setMask(OPLL *, uint32_t mask);
extern uint32_t OPLL_toggleMask(OPLL *, uint32_t mask);

#endif
