#ifndef ARG_CONFIGURATION_H
#define ARG_CONFIGURATION_H

#include <iostream>
#include <string>

#include "optionparser.h"

struct Arg : public option::Arg {
    static void print_error(const char *msg1, const option::Option &opt, const char *msg2)
    {
        fprintf(stderr, "%s", msg1);
        fwrite(opt.name, opt.namelen, 1, stderr);
        fprintf(stderr, "%s", msg2);
    }

    static option::ArgStatus Unknown(const option::Option &option, bool msg)
    {
        if (msg) {
            print_error("Unknown option '", option, "'\n");
        }
        return option::ARG_ILLEGAL;
    }

    static option::ArgStatus Required(const option::Option &option, bool msg)
    {
        if (option.arg != 0 && option.arg[0] != 0) {
            return option::ARG_OK;
        }

        if (msg) {
            print_error("Option '", option, "' requires an argument\n");
        }
        return option::ARG_ILLEGAL;
    }

    static option::ArgStatus Numeric(const option::Option &option, bool msg)
    {
        char *endptr = 0;
        if (option.arg != 0 && strtol(option.arg, &endptr, 10)) {
        }
        if (endptr != option.arg && *endptr == 0) {
            return option::ARG_OK;
        }

        if (msg) {
            print_error("Option '", option, "' requires a numeric argument\n");
        }
        return option::ARG_ILLEGAL;
    }

    static option::ArgStatus String(const option::Option &option, bool msg)
    {
        if (option.arg != 0) {
            return option::ARG_OK;
        }
        if (msg) {
            print_error("Option '", option, "' requires a string argument\n");
        }
        return option::ARG_ILLEGAL;
    }
};

enum optionIndex { UNKNOWN_OPT, HELP, DOMAIN_ID, N_BINS, T_INTERVAL, DUMP_FILE, RESET };

const option::Descriptor usage[] = {
    {UNKNOWN_OPT, 0, "", "", Arg::None, "\nUsage: monitor [options]\n\nOptions:"},
    {HELP, 0, "h", "help", Arg::None, "  -h \t--help  \tProduce help message."},
    {DOMAIN_ID, 0, "d", "domain", Arg::Numeric,
     "  -d <id> \t--domain=<id>  \tDDS domain ID (Default: 0)."},
    {N_BINS, 0, "b", "bins", Arg::Numeric,
     "  -b <num> \t--bins=<num>  \tNumber of bins in which a time interval is divided (Default: 1)"
     " (0 => no mean calculation, return raw data)."},
    {T_INTERVAL, 0, "t", "time", Arg::Numeric,
     "  -t <num> \t--time=<num>  \tDuration in seconds of each time frame (Default: 3000)(ms)."},
    {DUMP_FILE, 0, "f", "dump-file", Arg::String,
     "  -f \t--dump-file  \tIf set, path where the dump file will be stored (timestamp will be "
     "added to file name)."},
    {RESET, 0, "r", "reset", Arg::None,
     "  -r \t--reset  \tIf set, the Monitor will remove internal data and inactive entities every "
     "time interval ."},

    {0, 0, 0, 0, 0, 0}};

void print_warning(std::string type, const char *opt)
{
    std::cerr << "WARNING: " << opt << " is a " << type << " option, ignoring argument."
              << std::endl;
}

#endif 
