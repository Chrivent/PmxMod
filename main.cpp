#include "src/Program/Program.h"

using Chrivent::Program;

int main() {
    Program program;
    if (!program.Run())
        return 1;
    return 0;
}
