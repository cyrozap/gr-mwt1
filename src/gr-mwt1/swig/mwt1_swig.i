/* -*- c++ -*- */

#define MWT1_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "mwt1_swig_doc.i"

%{
#include "mwt1/mwt1_packet_decoder.h"
%}
%include "mwt1/mwt1_packet_decoder.h"
GR_SWIG_BLOCK_MAGIC2(mwt1, mwt1_packet_decoder);
