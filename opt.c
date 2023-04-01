#define _GNU_SOURCE
#include "opt.h"
#include "dnl.h"
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CHINADNS_VERSION "ChinaDNS-NG 2023.04.01 <https://github.com/zfl9/chinadns-ng>"

bool    g_verbose       = false;
bool    g_reuse_port    = false;
bool    g_noip_as_chnip = false; /* default: see as not-china-ip */
uint8_t g_noaaaa_query  = 0; /* disable AAAA query (bit flags) */
uint8_t g_default_tag   = NAME_TAG_NONE;

const char *g_gfwlist_fname = NULL; /* gfwlist filename(s) "a.txt,b.txt,..." */
const char *g_chnlist_fname = NULL; /* chnlist filename(s) "m.txt,n.txt,..." */
bool        g_gfwlist_first = true; /* match gfwlist first */
bool        g_add_tagchn_ip = false; /* add the answer ip of name-tag:chn to ipset/nftset */

const char *g_ipset_name4 = "chnroute"; /* ipset:"set_name" | nftset:"family_name@table_name@set_name" */
const char *g_ipset_name6 = "chnroute6"; /* ipset:"set_name" | nftset:"family_name@table_name@set_name" */

const char *g_bind_ipstr  = "127.0.0.1";
portno_t    g_bind_portno = 65353;
skaddr_u    g_bind_skaddr;

const char *g_remote_ipports[SERVER_MAXCNT];
skaddr_u    g_remote_skaddrs[SERVER_MAXCNT];
int         g_upstream_timeout_sec          = 5;
uint8_t     g_repeat_times                  = 1; /* used by trust-dns only */

#define OPT_BIND_ADDR 'b'
#define OPT_BIND_PORT 'l'
#define OPT_CHINA_DNS 'c'
#define OPT_TRUST_DNS 't'
#define OPT_IPSET_NAME4 '4'
#define OPT_IPSET_NAME6 '6'
#define OPT_GFWLIST_FILE 'g'
#define OPT_CHNLIST_FILE 'm'
#define OPT_DEFAULT_TAG 'd'
#define OPT_TIMEOUT_SEC 'o'
#define OPT_REPEAT_TIMES 'p'
#define OPT_CHNLIST_FIRST 'M'
#define OPT_ADD_TAGCHN_IP 'a'
#define OPT_NO_IPV6 'N'
#define OPT_FAIR_MODE 'f'
#define OPT_REUSE_PORT 'r'
#define OPT_NOIP_AS_CHNIP 'n'
#define OPT_VERBOSE 'v'
#define OPT_VERSION 'V'
#define OPT_HELP 'h'

static const char s_shortopts[] = {
    ':', /* return ':' if argument missing */
    OPT_BIND_ADDR, ':', /* required_argument */
    OPT_BIND_PORT, ':', /* required_argument */
    OPT_CHINA_DNS, ':', /* required_argument */
    OPT_TRUST_DNS, ':', /* required_argument */
    OPT_IPSET_NAME4, ':', /* required_argument */
    OPT_IPSET_NAME6, ':', /* required_argument */
    OPT_GFWLIST_FILE, ':', /* required_argument */
    OPT_CHNLIST_FILE, ':', /* required_argument */
    OPT_DEFAULT_TAG, ':', /* required_argument */
    OPT_TIMEOUT_SEC, ':', /* required_argument */
    OPT_REPEAT_TIMES, ':', /* required_argument */
    OPT_NO_IPV6, ':', ':', /* optional_argument */
    OPT_CHNLIST_FIRST, /* no_argument */
    OPT_ADD_TAGCHN_IP, /* no_argument */
    OPT_FAIR_MODE, /* no_argument */
    OPT_REUSE_PORT, /* no_argument */
    OPT_NOIP_AS_CHNIP, /* no_argument */
    OPT_VERBOSE, /* no_argument */
    OPT_VERSION, /* no_argument */
    OPT_HELP, /* no_argument */
    '\0',
};

