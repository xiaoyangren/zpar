// Copyright (C) University of Oxford 2010
/****************************************************************
 *                                                              *
 * zpar.cpp - main app; a convenience wrapper for all algorithms*
 *                                                              *
 * Author: Yue Zhang                                            *
 *                                                              *
 * Computing Laboratory, Oxford. 2006.11                        *
 *                                                              *
 ****************************************************************/

#include "definitions.h"
#include "tagger.h"
#include "conparser.h"
#include "depparser.h"
#include "doc2snt.h"
#include "reader.h"
#include "writer.h"
#include "stdlib.h"

using namespace chinese;

#define MAX_SENTENCE_SIZE 512
//#define JOINT_CONPARSER

#ifndef JOINT_CONPARSER
/*===============================================================
 *
 * tag
 *
 *==============================================================*/

void tag(const std::string sInputFile, const std::string sOutputFile, const std::string sFeaturePath, bool bOutDoc, bool bSegmented) {
   std::cout << "Tagging started" << std::endl;
   int time_start = clock();
   std::string sFeatureFile = sFeaturePath + "/tagger";
   if (!FileExists(sFeatureFile))
      THROW("Tagger model does not exists. It should be put at model_path/tagger");
   CTagger tagger(sFeatureFile, false, MAX_SENTENCE_SIZE, "", false);
   CDoc2Snt doc2snt(sInputFile, MAX_SENTENCE_SIZE);
   CSentenceReader input_reader(sInputFile);
   CSentenceWriter outout_writer(sOutputFile);
   CStringVector *input_sent = new CStringVector;
   CTwoStringVector *outout_sent = new CTwoStringVector; 
   CStringVector *output_sent_untag = new CStringVector;

   unsigned nCount=0;
   bool bNewLine = bOutDoc ? false : true;
   bool bDocPar = false;
   
   CBitArray prunes(MAX_SENTENCE_SIZE);

   //bool bReadSuccessful; 
   //bReadSuccessful = input_reader.readRawSentence(input_sent, true, false);

   // If we read segmented sentence, we will ignore spaces from input. 
   while( doc2snt.getSentence( *input_sent ) ) {
      TRACE("Sentence " << nCount);
      ++ nCount;
      bDocPar = false;
      if ( input_sent->back()=="\n" ) {
         bDocPar = true;
         input_sent->pop_back();
      }
      tagger.tag(input_sent, outout_sent, NULL, 1);
      // Ouptut sent
      if(!bSegmented)
    	  outout_writer.writeSentence(outout_sent, '_', bNewLine);
      else{
    	  UntagSentence( outout_sent, output_sent_untag );
    	  outout_writer.writeSentence(output_sent_untag);
      }

      input_sent->clear();
   }
   delete input_sent;
   delete outout_sent;
   delete output_sent_untag;

   std::cout << "Tagging has finished successfully. Total time taken is: " << double(clock()-time_start)/CLOCKS_PER_SEC << std::endl;
}

/*===============================================================
 *
 * parse
 *
 *==============================================================*/

void parse(const std::string sInputFile, const std::string sOutputFile, const std::string sFeaturePath) {
   std::cout << "Initializing ZPar..." << std::endl;
   int time_start = clock();
   std::ostream *outs; if (sOutputFile=="") outs=&std::cout; else outs = new std::ofstream(sOutputFile.c_str()); 
   std::string sTaggerFeatureFile = sFeaturePath + "/tagger";
   std::string sParserFeatureFile = sFeaturePath + "/conparser";
   if (!FileExists(sTaggerFeatureFile))
      THROW("Tagger model does not exists. It should be put at model_path/tagger");
   if (!FileExists(sParserFeatureFile))
      THROW("Parser model does not exists. It should be put at model_path/conparser");
   std::cout << "[The segmentation and tagging model] "; std::cout.flush();
   CTagger tagger(sTaggerFeatureFile, false, MAX_SENTENCE_SIZE, "", false);
   std::cout << "[The parsing model] "; std::cout.flush();
   CConParser conparser(sParserFeatureFile, false);
   CDoc2Snt doc2snt(sInputFile, MAX_SENTENCE_SIZE);
   CSentenceReader input_reader(sInputFile);
   CStringVector *input_sent = new CStringVector;
   CTwoStringVector *tagged_sent = new CTwoStringVector; 
   chinese::CCFGTree *outout_sent = new chinese::CCFGTree; 
   std::cout << "ZPar initialized." << std::endl; std::cout.flush();

   unsigned nCount=0;
   
   CBitArray prunes(MAX_SENTENCE_SIZE);

   // If we read segmented sentence, we will ignore spaces from input. 
   while( doc2snt.getSentence( *input_sent ) ) {
      TRACE("Sentence " << nCount);
      ++ nCount;
      if ( input_sent->back()=="\n" ) {
         input_sent->pop_back();
      }
      tagger.tag(input_sent, tagged_sent, NULL, 1);
      conparser.parse(*tagged_sent, outout_sent);
      // Ouptut sent
      (*outs) << outout_sent->str_unbinarized() << std::endl;
      input_sent->clear();
   }
   delete input_sent;
   delete tagged_sent;
   delete outout_sent;

   if (sOutputFile!="") delete outs;
   std::cout << "Parsing has finished successfully. Total time taken is: " << double(clock()-time_start)/CLOCKS_PER_SEC << std::endl;
}


