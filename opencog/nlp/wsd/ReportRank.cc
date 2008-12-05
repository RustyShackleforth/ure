/*
 * ReportRank.cc
 *
 * Implements the PageRank graph centrality algorithm for word-senses.
 *
 * Copyright (c) 2008 Linas Vepstas <linas@linas.org>
 */

#include "ReportRank.h"

#include <stdio.h>

#include <opencog/atomspace/types.h>
#include <opencog/atomspace/Node.h>
#include <opencog/atomspace/SimpleTruthValue.h>
#include <opencog/atomspace/TruthValue.h>
#include <opencog/nlp/wsd/ForeachWord.h>
#include <opencog/util/platform.h>

using namespace opencog;

#define DEBUG 1

ReportRank::ReportRank(void)
{
	parse_cnt = 0;
}

ReportRank::~ReportRank()
{
}

/**
 * For each parse of the sentence, make a report.
 */
void ReportRank::report_sentence(Handle h)
{
	parse_cnt = 0;
	foreach_parse(h, &ReportRank::report_parse_f, this);
}


void ReportRank::report_document(const std::deque<Handle> &parse_list)
{
	normalization = 0.0;
	sense_count = 0.0;
	choosen_sense_count = 0.0;
	word_count = 0;

	// Iterate over all the parses in the document.
	std::deque<Handle>::const_iterator i;
	for (i = parse_list.begin(); i != parse_list.end(); i++)
	{
		Handle h = *i;
		foreach_word_instance(h, &ReportRank::count_word, this);
	}

#ifdef DEBUG
	printf("; report_document: norm=%g senses=%g words=%d\n",
		normalization, sense_count, word_count);
#endif

	normalization = 1.0 / normalization;

	for (i = parse_list.begin(); i != parse_list.end(); i++)
	{
		Handle h = *i;
		foreach_word_instance(h, &ReportRank::renorm_word, this);
	}
#ifdef DEBUG
	printf("; report_document: chose=%g senses out of %g (%g percent)\n",
		choosen_sense_count, sense_count, 100.0*choosen_sense_count/sense_count);
	fflush(stdout);
#endif
}

/**
 * For each parse, walk over each word.
 */
void ReportRank::report_parse(Handle h)
{
#ifdef DEBUG
	printf ("; ReportRank: Sentence %d:\n", parse_cnt);
#endif
	parse_cnt ++;

	normalization = 0.0;
	sense_count = 0.0;
	choosen_sense_count = 0.0;
	foreach_word_instance(h, &ReportRank::count_word, this);

	normalization = 1.0 / normalization;
	foreach_word_instance(h, &ReportRank::renorm_word, this);
}

bool ReportRank::report_parse_f(Handle h)
{
	report_parse(h);
	return false;
}

bool ReportRank::count_word(Handle h)
{
	word_count ++;
	foreach_word_sense_of_inst(h, &ReportRank::count_sense, this);
	return false;
}

bool ReportRank::renorm_word(Handle h)
{
#ifdef DEBUG
	hi_score = -1e10;
	hi_sense = "(none)";
#endif
	foreach_word_sense_of_inst(h, &ReportRank::renorm_sense, this);

#ifdef DEBUG
	Handle wh = get_dict_word_of_word_instance(h);
	Node *n = dynamic_cast<Node *>(TLB::getAtom(wh));
	const char *wd = n->getName().c_str();
	printf("; hi score=%g word = %s sense=%s\n", hi_score, wd, hi_sense);
	fflush (stdout);
#endif
	return false;
}

bool ReportRank::count_sense(Handle word_sense_h,
                             Handle sense_link_h)
{
	Link *l = dynamic_cast<Link *>(TLB::getAtom(sense_link_h));
	normalization += l->getTruthValue().getMean();
	sense_count += 1.0;
	return false;
}

bool ReportRank::renorm_sense(Handle word_sense_h,
                              Handle sense_link_h)
{
	Link *l = dynamic_cast<Link *>(TLB::getAtom(sense_link_h));
	double score = l->getTruthValue().getMean();

	score *= normalization * sense_count;
	score -= 1.0;

	// Update the truth value, it will store deviation from average.
	// That is, initially, each word sense of each word instance is
	// assigned a (denormalized) probability of 1.0. Solving the
	// Markov chain/page rank causes the some of this to flow away
	// from less likely to more likely senses. The scoring is 
	// relative to this initial value: thus, unlikely scores will
	// go negative, likely scores will go positive.  "Typical"
	// distributions seem to go from -0.8 to +3.5 or there-abouts.
	//
	SimpleTruthValue stv((float) score, 1.0f);
	stv.setConfidence(l->getTruthValue().getConfidence());
	l->setTruthValue(stv);

#ifdef DEBUG
	if (hi_score < score) {
		Node *n = dynamic_cast<Node *>(TLB::getAtom(word_sense_h));
		hi_sense = n->getName().c_str();
		hi_score = score;
	}
	if (0.0 < score) {
		choosen_sense_count += 1.0;
	
#if 0
Node *n = dynamic_cast<Node *>(TLB::getAtom(word_sense_h));
printf ("duu word sense=%s score=%f\n", n->getName().c_str(), score);
fflush (stdout);
#endif
	}
#endif

	return false;
}

/* ============================== END OF FILE ====================== */