static const struct option s_options[] = {
    {"bind-addr",     required_argument, NULL, OPT_BIND_ADDR},
    {"bind-port",     required_argument, NULL, OPT_BIND_PORT},
    {"china-dns",     required_argument, NULL, OPT_CHINA_DNS},
    {"trust-dns",     required_argument, NULL, OPT_TRUST_DNS},
    {"ipset-name4",   required_argument, NULL, OPT_IPSET_NAME4},
    {"ipset-name6",   required_argument, NULL, OPT_IPSET_NAME6},
    {"gfwlist-file",  required_argument, NULL, OPT_GFWLIST_FILE},
    {"chnlist-file",  required_argument, NULL, OPT_CHNLIST_FILE},
    {"default-tag",   required_argument, NULL, OPT_DEFAULT_TAG},
    {"timeout-sec",   required_argument, NULL, OPT_TIMEOUT_SEC},
    {"repeat-times",  required_argument, NULL, OPT_REPEAT_TIMES},
    {"no-ipv6",       optional_argument, NULL, OPT_NO_IPV6},
    {"chnlist-first", no_argument,       NULL, OPT_CHNLIST_FIRST},
    {"add-tagchn-ip", no_argument,       NULL, OPT_ADD_TAGCHN_IP},
    {"fair-mode",     no_argument,       NULL, OPT_FAIR_MODE},
    {"reuse-port",    no_argument,       NULL, OPT_REUSE_PORT},
    {"noip-as-chnip", no_argument,       NULL, OPT_NOIP_AS_CHNIP},
    {"verbose",       no_argument,       NULL, OPT_VERBOSE},
    {"version",       no_argument,       NULL, OPT_VERSION},
    {"help",          no_argument,       NULL, OPT_HELP},
    {NULL,            0,                 NULL, 0},
};

static void show_help(void) {
    printf("usage: chinadns-ng <options...>. the existing options are as follows:\n"
           " -b, --bind-addr <ip-address>         listen address, default: 127.0.0.1\n"
           " -l, --bind-port <port-number>        listen port number, default: 65353\n"
           " -c, --china-dns <ip[#port],...>      china dns server, default: <114DNS>\n"
           " -t, --trust-dns <ip[#port],...>      trust dns server, default: <GoogleDNS>\n"
           " -4, --ipset-name4 <ipv4-setname>     ipset ipv4 set name, default: chnroute\n"
           " -6, --ipset-name6 <ipv6-setname>     ipset ipv6 set name, default: chnroute6\n"
           "                                      if it contains @, then use nftables set\n"
           "                                      format: family_name@table_name@set_name\n"
           " -g, --gfwlist-file <path,...>        path(s) of gfwlist, '-' indicate stdin\n"
           " -m, --chnlist-file <path,...>        path(s) of chnlist, '-' indicate stdin\n"
           " -d, --default-tag <name-tag>         domain default tag: gfw,chn,none(default)\n"
           " -o, --timeout-sec <query-timeout>    timeout of the upstream dns, default: 5\n"
           " -p, --repeat-times <repeat-times>    it is only used for trustdns, default: 1\n"
           " -N, --no-ipv6=[rules]                filter AAAA query, rules can be a seq of:\n"
           "                                      rule a: filter all domain name (default)\n"
           "                                      rule g: filter the name with tag gfw\n"
           "                                      rule m: filter the name with tag chn\n"
           "                                      rule n: filter the name with tag none\n"
           "                                      rule c: do not forward to china upstream\n"
           "                                      rule t: do not forward to trust upstream\n"
           "                                      rule C: check answer ip of china upstream\n"
           "                                      rule T: check answer ip of trust upstream\n"
           "                                      if no rules is given, it defaults to 'a'\n"
           " -M, --chnlist-first                  match chnlist first, default: <disabled>\n"
           " -a, --add-tagchn-ip                  add the ip of name-tag:chn to ipset/nftset\n"
           " -f, --fair-mode                      enable fair mode (nop, only fair mode now)\n"
           " -r, --reuse-port                     enable SO_REUSEPORT, default: <disabled>\n"
           " -n, --noip-as-chnip                  accept reply without ipaddr (A/AAAA query)\n"
           " -v, --verbose                        print the verbose log, default: <disabled>\n"
           " -V, --version                        print `chinadns-ng` version number and exit\n"
           " -h, --help                           print `chinadns-ng` help information and exit\n"
           "bug report: https://github.com/zfl9/chinadns-ng. email: zfl9.com@gmail.com (Otokaze)\n"
    );
}