#else
/*===============================================================
 *
 * parse
 *
 *==============================================================*/

void jointparse(const std::string sInputFile, const std::string sOutputFile, const std::string sFeaturePath, bool bSegmented, bool bTagged) {
   std::cout << "Initializing ZPar..." << std::endl;
   int time_start = clock();
   std::ostream *outs;
   CSentenceWriter *outout_writer;
   if(!bSegmented && !bTagged)
   {
	   if (sOutputFile=="") outs=&std::cout; else outs = new std::ofstream(sOutputFile.c_str());
   }
   else
   {
	    outout_writer = new CSentenceWriter(sOutputFile);
   }
   std::string sParserFeatureFile = sFeaturePath + "/conparser";
   if (!FileExists(sParserFeatureFile))
      THROW("Parser model does not exists. It should be put at model_path/conparser");
   std::cout << "[The parsing model] "; std::cout.flush();
   CConParser conparser(sParserFeatureFile, MAX_SENTENCE_SIZE, false);
   CDoc2Snt doc2snt(sInputFile, MAX_SENTENCE_SIZE);
   CSentenceReader input_reader(sInputFile);
   CStringVector *input_sent = new CStringVector;
   CStringVector *output_sent_untag = new CStringVector;
   CTwoStringVector *tagged_sent = new CTwoStringVector;
   chinese::CJointTree *outout_sent = new chinese::CJointTree;
   std::cout << "ZPar initialized." << std::endl; std::cout.flush();

   unsigned nCount=0;

   CBitArray prunes(MAX_SENTENCE_SIZE);

   // If we read segmented sentence, we will ignore spaces from input.
   while( doc2snt.getSentence( *input_sent ) ) {
      TRACE("Sentence " << nCount);
      ++ nCount;
      if ( input_sent->back()=="\n" ) {
         input_sent->pop_back();
      }
      conparser.parse(*input_sent, outout_sent , 0, 1 ) ;
      // Ouptut sent
      if(!bSegmented && !bTagged)
      {
    	  (*outs) << outout_sent->str_unbinarizedall() << std::endl;
      }
      else if(bTagged)
      {
    	  UnparseSentence( outout_sent, tagged_sent ) ;
    	  outout_writer->writeSentence(tagged_sent, '_', true);
      }
      else if(bSegmented)
      {
    	  UnparseSentence( outout_sent, tagged_sent ) ;
    	  UntagSentence( tagged_sent, output_sent_untag );
    	  outout_writer->writeSentence(output_sent_untag);
      }
      input_sent->clear();
   }
   delete input_sent;
   delete tagged_sent;
   delete outout_sent;
   delete output_sent_untag;

   if (sOutputFile!="" && !bSegmented && !bTagged) delete outs;
   if(bSegmented || bTagged) delete outout_writer;
   std::cout << "Parsing has finished successfully. Total time taken is: " << double(clock()-time_start)/CLOCKS_PER_SEC << std::endl;
}

#endif

/*===============================================================
 *
 * depparse
 *
 *==============================================================*/

