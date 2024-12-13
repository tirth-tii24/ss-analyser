#define CONVERT_BE16(val)	val = be16toh(val)
#define CONVERT_BE32(val)	val = be32toh(val)
#define CONVERT_BE64(val)	val = be64toh(val)
#define MAX_RSSI_SUPPORT 100

#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ss-analyser.h"

#define MAX_RSSI_SUPPORT 100
struct scanresult *result_list;
int scanresults_n;
int rssi_list[MAX_RSSI_SUPPORT] = {0};
int req_freq = 0;

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

	fprintf(stderr, "read %d scan results\n", scanresults_n);
	free(scandata);

	return 0;
}

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


int get_data(int rssi)
{
	int i,handle=0,data,bins;
	double sum = 0.0, sum_sq = 0.0, mean, variance, index = 0.0;
	struct scanresult *result;
	memset(rssi_list,0,sizeof(int)*100);
	for (result = result_list; result; result = result->next) {
		switch (result->sample.tlv.type) {

		case ATH_FFT_SAMPLE_HT20:
			if (result->sample.ht20.rssi != rssi)
				continue;
			for (i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
				data = result->sample.ht20.data[i] << result->sample.ht20.max_exp;
				if (data == 0)
					data = 1;
			}
			handle=1;
			break;
		case ATH_FFT_SAMPLE_HT20_40:
			if (result->sample.ht40.lower_rssi != rssi)
				continue;
			for (i = 0; i < SPECTRAL_HT20_40_NUM_BINS; i++) {
				data = result->sample.ht40.data[i] << result->sample.ht40.max_exp;
				if (data == 0)
					data = 1;
			}
			handle=1;
			break;
		case ATH_FFT_SAMPLE_ATH10K:
			if (result->sample.ath10k.header.rssi != rssi)
				continue;
			bins = result->sample.tlv.length - (sizeof(result->sample.ath10k.header) - sizeof(result->sample.ath10k.header.tlv));
			printf("max_exp %d bin %d header size %lu header tlv %lu\n",result->sample.ath10k.header.max_exp,bins,sizeof(result->sample.ath10k.header),sizeof(result->sample.ath10k.header.tlv));
			for (i = 0; i < bins; i++) {
				data = result->sample.ath10k.data[i] << result->sample.ath10k.header.max_exp;
				if (data == 0)
					data = 1;
				printf("%u,",data);
				sum += data;        // Sum of all elements
				sum_sq += data * data;  // Sum of squares of elements
			}
			handle=1;
			break;
		case ATH_FFT_SAMPLE_ATH11K:
			if (result->sample.ath11k.header.rssi != rssi)
				continue;
			bins = result->sample.tlv.length - (sizeof(result->sample.ath11k.header) - sizeof(result->sample.ath11k.header.tlv));
			for (i = 0; i < bins; i++) {
				data = result->sample.ath11k.data[i] << result->sample.ath11k.header.max_exp;
				if (data == 0)
					data = 1;
			}
			handle=1;
			break;
	}
	if (handle)
		break;
	}
        mean = sum / bins;
	variance = (sum_sq / bins) - (mean * mean);
        index = (0.5 * rssi) + (0.3 * mean) + (0.2 * variance);
	if ( index > 100 )
		index = 100;
        printf("[{\"rssi\":%d},{\"data_mean\":%.2f},{\"data_vari\":%2.f},{\"index\":%.2f}]\n", rssi, mean, variance,index);
	return handle;
}

static int index_rssi(void)
{
	int i,max_samples=0, rssi=0;
	struct scanresult *result;

	memset(rssi_list,0,sizeof(int)*100);
	for (result = result_list; result; result = result->next) {
		switch (result->sample.tlv.type) {

		case ATH_FFT_SAMPLE_HT20:
			if ((req_freq != 0) && ( req_freq !=  result->sample.ht20.freq))
				continue;
			if (result->sample.ht20.rssi < MAX_RSSI_SUPPORT)
				rssi_list[result->sample.ht20.rssi]++;
			break;
		case ATH_FFT_SAMPLE_HT20_40:
			if ((req_freq != 0) && ( req_freq !=  result->sample.ht40.freq))
				continue;
			if (result->sample.ht40.lower_rssi < MAX_RSSI_SUPPORT)
				rssi_list[result->sample.ht40.lower_rssi]++;
			break;
		case ATH_FFT_SAMPLE_ATH10K:
			if ((req_freq != 0) && ( req_freq !=  result->sample.ath10k.header.freq1))
				continue;
			if (result->sample.ath10k.header.rssi < MAX_RSSI_SUPPORT)
				rssi_list[result->sample.ath10k.header.rssi]++;
			break;
		case ATH_FFT_SAMPLE_ATH11K:
			if ((req_freq != 0) && ( req_freq !=  result->sample.ath11k.header.freq1))
				continue;
			if (result->sample.ath11k.header.rssi < MAX_RSSI_SUPPORT)
				rssi_list[result->sample.ath11k.header.rssi]++;
			break;
	}
	}
	for(i=0;i<MAX_RSSI_SUPPORT;i++)
	{
		if (rssi_list[i] > max_samples) {
			max_samples = rssi_list[i];
			rssi = i;
		}
	}
	//printf("Rssi %d has %d samples\n",rssi,max_samples);
	return rssi;
}

void ss_analyser_usage(const char *prog)
{
	if (!prog)
		prog = "ss-analyser";

	fprintf(stderr, "Usage: %s <bin_file> <freq>\n", prog);
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
	int rssi = index_rssi();
	get_data(rssi);
	ss_analyser_exit();

	return 0;
}
