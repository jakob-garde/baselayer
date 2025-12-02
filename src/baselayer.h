#ifndef __BASELAYER_H__
#define __BASELAYER_H__


#define BASELAYER_VERSION_MAJOR 0
#define BASELAYER_VERSION_MINOR 2
#define BASELAYER_VERSION_PATCH 6


void BaselayerAssertVersion(u32 major, u32 minor, u32 patch) {
    if (
        BASELAYER_VERSION_MAJOR != major ||
        BASELAYER_VERSION_MINOR != minor ||
        BASELAYER_VERSION_PATCH != patch
    ) {
        assert(1 == 0 && "baselayer version check failed");
    }
}

void BaselayerPrintVersion() {
    printf("%d.%d.%d\n", BASELAYER_VERSION_MAJOR, BASELAYER_VERSION_MINOR, BASELAYER_VERSION_PATCH);
}


#endif
