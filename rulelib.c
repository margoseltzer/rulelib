/*
 * Copyright 2015 President and Fellows of Harvard College.
 * All rights reserved.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rule.h"

/* Function declarations. */
unsigned long *ascii_to_vector(char *, size_t, int *, int *);
#define RULE_INC 100
#define BITS_PER_ENTRY (sizeof(unsigned long) * 8)

/*
 * Preprocessing step.
 * INPUTS: Using the python from the BRL_code.py: Call get_freqitemsets
 * to generate data files of the form:
 * 	Rule<TAB><bit vector>\n
 *
 * OUTPUTS: an array of rule_t's
 */
int
rules_init(const char *infile, int *nrules, int *nsamples, rule_t **rules_ret)
{
	FILE *fi;
	char *line, *rulestr;
	int rule_cnt, sample_cnt, rsize;
	int i, ones, ret;
	rule_t *rules;
	size_t len, rulelen;

	rule_cnt = sample_cnt = rsize = 0;

	if ((fi = fopen(infile, "r")) == NULL)
		return (errno);

	while ((line = fgetln(fi, &len)) != NULL) {
		if (rule_cnt >= rsize) {
			rsize += RULE_INC;
			rules = realloc(rules, rsize * sizeof(rule_t));
			if (rules == NULL)
				goto err;
		}
		/* Get the rule string; line will contain the bits. */
		if ((rulestr = strsep(&line, "\t")) == NULL)
			goto err;
		rulelen = strlen(rulestr) + 1;
		len -= rulelen;
		if ((rules[rule_cnt].features = malloc(rulelen)) == NULL)
			goto err;
		(void)strncpy(rules[rule_cnt].features, rulestr, rulelen);
		rules[rule_cnt].truthtable = 
		    ascii_to_vector(line, len, &sample_cnt, &ones);
		if (rules[rule_cnt].truthtable == NULL)
			goto err;
		rules[rule_cnt].support = ones;
		rule_cnt++;
	}
	/* All done! */
	fclose(fi);

	*nsamples = sample_cnt;
	*nrules = rule_cnt;
	*rules_ret = rules;

	return (0);

err:
	ret = errno;

	/* Reclaim space. */
	if (rules != NULL) {
		for (i = 0; i < rule_cnt; i++) {
			free(rules[i].features);
			free(rules[i].truthtable);
		}	
		free(rules);
	}
	(void)fclose(fi);
	return (ret);
}

/*
 * Convert an ascii sequence of 0's and 1's to a bit vector.
 * This is a hand-coded naive implementation; we'll also support
 * the GMP library, switching between the two with a compiler directive.
 */
unsigned long *
ascii_to_vector(char *line, size_t len, int *nsamples, int *nones)
{
	/*
	 * If *nsamples is 0, then we will set it to the number of
	 * 0's and 1's. If it is non-zero, then we'll ensure that
	 * the line is the right length.
	 */

	char *p;
	int i, bufsize, last_i, ones;
	unsigned long val;
	unsigned long *bufp, *buf;

	/* NOT DONE */
	assert(line != NULL);

	/* Compute bufsize in number of unsigned elements. */
	if (*nsamples == 0)
		bufsize = (len + BITS_PER_ENTRY - 1) / BITS_PER_ENTRY;
	else
		bufsize = (*nsamples + BITS_PER_ENTRY - 1) / BITS_PER_ENTRY;
	buf = malloc(bufsize * sizeof(unsigned long));
	if (buf == NULL)
		return(NULL);
	
	bufp = buf;
	val = 0;
	i = 0;
	last_i = 0;
	ones = 0;


	for(p = line; len-- > 0; p++) {
		switch (*p) {
			case '0':
				val <<= 1;
				i++;
				break;
			case '1':
				val <<= 1;
				val++;
				i++;
				ones++;
				break;
			default:
				break;
		}
		/* If we have filled up val, store it and reset it. */
		if (last_i != i && (i % BITS_PER_ENTRY) == 0) {
			*bufp = val;
			val = 0;
			bufp++;
			last_i = i;
		}
	}

	/* Store val if it contains any bits. */
	if ((i % BITS_PER_ENTRY) != 0)
		*bufp = val;

	if (*nsamples == 0)
		*nsamples = i;
	else if (*nsamples != i) {
		fprintf(stderr, "Wrong number of samples. Expected %d got %d\n",
		    *nsamples, i);
		/* free(buf); */
		buf = NULL;
	}
	*nones = ones;
	return (buf);
}
