#include "src/Program/Program.h"

int main() {
    Chrivent::Program program;
    if (!program.Run())
        return 1;
    return 0;
}
