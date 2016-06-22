#include "automaton.hh"
#include "util/usage.hh"

namespace {
void CheckEqual(const lm::FullScoreReturn& lhs, const lm::FullScoreReturn& rhs) {
#ifdef NDEBUG
#define AUTOMATON_NDEBUG_WAS_SET
#undef NDEBUG
#endif
    assert(lhs.prob == rhs.prob);
    assert(lhs.independent_left == rhs.independent_left);
    assert(lhs.ngram_length == rhs.ngram_length);
    assert(lhs.rest == rhs.rest);
#ifdef AUTOMATON_NDEBUG_WAS_SET
#define NDEBUG
#undef AUTOMATON_NDEBUG_WAS_SET
#endif
}
}

template <typename Callback>
void PipelineScore(lm::Pipeline<Callback>& pipeline, const lm::ngram::ProbingModel& model, char* test_file) {
    util::FilePiece in(test_file);
    StringPiece word;
    auto time = util::CPUTime();

    //start timer
    while (true) {
        if (in.ReadWordSameLine(word)) {
            lm::WordIndex vocab = model.GetVocabulary().Index(word);
            pipeline.FullScore(model.BeginSentenceState(), vocab);
        }

        while(in.ReadWordSameLine(word)) {
            lm::WordIndex vocab = model.GetVocabulary().Index(word);
            pipeline.AppendWord(vocab);
        }

        try {
            UTIL_THROW_IF('\n' != in.get(), util::Exception, "FilePiece is confused.");
        } catch (const util::EndOfFileException &e) { break; }
        
        pipeline.AppendWord(model.GetVocabulary().EndSentence());
    }
    pipeline.Drain();
    //stop timer
    time = util::CPUTime() - time;
    std::cout << time << " ";
}

void ModelScore(const lm::ngram::ProbingModel& model, char * test_file){
    util::FilePiece in(test_file);
    StringPiece word;
    lm::ngram::State in_state, out_state;
    auto score = 0.0;
    auto time = util::CPUTime();

    //start timer
    while (true) {
        if (in.ReadWordSameLine(word)) {
            lm::WordIndex vocab = model.GetVocabulary().Index(word);
            auto ret = model.FullScore(model.BeginSentenceState(), vocab, out_state); 
            score += ret.prob;
            in_state = out_state;
        }

        while(in.ReadWordSameLine(word)) {
            lm::WordIndex vocab = model.GetVocabulary().Index(word);
            auto ret = model.FullScore(in_state, vocab, out_state);
            score += ret.prob;
            in_state = out_state;
        }

        try {
            UTIL_THROW_IF('\n' != in.get(), util::Exception, "FilePiece is confused.");
        } catch (const util::EndOfFileException &e) { break; }

        auto ret = model.FullScore(in_state, model.GetVocabulary().EndSentence(), out_state);
        score += ret.prob;
    }
    //stop timer
    time = util::CPUTime() - time;
    std::cerr << "Score (model): " << score << std::endl;
    std::cout << time << " ";
}

int main(int argc, char* argv[]){
    if (argc < 6) {
        std::cerr << argv[0] <<" pipeline_size_start pipeline_size_end model_file test_file <probing|pipeline>" << std::endl;
        return 1;
    }
    int pipeline_size_start = std::stoi(std::string(argv[1]));
    int pipeline_size_end = std::stoi(std::string(argv[2]));
    char* arpa_file(argv[3]);
    char* test_file(argv[4]);
    std::string type(argv[5]);

    lm::ngram::Config config;
    config.arpa_complain = lm::ngram::Config::ALL;
    config.messages = &std::cout;
    config.positive_log_probability = lm::SILENT;
    config.probing_multiplier = 1.5;
    lm::ngram::ProbingModel model(arpa_file, config);

    if (type == "probing") ModelScore(model, test_file);
    else if (type == "pipeline") {
        auto score = 0.0;
        const auto callback = [&score](const lm::FullScoreReturn& r){score += r.prob;};
        typename lm::ngram::NGramAutomaton<lm::ngram::BackoffValue, decltype(callback)>::Construct construct{model.GetSearch(), callback};
        for (std::size_t pipeline_size = pipeline_size_start; pipeline_size <= pipeline_size_end; ++pipeline_size) {
            score = 0.0;
            lm::Pipeline<decltype(callback)> pipeline(pipeline_size, construct);
            PipelineScore(pipeline, model, test_file);
            std::cerr << "Score (pipeline): " << score << std::endl;
        }
    }
    std::cout << std::endl;

    
}
