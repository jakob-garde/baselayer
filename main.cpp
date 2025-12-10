#include "baselayer_includes.h"
#include "test.cpp"


int main (int argc, char **argv) {
    TimeProgram;
    bool force_tests = false;

    if (CLAContainsArg("--help", argc, argv)) {
        printf("Usage: ./baselayer <args>\n");
        printf("--help:         Display help (this text)\n");
        printf("--version:      Print baselayer version\n");
        printf("--release:      Combine source files into jg_baselayer.h\n");
        printf("--test:         Run test functions\n");
        exit(0);
    }

    else if (CLAContainsArg("--test", argc, argv) || force_tests) {
        Test();
    }

    else if (CLAContainsArg("--version", argc, argv) || force_tests) {
        printf("dev: ");
        BaselayerPrintVersion();
        exit(0);
    }

    else if (CLAContainsArg("--release", argc, argv) || force_tests) {

        MArena *a_files = GetContext()->a_life;
        StrInit();

        StrLst *f_sources = NULL;
        f_sources = StrLstPush("src/base.h", f_sources);
        f_sources = StrLstPush("src/profile.h", f_sources);
        f_sources = StrLstPush("src/memory.h", f_sources);
        f_sources = StrLstPush("src/string.h", f_sources);
        f_sources = StrLstPush("src/hash.h", f_sources);
        f_sources = StrLstPush("src/utils.h", f_sources);
        f_sources = StrLstPush("src/platform.h", f_sources);
        f_sources = StrLstPush("src/init.h", f_sources);
        f_sources = StrLstPush("src/baselayer.h", f_sources);
        f_sources = f_sources->first;

        StrBuff buff = StrBuffInit();
        StrBuffPrint1K(&buff, "/*\n", 0);
        StrBuffAppend(&buff, LoadTextFile(a_files, "LICENSE"));
        StrBuffPrint1K(&buff, "*/\n\n\n", 0);
        StrBuffPrint1K(&buff, "#ifndef __JG_BASELAYER_H__\n", 0);
        StrBuffPrint1K(&buff, "#define __JG_BASELAYER_H__\n\n\n", 0);
        StrBuffPrint1K(&buff, "\n\n", 0);

        while (f_sources) {
            StrBuffAppend(&buff, LoadTextFile(a_files, f_sources->GetStr()));
            StrBuffPrint1K(&buff, "\n\n", 0);

            f_sources = f_sources->next;
        }

        StrBuffPrint1K(&buff, "#endif // __JG_BASELAYER_H__\n", 0);

        printf("Generated: jg_baselayer.h_OUT\n");
        SaveFile("jg_baselayer.h_OUT", buff.str, buff.len);
    }

    else {
        printf("Use:\n");
        printf("./baselayer --help\n");

        exit(0);
    }
}