#define err_exit(fmt, args...) ({ \
    printf("[%s] " fmt "\n", __func__, ##args); \
    show_help(); \
    exit(1); \
})

static void parse_upstream_addrs(const char *arg, bool is_chinadns) {
    int cnt = 0;
    int has_next = 1;

    do {
        if (++cnt > SERVER_GROUP_CNT)
            err_exit("%s dns servers max count is %d", is_chinadns ? "china" : "trust", SERVER_GROUP_CNT);

        const char *d = strchr(arg, ',');
        size_t len = d ? (size_t)(d - arg) : strlen(arg);

        if (len + 1 > IP_PORT_STRLEN)
            err_exit("server addr max length is %d: %.*s", IP_PORT_STRLEN - 1, (int)len, arg);

        /* tmp buffer */
        char ipstr[IP_PORT_STRLEN];
        memcpy(ipstr, arg, len);
        ipstr[len] = '\0';

        /* g_remote_ipports */
        char *addr = malloc(len + 1);
        memcpy(addr, arg, len);
        addr[len] = '\0';

        if (d)
            arg += len + 1;
        else
            has_next = 0;

        portno_t port = 53;
        char *port_str = strchr(ipstr, '#');
        if (port_str) {
            *port_str++ = 0;
            if ((port = strtoul(port_str, NULL, 10)) == 0)
                err_exit("invalid server port number: %s", port_str);
        }

        int family = get_ipstr_family(ipstr);
        if (family == -1)
            err_exit("invalid server ip address: %s", ipstr);

        int idx = (is_chinadns ? CHINADNS1_IDX : TRUSTDNS1_IDX) + cnt - 1;
        build_socket_addr(family, &g_remote_skaddrs[idx], ipstr, port);
        g_remote_ipports[idx] = addr;
    } while (has_next);
}

static void parse_noaaaa_rules(const char *rules) {
    if (!rules) {
        g_noaaaa_query = NOAAAA_ALL;
        return;
    }

    if (*rules == '=')
        ++rules;

    if (strlen(rules) <= 0)
        err_exit("'-N/--no-ipv6' requires an argument");

    for (const char *c = rules; *c; ++c) {
        switch (*c) {
            case 'a':
                g_noaaaa_query = NOAAAA_ALL;
                break;
            case 'g':
                g_noaaaa_query |= NOAAAA_TAG_GFW;
                break;
            case 'm':
                g_noaaaa_query |= NOAAAA_TAG_CHN;
                break;
            case 'n':
                g_noaaaa_query |= NOAAAA_TAG_NONE;
                break;
            case 'c':
                g_noaaaa_query |= NOAAAA_CHINA_DNS;
                break;
            case 't':
                g_noaaaa_query |= NOAAAA_TRUST_DNS;
                break;
            case 'C':
                g_noaaaa_query |= NOAAAA_CHINA_IPCHK;
                break;
            case 'T':
                g_noaaaa_query |= NOAAAA_TRUST_IPCHK;
                break;
            default:
                err_exit("invalid no-aaaa rule: '%c'", *c);
                break;
        }
    }

    /* try simplify to NOAAAA_ALL */
    if (!is_filter_all_v6(g_noaaaa_query)) {
        if ((g_noaaaa_query & NOAAAA_TAG_GFW) && (g_noaaaa_query & NOAAAA_TAG_CHN) && (g_noaaaa_query & NOAAAA_TAG_NONE))
            g_noaaaa_query = NOAAAA_ALL;
        else if ((g_noaaaa_query & NOAAAA_CHINA_DNS) && (g_noaaaa_query & NOAAAA_TRUST_DNS))
            g_noaaaa_query = NOAAAA_ALL;
    }
}

