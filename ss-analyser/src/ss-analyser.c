#define CONVERT_BE16(val)	val = be16toh(val)
#define CONVERT_BE32(val)	val = be32toh(val)
#define CONVERT_BE64(val)	val = be64toh(val)

#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ss-analyser.h"

struct scanresult *result_list;
int scanresults_n;
int req_freq = 0, ath_type = 0;

/* read_file - reads an file into a big buffer and returns it
 *
 * @fname: file name
 *
 * returns the buffer with the files content
 */
static char *ss_read_file(char *fname, size_t *size)
{
	FILE *fp;
	char *buf = NULL;
	char *newbuf;
	size_t ret;

	fp = fopen(fname, "rb");

	if (!fp)
		return NULL;

	*size = 0;
	while (!feof(fp)) {

		newbuf = realloc(buf, *size + 4097);
		if (!newbuf) {
			free(buf);
			return NULL;
		}

		buf = newbuf;

		ret = fread(buf + *size, 1, 4096, fp);
		*size += ret;
	}
	fclose(fp);

	if (buf)
		buf[*size] = '\0';

	return buf;
}

/*
 * read_scandata - reads the fft scandata and compiles a linked list of datasets
 *
 * @fname: file name
 *
 * returns 0 on success, -1 on error.
 */
int ss_analyser_init(char *fname)
{
	char *pos, *scandata;
	size_t len, sample_len;
	size_t rel_pos, remaining_len;
	struct scanresult *result;
	struct sample_tlv *tlv;
	struct scanresult *tail = result_list;
	int handled, bins;

	scandata = ss_read_file(fname, &len);
	if (!scandata)
		return -1;

	pos = scandata;

	while ((uintptr_t)(pos - scandata) < len) {
		rel_pos = pos - scandata;
		remaining_len = len - rel_pos;

		if (remaining_len < sizeof(*tlv)) {
			fprintf(stderr, "Found incomplete TLV header at position 0x%zx\n", rel_pos);
			break;
		}

		tlv = (struct sample_tlv *) pos;
		CONVERT_BE16(tlv->length);
		sample_len = sizeof(*tlv) + tlv->length;
		pos += sample_len;

		if (remaining_len < sample_len) {
			fprintf(stderr, "Found incomplete TLV at position 0x%zx\n", rel_pos);
			break;
		}

		if (sample_len > sizeof(*result)) {
			fprintf(stderr, "sample length %zu too long\n", sample_len);
			continue;
		}

		result = malloc(sizeof(*result));
		if (!result)
			continue;

		memset(result, 0, sizeof(*result));
		memcpy(&result->sample, tlv, sample_len);

		handled = 0;
		switch (tlv->type) {
		case ATH_FFT_SAMPLE_HT20:
			if (sample_len != sizeof(result->sample.ht20)) {
				fprintf(stderr, "wrong sample length (have %zd, expected %zd)\n",
					sample_len, sizeof(result->sample.ht20));
				break;
			}

			CONVERT_BE16(result->sample.ht20.freq);
			CONVERT_BE16(result->sample.ht20.max_magnitude);
			CONVERT_BE64(result->sample.ht20.tsf);
			ath_type = ATH_FFT_SAMPLE_HT20;
			handled = 1;
			break;
		case ATH_FFT_SAMPLE_HT20_40:
			if (sample_len != sizeof(result->sample.ht40)) {
				fprintf(stderr, "wrong sample length (have %zd, expected %zd)\n",
					sample_len, sizeof(result->sample.ht40));
				break;
			}

			CONVERT_BE16(result->sample.ht40.freq);
			CONVERT_BE64(result->sample.ht40.tsf);
			CONVERT_BE16(result->sample.ht40.lower_max_magnitude);
			CONVERT_BE16(result->sample.ht40.upper_max_magnitude);

			handled = 1;
			ath_type = ATH_FFT_SAMPLE_HT20_40;
			break;
		case ATH_FFT_SAMPLE_ATH10K:
			if (sample_len < sizeof(result->sample.ath10k.header)) {
				fprintf(stderr, "wrong sample length (have %zd, expected at least %zd)\n",
					sample_len, sizeof(result->sample.ath10k.header));
				break;
			}

			bins = sample_len - sizeof(result->sample.ath10k.header);

			if (bins != 64 &&
			    bins != 128 &&
			    bins != 256) {
				fprintf(stderr, "invalid bin length %d\n", bins);
				break;
			}

			/*
			 * Zero noise level should not happen in a real environment
			 * but some datasets contain it which creates bogus results.
			 */
			if (result->sample.ath10k.header.noise == 0)
				break;

			CONVERT_BE16(result->sample.ath10k.header.freq1);
			CONVERT_BE16(result->sample.ath10k.header.freq2);
			CONVERT_BE16(result->sample.ath10k.header.noise);
			CONVERT_BE16(result->sample.ath10k.header.max_magnitude);
			CONVERT_BE16(result->sample.ath10k.header.total_gain_db);
			CONVERT_BE16(result->sample.ath10k.header.base_pwr_db);
			CONVERT_BE64(result->sample.ath10k.header.tsf);
			ath_type = ATH_FFT_SAMPLE_ATH10K;
			handled = 1;
			break;
		case ATH_FFT_SAMPLE_ATH11K:
			if (sample_len < sizeof(result->sample.ath11k.header)) {
				fprintf(stderr, "wrong sample length (have %zd, expected at least %zd)\n",
					sample_len, sizeof(result->sample.ath11k.header));
				break;
			}

			bins = sample_len - sizeof(result->sample.ath11k.header);

			if (bins != 16 &&
			    bins != 32 &&
			    bins != 64 &&
			    bins != 128 &&
			    bins != 256 &&
			    bins != 512) {
				fprintf(stderr, "invalid bin length %d\n", bins);
				break;
			}

			/*
			 * Zero noise level should not happen in a real environment
			 * but some datasets contain it which creates bogus results.
			 */
			if (result->sample.ath11k.header.noise == 0)
				break;

			CONVERT_BE16(result->sample.ath11k.header.freq1);
			CONVERT_BE16(result->sample.ath11k.header.freq2);
			CONVERT_BE16(result->sample.ath11k.header.max_magnitude);
			CONVERT_BE16(result->sample.ath11k.header.rssi);
			CONVERT_BE32(result->sample.ath11k.header.tsf);
			CONVERT_BE32(result->sample.ath11k.header.noise);

			ath_type = ATH_FFT_SAMPLE_ATH11K;
			handled = 1;
			break;
		default:
			fprintf(stderr, "unknown sample type (%d)\n", tlv->type);
			break;
		}

		if (!handled) {
			free(result);
			continue;
		}

		if (tail)
			tail->next = result;
		else
			result_list = result;

		tail = result;

		scanresults_n++;
	}

	//fprintf(stderr, "read %d scan results\n", scanresults_n);
	free(scandata);

	return 0;
}
/* Compute Frequency Quality Index */
int compute_index() 
{
	if ((ath_type == ATH_FFT_SAMPLE_HT20) || (ath_type == ATH_FFT_SAMPLE_HT20_40)) {
		return compute_ath9k_index();
	} else if (ath_type == ATH_FFT_SAMPLE_ATH10K) {
		return compute_ath10k_index();
	
	} else {
		printf("N/A");
		return -1;
	}
}

