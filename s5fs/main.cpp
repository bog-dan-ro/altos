#include "s52k.h"

static struct options {
    const char *filename;
    int showHelp;
} options;

#define OPTION(t, p)                           \
{ t, offsetof(struct options, p), 1 }

static const struct fuse_opt option_spec[] = {
    OPTION("--file=%s", filename),
    OPTION("-h", showHelp),
    OPTION("--help", showHelp),
    FUSE_OPT_END
};

static void show_help(const char *progname)
{
    printf("usage: %s <device|file> <mountpoint>\n\n", progname);
    printf("File-system specific options:\n"
           "    --file=<s>          S5K2 file to mount\n"
           "\n");
}

int main(int argc, char *argv[])
{
    fuse_args args = FUSE_ARGS_INIT(argc, argv);
    if (fuse_opt_parse(&args, &options, option_spec, nullptr) == -1)
        return 1;

    if (options.showHelp || !options.filename) {
        show_help(argv[0]);
        return -1;
    }

    int ret;
    {
        S52K fs{options.filename};
        ret = fuse_main(args.argc, args.argv, fs.operations(), &fs);
    }
    fuse_opt_free_args(&args);
    return ret;
}
