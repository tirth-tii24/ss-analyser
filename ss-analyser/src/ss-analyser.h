#ifndef _SS_ANALYSER_H
#define _SS_ANALYSER_H

#include <stdint.h>


typedef int8_t s8;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;

enum ath_sample_type {
        ATH_FFT_SAMPLE_HT20 = 1,
        ATH_FFT_SAMPLE_HT20_40 = 2,
	ATH_FFT_SAMPLE_ATH10K = 3,
	ATH_FFT_SAMPLE_ATH11K = 4,
};

enum nl80211_channel_type {
	NL80211_CHAN_NO_HT,
	NL80211_CHAN_HT20,
	NL80211_CHAN_HT40MINUS,
	NL80211_CHAN_HT40PLUS
};

/*
 * ath9k spectral definition
 */
#define SPECTRAL_HT20_NUM_BINS          56
#define SPECTRAL_HT20_40_NUM_BINS		128
#define MAX_RSSI_SUPPORT 256
#define IDEAL_SIGNAL_POWER -118

struct sample_tlv {
        u8 type;        /* see ath_sample */
        u16 length;
        /* type dependent data follows */
} __attribute__((packed));

struct sample_ht20 {
        struct sample_tlv tlv;

        u8 max_exp;

        u16 freq;
        s8 rssi;
        s8 noise;

        u16 max_magnitude;
        u8 max_index;
        u8 bitmap_weight;

        u64 tsf;

        u8 data[SPECTRAL_HT20_NUM_BINS];
} __attribute__((packed));

struct sample_ht20_40 {
	struct sample_tlv tlv;

	u8 channel_type;
	u16 freq;

	s8 lower_rssi;
	s8 upper_rssi;

	u64 tsf;

	s8 lower_noise;
	s8 upper_noise;

	u16 lower_max_magnitude;
	u16 upper_max_magnitude;

	u8 lower_max_index;
	u8 upper_max_index;

	u8 lower_bitmap_weight;
	u8 upper_bitmap_weight;

	u8 max_exp;

	u8 data[SPECTRAL_HT20_40_NUM_BINS];
} __attribute__((packed));

/*
 * ath10k spectral sample definition
 */

#define SPECTRAL_ATH10K_MAX_NUM_BINS            256

struct sample_ath10k {
	struct sample_tlv tlv;
	u8 chan_width_mhz;
	uint16_t freq1;
	uint16_t freq2;
	int16_t noise;
	uint16_t max_magnitude;
	uint16_t total_gain_db;
	uint16_t base_pwr_db;
	uint64_t tsf;
	s8 max_index;
	u8 rssi;
	u8 relpwr_db;
	u8 avgpwr_db;
	u8 max_exp;

	u8 data[0];
} __attribute__((packed));

/*
 * ath11k spectral sample definition
 */

#define SPECTRAL_ATH11K_MAX_NUM_BINS            512

struct sample_ath11k {
	struct sample_tlv tlv;
	u8 chan_width_mhz;
	s8 max_index;
	u8 max_exp;
	uint16_t freq1;
	uint16_t freq2;
	uint16_t max_magnitude;
	uint16_t rssi;
	uint32_t tsf;
	int32_t noise;

	u8 data[0];
} __attribute__((packed));

struct scanresult {
	union {
		struct sample_tlv tlv;
		struct sample_ht20 ht20;
		struct sample_ht20_40 ht40;
		struct {
			struct sample_ath10k header;
			u8 data[SPECTRAL_ATH10K_MAX_NUM_BINS];
		} ath10k;
		struct {
			struct sample_ath11k header;
			u8 data[SPECTRAL_ATH11K_MAX_NUM_BINS];
		} ath11k;
	} sample;
	struct scanresult *next;
};

int ss_analyser_init(char *fname);
void ss_analyser_exit(void);
void ss_analyser_usage(const char *prog);
int compute_index();
int compute_ath10k_index();
int compute_ath9k_index();
extern struct scanresult *result_list;
extern int scanresults_n;

#endif