void depparse(const std::string sInputFile, const std::string sOutputFile, const std::string sFeaturePath) {
   std::cout << "Initializing ZPar..." << std::endl;
   int time_start = clock();
   std::ostream *outs; if (sOutputFile=="") outs=&std::cout; else outs = new std::ofstream(sOutputFile.c_str());
   std::string sTaggerFeatureFile = sFeaturePath + "/tagger";
   std::string sParserFeatureFile = sFeaturePath + "/depparser";
   if (!FileExists(sTaggerFeatureFile))
      THROW("Tagger model does not exists. It should be put at model_path/tagger");
   if (!FileExists(sParserFeatureFile))
      THROW("Parser model does not exists. It should be put at model_path/depparser");
   std::cout << "[The segmentation and tagging model] "; std::cout.flush();
   CTagger tagger(sTaggerFeatureFile, false, MAX_SENTENCE_SIZE, "", false);
   std::cout << "[The parsing model] "; std::cout.flush();
   CDepParser depparser(sParserFeatureFile, false);
   CDoc2Snt doc2snt(sInputFile, MAX_SENTENCE_SIZE);
   CSentenceReader input_reader(sInputFile);
   CStringVector *input_sent = new CStringVector;
   CTwoStringVector *tagged_sent = new CTwoStringVector;
   CLabeledDependencyTree *outout_sent = new CLabeledDependencyTree;
   std::cout << "ZPar initialized." << std::endl; std::cout.flush();

   unsigned nCount=0;

   CBitArray prunes(MAX_SENTENCE_SIZE);

   // If we read segmented sentence, we will ignore spaces from input.
   while( doc2snt.getSentence( *input_sent ) ) {
      TRACE("Sentence " << nCount);
      ++ nCount;
      if ( input_sent->back()=="\n" ) {
         input_sent->pop_back();
      }
      tagger.tag(input_sent, tagged_sent, NULL, 1);
      depparser.parse(*tagged_sent, outout_sent);
      // Ouptut sent
      (*outs) << *outout_sent;
      input_sent->clear();
   }
   delete input_sent;
   delete tagged_sent;
   delete outout_sent;

   if (sOutputFile!="") delete outs;
   std::cout << "Parsing has finished successfully. Total time taken is: " << double(clock()-time_start)/CLOCKS_PER_SEC << std::endl;
}




/*===============================================================
 *
 * main
 *
 *==============================================================*/

int main(int argc, char* argv[]) {
   try {
      COptions options(argc, argv);
      CConfigurations configurations;
      configurations.defineConfiguration("o", "{s|t[d]|d|c}", "outout format; 's' segmented format, 't' pos-tagged format in sentences, 'td' pos-tagged format in documents without sentence boundary delimination, 'd' refers to dependency parse tree format, and 'c' refers to constituent parse tree format", "c");

      if (options.args.size() < 2 || options.args.size() > 4) {
         std::cout << "\nUsage: " << argv[0] << " feature_path [input_file [outout_file]]" << std::endl;
         std::cout << configurations.message() << std::endl;
         return 1;
      }
      std::string warning = configurations.loadConfigurations(options.opts);
      if (!warning.empty()) {
         std::cout << "Warning: " << warning << std::endl;
      }

      std::string sInputFile = options.args.size() > 2 ? options.args[2] : "";
      std::string sToFile = options.args.size() > 3 ? options.args[3] : "";
      std::string sOutFormat = configurations.getConfiguration("o");

      bool bOutDoc = (sOutFormat=="td");
      bool bSegmented = (sOutFormat=="s");
      bool bTagged = (sOutFormat=="t") || (sOutFormat=="td");
#ifndef JOINT_CONPARSER
      if (sOutFormat == "t" || sOutFormat == "td" || sOutFormat == "s")
          tag(sInputFile, sToFile, options.args[1], bOutDoc, bSegmented);
      if (sOutFormat == "c" )
          parse(sInputFile, sToFile, options.args[1]);
#else
      if(sOutFormat == "t" || sOutFormat == "td" || sOutFormat == "s" || sOutFormat == "c")
         jointparse(sInputFile, sToFile, options.args[1], bSegmented, bTagged);
#endif
      if (sOutFormat == "d" )
          depparse(sInputFile, sToFile, options.args[1]);
      return 0;
   } catch(const std::string&e) {std::cerr<<"Error: "<<e<<std::endl;return 1;}
}

