// Copyright (C) University of Oxford 2010
/****************************************************************
 *                                          *
 * train.cpp - the training of the general dependency parser.  *
 *                                          *
 * Author: Yue Zhang                              *
 *                                          *
 * Computing Laboratory, Oxford. 2007.8                 *
 *                                          *
 ****************************************************************/
#include <thread>

#include "definitions.h"
#include "depparser.h"
#include "reader.h"
#include "writer.h"

using namespace TARGET_LANGUAGE;


static void
read_input_file(const std::string &path, std::vector<CDependencyParse *> &sentences) {
  std::ifstream is(path.c_str());
  assert(is.is_open());

  CDependencyParse *sent = new CDependencyParse();
  while (is >> *sent) {
    sentences.push_back(sent);
    sent = new CDependencyParse();
  }
  delete sent;
}


static void
shard_sentences(const std::vector<CDependencyParse *> &sentences, std::vector<std::vector<CDependencyParse *>> &shards, const unsigned nthreads) {
  unsigned int t;
  for (t = 0; t != nthreads; ++t)
    shards[t].clear();
  t = 0;
  for (CDependencyParse *sent : sentences) {
    shards[t++].push_back(sent);
    if (t == nthreads)
      t = 0;
  }
}


static std::string
temp_model_path(const std::string &sModelPath, const unsigned int thread) {
  std::ostringstream path;
  path << sModelPath << "." << thread;
  return path.str();
}


static void
combine_partial_models(const std::string &sModelPath, std::vector<depparser::CWeight *> &weights, const std::vector<unsigned int> &nerrors, const unsigned int nthreads) {
  // Compute the total number of errors.
  const unsigned int nerrors_total = std::accumulate(nerrors.begin(), nerrors.end(), 0);
  std::cout << "errors=" << nerrors_total << std::flush;

  // Remove the model file before constructing the weights object over it so that the weights are zero to start off with.
  const std::string temp_model_path_0 = temp_model_path(sModelPath, 0);
  std::remove(sModelPath.c_str());

  // If we only have one thread, don't do anything complex.
  if (nthreads > 1) {
    // Function for combining two sets of weights together.
    const auto &fn = [&](const unsigned int t, const unsigned int delta) {
      weights[t]->combineAdd(*weights[t + delta]);
      delete weights[t + delta];
      std::cout << "." << std::flush;
    };

    // Add the weights in parallel.
    for (unsigned int delta = 1; ; delta *= 2) {
      std::cout << " | " << std::flush;

      // Fire up the appropriate threads and wait for them to finish.
      std::vector<std::thread> threads;
      for (unsigned int t = 0; t != nthreads; ++t)
        if (t % (2*delta) == 0 && t + delta < nthreads)
          threads.push_back(std::thread(fn, t, delta));
      for (auto &thread : threads)
        thread.join();

      // Have we combined all of the weights?
      if (delta >= nthreads)
        break;
    }
    std::cout << " > " << std::flush;

    // Divive through the resultant summed weights by the number of processes (the last step in the averaging).
    weights[0]->combineDiv(nthreads);
    weights[0]->saveScores();
    std::cout << "<" << std::flush;
  }
  // Rename the 0th partial weights file to the main model file.
  delete weights[0];
  std::rename(temp_model_path_0.c_str(), sModelPath.c_str());
  std::cout << std::endl;
}


/*===============================================================
 *
 * auto_train - train by the parser itself, black-box training
 *
 *===============================================================*/
