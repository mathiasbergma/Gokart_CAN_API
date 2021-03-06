/*
 * gps.h
 *
 *  Created on: 31 Mar 2022
 *      Author: michaelhynes
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <time.h>
#include <stdlib.h>

int hex2int(char *c);
int checksum_valid(char *string);
int parse_comma_delimited_str(char *string, char **fields, int max_fields);
int debug_print_fields(int numfields, char **fields);
int OpenGPSPort(const char *devname);
int SetTime(char *date, char *time);

int debug_print_fields(int numfields, char **fields);

int hexchar2int(char c);

int hex2int(char *c);

int checksum_valid(char *string);

int parse_comma_delimited_str(char *string, char **fields, int max_fields);

int SetTime(char *date, char *time);

int OpenGPSPort(const char *devname);

