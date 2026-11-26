/*
 * NSC station metadata reader, based on MS-MSB sections 2.2.1 and 2.2.1.3.
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 or any later version.
 */
#ifndef WATER_NSC_H
#define WATER_NSC_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define NSC_MAX_SIZE (1024u * 1024u)

enum nsc_result { NSC_OK, NSC_INVALID, NSC_UNSUPPORTED, NSC_NOMEM };

struct nsc_station
{
    char address[16];
    unsigned int port, formats;
};

static inline uint32_t nsc_be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static inline int nsc_digit(unsigned char c)
{
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if(c >= 'a' && c <= 'z') return c - 'a' + 36;
    if(c == '{') return 62;
    if(c == '}') return 63;
    return -1;
}

/* Decode into an allocation bounded by the actual input, not its length field.
 * On success the caller owns the buffer, including the nine-byte header. */
static inline enum nsc_result nsc_decode(const char *value, unsigned char **out,
        uint32_t *key, size_t *size)
{
    size_t len = strlen(value), i, used = 0;
    unsigned int bits = 0, acc = 0;
    unsigned char *buf, crc = 0;
    int digit;

    *out = NULL;
    *size = 0;
    *key = 0;
    if(len < 14 || len > NSC_MAX_SIZE || value[0] != '0' || value[1] != '2')
        return NSC_INVALID;
    if(!(buf = malloc((len - 2) * 6 / 8)))
        return NSC_NOMEM;
    for(i = 2; i < len; ++i) {
        if((digit = nsc_digit(value[i])) < 0) goto invalid;
        acc = (acc << 6) | digit;
        bits += 6;
        if(bits >= 8) {
            bits -= 8;
            buf[used++] = acc >> bits;
            acc &= (1u << bits) - 1;
        }
    }
    if(acc || bits >= 6 || used < 9 || nsc_be32(buf + 5) != used - 9)
        goto invalid;
    for(i = 0; i < used; ++i) crc ^= buf[i];
    if(crc) goto invalid;
    *key = nsc_be32(buf + 1);
    *size = used - 9;
    *out = buf;
    return NSC_OK;
invalid:
    free(buf);
    return NSC_INVALID;
}

/* The fields interpreted by this initial reader (version and IPv4 address)
 * are ASCII even when represented in the encoded UTF-16LE form. */
static inline enum nsc_result nsc_text(const char *value, char *out, size_t capacity)
{
    unsigned char *buf;
    uint32_t key;
    size_t i, size;
    enum nsc_result ret;

    if(strncmp(value, "02", 2)) {
        size = strlen(value);
        if(size >= capacity) return NSC_INVALID;
        memcpy(out, value, size + 1);
        return NSC_OK;
    }
    if((ret = nsc_decode(value, &buf, &key, &size)) != NSC_OK) return ret;
    ret = NSC_INVALID;
    if(key || size < 2 || size % 2 || size / 2 > capacity ||
            buf[9 + size - 2] || buf[9 + size - 1]) goto done;
    for(i = 0; i < size - 2; i += 2) {
        if(buf[9 + i] < 32 || buf[9 + i] > 126 || buf[10 + i]) goto done;
        out[i / 2] = buf[9 + i];
    }
    out[i / 2] = 0;
    ret = NSC_OK;
done:
    free(buf);
    return ret;
}

static inline int nsc_ipv4_multicast(const char *s)
{
    unsigned int i, octet, digits;
    for(i = 0; i < 4; ++i) {
        octet = digits = 0;
        while(*s >= '0' && *s <= '9') {
            if(++digits > 3) return 0;
            octet = octet * 10 + *s++ - '0';
        }
        if(!digits || octet > 255 || (!i && (octet < 224 || octet > 239)))
            return 0;
        if(i == 3) return !*s;
        if(*s++ != '.') return 0;
    }
    return 0;
}

