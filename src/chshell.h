/*
 * chshell.h
 *
 * A naive Linux shell written in pure ISO C99.
 *
 *  ------------------------------------------------------------------------------
 * Project Information
 * ------------------------------------------------------------------------------
 *
 * The shell was developed for educational purposes.
 * Specifically, it was a continuation of the development process that was
 * required as a semester project in the course:
 *     - ΗΥ1501 - Operating Systems
 *     - Prof. N. Pitsianis, Prof. G. Chasapis
 *     - Electrical & Computer Engineering Department ( ECE )
 *       Aristotle University of Thessaloniki ( AUTh ), Greece
 *
 * ------------------------------------------------------------------------------
 *
 * ------------------------------------------------------------------------------
 * Legal Information
 * ------------------------------------------------------------------------------
 *
 * MIT License

 * Copyright (c) 2018 Athanasios D. Charisoudis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ------------------------------------------------------------------------------
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zconf.h>
#include <stdbool.h>
#include <ctype.h>
#include <signal.h>
#include <wait.h>
#include <errno.h>
#include <termios.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include "termcap/src/termcap.h"

/*
 * ------------
 * Definitions
 * ------------
 *
 */
// General constants
#define VERSION "0.3"

// Pipe-only
#define READ_EDGE 0
#define WRITE_EDGE 1

// Length definitions
#define ROW_LEN_MAX 512     // maximum length of a single row in shell
#define ARG_LEN_MAX 50      // maximum length of command's individual argument
#define BUF_LEN_MAX 4096    // the output buffer ( same as max pipe size )
#define DIR_LEN_MAX 1024    // maximum length of cwd
#define SHM_LEN_MAX 1024    // 1KB

// SH_DBG_MODE is the main debugging control variable
// 0: no debugging messages
// 1: messages, states, inspection
// 2: advanced inspection
// 3: every message in code is visible to the terminal
#define SH_DBG_MODE_DEFAULT 0
#define SH_DBG_MODE_KEY "SH_DBG_MODE"

// Show working directory
#define SH_SHOW_WD_DEFAULT 0
#define SH_SHOW_WD_KEY "SH_SHOW_WD"

// Policies
// When error in row occurs abort shell?
#define SH_ON_ROW_ERR_ABRT_DEFAULT 0
#define SH_ON_ROW_ERR_ABRT_KEY "SH_ON_ROW_ERR_ABRT"

// When error in a batch file's row occurs abort shell?
#define SH_ON_FROW_ERR_ABRT_DEFAULT 0
#define SH_ON_FROW_ERR_ABRT_KEY "SH_ON_FROW_ERR_ABRT"

// When command fails search batch file?
#define SH_ON_CMD_FAIL_SEARCH_BF_DEFAULT 0
#define SH_ON_CMD_FAIL_SEARCH_BF_KEY "SH_ON_CMD_FAIL_SEARCH_BF"

/*
 * -------------
 * Define types
 * -------------
 *
 */
typedef struct sh_row_t sh_row_t;
typedef struct sh_cmd_t sh_cmd_t;
typedef struct sh_bltcmd_t sh_bltcmd_t;
typedef struct sh_rowops_t sh_rowops_t;
typedef struct sh_cmdops_t sh_cmdops_t;

/*
 * -------------
 * Create types
 * -------------
 *
 */
// Utils type
struct sh_cmdops_t {
    void ( *trim ) ( sh_cmd_t * );       // pointer to trim function
    bool ( *isempty ) ( sh_cmd_t * );    // pointer to isempty function
    bool ( *isvalid ) ( sh_cmd_t * );    // pointer to isvalid function
    void ( *parse ) ( sh_cmd_t * );      // pointer to parse function
    void ( *inspect ) ( sh_cmd_t * );    // pointer to inspect function
};
struct sh_rowops_t {
    void ( *trim ) ( sh_row_t * );       // pointer to trim function
    bool ( *iscomment ) ( sh_row_t * );  // pointer to iscomment function
    void ( *parse ) ( sh_row_t * );      // pointer to parse function
    void ( *inspect ) ( sh_row_t * );    // pointer to inspect function
    bool ( *exec ) ( sh_row_t * );       // pointer to exec function
};

// Command type
struct sh_cmd_t {
    char *cmd;      // command's name
    char **args;    // command's arguments array
    size_t nargs;   // number of arguments in command ( +2: first is command's name and last is NULL )

    // Glues
    char *glue_b;   // the glue between command and previous
    char *glue_a;   // the glue between command and next

    // Props
    bool is_prs;    // is parsed flag
    bool is_blt;    // is built-in command flag

    sh_bltcmd_t *bltcmd;    // associated built-in command ( if not built-in command, then NULL )
    sh_cmdops_t *utils;     // command utilities
};
struct sh_bltcmd_t {
    const char *cmd;                    // built-in command's name
    bool run_in_main_process;           // if should be run on main process
    bool ( *exec ) ( const sh_cmd_t * ); // pointer to the associated function to be executed
};

