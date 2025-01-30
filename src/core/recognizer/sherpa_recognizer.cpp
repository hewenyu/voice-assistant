#include "recognizer.h"
#include "sherpa_recognizer.h"

namespace core::recognizer {

std::unique_ptr<IRecognizer> createRecognizer() {
    return std::make_unique<SherpaRecognizer>();
}
} 