static inline int nsc_port(const char *s, unsigned int *port)
{
    unsigned int i, n = 0;
    int d;

    if(strlen(s) != 10 || strncmp(s, "0x", 2)) return 0;
    for(i = 2; i < 10; ++i) {
        d = nsc_digit(s[i]);
        if(d >= 36 && d <= 41) d -= 26;
        if(d < 0 || d > 15) return 0;
        n = (n << 4) | d;
    }
    if(!n || n > 65535) return 0;
    *port = n;
    return 1;
}

static inline int nsc_format_name(const char *s)
{
    if(strncmp(s, "Format", 6)) return 0;
    s += 6;
    if(!*s) return 0;
    while(*s >= '0' && *s <= '9') ++s;
    return !*s;
}

/* Metadata inspection only: not a full ABNF validator or ASF demuxer.
 * Recognize the documented version 3.0 subset; ignore optional properties
 * rather than using them to initiate network connections or logging. */
static inline enum nsc_result nsc_parse(char *data, size_t size, struct nsc_station *station)
{
    static const unsigned char asf_guid[] = {
        0x30,0x26,0xb2,0x75,0x8e,0x66,0xcf,0x11,0xa6,0xd9,0,0xaa,0,0x62,0xce,0x6c
    };
    unsigned char ids[256] = {0}, *block;
    unsigned int section = 0, have_version = 0;
    char *line, *end, *value, *tail, version[16];
    enum nsc_result ret;
    uint32_t key;
    size_t i, payload;

    memset(station, 0, sizeof(*station));
    if(!size || size > NSC_MAX_SIZE || data[size]) return NSC_INVALID;
    for(i = 0; i < size; ++i)
        if(data[i] != '\r' && data[i] != '\n' && data[i] != '\t' &&
                ((unsigned char)data[i] < 32 || (unsigned char)data[i] > 126))
            return NSC_INVALID;

    for(line = data; *line; line = end + 2) {
        if(!(end = strstr(line, "\r\n"))) return NSC_INVALID;
        *end = 0;
        if(strchr(line, '\r') || strchr(line, '\n')) return NSC_INVALID;
        if(!strcmp(line, "[Address]") && !section) { section = 1; continue; }
        if(!strcmp(line, "[Formats]") && section == 1) { section = 2; continue; }
        if(!section || !(value = strchr(line, '='))) return NSC_INVALID;
        tail = value++;
        while(tail > line && (tail[-1] == ' ' || tail[-1] == '\t')) --tail;
        *tail = 0;
        while(*value == ' ' || *value == '\t') ++value;

        if(section == 1 && !strcmp(line, "IP Address")) {
            if(station->address[0]) return NSC_INVALID;
            if((ret = nsc_text(value, station->address, sizeof(station->address))) != NSC_OK)
                return ret;
            if(!nsc_ipv4_multicast(station->address)) return NSC_INVALID;
        }else if(section == 1 && !strcmp(line, "IP Port")) {
            if(station->port || !nsc_port(value, &station->port)) return NSC_INVALID;
        }else if(section == 1 && !strcmp(line, "NSC Format Version")) {
            if(have_version++) return NSC_INVALID;
            if((ret = nsc_text(value, version, sizeof(version))) != NSC_OK) return ret;
            if(strcmp(version, "3.0")) return NSC_UNSUPPORTED;
        }else if(section == 2 && nsc_format_name(line)) {
            if((ret = nsc_decode(value, &block, &key, &payload)) != NSC_OK) return ret;
            /* Check the envelope, not the internal ASF object tree. */
            if(key > 2047 || payload < 80 || memcmp(block + 9, asf_guid, sizeof(asf_guid))
                    || (ids[key / 8] & (1u << (key % 8)))) {
                free(block);
                return NSC_INVALID;
            }
            ids[key / 8] |= 1u << (key % 8);
            ++station->formats;
            free(block);
        }
    }
    return section == 2 && station->address[0] && station->port && station->formats
            ? NSC_OK : NSC_INVALID;
}
#endif