// Row Type
struct sh_row_t {
    char *raw;          // up to ROW_LEN_MAX bytes
    size_t ncmds;       // total commands in row
    sh_cmd_t *cmds;     // commands array
    sh_rowops_t *utils; // row utilities
};

/*
 * -----------------
 * Global Constants
 * -----------------
 *
 */
// Delimiters
const char *CMD_DEL = " \r\n\t,~`";
const char *ROW_DEL = "|&;";
const char *DEL_ARR[] = {"||", "|", "&&", "&", ";"};

// Prompt data
const char *PROMPT_MESSAGE = "charisoudis_9026";

/*
 * ---------------
 * Static Globals
 * ---------------
 *
 * Static variables should be initialized once, and from then on be used as consts
 *
 */
// Shell Initialization
static pid_t SH_PID, SH_PGID;
static struct termios SH_TMODES;
static char SH_MODE;        // 'i': interactive | 'b': batch | 'm': mixed
static bool SH_STATUS;      // main() return status
static char *SH_WD_I;       // initial working directory

// Shared memory for 1KB
static void *SH_SHARED_MEMORY_PTR;

// Util pointers
static sh_cmdops_t *cmdutils;
static sh_rowops_t *rowutils;

/*
 * -----------------
 * Global variables
 * -----------------
 *
 */
char *SH_WD;            // the current working directory as shown in the prompt
bool SH_QUIT;           // if true then in current loop's end will exit
bool SH_FORCE_QUIT;     // a warning before killing process
bool SH_EXECUTING;      // if true then a command is currently executing

/*
 * -------------------
 * Methods Definition
 * -------------------
 *
 */
// Command methods
bool sh_cmd_iscomment ( sh_cmd_t * );
bool sh_cmd_isempty ( sh_cmd_t * );

bool sh_cmd_isvalid ( sh_cmd_t * );
void sh_cmd_trim ( sh_cmd_t * );
void sh_cmd_parse ( sh_cmd_t * );
void sh_cmd_inspect ( sh_cmd_t * );

// Row Methods
void sh_row_parse ( sh_row_t * );
void sh_row_inspect ( sh_row_t * );
bool sh_row_exec ( sh_row_t * );

// Executors
bool sh_parse_exec_row ( const char * );
bool sh_exec_minor_command_set ( const sh_row_t *, size_t, size_t );
bool sh_exec_major_command_set ( const sh_row_t *, size_t, size_t );
bool sh_exec_wrapper ( sh_cmd_t *, size_t, int ** );
void sh_exec_response ( size_t, int ** );
bool sh_exec_builtin ( sh_cmd_t * );
bool sh_exec ( sh_cmd_t * );

// Utilities
size_t sh_strlen ( const char * );
size_t sh_ntokens ( const char *, const char * );
size_t sh_cmd_lt_arg_exists( sh_cmd_t * );
size_t sh_cmd_gt_arg_exists( sh_cmd_t * );
bool sh_quit ( const char * );
void sh_inspect_pipes ( int **, size_t );
char *sh_file_exists ( const char * );

// Set / Get environment variables
int sh_get_env ( const char *, int );
void sh_set_env ( const char *, int );

// Printers
void sh_prt_welcome ( void );
void sh_prt_bye ( void );

// Signal handlers
void sh_SIGUSR2_handler ( int ); // only main process listens to SIGUSR2 to update its environment
void sh_SIGCHLD_handler ( int ); // SIGCHLD received when child dies
void sh_SIGINT_handler ( int );  // SIGINT received when user presses Ctrl + C

// Operation modes
bool mode_i ( bool );

bool mode_b ( const char * );

// Built-in commands
bool sh_bltcmd_cd ( const sh_cmd_t * );
bool sh_bltcmd_set ( const sh_cmd_t * );
bool sh_bltcmd_unset ( const sh_cmd_t * );
bool sh_bltcmd_get ( const sh_cmd_t * );
bool sh_bltcmd_cls ( const sh_cmd_t * );
bool sh_bltcmd_sleep ( const sh_cmd_t * );
bool sh_bltcmd_help ( const sh_cmd_t * );
bool sh_bltcmd_exit ( const sh_cmd_t * );

/*
 * -----------------------------
 * Built-in commands definition
 * -----------------------------
 *
 */
sh_bltcmd_t SH_BUILTIN_CMDS[] = {
        {"cd",    true,  sh_bltcmd_cd},      // change directory
        {"set",   true,  sh_bltcmd_set},     // set environment variables
        {"unset", true,  sh_bltcmd_unset},   // unset environment variables
        {"get",   false, sh_bltcmd_get},     // get environment variables
        {"clear", true,  sh_bltcmd_cls},     // clear screen
        {"sleep", false, sh_bltcmd_sleep},   // clear screen
        {"help",  false, sh_bltcmd_help},    // get useful info about built in commands
        {"exit",  true,  sh_bltcmd_exit}     // similar to quit raw data
};