static void
auto_train(const std::string &sInputPath, const std::string &sModelPath, CConfigurations &configurations, const unsigned int niterations, const unsigned int nthreads) {
  bool bCoNLL = configurations.getConfiguration("c").empty() ? false : true;
  std::string sSuperPath = configurations.getConfiguration("p");
  bool bRules = configurations.getConfiguration("r").empty() ? false : true;
  bool bExtract = false;
#ifdef SUPPORT_FEATURE_EXTRACTION
  bExtract = configurations.getConfiguration("f").empty() ? false : true;
#endif
  std::string sMetaPath;
#ifdef SUPPORT_META_FEATURE_DEFINITION
  sMetaPath = configurations.getConfiguration("t");
#endif

  // Read in the input.
  std::cout << "Reading in the training data..." << std::flush;
  std::vector<CDependencyParse *> all_sentences;
  read_input_file(sInputPath, all_sentences);
  std::cout << " found " << all_sentences.size() << " sentences." << std::endl;

  // Arrays of per-thread data.
  std::vector<std::vector<CDependencyParse *>> sharded_sentences(nthreads);
  std::vector<unsigned int> nerrors(nthreads);
  std::vector<depparser::CWeight *> weights(nthreads);

  // The per-thread training function.
  const auto &fn = [&](const unsigned int t) {
    // Construct the parser.
    CDepParser parser(weights[t], bCoNLL);
    parser.setRules(bRules);
#ifdef SUPPORT_META_FEATURE_DEFINITION
    if (!sMetaPath.empty())
      parser.loadMeta(sMetaPath);
#endif

    const size_t max_n = sharded_sentences[0].size();
    const std::vector<CDependencyParse *> &sentences = sharded_sentences[t];

    // Train on each sentence.
    for (size_t n = 0; n != max_n; ++n) {
      // Train on the next sentence in this shard, if it exists.
      if (n < sentences.size()) {
        CDependencyParse &sent = *sentences[n];
        if (bExtract) {
#ifdef SUPPORT_FEATURE_EXTRACTION
          parser.extract_features(sent);
#else
          ASSERT(false, "Internal error: feature extract not allowed but option set.");
#endif
        }
        else {
          parser.train(sent, n + 1);
        }
      }
    }

    parser.finishtraining();
    parser.setWeights(nullptr);
    nerrors[t] = parser.getTotalTrainingErrors();
  };


  // Run each iteration of the perceptron learning.
  for (unsigned int iteration = 1; iteration <= niterations; ++iteration) {
    // Shuffle the sentence order between each iteration.
    //const auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    //std::shuffle(all_sentences.begin(), all_sentences.end(), std::default_random_engine(seed));

    // Partition the sentences into shards.
    shard_sentences(all_sentences, sharded_sentences, nthreads);

    // Allocate the weights.
    for (unsigned int t = 0; t != nthreads; ++t)
      weights[t] = new depparser::CWeight(sModelPath, temp_model_path(sModelPath, t), true);

    // Run this iteration over the sharded sentences.
    std::cout << "[" << std::time(nullptr) << "][Iteration " << iteration << "] Started." << std::endl ; std::cout.flush();
    if (nthreads == 1)
      fn(0);
    else {
      std::vector<std::thread> threads;
      for (unsigned int t = 0; t != nthreads; ++t)
        threads.push_back(std::thread(fn, t));
      for (unsigned int t = 0; t != nthreads; ++t)
        threads[t].join();
    }

    // Combine the partial models together.
    std::cout << "[" << std::time(nullptr) << "][Iteration " << iteration << "] Combining partial models." << std::endl;
    combine_partial_models(sModelPath, weights, nerrors, nthreads);
    std::cout << "[" << std::time(nullptr) << "][Iteration " << iteration << "] Completed." << std::endl;
  }

  // Free up memory.
  for (auto &sent : all_sentences)
    delete sent;
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
    configurations.defineConfiguration("c", "", "process CoNLL format", "");
    configurations.defineConfiguration("p", "path", "supertags", "");
    configurations.defineConfiguration("r", "", "use rules", "");
#ifdef SUPPORT_FEATURE_EXTRACTION
    configurations.defineConfiguration("f", "", "extract features only: weights will be counts", "");
#endif
#ifdef SUPPORT_META_FEATURE_DEFINITION
    configurations.defineConfiguration("t", "path", "meta feature types", "");
#endif
    if (options.args.size() != 5) {
      std::cout << "\nUsage: " << argv[0] << " training_data model niterations nthreads" << std::endl ;
      std::cout << configurations.message() << std::endl;
      return 1;
    }
    configurations.loadConfigurations(options.opts);

    int niterations;
    if (!fromString(niterations, options.args[3])) {
      std::cerr << "Error: the number of training iterations must be an integer." << std::endl;
      return 1;
    }

    int nthreads;
    if (!fromString(nthreads, options.args[4])) {
      std::cerr << "Error: the number of threads must be an integer." << std::endl;
      return 1;
    }

    std::cout << "Training started" << std::endl;

    // Start training, capturing beginning and end wall clock and processor times.
    const time_t time_start = std::time(nullptr);
    const clock_t clock_start = std::clock();
    auto_train(options.args[1], options.args[2], configurations, niterations, nthreads);
    const clock_t clock_end = std::clock();
    const time_t time_end = std::time(nullptr);

    // Output the total elapsed time.
    const double clock_elapsed = static_cast<double>(clock_end - clock_start)/CLOCKS_PER_SEC;
    const double time_elapsed = time_end - time_start;
    std::cout << "Training has finished successfully. Total time taken: cpu=" << clock_elapsed << " wall=" << time_elapsed << std::endl;

    return 0;
  }
  catch (const std::string &e) {
    std::cerr << std::endl << "Error: " << e << std::endl;
    return 1;
  }
}

