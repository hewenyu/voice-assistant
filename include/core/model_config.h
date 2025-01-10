#pragma once

#include <string>

struct ModelConfig {
    // Model paths
    std::string model_path;
    std::string tokens_path;
    
    // Model parameters
    std::string language;
    bool use_itn;
    int num_threads;
    std::string provider;
    bool debug;
    std::string decoding_method;

    // Default constructor with reasonable defaults
    ModelConfig() 
        : language("auto")
        , use_itn(true)
        , num_threads(4)
        , provider("cpu")
        , debug(false)
        , decoding_method("greedy_search") {}

    // Constructor with model paths
    ModelConfig(const std::string& model, const std::string& tokens)
        : model_path(model)
        , tokens_path(tokens)
        , language("auto")
        , use_itn(true)
        , num_threads(4)
        , provider("cpu")
        , debug(false)
        , decoding_method("greedy_search") {}
}; 