/* Compute Frequency Quality Index from ATH9K based Spectral Scan BINi */
int compute_ath9k_index(void)
{
	int i, rnum;
        struct scanresult *result;
	double total_pow = 0.0, index = 0.0;
        rnum = 0;
        for (result = result_list; result; result = result->next) {
                float sub_total_pow  = 0.0;
		switch (result->sample.tlv.type) {
		case ATH_FFT_SAMPLE_HT20_40:
			{
				int datamax = 0, datamin = 65536;
				int datasquaresum_lower = 0;
				int datasquaresum_upper = 0;
				int datasquaresum;
				int i;
				s8 noise;
				s8 rssi;
				//todo build average
				if ((req_freq != 0) && (req_freq != result->sample.ht40.freq))
					continue;
				for (i = 0; i < SPECTRAL_HT20_40_NUM_BINS / 2; i++) {
					int data;
					data = result->sample.ht40.data[i];
					data <<= result->sample.ht40.max_exp;
					data *= data;
					datasquaresum_lower += data;
					if (data > datamax)
						datamax = data;
					if (data < datamin)
						datamin = data;
				}
				for (i = SPECTRAL_HT20_40_NUM_BINS / 2; i < SPECTRAL_HT20_40_NUM_BINS; i++) {
					int data;
					data = result->sample.ht40.data[i];
					data <<= result->sample.ht40.max_exp;
					datasquaresum_upper += data;
					if (data > datamax)
						datamax = data;
					if (data < datamin)
						datamin = data;
				}
				sub_total_pow=0;
				for (i = 0; i < SPECTRAL_HT20_40_NUM_BINS; i++) {
					int data;

					if (i < SPECTRAL_HT20_40_NUM_BINS / 2) {
						noise = result->sample.ht40.lower_noise;
						datasquaresum = datasquaresum_lower;
						rssi = result->sample.ht40.lower_rssi;
					} else {
						noise = result->sample.ht40.upper_noise;
						datasquaresum = datasquaresum_upper;
						rssi = result->sample.ht40.upper_rssi;
					}

					data = result->sample.ht40.data[i];
					data <<= result->sample.ht40.max_exp;
					if (data == 0)
						data = 1;

					float signal = noise + rssi + 20 * log10(data) - log10(datasquaresum) * 10;
					sub_total_pow+=signal;
				}
				if(isinf(sub_total_pow))
					continue;
				total_pow+=(sub_total_pow / SPECTRAL_HT20_40_NUM_BINS);
			}
			break;
		case ATH_FFT_SAMPLE_HT20:
			{
				int datamax = 0, datamin = 65536;
				int datasquaresum = 0;
				if ((req_freq != 0) && (req_freq != result->sample.ht20.freq))
					continue;
				for (i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
					int data;
					data = (result->sample.ht20.data[i] << result->sample.ht20.max_exp);
					data *= data;
					datasquaresum += data;
					if (data > datamax)
						datamax = data;
					if (data < datamin)
						datamin = data;
				}
				sub_total_pow=0;
				for (i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
					float signal;
					int data;
					data = result->sample.ht20.data[i] << result->sample.ht20.max_exp;
					if (data == 0)
						data = 1;
					signal = result->sample.ht20.noise + result->sample.ht20.rssi + 20 * log10(data) - log10(datasquaresum) * 10;
					sub_total_pow+=signal;
				}
				if(isinf(sub_total_pow))
					continue;
				total_pow+=(sub_total_pow / SPECTRAL_HT20_NUM_BINS);
			}
			break;
		default:
			return -1;
		}
		rnum++;
	}
	index = (total_pow / rnum) - IDEAL_SIGNAL_POWER;
	printf("[{\"rssi\":%d},{\"data_mean\":%.2f},{\"data_vari\":%2.f},{\"index\":%.2f}]\n",0, 0.0, 0.0,index);
	if (total_pow)
		return 0;
	else 
		return -1;

}