void opt_parse(int argc, char *argv[]) {
    opterr = 0; /* disable default error msg */

    int optindex = -1;
    int shortopt = -1;

    const char *chinadns_optarg = "114.114.114.114";
    const char *trustdns_optarg = "8.8.8.8";

    while ((shortopt = getopt_long(argc, argv, s_shortopts, s_options, &optindex)) != -1) {
        switch (shortopt) {
            case OPT_BIND_ADDR:
                if (get_ipstr_family(optarg) == -1)
                    err_exit("invalid listen ip address: %s", optarg);
                g_bind_ipstr = optarg;
                break;
            case OPT_BIND_PORT:
                if ((g_bind_portno = strtoul(optarg, NULL, 10)) == 0)
                    err_exit("invalid listen port number: %s", optarg);
                break;
            case OPT_CHINA_DNS:
                chinadns_optarg = optarg;
                break;
            case OPT_TRUST_DNS:
                trustdns_optarg = optarg;
                break;
            case OPT_IPSET_NAME4:
                g_ipset_name4 = optarg;
                break;
            case OPT_IPSET_NAME6:
                g_ipset_name6 = optarg;
                break;
            case OPT_GFWLIST_FILE:
                g_gfwlist_fname = optarg;
                break;
            case OPT_CHNLIST_FILE:
                g_chnlist_fname = optarg;
                break;
            case OPT_DEFAULT_TAG:
                if (strcmp(optarg, "gfw") == 0)
                    g_default_tag = NAME_TAG_GFW;
                else if (strcmp(optarg, "chn") == 0)
                    g_default_tag = NAME_TAG_CHN;
                else if (strcmp(optarg, "none") == 0)
                    g_default_tag = NAME_TAG_NONE;
                else
                    err_exit("invalid default domain tag: %s", optarg);
                break;
            case OPT_TIMEOUT_SEC:
                if ((g_upstream_timeout_sec = strtoul(optarg, NULL, 10)) <= 0)
                    err_exit("invalid upstream timeout sec: %s", optarg);
                break;
            case OPT_REPEAT_TIMES:
                if ((g_repeat_times = strtoul(optarg, NULL, 10)) == 0)
                    err_exit("invalid trustdns repeat times: %s", optarg);
                break;
            case OPT_NO_IPV6:
                parse_noaaaa_rules(optarg);
                break;
            case OPT_CHNLIST_FIRST:
                g_gfwlist_first = false;
                break;
            case OPT_ADD_TAGCHN_IP:
                g_add_tagchn_ip = true;
                break;
            case OPT_FAIR_MODE:
                /* no operation */
                break;
            case OPT_REUSE_PORT:
                g_reuse_port = true;
                break;
            case OPT_NOIP_AS_CHNIP:
                g_noip_as_chnip = true;
                break;
            case OPT_VERBOSE:
                g_verbose = true;
                break;
            case OPT_VERSION:
                printf(CHINADNS_VERSION "\n");
                exit(0);
                break;
            case OPT_HELP:
                show_help();
                exit(0);
                break;
            case ':':
                /* missing argument */
                err_exit("missing optarg: '%s'", argv[optind - 1]);
                break;
            case '?':
                /* unknown option */
                if (optopt) {
                    /* short opt */
                    err_exit("unknown option: '-%c'", (char)optopt);
                } else {
                    /* long opt */
                    const char *longopt = argv[optind - 1];
                    const char *p = strchr(longopt, '=');
                    int len = p ? p - longopt : (int)strlen(longopt);
                    err_exit("unknown option: '%.*s'", len, longopt);
                }
                break;
        }
    }

    build_socket_addr(get_ipstr_family(g_bind_ipstr), &g_bind_skaddr, g_bind_ipstr, g_bind_portno);
    parse_upstream_addrs(chinadns_optarg, true);
    parse_upstream_addrs(trustdns_optarg, false);
}
