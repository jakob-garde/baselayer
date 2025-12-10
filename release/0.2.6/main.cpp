#include "jg_baselayer.h"


int main (int argc, char **argv) {
    printf("Compile test for baselayer %d.%d.%d\n", BASELAYER_VERSION_MAJOR, BASELAYER_VERSION_MINOR, BASELAYER_VERSION_PATCH);

    BaselayerAssertVersion(0, 2, 6);
}