/* Compute Frequency Quality Index from ATH10K based Spectral Scan BIN */
int compute_ath10k_index(void)
{
	int i, max_samples = 0, rssi = 0, bins = 0, data = 0;
	struct scanresult *result;
	double sum = 0.0, sum_sq = 0.0, mean, variance, index = 0.0;
        int rssi_list[MAX_RSSI_SUPPORT] = {0};

	memset(rssi_list,0,sizeof(int)*MAX_RSSI_SUPPORT);
	for (result = result_list; result; result = result->next) {
		if (result->sample.tlv.type == ATH_FFT_SAMPLE_ATH10K) {
			if ((req_freq != 0) && ( req_freq !=  result->sample.ath10k.header.freq1))
				continue;
			rssi_list[result->sample.ath10k.header.rssi]++;
		}
	}
	for(i=0;i<MAX_RSSI_SUPPORT;i++) {
		if (rssi_list[i] > max_samples) {
			max_samples = rssi_list[i];
			rssi = i;
		}
	}
	//printf("Rssi %d has %d samples\n",(signed char)rssi,max_samples);
	for (result = result_list; result; result = result->next) {
		if (result->sample.tlv.type == ATH_FFT_SAMPLE_ATH10K) {
			if ((req_freq != 0) && ( req_freq !=  result->sample.ath10k.header.freq1))
				continue;
			bins = result->sample.tlv.length - (sizeof(result->sample.ath10k.header) - sizeof(result->sample.ath10k.header.tlv));
			for (i = 0; i < bins; i++) {
				data = result->sample.ath10k.data[i] << result->sample.ath10k.header.max_exp;
				if (data == 0)
					data = 1;
				sum += data;        // Sum of all elements
				sum_sq += data * data;  // Sum of squares of elements
			}
			break;
		}
	}
	if (sum) {
        	mean = sum / bins;
		variance = (sum_sq / bins) - (mean * mean);
	        index = (0.5 * (signed char)rssi) + (0.3 * mean) + (0.2 * variance);
		if ( index > 100 )
			index = 100;
	        printf("[{\"rssi\":%d},{\"data_mean\":%.2f},{\"data_vari\":%2.f},{\"index\":%.2f}]\n",rssi, mean, variance,index);
		return 0;
	}
	return -1;
}

/* SS-ANALYSER Usuage */
void ss_analyser_usage(const char *prog)
{
	if (!prog)
		prog = "ss-analyser";

	fprintf(stderr, "Usage: %s <bin_file> <freq>\n", prog);
}

/* SS-ANALYSER Exit */
void ss_analyser_exit(void)
{
	struct scanresult *list = result_list;
	struct scanresult *next;

	while (list) {
		next = list->next;
		free(list);
		list = next;
	}

	result_list = NULL;
}


int main(int argc, char *argv[])
{
	char *ss_name = NULL;
	char *prog = NULL;

	if (argc >= 1)
		prog = argv[0];

	if (argc >= 2)
		ss_name = argv[1];

	if (argc >= 3)
		req_freq = atoi(argv[2]);

	if (ss_analyser_init(ss_name) < 0) {
		fprintf(stderr, "Couldn't read scanfile ...\n");
		ss_analyser_usage(prog);
		return -1;
	}
	if (!compute_index()) {
		ss_analyser_exit();
		return 0;
	}
	else { 
		ss_analyser_exit();
		return -1;
	}
}
